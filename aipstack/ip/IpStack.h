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

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Callback.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aipstack/misc/Err.h>
#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Chksum.h>
#include <aipstack/misc/Struct.h>
#include <aipstack/misc/SendRetry.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpIfaceDriver.h>

#include <aprinter/BeginNamespace.h>

struct IpIfaceIp4AddrSetting {
    bool present;
    uint8_t prefix;
    Ip4Addr addr;
};

struct IpIfaceIp4GatewaySetting {
    bool present;
    Ip4Addr addr;
};

template <typename Arg>
class IpStack {
    APRINTER_USE_VALS(Arg::Params, (HeaderBeforeIp, IcmpTTL, MaxReassEntrys, MaxReassSize))
    APRINTER_USE_TYPES1(Arg, (Context, BufAllocator))
    
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    APRINTER_USE_TYPE1(Context::EventLoop, TimedEvent)
    using TheClockUtils = ClockUtils<Context>;
    
    static_assert(MaxReassEntrys > 0, "");
    static_assert(MaxReassSize >= 576, "");
    
    static uint16_t const ReassNullLink = UINT16_MAX;
    
    APRINTER_TSTRUCT(HoleDescriptor,
        (HoleSize,       StructRawField<uint16_t>)
        (NextHoleOffset, StructRawField<uint16_t>)
    )
    
    // We need to be able to put a hole descriptor after the reassembled data.
    static_assert(MaxReassSize <= UINT16_MAX - HoleDescriptor::Size, "");
    
    // The size of the reassembly buffers, with additional space for a hole descriptor at the end.
    static uint16_t const ReassBufferSize = MaxReassSize + HoleDescriptor::Size;
    
    // Maximum time that a reassembly entry can be valid.
    static TimeType const ReassMaxExpirationTicks = 255.0 * (TimeType)Clock::time_freq;
    
    // Maximum number of holes during reassembly.
    static uint8_t const MaxReassHoles = 10;
    
    static_assert(ReassMaxExpirationTicks <= TheClockUtils::WorkingTimeSpanTicks, "");
    static_assert(MaxReassHoles <= 250, "");
    
public:
    static size_t const HeaderBeforeIp4Dgram = HeaderBeforeIp + Ip4Header::Size;
    
    // Minimum MTU is smallest IP header plus 8 bytes (for fragmentation to work).
    static size_t const MinIpIfaceMtu = Ip4Header::Size + 8;
    
    class Iface;
    
public:
    void init ()
    {
        m_reass_purge_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&IpStack::reass_purge_timer_handler, this));
        m_iface_list.init();
        m_proto_listeners_list.init();
        m_next_id = 0;
        
        // Start the reassembly purge timer.
        m_reass_purge_timer.appendAfter(Context(), ReassMaxExpirationTicks);
        
        // Initialize reassembly entries to unused.
        for (auto &reass : m_reass_packets) {
            reass.first_hole_offset = ReassNullLink;
        }
    }
    
    void deinit ()
    {
        AMBRO_ASSERT(m_iface_list.isEmpty())
        AMBRO_ASSERT(m_proto_listeners_list.isEmpty())
        
        m_reass_purge_timer.deinit(Context());
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
                        IpSendRetry::Request *retryReq = nullptr)
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
        
        // Check if fragmentation is needed and calculate the length of
        // the first packet.
        size_t mtu = route_iface->m_ip_mtu;
        bool more_fragments = pkt.tot_len > mtu;
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
    
    class ProtoListenerCallback {
    public:
        virtual void recvIp4Dgram (Ip4DgramMeta const &meta, IpBufRef dgram) = 0;
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
        DoubleEndedListNode<ProtoListener> m_listeners_list_node;
        uint8_t m_proto;
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
            m_ip_mtu = MinValueU((uint16_t)UINT16_MAX, m_driver->getIpMtu());
            AMBRO_ASSERT(m_ip_mtu >= MinIpIfaceMtu)
            
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
        
        // NOTE: Assuming no IP options.
        inline size_t getIp4DgramMtu ()
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
        DoubleEndedListNode<Iface> m_iface_list_node;
        IpStack *m_stack;
        IpIfaceDriver<CallbackImpl> *m_driver;
        size_t m_ip_mtu;
        IpIfaceIp4Addrs m_addr;
        Ip4Addr m_gateway;
        bool m_have_addr;
        bool m_have_gateway;
    };
    
private:
    struct ReassEntry {
        // Offset in data to the first hole, or ReassNullLink for free entry.
        uint16_t first_hole_offset;
        // The total data length, or 0 if last fragment not yet received.
        uint16_t data_length;
        // Time after which the entry is considered invalid.
        TimeType expiration_time;
        // IPv4 header.
        char header[Ip4MaxHeaderSize];
        // Data and holes, each hole starts with a HoleDescriptor.
        // The last HoleDescriptor::Size bytes are to ensure these is space
        // for the last hole descriptor, they cannot contain data.
        char data[ReassBufferSize];
    };
    
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
            if (!reassembleIp4(
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
        
        for (ProtoListener *lis = m_proto_listeners_list.first(); lis != nullptr; lis = m_proto_listeners_list.next(lis)) {
            if (lis->m_proto == meta.proto) {
                return lis->m_callback->recvIp4Dgram(meta, dgram);
            }
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
        
        // Verify ICMP checksum.
        uint16_t calc_chksum = IpChksum(dgram);
        if (calc_chksum != 0) {
            return;
        }
        
        // Get ICMP data by hiding the ICMP header.
        IpBufRef icmp_data = dgram.hideHeader(Icmp4Header::Size);
        
        if (type == Icmp4TypeEchoRequest) {
            // Got echo request, send echo reply.
            auto rest = icmp4_header.get(Icmp4Header::Rest());
            sendIcmp4EchoReply(rest, icmp_data, meta.remote_addr, meta.iface);
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
    
    bool reassembleIp4 (uint16_t ident, Ip4Addr src_addr, Ip4Addr dst_addr, uint8_t proto,
                        uint8_t ttl, bool more_fragments, uint16_t fragment_offset,
                        char const *header, uint8_t header_len, IpBufRef &dgram)
    {
        AMBRO_ASSERT(header_len <= Ip4MaxHeaderSize)
        AMBRO_ASSERT(dgram.tot_len <= UINT16_MAX)
        
        // Sanity check data length.
        if (dgram.tot_len == 0) {
            return false;
        }
        
        // Check if we have a reassembly entry for this datagram.
        TimeType now = Clock::getTime(Context());
        ReassEntry *reass = find_reass_entry(now, ident, src_addr, dst_addr, proto);
        
        if (reass == nullptr) {
            // Allocate an entry.
            reass = alloc_reass_entry(now, ttl);
            
            // Copy the IP header.
            ::memcpy(reass->header, header, header_len);
            
            // Set first hole and unknown data length.
            reass->first_hole_offset = 0;
            reass->data_length = 0;
            
            // Write a hole from start of data to infinity (ReassBufferSize).
            // The final HoleDescriptor::Size bytes of the hole serve as
            // infinity because they cannot be filled by a fragment. This also
            // means that we will always have at least one hole in the list.
            auto hole = HoleDescriptor::MakeRef(reass->data);
            hole.set(typename HoleDescriptor::HoleSize(),       ReassBufferSize);
            hole.set(typename HoleDescriptor::NextHoleOffset(), ReassNullLink);
        } else {
            // If this is the first fragment, update the IP header.
            if (fragment_offset == 0) {
                ::memcpy(reass->header, header, header_len);
            }
        }
        
        do {
            // Verify that the fragment fits into the buffer.
            if (fragment_offset > MaxReassSize || dgram.tot_len > MaxReassSize - fragment_offset) {
                goto invalidate_reass;
            }
            uint16_t fragment_end = fragment_offset + dgram.tot_len;
            
            // Summary of last-fragment related sanity checks:
            // - When we first receive a last fragment, we remember the data size and
            //   also check that we have not yet received any data that would fall
            //   beyond the end of this last fragment.
            // - When we receive any subsequent fragment after having received a last
            //   fragment, we check that it does not contain any data beyond the
            //   remembered end of data.
            // - When we receive any additional last fragment we check that it has
            //   the same end as the first received last fragment.
            
            // Is this the last fragment?
            if (!more_fragments) {
                // Check for inconsistent data_length.
                if (reass->data_length != 0 && fragment_end != reass->data_length) {
                    goto invalidate_reass;
                }
                
                // Remember the data_length.
                reass->data_length = fragment_end;
            } else {
                // Check for data beyond the end.
                if (reass->data_length != 0 && fragment_end > reass->data_length) {
                    goto invalidate_reass;
                }
            }
            
            // Update the holes based on this fragment.
            uint16_t prev_hole_offset = ReassNullLink;
            uint16_t hole_offset = reass->first_hole_offset;
            uint8_t num_holes = 0;
            do {
                AMBRO_ASSERT(prev_hole_offset == ReassNullLink ||
                             hole_offset_valid(prev_hole_offset))
                AMBRO_ASSERT(hole_offset_valid(hole_offset))
                
                // Get the hole info.
                auto hole = HoleDescriptor::MakeRef(reass->data + hole_offset);
                uint16_t hole_size        = hole.get(typename HoleDescriptor::HoleSize());
                uint16_t next_hole_offset = hole.get(typename HoleDescriptor::NextHoleOffset());
                
                // Calculate the hole end.
                AMBRO_ASSERT(hole_size <= ReassBufferSize - hole_offset)
                uint16_t hole_end = hole_offset + hole_size;
                
                // If this is the last fragment, sanity check that the hole offset
                // is not greater than the end of this fragment; this would mean
                // that some data was received beyond the end.
                if (!more_fragments && hole_offset > fragment_end) {
                    goto invalidate_reass;
                }
                
                // If the fragment does not overlap with the hole, skip the hole.
                if (fragment_offset >= hole_end || fragment_end <= hole_offset) {
                    prev_hole_offset = hole_offset;
                    hole_offset = next_hole_offset;
                    num_holes++;
                    continue;
                }
                
                // The fragment overlaps with the hole. We will be dismantling
                // this hole and creating between zero and two new holes.
                
                // Create a new hole on the left if needed.
                if (fragment_offset > hole_offset) {
                    // Sanity check hole size.
                    uint16_t new_hole_size = fragment_offset - hole_offset;
                    if (new_hole_size < HoleDescriptor::Size) {
                        goto invalidate_reass;
                    }
                    
                    // Write the hole size.
                    // Note that the hole is in the same place as the old hole.
                    hole.set(typename HoleDescriptor::HoleSize(), new_hole_size);
                    
                    // The link to this hole is already set up.
                    //reass_link_prev(reass, prev_hole_offset, hole_offset);
                    
                    // Advance prev_hole_offset to this hole.
                    prev_hole_offset = hole_offset;
                    
                    num_holes++;
                }
                
                // Create a new hole on the right if needed.
                if (fragment_end < hole_end) {
                    // Sanity check hole size.
                    uint16_t new_hole_size = hole_end - fragment_end;
                    if (new_hole_size < HoleDescriptor::Size) {
                        goto invalidate_reass;
                    }
                    
                    // Write the hole size.
                    auto new_hole = HoleDescriptor::MakeRef(reass->data + fragment_end);
                    new_hole.set(typename HoleDescriptor::HoleSize(), new_hole_size);
                    
                    // Setup the link to this hole.
                    reass_link_prev(reass, prev_hole_offset, fragment_end);
                    
                    // Advance prev_hole_offset to this hole.
                    prev_hole_offset = fragment_end;
                    
                    num_holes++;
                }
                
                // Setup the link to the next hole.
                reass_link_prev(reass, prev_hole_offset, next_hole_offset);
                
                // Advance to the next hole (if any).
                hole_offset = next_hole_offset;
            } while (hole_offset != ReassNullLink);
            
            // It is not possible that there are no more holes due to
            // the final HoleDescriptor::Size bytes that cannot be filled.
            AMBRO_ASSERT(reass->first_hole_offset != ReassNullLink)
            
            // Copy the fragment data into the reassembly buffer.
            IpBufRef dgram_tmp = dgram;
            dgram_tmp.takeBytes(dgram.tot_len, reass->data + fragment_offset);
            
            // If we have not yet received the final fragment or there
            // are still holes after the end, the reassembly is not complete.
            if (reass->data_length == 0 || reass->first_hole_offset < reass->data_length) {
                // If there are too many holes, invalidate.
                if (num_holes > MaxReassHoles) {
                    goto invalidate_reass;
                }
                return false;
            }
            
            // Invalidate the reassembly entry.
            reass->first_hole_offset = ReassNullLink;
            
            // Setup dgram to point to the reassembled data.
            m_reass_node = IpBufNode{reass->data, MaxReassSize};
            dgram = IpBufRef{&m_reass_node, 0, reass->data_length};
            
            // Continue to process the reassembled datagram.
            return true;
        } while (false);
        
    invalidate_reass:
        reass->first_hole_offset = ReassNullLink;
        return false;
    }
    
    ReassEntry * find_reass_entry (TimeType now, uint16_t ident, Ip4Addr src_addr, Ip4Addr dst_addr, uint8_t proto)
    {
        ReassEntry *found_entry = nullptr;
        
        for (auto &reass : m_reass_packets) {
            // Ignore free entries.
            if (reass.first_hole_offset == ReassNullLink) {
                continue;
            }
            
            // If the entry has expired, mark is as free and ignore.
            if ((TimeType)(reass.expiration_time - now) > ReassMaxExpirationTicks) {
                reass.first_hole_offset = ReassNullLink;
                continue;
            }
            
            // If the entry matches, return it after goingh through all.
            auto reass_hdr = Ip4Header::MakeRef(reass.header);
            if (reass_hdr.get(Ip4Header::Ident())    == ident &&
                reass_hdr.get(Ip4Header::SrcAddr())  == src_addr &&
                reass_hdr.get(Ip4Header::DstAddr())  == dst_addr &&
                reass_hdr.get(Ip4Header::Protocol()) == proto)
            {
                found_entry = &reass;
            }
        }
        
        return found_entry;
    }
    
    ReassEntry * alloc_reass_entry (TimeType now, uint8_t ttl)
    {
        TimeType future = now + ReassMaxExpirationTicks;
        
        ReassEntry *result_reass = nullptr;
        
        for (auto &reass : m_reass_packets) {
            // If the entry is unused, use it.
            if (reass.first_hole_offset == ReassNullLink) {
                result_reass = &reass;
                break;
            }
            
            // Look for the entry with the least expiration time.
            if (result_reass == nullptr ||
                (TimeType)(future - reass.expiration_time) > (TimeType)(future - result_reass->expiration_time))
            {
                result_reass = &reass;
            }
        }
        
        // Set the expiration time to TTL seconds in the future.
        result_reass->expiration_time = now + ttl * (TimeType)Clock::time_freq;
        
        return result_reass;
    }
    
    static void reass_link_prev (ReassEntry *reass, uint16_t prev_hole_offset, uint16_t hole_offset)
    {
        AMBRO_ASSERT(prev_hole_offset == ReassNullLink || hole_offset_valid(prev_hole_offset))
        
        if (prev_hole_offset == ReassNullLink) {
            reass->first_hole_offset = hole_offset;
        } else {
            auto prev_hole = HoleDescriptor::MakeRef(reass->data + prev_hole_offset);
            prev_hole.set(typename HoleDescriptor::NextHoleOffset(), hole_offset);
        }
    }
    
    static bool hole_offset_valid (uint16_t hole_offset)
    {
        return hole_offset <= MaxReassSize;
    }
    
    void reass_purge_timer_handler (Context)
    {
        // Restart the timer.
        m_reass_purge_timer.appendAfter(Context(), ReassMaxExpirationTicks);
        
        // Purge any expired reassembly entries.
        TimeType now = Clock::getTime(Context());
        find_reass_entry(now, 0, Ip4Addr::ZeroAddr(), Ip4Addr::ZeroAddr(), 0);
    }
    
    static size_t round_frag_length (uint8_t header_length, size_t pkt_length)
    {
        return header_length + (((pkt_length - header_length) / 8) * 8);
    }
    
private:
    using IfaceList = DoubleEndedList<Iface, &Iface::m_iface_list_node, false>;
    using ProtoListenersList = DoubleEndedList<ProtoListener, &ProtoListener::m_listeners_list_node, false>;
    
    TimedEvent m_reass_purge_timer;
    IfaceList m_iface_list;
    ProtoListenersList m_proto_listeners_list;
    uint16_t m_next_id;
    IpBufNode m_reass_node;
    ReassEntry m_reass_packets[MaxReassEntrys];
};

APRINTER_ALIAS_STRUCT_EXT(IpStackService, (
    APRINTER_AS_VALUE(size_t, HeaderBeforeIp),
    APRINTER_AS_VALUE(uint8_t, IcmpTTL),
    APRINTER_AS_VALUE(int, MaxReassEntrys),
    APRINTER_AS_VALUE(uint16_t, MaxReassSize)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(BufAllocator)
    ), (
        using Params = IpStackService;
        APRINTER_DEF_INSTANCE(Compose, IpStack)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
