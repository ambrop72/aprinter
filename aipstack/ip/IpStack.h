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

#ifndef APRINTER_IPSTACK_IPSTACK_H
#define APRINTER_IPSTACK_IPSTACK_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <tuple>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/system/TimedEventWrapper.h>

#include <aipstack/misc/Err.h>
#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Chksum.h>
#include <aipstack/misc/SendRetry.h>
#include <aipstack/misc/Allocator.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpIfaceDriver.h>
#include <aipstack/ip/IpReassembly.h>

#include <aipstack/BeginNamespace.h>

struct IpIfaceIp4AddrSetting {
    bool present;
    uint8_t prefix;
    Ip4Addr addr;
};

struct IpIfaceIp4GatewaySetting {
    bool present;
    Ip4Addr addr;
};

struct IpSendFlags {
    enum : uint8_t {
        DontFragmentNow   = 1 << 0,
        DontFragmentFlag  = 1 << 1,
    };
};

template <typename Arg>
class IpStack;

template <typename Arg>
APRINTER_DECL_TIMERS_CLASS(IpStackTimers, typename Arg::Context, IpStack<Arg>, (MtuTimer))

template <typename Arg>
class IpStack :
    private IpStackTimers<Arg>::Timers
{
    APRINTER_USE_TYPES1(Arg, (Params))
    APRINTER_USE_VALS(Params, (HeaderBeforeIp, IcmpTTL, NumMtuEntries, MtuTimeoutMinutes))
    APRINTER_USE_TYPES1(Params, (MtuIndexService))
    APRINTER_USE_TYPES1(Arg, (Context, BufAllocator))
    
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    
    static_assert(NumMtuEntries > 0, "");
    static_assert(MtuTimeoutMinutes > 0, "");
    
    APRINTER_USE_TIMERS_CLASS(IpStackTimers<Arg>, (MtuTimer))
    
    using ReassemblyService = IpReassemblyService<Params::MaxReassEntrys, Params::MaxReassSize>;
    APRINTER_MAKE_INSTANCE(Reassembly, (ReassemblyService::template Compose<Context>))
    
    struct MtuEntry;
    
    using MtuIndexType = APrinter::ChooseIntForMax<NumMtuEntries, false>;
    static MtuIndexType const MtuIndexNull = MtuIndexType(-1);
    
    using MtuLinkModel = APrinter::ArrayLinkModel<MtuEntry, MtuIndexType, MtuIndexNull>;
    
    struct MtuIndexAccessor;
    using MtuIndexLookupKeyArg = Ip4Addr; // remote_addr
    struct MtuIndexKeyFuncs;
    APRINTER_MAKE_INSTANCE(MtuIndex, (MtuIndexService::template Index<
        MtuIndexAccessor, MtuIndexLookupKeyArg, MtuIndexKeyFuncs, MtuLinkModel>))
    
    struct MtuFreeListAccessor;
    using MtuFreeList = APrinter::LinkedList<MtuFreeListAccessor, MtuLinkModel, false>;
    
    // MTU timer expires once per second
    static TimeType const MtuTimerTicks = 60.0 * (TimeType)Clock::time_freq;
    
public:
    static size_t const HeaderBeforeIp4Dgram = HeaderBeforeIp + Ip4Header::Size;
    
    class Iface;
    
public:
    void init ()
    {
        m_reassembly.init();
        tim(MtuTimer()).init(Context());
        
        m_iface_list.init();
        m_proto_listeners_list.init();
        m_next_id = 0;
        m_mtu_index.init();
        m_mtu_free_list.init();
        
        tim(MtuTimer()).appendAfter(Context(), MtuTimerTicks);
        
        for (MtuEntry &mtu_entry : m_mtu_entries) {
            mtu_entry.mtu = 0;
            mtu_entry.referenced = false;
            m_mtu_free_list.prepend(mtuRef(mtu_entry), mtuState());
        }
    }
    
    void deinit ()
    {
        AMBRO_ASSERT(m_iface_list.isEmpty())
        AMBRO_ASSERT(m_proto_listeners_list.isEmpty())
        
        tim(MtuTimer()).deinit(Context());
        m_reassembly.deinit();
    }
    
public:
    struct Ip4DgramMeta {
        Ip4Addr local_addr;
        Ip4Addr remote_addr;
        uint8_t ttl;
        uint8_t proto;
        Iface *iface;
    };
    
    IpErr sendIp4Dgram (Ip4DgramMeta const &meta, IpBufRef dgram,
                        IpSendRetry::Request *retryReq = nullptr,
                        uint8_t send_flags = 0)
    {
        // Reveal IP header.
        IpBufRef pkt;
        if (!dgram.revealHeader(Ip4Header::Size, &pkt)) {
            return IpErr::NO_HEADER_SPACE;
        }
        
        // Find an interface and address for output.
        Iface *route_iface;
        Ip4Addr route_addr;
        if (!routeIp4(meta.remote_addr, meta.iface, &route_iface, &route_addr)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Sanity check length.
        if (AMBRO_UNLIKELY(dgram.tot_len > UINT16_MAX)) {
            return IpErr::PKT_TOO_LARGE;
        }
        
        // Check if fragmentation is needed.
        size_t mtu = route_iface->m_ip_mtu;
        bool more_fragments = pkt.tot_len > mtu;
        
        // If fragmentation is needed check that DontFragmentNow is not set.
        if (AMBRO_UNLIKELY(more_fragments && (send_flags & IpSendFlags::DontFragmentNow) != 0)) {
            return IpErr::FRAG_NEEDED;
        }
        
        // Calculate the length of the first packet.
        size_t pkt_send_len = more_fragments ? round_frag_length(Ip4Header::Size, mtu) : pkt.tot_len;
        
        // Generate an identification number.
        uint16_t ident = m_next_id++;
        
        // Write IP header fields.
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        ip4_header.set(Ip4Header::VersionIhl(),   (4<<Ip4VersionShift)|5);
        ip4_header.set(Ip4Header::DscpEcn(),      0);
        ip4_header.set(Ip4Header::TotalLen(),     pkt_send_len);
        ip4_header.set(Ip4Header::Ident(),        ident);
        ip4_header.set(Ip4Header::FlagsOffset(),  more_fragments?Ip4FlagMF:0);
        ip4_header.set(Ip4Header::TimeToLive(),   meta.ttl);
        ip4_header.set(Ip4Header::Protocol(),     meta.proto);
        ip4_header.set(Ip4Header::HeaderChksum(), 0);
        ip4_header.set(Ip4Header::SrcAddr(),      meta.local_addr);
        ip4_header.set(Ip4Header::DstAddr(),      meta.remote_addr);
        
        // Calculate the IP header checksum.
        uint16_t calc_chksum = IpChksum(ip4_header.data, Ip4Header::Size);
        ip4_header.set(Ip4Header::HeaderChksum(), calc_chksum);
        
        // Send the packet to the driver.
        IpErr err = route_iface->m_driver->sendIp4Packet(pkt.subTo(pkt_send_len), route_addr, retryReq);
        
        // If no fragmentation is needed or sending failed, this is the end.
        if (AMBRO_LIKELY(!more_fragments) || err != IpErr::SUCCESS) {
            return err;
        }
        
        // Calculate the next fragment offset and skip the sent data.
        size_t fragment_offset = pkt_send_len - Ip4Header::Size;
        dgram.skipBytes(fragment_offset);
        
        // Send remaining fragments.
        while (true) {
            // We must send fragments such that the fragment offset is a multiple of 8.
            // This is achieved by round_frag_length.
            AMBRO_ASSERT(fragment_offset % 8 == 0)
            
            // Calculate how much to send and whether we have more fragments.
            size_t rem_pkt_length = Ip4Header::Size + dgram.tot_len;
            more_fragments = rem_pkt_length > mtu;
            pkt_send_len = more_fragments ? round_frag_length(Ip4Header::Size, mtu) : rem_pkt_length;
            
            // Write the fragment-specific IP header fields.
            ip4_header.set(Ip4Header::TotalLen(),     pkt_send_len);
            ip4_header.set(Ip4Header::FlagsOffset(),  (more_fragments?Ip4FlagMF:0)|(fragment_offset/8));
            ip4_header.set(Ip4Header::HeaderChksum(), 0);
            
            // Calculate the IP header checksum.
            uint16_t calc_chksum = IpChksum(ip4_header.data, Ip4Header::Size);
            ip4_header.set(Ip4Header::HeaderChksum(), calc_chksum);
            
            // Construct a packet with header and partial data.
            IpBufNode data_node = dgram.toNode();
            IpBufNode header_node;
            IpBufRef frag_pkt = pkt.subHeaderToContinuedBy(Ip4Header::Size, &data_node, pkt_send_len, &header_node);
            
            // Send the packet to the driver.
            err = route_iface->m_driver->sendIp4Packet(frag_pkt, route_addr, retryReq);
            
            // If this was the last fragment or there was an error, return.
            if (!more_fragments || err != IpErr::SUCCESS) {
                return err;
            }
            
            // Update the fragment offset and skip the sent data.
            size_t data_sent = pkt_send_len - Ip4Header::Size;
            fragment_offset += data_sent;
            dgram.skipBytes(data_sent);
        }
    }
    
    bool routeIp4 (Ip4Addr dst_addr, Iface *force_iface, Iface **route_iface, Ip4Addr *route_addr)
    {
        // When an interface is forced the logic is almost the same except that only this
        // interface is considered and we also allow the all-ones broadcast address.
        
        if (force_iface != nullptr) {
            if (dst_addr == Ip4Addr::AllOnesAddr() || force_iface->ip4AddrIsLocal(dst_addr)) {
                *route_addr = dst_addr;
            }
            else if (force_iface->m_have_gateway && force_iface->ip4AddrIsLocal(force_iface->m_gateway)) {
                *route_addr = force_iface->m_gateway;
            }
            else {
                return false;
            }
            *route_iface = force_iface;
            return true;
        }
        
        // Look for an interface where dst_addr is inside the local subnet
        // (and in case of multiple matches find a most specific one).
        // Also look for an interface with a gateway to use in case there
        // is no local subnet match.
        
        Iface *local_iface = nullptr;
        Iface *gw_iface = nullptr;
        
        for (Iface *iface = m_iface_list.first(); iface != nullptr; iface = m_iface_list.next(iface)) {
            if (iface->ip4AddrIsLocal(dst_addr)) {
                if (local_iface == nullptr || iface->m_addr.prefix > local_iface->m_addr.prefix) {
                    local_iface = iface;
                }
            }
            if (iface->m_have_gateway && iface->ip4AddrIsLocal(iface->m_gateway)) {
                if (gw_iface == nullptr) {
                    gw_iface = iface;
                }
            }
        }
        
        if (local_iface != nullptr) {
            *route_iface = local_iface;
            *route_addr = dst_addr;
        }
        else if (gw_iface != nullptr) {
            *route_iface = gw_iface;
            *route_addr = gw_iface->m_gateway;
        }
        else {
            return false;
        }
        return true;
    }
    
    struct Ip4DestUnreachMeta {
        uint8_t icmp_code;
        Icmp4RestType icmp_rest;
    };
    
    class ProtoListenerCallback {
    public:
        virtual void recvIp4Dgram (Ip4DgramMeta const &ip_meta, IpBufRef dgram) = 0;
        virtual void handleIp4DestUnreach (Ip4DestUnreachMeta const &du_meta,
                                           Ip4DgramMeta const &ip_meta, IpBufRef dgram_initial) = 0;
    };
    
    class ProtoListener {
        friend IpStack;
        
    public:
        void init (IpStack *stack, uint8_t proto, ProtoListenerCallback *callback)
        {
            AMBRO_ASSERT(stack != nullptr)
            AMBRO_ASSERT(callback != nullptr)
            
            m_stack = stack;
            m_callback = callback;
            m_proto = proto;
            
            m_stack->m_proto_listeners_list.prepend(this);
        }
        
        void deinit ()
        {
            m_stack->m_proto_listeners_list.remove(this);
        }
        
    private:
        IpStack *m_stack;
        ProtoListenerCallback *m_callback;
        APrinter::DoubleEndedListNode<ProtoListener> m_listeners_list_node;
        uint8_t m_proto;
    };
    
    class MtuRef {
        friend IpStack;
        
    public:
        void init ()
        {
            m_entry_idx = MtuIndexNull;
        }
        
        inline void deinit (IpStack *stack)
        {
            reset(stack);
        }
        
        void reset (IpStack *stack)
        {
            if (m_entry_idx != MtuIndexNull) {
                MtuEntry &mtu_entry = stack->m_mtu_entries[m_entry_idx];
                AMBRO_ASSERT(mtu_entry.mtu > 0)
                AMBRO_ASSERT(mtu_entry.referenced)
                AMBRO_ASSERT(mtu_entry.num_refs > 0)
                
                if (mtu_entry.num_refs > 1) {
                    mtu_entry.num_refs--;
                } else {
                    mtu_entry.referenced = false;
                    stack->m_mtu_free_list.prepend(stack->mtuRef(mtu_entry), stack->mtuState());
                }
                
                m_entry_idx = MtuIndexNull;
            }
        }
        
        inline bool isSetup ()
        {
            return m_entry_idx != MtuIndexNull;
        }
        
        bool setup (IpStack *stack, Ip4Addr remote_addr, Iface *iface)
        {
            AMBRO_ASSERT(!isSetup())
            
            typename MtuLinkModel::Ref mtu_ref = stack->m_mtu_index.findEntry(stack->mtuState(), remote_addr);
            
            if (!mtu_ref.isNull()) {
                MtuEntry &mtu_entry = *mtu_ref;
                AMBRO_ASSERT(mtu_entry.mtu > 0)
                
                if (mtu_entry.referenced) {
                    AMBRO_ASSERT(mtu_entry.num_refs > 0)
                    mtu_entry.num_refs++;
                } else {
                    stack->m_mtu_free_list.remove(mtu_ref, stack->mtuState());
                    mtu_entry.referenced = true;
                    mtu_entry.num_refs = 1;
                }
            } else {
                if (iface == nullptr) {
                    Ip4Addr route_addr;
                    if (!stack->routeIp4(remote_addr, nullptr, &iface, &route_addr)) {
                        return false;
                    }
                }
                
                uint16_t iface_mtu = iface->getMtu();
                AMBRO_ASSERT(iface_mtu > 0)
                
                mtu_ref = stack->m_mtu_free_list.first(stack->mtuState());
                if (mtu_ref.isNull()) {
                    return false;
                }
                
                MtuEntry &mtu_entry = *mtu_ref;
                AMBRO_ASSERT(mtu_entry.mtu == 0)
                AMBRO_ASSERT(!mtu_entry.referenced)
                
                stack->m_mtu_free_list.removeFirst(stack->mtuState());
                
                mtu_entry.mtu = iface_mtu;
                mtu_entry.referenced = true;
                mtu_entry.minutes_old = 0;
                mtu_entry.num_refs = 1;
                mtu_entry.remote_addr = remote_addr;
                
                stack->m_mtu_index.addEntry(stack->mtuState(), mtu_ref);
            }
            
            m_entry_idx = mtu_ref.getIndex();
            
            return true;
        }
        
        void getMtuAndDf (IpStack *stack, uint16_t *out_mtu, bool *out_df)
        {
            AMBRO_ASSERT(m_entry_idx != MtuIndexNull)
            
            MtuEntry &mtu_entry = stack->m_mtu_entries[m_entry_idx];
            AMBRO_ASSERT(mtu_entry.mtu > 0)
            
            if (mtu_entry.mtu == 1) {
                *out_mtu = Ip4RequiredRecvSize;
                *out_df = false;
            } else {
                *out_mtu = mtu_entry.mtu;
                *out_df = true;
            }
        }
        
        bool handleIcmpPacketTooBig (IpStack *stack, uint16_t mtu_info)
        {
            AMBRO_ASSERT(m_entry_idx != MtuIndexNull)
            
            MtuEntry &mtu_entry = stack->m_mtu_entries[m_entry_idx];
            AMBRO_ASSERT(mtu_entry.mtu > 0)
            AMBRO_ASSERT(mtu_entry.referenced)
            AMBRO_ASSERT(mtu_entry.num_refs > 0)
            
            if (mtu_info < Ip4MinMtu) {
                // ICMP message lacks next hop MTU.
                // Assume MTU Ip4RequiredRecvSize and don't set the DF flag
                // in outgoing packets. This complies with RFC 1191 page 7,
                // though better but more complex approaches are suggested.
                // Presumably by now all routers have been upgraded to include
                // the path MTU in the message and there isn't much reason to
                // implement those suggestions.
                
                // Ignore if already in the desired state.
                if (mtu_entry.mtu == 1) {
                    return false;
                }
                
                // We use the special MTU value 1 for this situation.
                mtu_entry.mtu = 1;
            } else {
                // ICMP message includes next hop MTU (mtu_info).
                
                // Ignore if the provided MTU is not lesser than our current.
                // If we are in the special mtu==1 state, nothing is done
                // since we are not supposed to get MTU errors since we send
                // without DF flag.
                if (mtu_info >= mtu_entry.mtu) {
                    return false;
                }
                
                // Remember this MTU as the new PMTU.
                mtu_entry.mtu = mtu_info;
            }
            
            // Reset the MTU entry timeout.
            mtu_entry.minutes_old = 0;
            
            return true;
        }
        
    private:
        MtuIndexType m_entry_idx;
    };
    
public:
    class Iface :
        private IpIfaceDriverCallback<Iface>
    {
        friend IpStack;
        
    public:
        using CallbackImpl = Iface;
        
        void init (IpStack *stack, IpIfaceDriver<CallbackImpl> *driver)
        {
            AMBRO_ASSERT(stack != nullptr)
            AMBRO_ASSERT(driver != nullptr)
            
            // Initialize stuffs.
            m_stack = stack;
            m_driver = driver;
            m_have_addr = false;
            m_have_gateway = false;
            
            // Get the MTU.
            m_ip_mtu = APrinter::MinValueU((uint16_t)UINT16_MAX, m_driver->getIpMtu());
            AMBRO_ASSERT(m_ip_mtu >= Ip4MinMtu)
            
            // Connect driver callbacks.
            m_driver->setCallback(this);
            
            // Register interface.
            m_stack->m_iface_list.prepend(this);
        }
        
        void deinit ()
        {
            // Unregister interface.
            m_stack->m_iface_list.remove(this);
            
            // Disconnect driver callbacks.
            m_driver->setCallback(nullptr);
        }
        
        void setIp4Addr (IpIfaceIp4AddrSetting value)
        {
            AMBRO_ASSERT(!value.present || value.prefix <= Ip4Addr::Bits)
            
            m_have_addr = value.present;
            if (value.present) {
                m_addr.addr = value.addr;
                m_addr.netmask = Ip4Addr::PrefixMask(value.prefix);
                m_addr.netaddr = m_addr.addr & m_addr.netmask;
                m_addr.bcastaddr = m_addr.netaddr | (Ip4Addr::AllOnesAddr() & ~m_addr.netmask);
                m_addr.prefix = value.prefix;
            }
        }
        
        IpIfaceIp4AddrSetting getIp4Addr ()
        {
            IpIfaceIp4AddrSetting value = {m_have_addr};
            if (m_have_addr) {
                value.prefix = m_addr.prefix;
                value.addr = m_addr.addr;
            }
            return value;
        }
        
        void setIp4Gateway (IpIfaceIp4GatewaySetting value)
        {
            m_have_gateway = value.present;
            if (value.present) {
                m_gateway = value.addr;
            }
        }
        
        IpIfaceIp4GatewaySetting getIp4Gateway ()
        {
            IpIfaceIp4GatewaySetting value = {m_have_gateway};
            if (m_have_gateway) {
                value.addr = m_gateway;
            }
            return value;
        }
        
    public:
        inline bool ip4AddrIsLocal (Ip4Addr addr)
        {
            return m_have_addr && (addr & m_addr.netmask) == m_addr.netaddr;
        }
        
        inline bool ip4AddrIsLocalBcast (Ip4Addr addr)
        {
            return m_have_addr && addr == m_addr.bcastaddr;
        }
        
        inline bool ip4AddrIsLocalAddr (Ip4Addr addr)
        {
            return m_have_addr && addr == m_addr.addr;
        }
        
        inline uint16_t getMtu ()
        {
            return m_ip_mtu;
        }
        
        // NOTE: Assuming no IP options.
        inline uint16_t getIp4DgramMtu ()
        {
            return m_ip_mtu - Ip4Header::Size;
        }
        
    private:
        friend IpIfaceDriverCallback<Iface>;
        
        IpIfaceIp4Addrs const * getIp4Addrs ()
        {
            return m_have_addr ? &m_addr : nullptr;
        }
        
        void recvIp4Packet (IpBufRef pkt)
        {
            m_stack->processRecvedIp4Packet(this, pkt);
        }
        
    private:
        APrinter::DoubleEndedListNode<Iface> m_iface_list_node;
        IpStack *m_stack;
        IpIfaceDriver<CallbackImpl> *m_driver;
        uint16_t m_ip_mtu;
        IpIfaceIp4Addrs m_addr;
        Ip4Addr m_gateway;
        bool m_have_addr;
        bool m_have_gateway;
    };
    
private:
    using MtuRefCntType = uint16_t;
    
    // Entry for MTU information.
    struct MtuEntry {
        typename MtuIndex::Node index_hook;
        union {
            APrinter::LinkedListNode<MtuLinkModel> free_list_hook;
            MtuRefCntType num_refs;
        };
        uint16_t mtu; // 0 -> unassociated, 1 -> assume Ip4RequiredRecvSize and inhibit DF flag
        bool referenced;
        uint8_t minutes_old;
        Ip4Addr remote_addr;
    };
    
    // Hook accessors for the MTU data structures.
    struct MtuIndexAccessor : public APRINTER_MEMBER_ACCESSOR(&MtuEntry::index_hook) {};
    struct MtuFreeListAccessor : public APRINTER_MEMBER_ACCESSOR(&MtuEntry::free_list_hook) {};
    
    // Obtains the key of an MTU entry for the MTU index.
    struct MtuIndexKeyFuncs {
        inline static Ip4Addr GetKeyOfEntry (MtuEntry &mtu_entry)
        {
            return mtu_entry.remote_addr;
        }
    };
    
    inline typename MtuLinkModel::Ref mtuRef (MtuEntry &mtu_entry)
    {
        return typename MtuLinkModel::Ref(mtu_entry, &mtu_entry - m_mtu_entries);
    }
    
    inline typename MtuLinkModel::State mtuState ()
    {
        return m_mtu_entries;
    }
    
    void processRecvedIp4Packet (Iface *iface, IpBufRef pkt)
    {
        // Check base IP header length.
        if (AMBRO_UNLIKELY(!pkt.hasHeader(Ip4Header::Size))) {
            return;
        }
        
        // Read IP header fields.
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        uint8_t version_ihl    = ip4_header.get(Ip4Header::VersionIhl());
        uint16_t total_len     = ip4_header.get(Ip4Header::TotalLen());
        uint16_t flags_offset  = ip4_header.get(Ip4Header::FlagsOffset());
        uint8_t ttl            = ip4_header.get(Ip4Header::TimeToLive());
        uint8_t proto          = ip4_header.get(Ip4Header::Protocol());
        Ip4Addr src_addr       = ip4_header.get(Ip4Header::SrcAddr());
        Ip4Addr dst_addr       = ip4_header.get(Ip4Header::DstAddr());
        
        // Check IP version.
        if (AMBRO_UNLIKELY((version_ihl >> Ip4VersionShift) != 4)) {
            return;
        }
        
        // Check header length.
        // We require the entire header to fit into the first buffer.
        uint8_t header_len = (version_ihl & Ip4IhlMask) * 4;
        if (AMBRO_UNLIKELY(header_len < Ip4Header::Size || !pkt.hasHeader(header_len))) {
            return;
        }
        
        // Check total length.
        if (AMBRO_UNLIKELY(total_len < header_len || total_len > pkt.tot_len)) {
            return;
        }
        
        // Sanity check source address - reject broadcast addresses.
        if (AMBRO_UNLIKELY(
            src_addr == Ip4Addr::AllOnesAddr() ||
            iface->ip4AddrIsLocalBcast(src_addr)))
        {
            return;
        }
        
        // Check destination address.
        // Accept only: all-ones broadcast, subnet broadcast, unicast to interface address.
        if (AMBRO_UNLIKELY(
            !iface->ip4AddrIsLocalAddr(dst_addr) &&
            !iface->ip4AddrIsLocalBcast(dst_addr) &&
            dst_addr != Ip4Addr::AllOnesAddr()))
        {
            return;
        }
        
        // Verify IP header checksum.
        uint16_t calc_chksum = IpChksum(ip4_header.data, header_len);
        if (AMBRO_UNLIKELY(calc_chksum != 0)) {
            return;
        }
        
        // Create a reference to the payload.
        IpBufRef dgram = pkt.hideHeader(header_len).subTo(total_len - header_len);
        
        // Check for fragmentation.
        bool more_fragments = (flags_offset & Ip4FlagMF) != 0;
        uint16_t fragment_offset_8b = flags_offset & Ip4OffsetMask;
        if (AMBRO_UNLIKELY(more_fragments || fragment_offset_8b != 0)) {
            // Get the fragment offset in bytes.
            uint16_t fragment_offset = fragment_offset_8b * 8;
            
            // Perform reassembly.
            if (!m_reassembly.reassembleIp4(
                ip4_header.get(Ip4Header::Ident()), src_addr, dst_addr, proto, ttl,
                more_fragments, fragment_offset, ip4_header.data, header_len, dgram))
            {
                return;
            }
            // Continue processing the reassembled datagram.
            // Note, dgram was modified pointing to the reassembled data.
        }
        
        // Create the datagram meta-info struct.
        Ip4DgramMeta meta = {dst_addr, src_addr, ttl, proto, iface};
        
        // Do protocol-specific processing.
        recvIp4Dgram(meta, dgram);
    }
    
    void recvIp4Dgram (Ip4DgramMeta const &meta, IpBufRef dgram)
    {
        if (meta.proto == Ip4ProtocolIcmp) {
            return recvIcmp4Dgram(meta, dgram);
        }
        
        ProtoListener *lis = find_proto_listener(meta.proto);
        if (lis != nullptr) {
            return lis->m_callback->recvIp4Dgram(meta, dgram);
        }
    }
    
    void recvIcmp4Dgram (Ip4DgramMeta const &meta, IpBufRef dgram)
    {
        // Check ICMP header length.
        if (!dgram.hasHeader(Icmp4Header::Size)) {
            return;
        }
        
        // Read ICMP header fields.
        auto icmp4_header = Icmp4Header::MakeRef(dgram.getChunkPtr());
        uint8_t type    = icmp4_header.get(Icmp4Header::Type());
        uint8_t code    = icmp4_header.get(Icmp4Header::Code());
        uint16_t chksum = icmp4_header.get(Icmp4Header::Chksum());
        auto rest       = icmp4_header.get(Icmp4Header::Rest());
        
        // Verify ICMP checksum.
        uint16_t calc_chksum = IpChksum(dgram);
        if (calc_chksum != 0) {
            return;
        }
        
        // Get ICMP data by hiding the ICMP header.
        IpBufRef icmp_data = dgram.hideHeader(Icmp4Header::Size);
        
        if (type == Icmp4TypeEchoRequest) {
            // Got echo request, send echo reply.
            sendIcmp4EchoReply(rest, icmp_data, meta.remote_addr, meta.iface);
        }
        else if (type == Icmp4TypeDestUnreach) {
            handleIcmp4DestUnreach(code, rest, icmp_data, meta.iface);
        }
    }
    
    void sendIcmp4EchoReply (Icmp4RestType rest, IpBufRef data, Ip4Addr dst_addr, Iface *iface)
    {
        // Can only reply when we have an address assigned.
        if (!iface->m_have_addr) {
            return;
        }
        
        // Allocate memory for headers.
        TxAllocHelper<BufAllocator, Icmp4Header::Size, HeaderBeforeIp4Dgram> dgram_alloc(Icmp4Header::Size);
        
        // Write the ICMP header.
        auto icmp4_header = Icmp4Header::MakeRef(dgram_alloc.getPtr());
        icmp4_header.set(Icmp4Header::Type(),   Icmp4TypeEchoReply);
        icmp4_header.set(Icmp4Header::Code(),   0);
        icmp4_header.set(Icmp4Header::Chksum(), 0);
        icmp4_header.set(Icmp4Header::Rest(),   rest);
        
        // Construct the datagram reference with header and data.
        IpBufNode data_node = data.toNode();
        dgram_alloc.setNext(&data_node, data.tot_len);
        IpBufRef dgram = dgram_alloc.getBufRef();
        
        // Calculate ICMP checksum.
        uint16_t calc_chksum = IpChksum(dgram);
        icmp4_header.set(Icmp4Header::Chksum(), calc_chksum);
        
        // Send the datagram.
        Ip4DgramMeta meta = {iface->m_addr.addr, dst_addr, IcmpTTL, Ip4ProtocolIcmp, iface};
        sendIp4Dgram(meta, dgram);
    }
    
    void handleIcmp4DestUnreach (uint8_t code, Icmp4RestType rest, IpBufRef icmp_data, Iface *iface)
    {
        // Check base IP header length.
        if (AMBRO_UNLIKELY(!icmp_data.hasHeader(Ip4Header::Size))) {
            return;
        }
        
        // Read IP header fields.
        auto ip4_header = Ip4Header::MakeRef(icmp_data.getChunkPtr());
        uint8_t version_ihl    = ip4_header.get(Ip4Header::VersionIhl());
        uint16_t total_len     = ip4_header.get(Ip4Header::TotalLen());
        uint8_t ttl            = ip4_header.get(Ip4Header::TimeToLive());
        uint8_t proto          = ip4_header.get(Ip4Header::Protocol());
        Ip4Addr src_addr       = ip4_header.get(Ip4Header::SrcAddr());
        Ip4Addr dst_addr       = ip4_header.get(Ip4Header::DstAddr());
        
        // Check IP version.
        if (AMBRO_UNLIKELY((version_ihl >> Ip4VersionShift) != 4)) {
            return;
        }
        
        // Check header length.
        // We require the entire header to fit into the first buffer.
        uint8_t header_len = (version_ihl & Ip4IhlMask) * 4;
        if (AMBRO_UNLIKELY(header_len < Ip4Header::Size || !icmp_data.hasHeader(header_len))) {
            return;
        }
        
        // Check that total_len includes at least the header.
        if (AMBRO_UNLIKELY(total_len < header_len)) {
            return;
        }
        
        // Create the Ip4DestUnreachMeta struct.
        Ip4DestUnreachMeta du_meta = {code, rest};
        
        // Create the datagram meta-info struct.
        Ip4DgramMeta ip_meta = {src_addr, dst_addr, ttl, proto, iface};
        
        // Get the included IP data.
        size_t data_len = APrinter::MinValueU(icmp_data.tot_len, total_len) - header_len;
        IpBufRef dgram_initial = icmp_data.hideHeader(header_len).subTo(data_len);
        
        // Dispatch based on the protocol.
        ProtoListener *lis = find_proto_listener(ip_meta.proto);
        if (lis != nullptr) {
            return lis->m_callback->handleIp4DestUnreach(du_meta, ip_meta, dgram_initial);
        }
    }
    
    static size_t round_frag_length (uint8_t header_length, size_t pkt_length)
    {
        return header_length + (((pkt_length - header_length) / 8) * 8);
    }
    
    void timerExpired (MtuTimer, Context)
    {
        // Restart the timer.
        tim(MtuTimer()).appendAfter(Context(), MtuTimerTicks);
        
        // Update MTU entries.
        for (MtuEntry &mtu_entry : m_mtu_entries) {
            if (mtu_entry.mtu > 0) {
                update_mtu_entry_expiry(mtu_entry);
            }
        }
    }
    
    void update_mtu_entry_expiry (MtuEntry &mtu_entry)
    {
        AMBRO_ASSERT(mtu_entry.mtu > 0)
        AMBRO_ASSERT(mtu_entry.minutes_old < MtuTimeoutMinutes)
        
        // If the entry is not expired yet, just increment minutes_old.
        if (mtu_entry.minutes_old < MtuTimeoutMinutes - 1) {
            mtu_entry.minutes_old++;
            return;
        }
        
        // Find the route to the destination.
        Iface *iface;
        Ip4Addr route_addr;
        if (!routeIp4(mtu_entry.remote_addr, nullptr, &iface, &route_addr)) {
            // Couldn't find an interface, will retry next time.
            return;
        }
        
        // Get the MTU from the interface.
        uint16_t iface_mtu = iface->getMtu();
        AMBRO_ASSERT(iface_mtu > 0)
        
        // Reset the MTU to that, reset minutes_old.
        mtu_entry.mtu = iface_mtu;
        mtu_entry.minutes_old = 0;
    }
    
    ProtoListener * find_proto_listener (uint8_t proto)
    {
        for (ProtoListener *lis = m_proto_listeners_list.first(); lis != nullptr; lis = m_proto_listeners_list.next(lis)) {
            if (lis->m_proto == proto) {
                return lis;
            }
        }
        return nullptr;
    }
    
private:
    using IfaceList = APrinter::DoubleEndedList<Iface, &Iface::m_iface_list_node, false>;
    using ProtoListenersList = APrinter::DoubleEndedList<ProtoListener, &ProtoListener::m_listeners_list_node, false>;
    
    Reassembly m_reassembly;
    IfaceList m_iface_list;
    ProtoListenersList m_proto_listeners_list;
    uint16_t m_next_id;
    typename MtuIndex::Index m_mtu_index;
    MtuFreeList m_mtu_free_list;
    MtuEntry m_mtu_entries[NumMtuEntries];
};

APRINTER_ALIAS_STRUCT_EXT(IpStackService, (
    APRINTER_AS_VALUE(size_t, HeaderBeforeIp),
    APRINTER_AS_VALUE(uint8_t, IcmpTTL),
    APRINTER_AS_VALUE(int, MaxReassEntrys),
    APRINTER_AS_VALUE(uint16_t, MaxReassSize),
    APRINTER_AS_VALUE(int, NumMtuEntries),
    APRINTER_AS_TYPE(MtuIndexService),
    APRINTER_AS_VALUE(uint8_t, MtuTimeoutMinutes)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(BufAllocator)
    ), (
        using Params = IpStackService;
        APRINTER_DEF_INSTANCE(Compose, IpStack)
    ))
))

#include <aipstack/EndNamespace.h>

#endif
