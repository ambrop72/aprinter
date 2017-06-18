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

#include <type_traits>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/ObserverNotification.h>

#include <aipstack/misc/Err.h>
#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Chksum.h>
#include <aipstack/misc/SendRetry.h>
#include <aipstack/misc/TxAllocHelper.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpReassembly.h>
#include <aipstack/ip/IpPathMtuCache.h>
#include <aipstack/ip/hw/IpHwCommon.h>

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

struct IpIfaceIp4Addrs {
    Ip4Addr addr;
    Ip4Addr netmask;
    Ip4Addr netaddr;
    Ip4Addr bcastaddr;
    uint8_t prefix;
};

struct IpIfaceDriverState {
    bool link_up;
};

struct IpSendFlags {
    static int const InternalBits = 1;
    static uint8_t const InternalMask = (1 << InternalBits) - 1;
    
    enum : uint8_t {
        // These are internal not real IP flags. They can be
        // removed by masking with ~InternalMask.
        DontFragmentNow  = 1 << 0,
        
        // These are real IP flags, the bits are correct but
        // relative to the high byte of FlagsOffset.
        DontFragmentFlag = Ip4FlagDF >> 8,
        
        // Mask of all allowed flags passed to send functions.
        AllFlags = DontFragmentNow|DontFragmentFlag,
    };
};

struct Ip4DestUnreachMeta {
    uint8_t icmp_code;
    Icmp4RestType icmp_rest;
};

template <typename Arg>
class IpStack
{
    APRINTER_USE_TYPES1(Arg, (Params, Context, ProtocolServicesList))
    APRINTER_USE_VALS(Params, (HeaderBeforeIp, IcmpTTL))
    
    APRINTER_USE_TYPES1(APrinter::ObserverNotification, (Observer, Observable))
    
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    
    using ReassemblyService = IpReassemblyService<Params::MaxReassEntrys, Params::MaxReassSize>;
    APRINTER_MAKE_INSTANCE(Reassembly, (ReassemblyService::template Compose<Context>))
    
    using PathMtuCacheService = IpPathMtuCacheService<typename Params::PathMtuParams>;
    APRINTER_MAKE_INSTANCE(PathMtuCache, (PathMtuCacheService::template Compose<Context, IpStack>))
    
    // Instantiate the protocols.
    template <int ProtocolIndex>
    struct ProtocolHelper {
        // Get the protocol service.
        using ProtocolService = APrinter::TypeListGet<ProtocolServicesList, ProtocolIndex>;
        
        // Expose the protocol number for TypeListGetMapped (GetProtocolType).
        using IpProtocolNumber = typename ProtocolService::IpProtocolNumber;
        
        // Instantiate the protocol.
        APRINTER_MAKE_INSTANCE(Protocol, (ProtocolService::template Compose<Context, IpStack>))
        
        // Helper function to get the pointer to the protocol.
        inline static Protocol * get (IpStack *stack)
        {
            return APrinter::TupleFindElem<Protocol>(&stack->m_protocols);
        }
    };
    using ProtocolHelpersList = APrinter::IndexElemList<ProtocolServicesList, ProtocolHelper>;
    
    // Create a list of the instantiated protocols, for the tuple.
    template <typename Helper>
    using ProtocolForHelper = typename Helper::Protocol;
    using ProtocolsList = APrinter::MapTypeList<ProtocolHelpersList, APrinter::TemplateFunc<ProtocolForHelper>>;
    
    APRINTER_DEFINE_MEMBER_TYPE(MemberTypeIpProtocolNumber, IpProtocolNumber)
    
public:
    static size_t const HeaderBeforeIp4Dgram = HeaderBeforeIp + Ip4Header::Size;
    
    class Iface;
    class IfaceListener;
    
private:
    using IfaceListenerLinkModel = APrinter::PointerLinkModel<IfaceListener>;
    
public:
    // Minimum permitted MTU and PMTU.
    // RFC 791 requires that routers can pass through 68 byte packets, so enforcing
    // this larger value theoreticlaly violates the standard. We need this to
    // simplify the implementation of TCP, notably so that the TCP headers do not
    // need to be fragmented and the DF flag never needs to be turned off. Note that
    // Linux enforces a minimum of 552, this must be perfectly okay in practice.
    static uint16_t const MinMTU = 256;
    
    void init ()
    {
        m_reassembly.init();
        m_path_mtu_cache.init(this);
        
        m_iface_list.init();
        m_next_id = 0;
        
        APrinter::ListFor<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            Helper::get(this)->init(this);
        }));
    }
    
    void deinit ()
    {
        AMBRO_ASSERT(m_iface_list.isEmpty())
        
        APrinter::ListForReverse<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            Helper::get(this)->deinit();
        }));
        
        m_path_mtu_cache.deinit();
        m_reassembly.deinit();
    }
    
    /**
     * Get the type of the protocol instance for a protocol number.
     */
    template <uint8_t ProtocolNumber>
    using GetProtocolType = typename APrinter::TypeListGetMapped<
        ProtocolHelpersList,
        typename MemberTypeIpProtocolNumber::Get,
        APrinter::WrapValue<uint8_t, ProtocolNumber>
    >::Protocol;
    
    /**
     * Get the pointer to a protocol instance given the instance
     * type (as returned by GetProtocolType).
     */
    template <typename Protocol>
    inline Protocol * getProtocol ()
    {
        return APrinter::TupleFindElem<Protocol>(&m_protocols);
    }
    
public:
    struct Ip4DgramMeta {
        Ip4Addr src_addr;
        Ip4Addr dst_addr;
        uint8_t ttl;
        uint8_t proto;
        Iface *iface;
    };
    
    IpErr sendIp4Dgram (Ip4DgramMeta const &meta, IpBufRef dgram,
                        IpSendRetry::Request *retryReq = nullptr, uint8_t send_flags = 0)
    {
        AMBRO_ASSERT(dgram.tot_len <= UINT16_MAX)
        AMBRO_ASSERT((send_flags & ~IpSendFlags::AllFlags) == 0)
        
        // Reveal IP header.
        IpBufRef pkt;
        if (AMBRO_UNLIKELY(!dgram.revealHeader(Ip4Header::Size, &pkt))) {
            return IpErr::NO_HEADER_SPACE;
        }
        
        // Find an interface and address for output.
        Iface *route_iface;
        Ip4Addr route_addr;
        bool route_ok;
        if (AMBRO_UNLIKELY(meta.iface != nullptr)) {
            route_ok = routeIp4ForceIface(meta.dst_addr, meta.iface, &route_iface, &route_addr);
        } else {
            route_ok = routeIp4(meta.dst_addr, &route_iface, &route_addr);
        }
        if (AMBRO_UNLIKELY(!route_ok)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Compute the common IP flags (remove internal flags).
        // This is relative to the high byte of FlagsOffset.
        uint8_t ip_flags = send_flags & ~IpSendFlags::InternalMask;
        
        // Check if fragmentation is needed...
        uint16_t pkt_send_len;
        
        if (AMBRO_UNLIKELY(pkt.tot_len > route_iface->getMtu())) {
            // Reject fragmentation?
            uint8_t df_flags = IpSendFlags::DontFragmentNow|IpSendFlags::DontFragmentFlag;
            if ((send_flags & df_flags) != 0) {
                return IpErr::FRAG_NEEDED;
            }
            
            // Calculate length of first fragment.
            pkt_send_len = Ip4RoundFragMength(Ip4Header::Size, route_iface->getMtu());
            
            // Set the MoreFragments IP flag (will be cleared for the last fragment).
            ip_flags |= Ip4FlagMF >> 8;
        } else {
            // First packet has all the data.
            pkt_send_len = pkt.tot_len;
        }
        
        // Write IP header fields and calculate header checksum inline...
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        IpChksumAccumulator chksum;
        
        uint16_t version_ihl_dscp_ecn = (uint16_t)((4 << Ip4VersionShift) | 5) << 8;
        chksum.addWord(APrinter::WrapType<uint16_t>(), version_ihl_dscp_ecn);
        ip4_header.set(Ip4Header::VersionIhlDscpEcn(), version_ihl_dscp_ecn);
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), pkt_send_len);
        ip4_header.set(Ip4Header::TotalLen(), pkt_send_len);
        
        uint16_t ident = m_next_id++; // generate identification number
        chksum.addWord(APrinter::WrapType<uint16_t>(), ident);
        ip4_header.set(Ip4Header::Ident(), ident);
        
        uint16_t flags_offset = (uint16_t)ip_flags << 8;
        chksum.addWord(APrinter::WrapType<uint16_t>(), flags_offset);
        ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
        
        uint16_t ttl_proto = ((uint16_t)meta.ttl << 8) | meta.proto;
        chksum.addWord(APrinter::WrapType<uint16_t>(), ttl_proto);
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto);
        
        chksum.addWords(&meta.src_addr.data);
        ip4_header.set(Ip4Header::SrcAddr(), meta.src_addr);
        
        chksum.addWords(&meta.dst_addr.data);
        ip4_header.set(Ip4Header::DstAddr(), meta.dst_addr);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        // Fast path is no fragmentation, this permits tail call optimization.
        if (AMBRO_LIKELY((ip_flags & (Ip4FlagMF >> 8)) == 0)) {
            return route_iface->driverSendIp4Packet(pkt, route_addr, retryReq);
        }
        
        // Slow path...
        return send_fragmented(pkt, route_iface, route_addr, ip_flags, retryReq);
    }
    
    // Optimized send function. It does not support fragmentation or
    // forcing an interface, and is always inlined. It also does not
    // sanity check that there is space for the IP header.
    AMBRO_ALWAYS_INLINE
    IpErr sendIp4DgramFast (Ip4Addr src_addr, Ip4Addr dst_addr, uint8_t ttl,
                            uint8_t proto, IpBufRef dgram, IpSendRetry::Request *retryReq,
                            uint8_t send_flags)
    {
        AMBRO_ASSERT(dgram.tot_len <= UINT16_MAX)
        AMBRO_ASSERT((send_flags & ~IpSendFlags::AllFlags) == 0)
        AMBRO_ASSERT(dgram.offset >= Ip4Header::Size)
        
        // Reveal IP header.
        IpBufRef pkt = dgram.revealHeaderMust(Ip4Header::Size);
        
        // Find an interface and address for output.
        Iface *route_iface;
        Ip4Addr route_addr;
        if (AMBRO_UNLIKELY(!routeIp4(dst_addr, &route_iface, &route_addr))) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Compute the common IP flags (remove internal flags).
        // This is relative to the high byte of FlagsOffset.
        uint8_t ip_flags = send_flags & ~IpSendFlags::InternalMask;
        
        // This function does not support fragmentation.
        if (AMBRO_UNLIKELY(pkt.tot_len > route_iface->getMtu())) {
            return IpErr::FRAG_NEEDED;
        }
        
        // Write IP header fields and calculate header checksum inline...
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        IpChksumAccumulator chksum;
        
        uint16_t version_ihl_dscp_ecn = (uint16_t)((4 << Ip4VersionShift) | 5) << 8;
        chksum.addWord(APrinter::WrapType<uint16_t>(), version_ihl_dscp_ecn);
        ip4_header.set(Ip4Header::VersionIhlDscpEcn(), version_ihl_dscp_ecn);
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), pkt.tot_len);
        ip4_header.set(Ip4Header::TotalLen(), pkt.tot_len);
        
        uint16_t ident = m_next_id++; // generate identification number
        chksum.addWord(APrinter::WrapType<uint16_t>(), ident);
        ip4_header.set(Ip4Header::Ident(), ident);
        
        uint16_t flags_offset = (uint16_t)ip_flags << 8;
        chksum.addWord(APrinter::WrapType<uint16_t>(), flags_offset);
        ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
        
        uint16_t ttl_proto = ((uint16_t)ttl << 8) | proto;
        chksum.addWord(APrinter::WrapType<uint16_t>(), ttl_proto);
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto);
        
        chksum.addWords(&src_addr.data);
        ip4_header.set(Ip4Header::SrcAddr(), src_addr);
        
        chksum.addWords(&dst_addr.data);
        ip4_header.set(Ip4Header::DstAddr(), dst_addr);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        return route_iface->driverSendIp4Packet(pkt, route_addr, retryReq);
    }
    
private:
    IpErr send_fragmented (IpBufRef pkt, Iface *route_iface, Ip4Addr route_addr,
                           uint8_t ip_flags, IpSendRetry::Request *retryReq)
    {
        // Recalculate pkt_send_len (not passed for optimization).
        uint16_t pkt_send_len = Ip4RoundFragMength(Ip4Header::Size, route_iface->getMtu());
        
        // Send the first fragment.
        IpErr err = route_iface->driverSendIp4Packet(pkt.subTo(pkt_send_len), route_addr, retryReq);
        if (err != IpErr::SUCCESS) {
            return err;
        }
        
        // Get back the dgram (we don't pass it for better optimization).
        IpBufRef dgram = pkt.hideHeader(Ip4Header::Size);
        
        // Calculate the next fragment offset and skip the sent data.
        uint16_t fragment_offset = pkt_send_len - Ip4Header::Size;
        dgram.skipBytes(fragment_offset);
        
        // Send remaining fragments.
        while (true) {
            // We must send fragments such that the fragment offset is a multiple of 8.
            // This is achieved by Ip4RoundFragMength.
            AMBRO_ASSERT(fragment_offset % 8 == 0)
            
            // If this is the last fragment, calculate its length and clear
            // the MoreFragments flag. Otherwise pkt_send_len is still correct
            // and MoreFragments still set.
            size_t rem_pkt_length = Ip4Header::Size + dgram.tot_len;
            bool more_fragments = rem_pkt_length > route_iface->getMtu();
            if (!more_fragments) {
                pkt_send_len = rem_pkt_length;
                ip_flags &= ~(Ip4FlagMF >> 8);
            }
            
            auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
            
            // Write the fragment-specific IP header fields.
            ip4_header.set(Ip4Header::TotalLen(),     pkt_send_len);
            ip4_header.set(Ip4Header::FlagsOffset(),  ((uint16_t)ip_flags << 8) | (fragment_offset / 8));
            ip4_header.set(Ip4Header::HeaderChksum(), 0);
            
            // Calculate the IP header checksum.
            // Not inline since fragmentation is uncommon, better save program space.
            uint16_t calc_chksum = IpChksum(ip4_header.data, Ip4Header::Size);
            ip4_header.set(Ip4Header::HeaderChksum(), calc_chksum);
            
            // Construct a packet with header and partial data.
            IpBufNode data_node = dgram.toNode();
            IpBufNode header_node;
            IpBufRef frag_pkt = pkt.subHeaderToContinuedBy(Ip4Header::Size, &data_node, pkt_send_len, &header_node);
            
            // Send the packet to the driver.
            err = route_iface->driverSendIp4Packet(frag_pkt, route_addr, retryReq);
            
            // If this was the last fragment or there was an error, return.
            if (!more_fragments || err != IpErr::SUCCESS) {
                return err;
            }
            
            // Update the fragment offset and skip the sent data.
            uint16_t data_sent = pkt_send_len - Ip4Header::Size;
            fragment_offset += data_sent;
            dgram.skipBytes(data_sent);
        }
    }
    
public:
    bool routeIp4 (Ip4Addr dst_addr, Iface **route_iface, Ip4Addr *route_addr)
    {
        // Look for an interface where dst_addr is inside the local subnet
        // (and in case of multiple matches find a most specific one).
        // Also look for an interface with a gateway to use in case there
        // is no local subnet match.
        
        int best_prefix = -1;
        Iface *best_iface = nullptr;
        
        for (Iface *iface = m_iface_list.first(); iface != nullptr; iface = m_iface_list.next(iface)) {
            if (iface->ip4AddrIsLocal(dst_addr)) {
                int iface_prefix = iface->m_addr.prefix;
                if (iface_prefix > best_prefix) {
                    best_prefix = iface_prefix;
                    best_iface = iface;
                }
            }
            else if (iface->m_have_gateway && best_iface == nullptr) {
                best_iface = iface;
            }
        }
        
        if (AMBRO_UNLIKELY(best_iface == nullptr)) {
            return false;
        }
        
        *route_iface = best_iface;
        *route_addr = (best_prefix >= 0) ? dst_addr : best_iface->m_gateway;
        
        return true;
    }
    
    bool routeIp4ForceIface (Ip4Addr dst_addr, Iface *force_iface, Iface **route_iface, Ip4Addr *route_addr)
    {
        AMBRO_ASSERT(force_iface != nullptr)
        
        // When an interface is forced the logic is almost the same except that only this
        // interface is considered and we also allow the all-ones broadcast address.
        
        if (dst_addr == Ip4Addr::AllOnesAddr() || force_iface->ip4AddrIsLocal(dst_addr)) {
            *route_addr = dst_addr;
        }
        else if (force_iface->m_have_gateway) {
            *route_addr = force_iface->m_gateway;
        }
        else {
            return false;
        }
        *route_iface = force_iface;
        return true;
    }
    
    inline bool handleIcmpPacketTooBig (Ip4Addr remote_addr, uint16_t mtu_info)
    {
        return m_path_mtu_cache.handleIcmpPacketTooBig(remote_addr, mtu_info);
    }
    
    static bool checkUnicastSrcAddr (Ip4DgramMeta const &ip_meta)
    {
        return !ip_meta.src_addr.isBroadcastOrMulticast() &&
               !ip_meta.iface->ip4AddrIsLocalBcast(ip_meta.src_addr);
    }
    
    class IfaceListener {
        friend IpStack;
        
    public:
        void init (Iface *iface, uint8_t proto)
        {
            m_iface = iface;
            m_proto = proto;
            
            m_iface->m_listeners_list.prepend(*this);
        }
        
        void deinit ()
        {
            m_iface->m_listeners_list.remove(*this);
        }
        
        inline Iface * getIface ()
        {
            return m_iface;
        }
        
    private:
        virtual bool recvIp4Dgram (Ip4DgramMeta const &ip_meta, IpBufRef dgram) = 0;
        
    private:
        APrinter::LinkedListNode<IfaceListenerLinkModel> m_list_node;
        Iface *m_iface;
        uint8_t m_proto;
    };
    
    class IfaceStateObserver : private Observer {
        friend IpStack;
        
    public:
        using Observer::init;
        using Observer::deinit;
        using Observer::reset;
        using Observer::isActive;
        
        inline void observe (Iface &iface)
        {
            Observer::observe(iface.m_state_observable);
        }
        
    private:
        virtual void ifaceStateChanged () = 0;
    };
    
private:
    using IfaceListenerList = APrinter::LinkedList<
        APRINTER_MEMBER_ACCESSOR_TN(&IfaceListener::m_list_node), IfaceListenerLinkModel, false>;
    
public:
    class Iface {
        friend IpStack;
        
    public:
        using IfaceIpStack = IpStack;
        
        void init (IpStack *stack)
        {
            AMBRO_ASSERT(stack != nullptr)
            
            // Initialize stuffs.
            m_stack = stack;
            m_hw_type = driverGetHwType();
            m_hw_iface = driverGetHwIface();
            m_have_addr = false;
            m_have_gateway = false;
            m_listeners_list.init();
            m_state_observable.init();
            
            // Get the MTU.
            m_ip_mtu = APrinter::MinValueU((uint16_t)UINT16_MAX, driverGetIpMtu());
            AMBRO_ASSERT(m_ip_mtu >= MinMTU)
            
            // Register interface.
            m_stack->m_iface_list.prepend(this);
        }
        
        void deinit ()
        {
            AMBRO_ASSERT(m_listeners_list.isEmpty())
            AMBRO_ASSERT(!m_state_observable.hasObservers())
            
            // Unregister interface.
            m_stack->m_iface_list.remove(this);
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
        
        inline IpHwType getHwType ()
        {
            return m_hw_type;
        }
        
        template <typename HwIface>
        inline HwIface * getHwIface ()
        {
            return static_cast<HwIface *>(m_hw_iface);
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
        
        inline IpIfaceDriverState getDriverState ()
        {
            return driverGetState();
        }
        
    protected:
        // These functions are implemented or called by the IP driver.
        
        virtual size_t driverGetIpMtu () = 0;
        
        virtual IpErr driverSendIp4Packet (IpBufRef pkt, Ip4Addr ip_addr,
                                           IpSendRetry::Request *sendRetryReq) = 0;
        
        virtual IpHwType driverGetHwType () = 0;
        
        virtual void * driverGetHwIface () = 0;
        
        virtual IpIfaceDriverState driverGetState () = 0;
        
        inline void recvIp4PacketFromDriver (IpBufRef pkt)
        {
            m_stack->processRecvedIp4Packet(this, pkt);
        }
        
        inline IpIfaceIp4Addrs const * getIp4AddrsFromDriver ()
        {
            return m_have_addr ? &m_addr : nullptr;
        }
        
        void stateChangedFromDriver ()
        {
            m_state_observable.template notifyObservers<false>([&](Observer &observer_base) {
                IfaceStateObserver &observer = static_cast<IfaceStateObserver &>(observer_base);
                observer.ifaceStateChanged();
            });
        }
        
    private:
        APrinter::DoubleEndedListNode<Iface> m_iface_list_node;
        IfaceListenerList m_listeners_list;
        Observable m_state_observable;
        IpStack *m_stack;
        void *m_hw_iface;
        uint16_t m_ip_mtu;
        IpIfaceIp4Addrs m_addr;
        Ip4Addr m_gateway;
        IpHwType m_hw_type;
        bool m_have_addr;
        bool m_have_gateway;
    };
    
private:
    using BaseMtuRef = typename PathMtuCache::MtuRef;
    
public:
    class MtuRef : private BaseMtuRef
    {
    public:
        using BaseMtuRef::init;
        
        inline void reset (IpStack *stack)
        {
            return BaseMtuRef::reset(mtu_cache(stack));
        }
        
        using BaseMtuRef::isSetup;
        
        inline bool setup (IpStack *stack, Ip4Addr remote_addr, Iface *iface, uint16_t &out_pmtu)
        {
            return BaseMtuRef::setup(mtu_cache(stack), remote_addr, iface, out_pmtu);
        }
        
        inline void moveFrom (MtuRef &src)
        {
            return BaseMtuRef::moveFrom(src);
        }
        
    protected:
        using BaseMtuRef::pmtuChanged;
        
    private:
        inline static PathMtuCache * mtu_cache (IpStack *stack)
        {
            return &stack->m_path_mtu_cache;
        }
    };
    
private:
    void processRecvedIp4Packet (Iface *iface, IpBufRef pkt)
    {
        // Check base IP header length.
        if (AMBRO_UNLIKELY(!pkt.hasHeader(Ip4Header::Size))) {
            return;
        }
        
        // Get a reference to the IP header.
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        
        // We will be calculating the header checksum inline.
        IpChksumAccumulator chksum;
        
        // Read Version+IHL+DSCP+ECN and add to checksum.
        uint16_t version_ihl_dscp_ecn = ip4_header.get(Ip4Header::VersionIhlDscpEcn());
        chksum.addWord(APrinter::WrapType<uint16_t>(), version_ihl_dscp_ecn);
        
        // Check IP version and header length...
        uint8_t version_ihl = version_ihl_dscp_ecn >> 8;
        uint8_t header_len;
        
        // Fast path is that the version is correctly 4 and the header
        // length is minimal (5 words = 20 bytes).
        if (AMBRO_LIKELY(version_ihl == ((4 << Ip4VersionShift) | 5))) {
            // Header length is minimal, no options. There is no need to check
            // pkt.hasHeader(header_len) since that was already done above.
            header_len = Ip4Header::Size;
        } else {
            // Check IP version.
            if (AMBRO_UNLIKELY((version_ihl >> Ip4VersionShift) != 4)) {
                return;
            }
            
            // Check header length.
            // We require the entire header to fit into the first buffer.
            header_len = (version_ihl & Ip4IhlMask) * 4;
            if (AMBRO_UNLIKELY(header_len < Ip4Header::Size || !pkt.hasHeader(header_len))) {
                return;
            }
            
            // Add options to checksum.
            chksum.addEvenBytes(ip4_header.data + Ip4Header::Size, header_len - Ip4Header::Size);
        }
        
        // Read total length and add to checksum.
        uint16_t total_len = ip4_header.get(Ip4Header::TotalLen());
        chksum.addWord(APrinter::WrapType<uint16_t>(), total_len);
        
        // Check total length.
        if (AMBRO_UNLIKELY(total_len < header_len || total_len > pkt.tot_len)) {
            return;
        }
        
        // Create a reference to the payload.
        IpBufRef dgram = pkt.hideHeader(header_len).subTo(total_len - header_len);
        
        // Add ident and header checksum to checksum.
        chksum.addWord(APrinter::WrapType<uint16_t>(), ip4_header.get(Ip4Header::Ident()));
        chksum.addWord(APrinter::WrapType<uint16_t>(), ip4_header.get(Ip4Header::HeaderChksum()));
        
        // Read TTL+protocol and add to checksum.
        uint16_t ttl_proto = ip4_header.get(Ip4Header::TtlProto());
        chksum.addWord(APrinter::WrapType<uint16_t>(), ttl_proto);
        
        // Create the datagram meta-info struct.
        Ip4DgramMeta const meta = {
            ip4_header.get(Ip4Header::SrcAddr()),
            ip4_header.get(Ip4Header::DstAddr()),
            (uint8_t)(ttl_proto >> 8), // ttl
            (uint8_t)ttl_proto, // protocol
            iface
        };
        
        // Add source and destination address to checksum.
        chksum.addWords(&meta.src_addr.data);
        chksum.addWords(&meta.dst_addr.data);
        
        // Get flags+offset and add to checksum.
        uint16_t flags_offset = ip4_header.get(Ip4Header::FlagsOffset());
        chksum.addWord(APrinter::WrapType<uint16_t>(), flags_offset);        
        
        // Verify IP header checksum.
        if (AMBRO_UNLIKELY(chksum.getChksum() != 0)) {
            return;
        }
        
        // Check if the more-fragments flag is set or the fragment offset is nonzero.
        if (AMBRO_UNLIKELY((flags_offset & (Ip4FlagMF|Ip4OffsetMask)) != 0)) {
            // Only accept fragmented packets which are unicasts to the
            // incoming interface address. This is to prevent filling up
            // our reassembly buffers with irrelevant packets. Note that
            // we don't check this for non-fragmented packets for
            // performance reasons, it generally up to protocol handlers.
            if (!meta.iface->ip4AddrIsLocalAddr(meta.dst_addr)) {
                return;
            }
            
            // Get the more-fragments flag and the fragment offset in bytes.
            bool more_fragments = (flags_offset & Ip4FlagMF) != 0;
            uint16_t fragment_offset = (flags_offset & Ip4OffsetMask) * 8;
            
            // Perform reassembly.
            if (!m_reassembly.reassembleIp4(ip4_header.get(Ip4Header::Ident()),
                meta.src_addr, meta.dst_addr, meta.proto, meta.ttl,
                more_fragments, fragment_offset, ip4_header.data, header_len, dgram))
            {
                return;
            }
            // Continue processing the reassembled datagram.
            // Note, dgram was modified pointing to the reassembled data.
        }
        
        // Do the real processing now that the datagram is complete and
        // sanity checked.
        recvIp4Dgram(meta, dgram);
    }
    
    void recvIp4Dgram (Ip4DgramMeta const &meta, IpBufRef dgram)
    {
        Iface *iface = meta.iface;
        uint8_t proto = meta.proto;
        
        // Pass to interface listeners. If any listener accepts the
        // packet, inhibit further processing.
        for (IfaceListener *lis = iface->m_listeners_list.first();
             lis != nullptr; lis = iface->m_listeners_list.next(*lis))
        {
            if (lis->m_proto == proto) {
                if (AMBRO_UNLIKELY(lis->recvIp4Dgram(meta, dgram))) {
                    return;
                }
            }
        }
        
        // Handle using a protocol listener if existing.
        bool not_handled = APrinter::ListForBreak<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            if (proto == Helper::IpProtocolNumber::Value) {
                Helper::get(this)->recvIp4Dgram(meta, dgram);
                return false;
            }
            return true;
        }));
        
        if (!not_handled) {
            return;
        }
        
        // Handle ICMP packets.
        if (proto == Ip4ProtocolIcmp) {
            return recvIcmp4Dgram(meta, dgram);
        }
    }
    
    void recvIcmp4Dgram (Ip4DgramMeta const &meta, IpBufRef const &dgram)
    {
        // Sanity check source address - reject broadcast addresses.
        if (AMBRO_UNLIKELY(!checkUnicastSrcAddr(meta))) {
            return;
        }
        
        // Check destination address.
        // Accept only: all-ones broadcast, subnet broadcast, unicast to interface address.
        if (AMBRO_UNLIKELY(
            !meta.iface->ip4AddrIsLocalAddr(meta.dst_addr) &&
            !meta.iface->ip4AddrIsLocalBcast(meta.dst_addr) &&
            meta.dst_addr != Ip4Addr::AllOnesAddr()))
        {
            return;
        }
        
        // Check ICMP header length.
        if (AMBRO_UNLIKELY(!dgram.hasHeader(Icmp4Header::Size))) {
            return;
        }
        
        // Read ICMP header fields.
        auto icmp4_header = Icmp4Header::MakeRef(dgram.getChunkPtr());
        uint8_t type       = icmp4_header.get(Icmp4Header::Type());
        uint8_t code       = icmp4_header.get(Icmp4Header::Code());
        uint16_t chksum    = icmp4_header.get(Icmp4Header::Chksum());
        Icmp4RestType rest = icmp4_header.get(Icmp4Header::Rest());
        
        // Verify ICMP checksum.
        uint16_t calc_chksum = IpChksum(dgram);
        if (AMBRO_UNLIKELY(calc_chksum != 0)) {
            return;
        }
        
        // Get ICMP data by hiding the ICMP header.
        IpBufRef icmp_data = dgram.hideHeader(Icmp4Header::Size);
        
        if (type == Icmp4TypeEchoRequest) {
            // Got echo request, send echo reply.
            sendIcmp4EchoReply(rest, icmp_data, meta.src_addr, meta.iface);
        }
        else if (type == Icmp4TypeDestUnreach) {
            handleIcmp4DestUnreach(code, rest, icmp_data, meta.iface);
        }
    }
    
    void sendIcmp4EchoReply (Icmp4RestType rest, IpBufRef const &data, Ip4Addr dst_addr, Iface *iface)
    {
        // Can only reply when we have an address assigned.
        if (!iface->m_have_addr) {
            return;
        }
        
        // Allocate memory for headers.
        TxAllocHelper<Icmp4Header::Size, HeaderBeforeIp4Dgram> dgram_alloc(Icmp4Header::Size);
        
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
    
    void handleIcmp4DestUnreach (uint8_t code, Icmp4RestType rest, IpBufRef const &icmp_data, Iface *iface)
    {
        // Check base IP header length.
        if (AMBRO_UNLIKELY(!icmp_data.hasHeader(Ip4Header::Size))) {
            return;
        }
        
        // Read IP header fields.
        auto ip4_header = Ip4Header::MakeRef(icmp_data.getChunkPtr());
        uint8_t version_ihl    = ip4_header.get(Ip4Header::VersionIhlDscpEcn()) >> 8;
        uint16_t total_len     = ip4_header.get(Ip4Header::TotalLen());
        uint16_t ttl_proto     = ip4_header.get(Ip4Header::TtlProto());
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
        uint8_t ttl = ttl_proto >> 8;
        uint8_t proto = ttl_proto;
        Ip4DgramMeta ip_meta = {src_addr, dst_addr, ttl, proto, iface};
        
        // Get the included IP data.
        size_t data_len = APrinter::MinValueU(icmp_data.tot_len, total_len) - header_len;
        IpBufRef dgram_initial = icmp_data.hideHeader(header_len).subTo(data_len);
        
        // Dispatch based on the protocol.
        APrinter::ListForBreak<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            if (ip_meta.proto == Helper::IpProtocolNumber::Value) {
                Helper::get(this)->handleIp4DestUnreach(du_meta, ip_meta, dgram_initial);
                return false;
            }
            return true;
        }));
    }
    
private:
    using IfaceList = APrinter::DoubleEndedList<Iface, &Iface::m_iface_list_node, false>;
    
    Reassembly m_reassembly;
    PathMtuCache m_path_mtu_cache;
    IfaceList m_iface_list;
    uint16_t m_next_id;
    APrinter::Tuple<ProtocolsList> m_protocols;
};

APRINTER_ALIAS_STRUCT_EXT(IpStackService, (
    APRINTER_AS_VALUE(size_t, HeaderBeforeIp),
    APRINTER_AS_VALUE(uint8_t, IcmpTTL),
    APRINTER_AS_VALUE(int, MaxReassEntrys),
    APRINTER_AS_VALUE(uint16_t, MaxReassSize),
    APRINTER_AS_TYPE(PathMtuParams)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ProtocolServicesList)
    ), (
        using Params = IpStackService;
        APRINTER_DEF_INSTANCE(Compose, IpStack)
    ))
))

#include <aipstack/EndNamespace.h>

#endif
