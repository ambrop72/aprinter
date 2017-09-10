/*
 * Copyright (c) 2016 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AIPSTACK_ETH_IP_IFACE_H
#define AIPSTACK_ETH_IP_IFACE_H

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include <aipstack/meta/Instance.h>
#include <aipstack/meta/ChooseInt.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/LoopUtils.h>
#include <aipstack/misc/Preprocessor.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/structure/LinkModel.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/structure/TimerQueue.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/common/Struct.h>
#include <aipstack/common/Buf.h>
#include <aipstack/common/SendRetry.h>
#include <aipstack/common/TxAllocHelper.h>
#include <aipstack/common/Err.h>
#include <aipstack/common/Options.h>
#include <aipstack/common/ObserverNotification.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/proto/ArpProto.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/ip/hw/IpEthHw.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/platform/TimerWrapper.h>

namespace AIpStack {

struct EthIfaceState {
    bool link_up;
};

template <typename Arg>
class EthIpIface;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template <typename Arg>
AIPSTACK_DECL_TIMERS_CLASS(EthIpIfaceTimers, typename Arg::PlatformImpl,
                           EthIpIface<Arg>, (ArpTimer))
#endif

template <typename Arg>
class EthIpIface :
    public Arg::Iface,
    private EthIpIfaceTimers<Arg>::Timers,
    private IpEthHw::HwIface,
    private NonCopyable<EthIpIface<Arg>>
{
    AIPSTACK_USE_VALS(Arg::Params, (NumArpEntries, ArpProtectCount, HeaderBeforeEth))
    AIPSTACK_USE_TYPES1(Arg::Params, (TimersStructureService))
    AIPSTACK_USE_TYPES1(Arg, (PlatformImpl, Iface))
    
    using Platform = PlatformFacade<PlatformImpl>;
    AIPSTACK_USE_TYPES1(Platform, (TimeType))
    AIPSTACK_USE_TIMERS_CLASS(EthIpIfaceTimers<Arg>, (ArpTimer))
    using EthIpIfaceTimers<Arg>::Timers::platform;
    
    using IpStack = typename Iface::IfaceIpStack;
    
    static size_t const EthArpPktSize = EthHeader::Size + ArpIp4Header::Size;
    
    // Sanity check ARP table configuration.
    static_assert(NumArpEntries > 0, "");
    static_assert(ArpProtectCount >= 0, "");
    static_assert(ArpProtectCount <= NumArpEntries, "");
    
    static int const ArpNonProtectCount = NumArpEntries - ArpProtectCount;
    
    // Get an unsigned integer type sufficient for ARP entry indexes and null value.
    using ArpEntryIndexType = ChooseIntForMax<NumArpEntries, false>;
    static ArpEntryIndexType const ArpEntryNull =
        std::numeric_limits<ArpEntryIndexType>::max();
    
    // Number of ARP resolution attempts in the Query and Refreshing states.
    static uint8_t const ArpQueryAttempts = 3;
    static uint8_t const ArpRefreshAttempts = 2;
    
    // These need to fit in 4 bits available in ArpEntry::attempts_left.
    static_assert(ArpQueryAttempts <= 15, "");
    static_assert(ArpRefreshAttempts <= 15, "");
    
    // Base ARP response timeout, doubled for each retransmission.
    static TimeType const ArpBaseResponseTimeoutTicks = 1.0 * Platform::TimeFreq;
    
    // Time after a Valid entry will go to Refreshing when used.
    static TimeType const ArpValidTimeoutTicks = 60.0 * Platform::TimeFreq;
    
    struct ArpEntry;
    struct ArpEntryTimerQueueNodeUserData;
    struct ArpEntriesAccessor;
    
    // Get the TimerQueueService type.
    using TheTimerQueueService = TimerQueueService<TimersStructureService>;
    
    // Link model for ARP entry data structures.
    //struct ArpEntriesLinkModel = PointerLinkModel<ArpEntry> {};
    struct ArpEntriesLinkModel : public ArrayLinkModelWithAccessor<
        ArpEntry, ArpEntryIndexType, ArpEntryNull, EthIpIface, ArpEntriesAccessor> {};
    using ArpEntryRef = typename ArpEntriesLinkModel::Ref;
    
    // Nodes in ARP entry data structures.
    using ArpEntryListNode = LinkedListNode<ArpEntriesLinkModel>;
    using ArpEntryTimerQueueNode = typename TheTimerQueueService::template Node<
        ArpEntriesLinkModel, TimeType, ArpEntryTimerQueueNodeUserData>;
    
    // ARP entry states.
    struct ArpEntryState { enum : uint8_t {Free, Query, Valid, Refreshing}; };
    
    // ARP entry states where the entry timer is allowed to be active.
    inline static auto one_of_timer_entry_states ()
    {
        return OneOf(ArpEntryState::Query, ArpEntryState::Valid, ArpEntryState::Refreshing);
    }
    
    struct ArpEntryTimerQueueNodeUserData {
        // Entry state (ArpEntryState::*).
        uint8_t state : 2;
        
        // Whether the entry is weak (seen by chance) or hard (needed at some point).
        // Free entries must be weak.
        bool weak : 1;
        
        // Whether the entry timer is active (inserted into the timer queue).
        bool timer_active : 1;
        
        // Query and Refreshing states: How many more response timeouts before the
        // entry becomes Free or Refreshing respectively.
        // Valid state: 1 if the timeout has not elapsed yet, 0 if it has.
        uint8_t attempts_left : 4;
    };
    
    // ARP table entry (in array m_arp_entries)
    struct ArpEntry {
        inline ArpEntryTimerQueueNodeUserData & nud()
        {
            return timer_queue_node;
        }
        
        // MAC address of the entry (valid in Valid and Refreshing states).
        MacAddr mac_addr;
        
        // Node in linked lists (m_used_entries_list or m_free_entries_list).
        ArpEntryListNode list_node;
        
        // Node in the timer queue (m_timer_queue).
        ArpEntryTimerQueueNode timer_queue_node;
        
        // IP address of the entry (valid in all states except Free).
        Ip4Addr ip_addr;
        
        // List of send-retry waiters to be notified when resolution is complete.
        IpSendRetryList retry_list;
    };
    
    // Accessors for data structure nodes.
    struct ArpEntryListNodeAccessor :
        public AIPSTACK_MEMBER_ACCESSOR(&ArpEntry::list_node) {};
    struct ArpEntryTimerQueueNodeAccessor :
        public AIPSTACK_MEMBER_ACCESSOR(&ArpEntry::timer_queue_node) {};
    
    // Linked list type.
    using ArpEntryList = LinkedList<
        ArpEntryListNodeAccessor, ArpEntriesLinkModel, true>;
    
    // Data structure type for ARP entry timers.
    using ArpEntryTimerQueue = typename TheTimerQueueService::template Queue<
        ArpEntriesLinkModel, ArpEntryTimerQueueNodeAccessor, TimeType,
        ArpEntryTimerQueueNodeUserData>;
    
public:
    struct InitInfo {
        size_t eth_mtu = 0;
        MacAddr const *mac_addr = nullptr;
    };
    
    EthIpIface (Platform platform, IpStack *stack, InitInfo const &info) :
        Iface(stack, {
            /*ip_mtu=*/ (size_t)(info.eth_mtu - EthHeader::Size),
            /*hw_type=*/ IpHwType::Ethernet,
            /*hw_iface=*/ static_cast<IpEthHw::HwIface *>(this)
        }),
        EthIpIfaceTimers<Arg>::Timers(platform),
        m_mac_addr(info.mac_addr)
    {
        AIPSTACK_ASSERT(info.eth_mtu >= EthHeader::Size)
        AIPSTACK_ASSERT(info.mac_addr != nullptr)
        
        // Initialize ARP entries...
        for (auto &e : m_arp_entries) {
            // State Free, timer not active.
            e.nud().state = ArpEntryState::Free;
            e.nud().weak = false; // irrelevant, for efficiency
            e.nud().timer_active = false;
            e.nud().attempts_left = 0; // irrelevant, for efficiency
            
            // Insert to free list.
            m_free_entries_list.append({e, *this}, *this);
        }
    }
    
protected:
    // These functions are implemented or called by the Ethernet driver.
    
    virtual IpErr driverSendFrame (IpBufRef frame) = 0;
    
    virtual EthIfaceState driverGetEthState () = 0;
    
    void recvFrameFromDriver (IpBufRef frame)
    {
        // Check that we have an Ethernet header.
        if (AIPSTACK_UNLIKELY(!frame.hasHeader(EthHeader::Size))) {
            return;
        }
        
        // Store the reference to the Ethernet header (for getRxEthHeader).
        m_rx_eth_header = EthHeader::MakeRef(frame.getChunkPtr());
        
        // Get the EtherType.
        uint16_t ethtype = m_rx_eth_header.get(EthHeader::EthType());
        
        // Hide the header to get the payload.
        auto pkt = frame.hideHeader(EthHeader::Size);
        
        // Handle based on the EtherType.
        if (AIPSTACK_LIKELY(ethtype == EthTypeIpv4)) {
            Iface::recvIp4PacketFromDriver(pkt);
        }
        else if (ethtype == EthTypeArp) {
            recvArpPacket(pkt);
        }
    }
    
    inline void ethStateChangedFromDriver ()
    {
        // Forward notification to IP stack.
        Iface::stateChangedFromDriver();
    }
    
private:
    IpErr driverSendIp4Packet (IpBufRef pkt, Ip4Addr ip_addr,
                               IpSendRetryRequest *retryReq) override final
    {
        // Try to resolve the MAC address.
        MacAddr dst_mac;
        IpErr resolve_err = resolve_hw_addr(ip_addr, &dst_mac, retryReq);
        if (AIPSTACK_UNLIKELY(resolve_err != IpErr::SUCCESS)) {
            return resolve_err;
        }
        
        // Reveal the Ethernet header.
        IpBufRef frame;
        if (AIPSTACK_UNLIKELY(!pkt.revealHeader(EthHeader::Size, &frame))) {
            return IpErr::NO_HEADER_SPACE;
        }
        
        // Write the Ethernet header.
        auto eth_header = EthHeader::MakeRef(frame.getChunkPtr());
        eth_header.set(EthHeader::DstMac(),  dst_mac);
        eth_header.set(EthHeader::SrcMac(),  *m_mac_addr);
        eth_header.set(EthHeader::EthType(), EthTypeIpv4);
        
        // Send the frame via the lower-layer driver.
        return driverSendFrame(frame);
    }
    
    IpIfaceDriverState driverGetState () override final
    {
        // Get the state from the lower-layer driver.
        EthIfaceState eth_state = driverGetEthState();
        
        // Return the state based on that: copy link_up.
        IpIfaceDriverState state = {};
        state.link_up = eth_state.link_up;
        return state;
    }
    
private: // IpEthHw::HwIface
    MacAddr getMacAddr () override final
    {
        return *m_mac_addr;
    }
    
    EthHeader::Ref getRxEthHeader () override final
    {
        return m_rx_eth_header;
    }
    
    IpErr sendArpQuery (Ip4Addr ip_addr) override final
    {
        return send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), ip_addr);
    }
    
    IpEthHw::ArpObservable & getArpObservable () override final
    {
        return m_arp_observable;
    }
    
private:
    void recvArpPacket (IpBufRef pkt)
    {
        // Check that we have the ARP header.
        if (AIPSTACK_UNLIKELY(!pkt.hasHeader(ArpIp4Header::Size))) {
            return;
        }
        auto arp_header = ArpIp4Header::MakeRef(pkt.getChunkPtr());
        
        // Sanity check ARP header.
        if (arp_header.get(ArpIp4Header::HwType())       != ArpHwTypeEth  ||
            arp_header.get(ArpIp4Header::ProtoType())    != EthTypeIpv4   ||
            arp_header.get(ArpIp4Header::HwAddrLen())    != MacAddr::Size ||
            arp_header.get(ArpIp4Header::ProtoAddrLen()) != Ip4Addr::Size)
        {
            return;
        }
        
        // Get some ARP header fields.
        uint16_t op_type    = arp_header.get(ArpIp4Header::OpType());
        MacAddr src_mac     = arp_header.get(ArpIp4Header::SrcHwAddr());
        Ip4Addr src_ip_addr = arp_header.get(ArpIp4Header::SrcProtoAddr());
        
        // Try to save the hardware address.
        save_hw_addr(src_ip_addr, src_mac);
        
        // If this is an ARP request for our IP address, send a response.
        if (op_type == ArpOpTypeRequest) {
            IpIfaceIp4Addrs const *ifaddr = Iface::getIp4AddrsFromDriver();
            if (ifaddr != nullptr &&
                arp_header.get(ArpIp4Header::DstProtoAddr()) == ifaddr->addr)
            {
                send_arp_packet(ArpOpTypeReply, src_mac, src_ip_addr);
            }
        }
    }
    
    AIPSTACK_ALWAYS_INLINE
    IpErr resolve_hw_addr (
        Ip4Addr ip_addr, MacAddr *mac_addr, IpSendRetryRequest *retryReq)
    {
        // First look if the first used entry is a match, as an optimization.
        ArpEntryRef entry_ref = m_used_entries_list.first(*this);
        
        if (AIPSTACK_LIKELY(!entry_ref.isNull() && (*entry_ref).ip_addr == ip_addr)) {
            // Fast path, the first used entry is a match.
            AIPSTACK_ASSERT((*entry_ref).nud().state != ArpEntryState::Free)
            
            // Make sure the entry is hard as get_arp_entry would do below.
            (*entry_ref).nud().weak = false;
        } else {
            // Slow path: use get_arp_entry, make a hard entry.
            GetArpEntryRes get_res = get_arp_entry(ip_addr, false, entry_ref);
            
            // Did we not get an (old or new) entry for this address?
            if (AIPSTACK_UNLIKELY(get_res != GetArpEntryRes::GotArpEntry)) {
                // If this is a broadcast IP address, return the broadcast MAC address.
                if (get_res == GetArpEntryRes::BroadcastAddr) {
                    *mac_addr = MacAddr::BroadcastAddr();
                    return IpErr::SUCCESS;
                } else {
                    // Failure, cannot get MAC address.
                    return IpErr::NO_HW_ROUTE;
                }
            }
        }
        
        ArpEntry &entry = *entry_ref;
        
        // Got a Valid or Refreshing entry?
        if (AIPSTACK_LIKELY(entry.nud().state >= ArpEntryState::Valid)) {
            // If it is a timed out Valid entry, transition to Refreshing.
            if (AIPSTACK_UNLIKELY(entry.nud().attempts_left == 0)) {
                // Refreshing entry never has attempts_left==0 so no need to check for
                // Valid state in the if. We have a Valid entry and the timer is also
                // not active (needed by set_entry_timer) since attempts_left==0 implies
                // that it has expired already
                AIPSTACK_ASSERT(entry.nud().state == ArpEntryState::Valid)
                AIPSTACK_ASSERT(!entry.nud().timer_active)
                
                // Go to Refreshing state, start timeout, send first unicast request.
                entry.nud().state = ArpEntryState::Refreshing;
                entry.nud().attempts_left = ArpRefreshAttempts;
                set_entry_timer(entry);
                update_timer();
                send_arp_packet(ArpOpTypeRequest, entry.mac_addr, entry.ip_addr);
            }
            
            // Success, return MAC address.
            *mac_addr = entry.mac_addr;
            return IpErr::SUCCESS;
        } else {
            // If this is a Free entry, initialize it.
            if (entry.nud().state == ArpEntryState::Free) {
                // Timer is not active for Free entries (needed by set_entry_timer).
                AIPSTACK_ASSERT(!entry.nud().timer_active)
                
                // Go to Query state, start timeout, send first broadcast request.
                // NOTE: Entry is already inserted to m_used_entries_list.
                entry.nud().state = ArpEntryState::Query;
                entry.nud().attempts_left = ArpQueryAttempts;
                set_entry_timer(entry);
                update_timer();
                send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), ip_addr);
            }
            
            // Add a request to the retry list if a request is supplied.
            entry.retry_list.addRequest(retryReq);
            
            // Return ARP_QUERY error.
            return IpErr::ARP_QUERY;
        }
    }
    
    void save_hw_addr (Ip4Addr ip_addr, MacAddr mac_addr)
    {
        // Sanity check MAC address: not broadcast.
        if (AIPSTACK_UNLIKELY(mac_addr == MacAddr::BroadcastAddr())) {
            return;
        }
        
        // Get an entry, if a new entry is allocated it will be weak.
        ArpEntryRef entry_ref;
        GetArpEntryRes get_res = get_arp_entry(ip_addr, true, entry_ref);
        
        // Did we get an (old or new) entry for this address?
        if (get_res == GetArpEntryRes::GotArpEntry) {
            ArpEntry &entry = *entry_ref;
            
            // Set entry to Valid state, remember MAC address, start timeout.
            entry.nud().state = ArpEntryState::Valid;
            entry.mac_addr = mac_addr;
            entry.nud().attempts_left = 1;
            clear_entry_timer(entry); // set_entry_timer requires !timer_active
            set_entry_timer(entry);
            update_timer();
            
            // Dispatch send-retry requests.
            // NOTE: The handlers called may end up changing this ARP entry, including
            // reusing it for a different IP address. In that case retry_list.reset()
            // would be called from reset_arp_entry, but that is safe since
            // SentRetry::List supports it.
            entry.retry_list.dispatchRequests();
        }
        
        // Notify the ARP observers so long as the address is not obviously bad.
        // It is important to do this even if no ARP entry was obtained, since that
        // will not happen if the interface has no IP address configured, which is
        // exactly when DHCP needs to be notified.
        if (ip_addr != Ip4Addr::AllOnesAddr() && ip_addr != Ip4Addr::ZeroAddr()) {
            m_arp_observable.notifyKeepObservers([&](IpEthHw::ArpObserver &observer) {
                IpEthHw::HwIface::notifyArpObserver(observer, ip_addr, mac_addr);
            });
        }
    }
    
    enum class GetArpEntryRes {GotArpEntry, BroadcastAddr, InvalidAddr};
    
    // NOTE: If a Free entry is obtained, then 'weak' and 'ip_addr' have been
    // set, the entry is already in m_used_entries_list, but the caller must
    // complete initializing it to a non-Free state. Also, update_timer is needed
    // afterward then.
    GetArpEntryRes get_arp_entry (Ip4Addr ip_addr, bool weak, ArpEntryRef &out_entry)
    {
        // Look for a used entry with this IP address while also collecting
        // some information to be used in case we don't find an entry...
        
        int num_hard = 0;
        ArpEntryRef last_weak_entry_ref = ArpEntryRef::null();
        ArpEntryRef last_hard_entry_ref = ArpEntryRef::null();
        
        ArpEntryRef entry_ref = m_used_entries_list.first(*this);
        
        while (!entry_ref.isNull()) {
            ArpEntry &entry = *entry_ref;
            AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Free)
            
            if (entry.ip_addr == ip_addr) {
                break;
            }
            
            if (entry.nud().weak) {
                last_weak_entry_ref = entry_ref;
            } else {
                num_hard++;
                last_hard_entry_ref = entry_ref;
            }
            
            entry_ref = m_used_entries_list.next(entry_ref, *this);
        }
        
        if (AIPSTACK_LIKELY(!entry_ref.isNull())) {
            // We found an entry with this IP address.
            // If this is a hard request, make sure the entry is hard.
            if (!weak) {
                (*entry_ref).nud().weak = false;
            }
        } else {
            // We did not find an entry with this IP address.
            // First do some checks of the IP address...
            
            // If this is the all-ones address, return the broadcast MAC address.
            if (ip_addr == Ip4Addr::AllOnesAddr()) {
                return GetArpEntryRes::BroadcastAddr;
            }
            
            // Check for zero IP address.
            if (ip_addr == Ip4Addr::ZeroAddr()) {
                return GetArpEntryRes::InvalidAddr;
            }
            
            // Check if the interface has an IP address assigned.
            IpIfaceIp4Addrs const *ifaddr = Iface::getIp4AddrsFromDriver();
            if (ifaddr == nullptr) {
                return GetArpEntryRes::InvalidAddr;
            }
            
            // Check if the given IP address is in the subnet.
            if ((ip_addr & ifaddr->netmask) != ifaddr->netaddr) {
                return GetArpEntryRes::InvalidAddr;
            }
            
            // If this is the local broadcast address, return the broadcast MAC address.
            if (ip_addr == ifaddr->bcastaddr) {
                return GetArpEntryRes::BroadcastAddr;
            }
            
            // Check if there is a Free entry available.
            entry_ref = m_free_entries_list.first(*this);
            
            if (!entry_ref.isNull()) {
                // Got a Free entry.
                AIPSTACK_ASSERT((*entry_ref).nud().state == ArpEntryState::Free)
                AIPSTACK_ASSERT(!(*entry_ref).nud().timer_active)
                AIPSTACK_ASSERT(!(*entry_ref).retry_list.hasRequests())
                
                // Move the entry from the free list to the used list.
                m_free_entries_list.removeFirst(*this);
                m_used_entries_list.prepend(entry_ref, *this);
            } else {
                // There is no Free entry available, we will recycle a used entry.
                // Determine whether to recycle a weak or hard entry.
                bool use_weak;
                if (weak) {
                    use_weak =
                        !(num_hard > ArpProtectCount || last_weak_entry_ref.isNull());
                } else {
                    int num_weak = NumArpEntries - num_hard;
                    use_weak =
                        (num_weak > ArpNonProtectCount || last_hard_entry_ref.isNull());
                }
                
                // Get the entry to be recycled.
                entry_ref = use_weak ? last_weak_entry_ref : last_hard_entry_ref;
                AIPSTACK_ASSERT(!entry_ref.isNull())
                
                // Reset the entry, but keep it in the used list.
                reset_arp_entry(*entry_ref, true);
            }
            
            // NOTE: The entry is in Free state now but in the used list.
            // The caller is responsible to set a non-Free state ensuring
            // that the state corresponds with the list membership again.
            
            // Set IP address and weak flag.
            (*entry_ref).ip_addr = ip_addr;
            (*entry_ref).nud().weak = weak;
        }
        
        // Bump to entry to the front of the used entries list.
        if (!(entry_ref == m_used_entries_list.first(*this))) {
            m_used_entries_list.remove(entry_ref, *this);
            m_used_entries_list.prepend(entry_ref, *this);
        }
        
        // Return the entry.
        out_entry = entry_ref;
        return GetArpEntryRes::GotArpEntry;
    }
    
    // NOTE: update_timer is needed after this.
    void reset_arp_entry (ArpEntry &entry, bool leave_in_used_list)
    {
        AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Free)
        
        // Make sure the entry timeout is not active.
        clear_entry_timer(entry);
        
        // Set the entry to Free state.
        entry.nud().state = ArpEntryState::Free;
        
        // Reset the send-retry list for the entry.
        entry.retry_list.reset();
        
        // Move from used list to free list, unless requested not to.
        if (!leave_in_used_list) {
            m_used_entries_list.remove({entry, *this}, *this);
            m_free_entries_list.prepend({entry, *this}, *this);
        }
    }
    
    IpErr send_arp_packet (uint16_t op_type, MacAddr dst_mac, Ip4Addr dst_ipaddr)
    {
        // Get a local buffer for the frame,
        TxAllocHelper<EthArpPktSize, HeaderBeforeEth> frame_alloc(EthArpPktSize);
        
        // Write the Ethernet header.
        auto eth_header = EthHeader::MakeRef(frame_alloc.getPtr());
        eth_header.set(EthHeader::DstMac(), dst_mac);
        eth_header.set(EthHeader::SrcMac(), *m_mac_addr);
        eth_header.set(EthHeader::EthType(), EthTypeArp);
        
        // Determine the source IP address.
        IpIfaceIp4Addrs const *ifaddr = Iface::getIp4AddrsFromDriver();
        Ip4Addr src_addr = (ifaddr != nullptr) ? ifaddr->addr : Ip4Addr::ZeroAddr();
        
        // Write the ARP header.
        auto arp_header = ArpIp4Header::MakeRef(frame_alloc.getPtr() + EthHeader::Size);
        arp_header.set(ArpIp4Header::HwType(),       ArpHwTypeEth);
        arp_header.set(ArpIp4Header::ProtoType(),    EthTypeIpv4);
        arp_header.set(ArpIp4Header::HwAddrLen(),    MacAddr::Size);
        arp_header.set(ArpIp4Header::ProtoAddrLen(), Ip4Addr::Size);
        arp_header.set(ArpIp4Header::OpType(),       op_type);
        arp_header.set(ArpIp4Header::SrcHwAddr(),    *m_mac_addr);
        arp_header.set(ArpIp4Header::SrcProtoAddr(), src_addr);
        arp_header.set(ArpIp4Header::DstHwAddr(),    dst_mac);
        arp_header.set(ArpIp4Header::DstProtoAddr(), dst_ipaddr);
        
        // Send the frame via the lower-layer driver.
        return driverSendFrame(frame_alloc.getBufRef());
    }
    
    // Set tne ARP entry timeout based on the entry state and attempts_left.
    void set_entry_timer (ArpEntry &entry)
    {
        AIPSTACK_ASSERT(!entry.nud().timer_active)
        AIPSTACK_ASSERT(entry.nud().state == one_of_timer_entry_states())
        AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Valid ||
                     entry.nud().attempts_left == 1)
        
        // Determine the relative timeout...
        TimeType timeout;
        if (entry.nud().state == ArpEntryState::Valid) {
            // Valid entry (not expired yet, i.e. with attempts_left==1).
            timeout = ArpValidTimeoutTicks;
        } else {
            // Query or Refreshing entry, compute timeout with exponential backoff.
            uint8_t attempts = (entry.nud().state == ArpEntryState::Query) ?
                ArpQueryAttempts : ArpRefreshAttempts;
            AIPSTACK_ASSERT(entry.nud().attempts_left <= attempts)
            timeout = ArpBaseResponseTimeoutTicks << (attempts - entry.nud().attempts_left);
        }
        
        // Get the current time.
        TimeType now = platform().getTime();
        
        // Update the reference time of the timer queue (needed before insert).
        m_timer_queue.updateReferenceTime(now, *this);
        
        // Calculate the absolute timeout time.
        TimeType abs_time = now + timeout;
        
        // Insert the timer to the timer queue and set the timer_active flag.
        m_timer_queue.insert({entry, *this}, abs_time, *this);
        entry.nud().timer_active = true;
    }
    
    // Make sure the entry timeout is not active.
    // NOTE: update_timer is needed after this.
    void clear_entry_timer (ArpEntry &entry)
    {
        // If the timer is active, remove it from the timer queue and clear the
        // timer_active flag.
        if (entry.nud().timer_active) {
            m_timer_queue.remove({entry, *this}, *this);
            entry.nud().timer_active = false;
        }
    }
    
    // Make sure the ArpTimer is set to expire at the first entry timeout,
    // or unset it if there is no active entry timeout. This must be called
    // after every insertion/removal of an entry to the timer queue.
    void update_timer ()
    {
        TimeType time;
        if (m_timer_queue.getFirstTime(time, *this)) {
            tim(ArpTimer()).setAt(time);
        } else {
            tim(ArpTimer()).unset();
        }
    }
    
    void timerExpired (ArpTimer)
    {
        // Prepare timer queue for removing expired timers.
        m_timer_queue.prepareForRemovingExpired(platform().getTime(), *this);
        
        // Dispatch expired timers...
        ArpEntryRef timer_ref;
        while (!(timer_ref = m_timer_queue.removeExpired(*this)).isNull()) {
            ArpEntry &entry = *timer_ref;
            AIPSTACK_ASSERT(entry.nud().timer_active)
            AIPSTACK_ASSERT(entry.nud().state == one_of_timer_entry_states())
            
            // Clear the timer_active flag since the entry has just been removed
            // from the timer queue.
            entry.nud().timer_active = false;
            
            // Perform timeout processing for the entry.
            handle_entry_timeout(entry);
        }
        
        // Set the ArpTimer for the next expiration or unset.
        update_timer();
    }
    
    void handle_entry_timeout (ArpEntry &entry)
    {
        AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Free)
        AIPSTACK_ASSERT(!entry.nud().timer_active)
        
        // Check if the IP address is still consistent with the interface
        // address settings. If not, reset the ARP entry.
        IpIfaceIp4Addrs const *ifaddr = Iface::getIp4AddrsFromDriver();        
        if (ifaddr == nullptr ||
            (entry.ip_addr & ifaddr->netmask) != ifaddr->netaddr ||
            entry.ip_addr == ifaddr->bcastaddr)
        {
            reset_arp_entry(entry, false);
            return;
        }
        
        switch (entry.nud().state) {
            case ArpEntryState::Query: {
                // Query state: Decrement attempts_left then either reset the entry
                // in case of last attempt, else retransmit the broadcast query.
                AIPSTACK_ASSERT(entry.nud().attempts_left > 0)
                
                entry.nud().attempts_left--;
                if (entry.nud().attempts_left == 0) {
                    reset_arp_entry(entry, false);
                } else {
                    set_entry_timer(entry);
                    send_arp_packet(
                        ArpOpTypeRequest, MacAddr::BroadcastAddr(), entry.ip_addr);
                }
            } break;
            
            case ArpEntryState::Valid: {
                // Valid state: Set attempts_left to 0 to consider the entry expired.
                // Upon next use the entry it will go to Refreshing state.
                AIPSTACK_ASSERT(entry.nud().attempts_left == 1)
                
                entry.nud().attempts_left = 0;
            } break;
            
            case ArpEntryState::Refreshing: {
                // Refreshing state: Decrement attempts_left then either move
                // the entry to Query state (and send the first broadcast query),
                // else retransmit the unicast query.
                AIPSTACK_ASSERT(entry.nud().attempts_left > 0)
                
                entry.nud().attempts_left--;
                if (entry.nud().attempts_left == 0) {
                    entry.nud().state = ArpEntryState::Query;
                    entry.nud().attempts_left = ArpQueryAttempts;
                    send_arp_packet(
                        ArpOpTypeRequest, MacAddr::BroadcastAddr(), entry.ip_addr);
                } else {
                    send_arp_packet(ArpOpTypeRequest, entry.mac_addr, entry.ip_addr);
                }
                set_entry_timer(entry);
            } break;
            
            default:
                AIPSTACK_ASSERT(false);
        }
    }
    
private:
    IpEthHw::ArpObservable m_arp_observable;
    MacAddr const *m_mac_addr;
    StructureRaiiWrapper<ArpEntryList> m_used_entries_list;
    StructureRaiiWrapper<ArpEntryList> m_free_entries_list;
    StructureRaiiWrapper<ArpEntryTimerQueue> m_timer_queue;
    TimeType m_timers_ref_time;
    EthHeader::Ref m_rx_eth_header;
    ArpEntry m_arp_entries[NumArpEntries];
    
    struct ArpEntriesAccessor :
        public AIPSTACK_MEMBER_ACCESSOR(&EthIpIface::m_arp_entries) {};
};

struct EthIpIfaceOptions {
    AIPSTACK_OPTION_DECL_VALUE(NumArpEntries, int, 16)
    AIPSTACK_OPTION_DECL_VALUE(ArpProtectCount, int, 8)
    AIPSTACK_OPTION_DECL_VALUE(HeaderBeforeEth, size_t, 0)
    AIPSTACK_OPTION_DECL_TYPE(TimersStructureService, void)
};

template <typename... Options>
class EthIpIfaceService {
    template <typename>
    friend class EthIpIface;
    
    AIPSTACK_OPTION_CONFIG_VALUE(EthIpIfaceOptions, NumArpEntries)
    AIPSTACK_OPTION_CONFIG_VALUE(EthIpIfaceOptions, ArpProtectCount)
    AIPSTACK_OPTION_CONFIG_VALUE(EthIpIfaceOptions, HeaderBeforeEth)
    AIPSTACK_OPTION_CONFIG_TYPE(EthIpIfaceOptions, TimersStructureService)
    
public:
    template <typename PlatformImpl_, typename Iface_>
    struct Compose {
        using PlatformImpl = PlatformImpl_;
        using Iface = Iface_;
        using Params = EthIpIfaceService;
        AIPSTACK_DEF_INSTANCE(Compose, EthIpIface)
    };
};

}

#endif
