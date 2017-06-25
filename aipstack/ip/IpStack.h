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
#include <aipstack/ip/IpStackHelperTypes.h>
#include <aipstack/ip/hw/IpHwCommon.h>

#include <aipstack/BeginNamespace.h>

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
    
    // Helper to extract IpProtocolNumber from a ProtocolHelper.
    APRINTER_DEFINE_MEMBER_TYPE(MemberTypeIpProtocolNumber, IpProtocolNumber)
    
public:
    // How much space that must be available in outgoing packets for headers.
    static size_t const HeaderBeforeIp4Dgram = HeaderBeforeIp + Ip4Header::Size;
    
    class Iface;
    class IfaceListener;
    
private:
    using IfaceLinkModel = APrinter::PointerLinkModel<Iface>;
    using IfaceListenerLinkModel = APrinter::PointerLinkModel<IfaceListener>;
    
public:
    /**
     * Minimum permitted MTU and PMTU.
     * 
     * RFC 791 requires that routers can pass through 68 byte packets, so enforcing
     * this larger value theoreticlaly violates the standard. We need this to
     * simplify the implementation of TCP, notably so that the TCP headers do not
     * need to be fragmented and the DF flag never needs to be turned off. Note that
     * Linux enforces a minimum of 552, this must be perfectly okay in practice.
     */
    static uint16_t const MinMTU = 256;
    
    /**
     * Initialize the IP stack.
     */
    void init ()
    {
        // Initialize helper objects.
        m_reassembly.init();
        m_path_mtu_cache.init(this);
        
        // Initialize the list of interfaces.
        m_iface_list.init();
        
        // Initialize the packet identification counter.
        m_next_id = 0;
        
        // Initialize protocol handlers.
        APrinter::ListFor<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            Helper::get(this)->init(this);
        }));
    }
    
    /**
     * Deinitialize the IP stack.
     * 
     * There must be no remaining interfaces associated with this stack
     * when this is called.
     */
    void deinit ()
    {
        AMBRO_ASSERT(m_iface_list.isEmpty())
        
        // Deinitialize the protocol handlers.
        APrinter::ListForReverse<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            Helper::get(this)->deinit();
        }));
        
        // Deinitialize helper objects.
        m_path_mtu_cache.deinit();
        m_reassembly.deinit();
    }
    
    /**
     * Get the protocol instance type for a protocol number.
     * 
     * @tparam ProtocolNumber The IP procol number to get the type for. It must be the
     *                        number of one of the configured procotols.
     */
    template <uint8_t ProtocolNumber>
    using GetProtocolType = typename APrinter::TypeListGetMapped<
        ProtocolHelpersList,
        typename MemberTypeIpProtocolNumber::Get,
        APrinter::WrapValue<uint8_t, ProtocolNumber>
    >::Protocol;
    
    /**
     * Get the pointer to a protocol instance given the protocol instance type.
     * 
     * @tparam Protocol The protocol instance type. It must be the instance type of one
     *                  of the configured protocols.
     * @return Pointer to protocol instance.
     */
    template <typename Protocol>
    inline Protocol * getProtocol ()
    {
        return APrinter::TupleFindElem<Protocol>(&m_protocols);
    }
    
public:
    /**
     * Encapsulates route information returned route functions.
     * 
     * Functions such as @ref routeIp4 and @ref routeIp4ForceIface will fill in
     * this structure. The result is only valid temporarily because it contains
     * a pointer to an interface, which could be removed.
     */
    struct Ip4RouteInfo {
        /**
         * The interface to send through.
         */
        Iface *iface;
        
        /**
         * The address of the next hop.
         */
        Ip4Addr addr;
    };
    
    /**
     * Encapsulates information about a received IPv4 datagram.
     * 
     * This is filled in by the stack and passed to the recvIp4Dgram function of
     * protocol handlers and also to @ref IfaceListener::recvIp4Dgram.
     */
    struct Ip4RxInfo {
        /**
         * The source address.
         */
        Ip4Addr src_addr;
        
        /**
         * The destination address.
         */
        Ip4Addr dst_addr;
        
        /**
         * The TTL and protocol fields combined.
         */
        Ip4TtlProto ttl_proto;
        
        /**
         * The interface through which the packet was received.
         */
        Iface *iface;
    };
    
    /**
     * Send an IPv4 datagram.
     * 
     * This is the primary send function intended to be used by protocol handlers.
     * 
     * This function internally uses @ref routeIp4 or @ref routeIp4ForceIface (depending
     * on whether iface is given) to determine the required routing information. If this
     * fails, the error @ref IpErr::NO_IP_ROUTE will be returned.
     * 
     * This function will perform IP fragmentation unless send_flags includes
     * @ref IpSendFlags::DontFragmentFlag. If fragmentation would be needed but this
     * flag is set, the error @ref IpErr::FRAG_NEEDED will be returned. If sending one
     * fragment fails, further fragments are not sent.
     * 
     * Each attempt to send a datagram will result in assignment of an identification
     * number, except when the function fails with @ref IpErr::NO_IP_ROUTE or
     * @ref IpErr::FRAG_NEEDED as noted above. Identification numbers are generated
     * sequentially and there is no attempt to track which numbers are in use.
     * 
     * @param addrs Source and destination address.
     * @param ttl_proto The TTL and protocol fields combined.
     * @param dgram The data to be sent. There must be space available before the
     *              data for the IPv4 header and lower-layer headers (reserving
     *              @ref HeaderBeforeIp4Dgram will suffice). The tot_len of the data
     *              must not exceed 2^16-1.
     * @param iface If not null, force sending through this interface.
     * @param retryReq If not null, this may provide notification when to retry sending
     *                 after an unsuccessful attempt (notification is not guaranteed).
     * @param send_flags IP flags to send. The only allowed flag is
     *                   @ref IpSendFlags::DontFragmentFlag, other bits must not be set.
     * @return Success or error code.
     */
    APRINTER_NO_INLINE
    IpErr sendIp4Dgram (Ip4Addrs const &addrs, Ip4TtlProto ttl_proto, IpBufRef dgram,
                        Iface *iface, IpSendRetry::Request *retryReq, uint16_t send_flags)
    {
        AMBRO_ASSERT(dgram.tot_len <= UINT16_MAX)
        AMBRO_ASSERT(dgram.offset >= Ip4Header::Size)
        AMBRO_ASSERT((send_flags & ~IpSendFlags::AllFlags) == 0)
        
        // Reveal IP header.
        IpBufRef pkt = dgram.revealHeaderMust(Ip4Header::Size);
        
        // Find an interface and address for output.
        Ip4RouteInfo route_info;
        bool route_ok;
        if (AMBRO_UNLIKELY(iface != nullptr)) {
            route_ok = routeIp4ForceIface(addrs.remote_addr, iface, route_info);
        } else {
            route_ok = routeIp4(addrs.remote_addr, route_info);
        }
        if (AMBRO_UNLIKELY(!route_ok)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Check if fragmentation is needed...
        uint16_t pkt_send_len;
        
        if (AMBRO_UNLIKELY(pkt.tot_len > route_info.iface->getMtu())) {
            // Reject fragmentation?
            if (AMBRO_UNLIKELY((send_flags & IpSendFlags::DontFragmentFlag) != 0)) {
                return IpErr::FRAG_NEEDED;
            }
            
            // Calculate length of first fragment.
            pkt_send_len = Ip4RoundFragLen(Ip4Header::Size, route_info.iface->getMtu());
            
            // Set the MoreFragments IP flag (will be cleared for the last fragment).
            send_flags |= Ip4FlagMF;
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
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), send_flags);
        ip4_header.set(Ip4Header::FlagsOffset(), send_flags);
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), ttl_proto.value);
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto.value);
        
        chksum.addWords(&addrs.local_addr.data);
        ip4_header.set(Ip4Header::SrcAddr(), addrs.local_addr);
        
        chksum.addWords(&addrs.remote_addr.data);
        ip4_header.set(Ip4Header::DstAddr(), addrs.remote_addr);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        // Fast path is no fragmentation, this permits tail call optimization.
        if (AMBRO_LIKELY((send_flags & Ip4FlagMF) == 0)) {
            return route_info.iface->driverSendIp4Packet(pkt, route_info.addr, retryReq);
        }
        
        // Slow path...
        return send_fragmented(pkt, route_info, send_flags, retryReq);
    }
    
private:
    IpErr send_fragmented (IpBufRef pkt, Ip4RouteInfo route_info,
                           uint16_t send_flags, IpSendRetry::Request *retryReq)
    {
        // Recalculate pkt_send_len (not passed for optimization).
        uint16_t pkt_send_len =
            Ip4RoundFragLen(Ip4Header::Size, route_info.iface->getMtu());
        
        // Send the first fragment.
        IpErr err = route_info.iface->driverSendIp4Packet(
            pkt.subTo(pkt_send_len), route_info.addr, retryReq);
        if (AMBRO_UNLIKELY(err != IpErr::SUCCESS)) {
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
            // This is achieved by Ip4RoundFragLen.
            AMBRO_ASSERT(fragment_offset % 8 == 0)
            
            // If this is the last fragment, calculate its length and clear
            // the MoreFragments flag. Otherwise pkt_send_len is still correct
            // and MoreFragments still set.
            size_t rem_pkt_length = Ip4Header::Size + dgram.tot_len;
            if (rem_pkt_length <= route_info.iface->getMtu()) {
                pkt_send_len = rem_pkt_length;
                send_flags &= ~Ip4FlagMF;
            }
            
            auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
            
            // Write the fragment-specific IP header fields.
            ip4_header.set(Ip4Header::TotalLen(),     pkt_send_len);
            ip4_header.set(Ip4Header::FlagsOffset(),  send_flags | (fragment_offset / 8));
            ip4_header.set(Ip4Header::HeaderChksum(), 0);
            
            // Calculate the IP header checksum.
            // Not inline since fragmentation is uncommon, better save program space.
            uint16_t calc_chksum = IpChksum(ip4_header.data, Ip4Header::Size);
            ip4_header.set(Ip4Header::HeaderChksum(), calc_chksum);
            
            // Construct a packet with header and partial data.
            IpBufNode data_node = dgram.toNode();
            IpBufNode header_node;
            IpBufRef frag_pkt = pkt.subHeaderToContinuedBy(
                Ip4Header::Size, &data_node, pkt_send_len, &header_node);
            
            // Send the packet to the driver.
            err = route_info.iface->driverSendIp4Packet(
                frag_pkt, route_info.addr, retryReq);
            
            // If this was the last fragment or there was an error, return.
            if ((send_flags & Ip4FlagMF) == 0 || AMBRO_UNLIKELY(err != IpErr::SUCCESS)) {
                return err;
            }
            
            // Update the fragment offset and skip the sent data.
            uint16_t data_sent = pkt_send_len - Ip4Header::Size;
            fragment_offset += data_sent;
            dgram.skipBytes(data_sent);
        }
    }
    
public:
    /**
     * Stores reusable data for sending multiple packets efficiently.
     * 
     * This structure is filled in by @ref prepareSendIp4Dgram and can then be
     * used with @ref sendIp4DgramFast multiple times to send datagrams.
     * 
     * Values filled in this structure are only valid temporarily because the
     * route_info contains a pointer to an interface, which could be removed.
     */
    struct Ip4SendPrepared {
        Ip4RouteInfo route_info;
        IpChksumAccumulator::State partial_chksum_state;
    };
    
    /**
     * Prepare for sending multiple datagrams with similar header fields.
     * 
     * This determines routing information, fills in common header fields and
     * stores internal information into the given @ref Ip4SendPrepared structure.
     * After this is successful, @ref sendIp4DgramFast can be used to send multiple
     * datagrams in succession, with IP header fields as specified here.
     * 
     * This mechanism is intended for bulk transmission where performance is desired.
     * Fragmentation or forcing an interface are not supported.
     * 
     * @param addrs Source and destination address.
     * @param ttl_proto The TTL and protocol fields combined.
     * @param header_end_ptr Pointer to the end of the IPv4 header (and start of data).
     *                       This must be the same location as for subsequent datagrams.
     * @param send_flags IP flags to send. The only allowed flag is
     *                   @ref IpSendFlags::DontFragmentFlag, other bits must not be set.
     * @param prep Internal information is stored into this structure.
     * @return Success or error code.
     */
    AMBRO_ALWAYS_INLINE
    IpErr prepareSendIp4Dgram (Ip4Addrs const &addrs, Ip4TtlProto ttl_proto,
                        char *header_end_ptr, uint16_t send_flags, Ip4SendPrepared &prep)
    {
        AMBRO_ASSERT((send_flags & ~IpSendFlags::AllFlags) == 0)
        
        // Get routing information (fill in route_info).
        if (AMBRO_UNLIKELY(!routeIp4(addrs.remote_addr, prep.route_info))) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Write IP header fields and calculate partial header checksum inline...
        auto ip4_header = Ip4Header::MakeRef(header_end_ptr - Ip4Header::Size);
        IpChksumAccumulator chksum;
        
        uint16_t version_ihl_dscp_ecn = (uint16_t)((4 << Ip4VersionShift) | 5) << 8;
        chksum.addWord(APrinter::WrapType<uint16_t>(), version_ihl_dscp_ecn);
        ip4_header.set(Ip4Header::VersionIhlDscpEcn(), version_ihl_dscp_ecn);
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), send_flags);
        ip4_header.set(Ip4Header::FlagsOffset(), send_flags);
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), ttl_proto.value);
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto.value);
        
        chksum.addWords(&addrs.local_addr.data);
        ip4_header.set(Ip4Header::SrcAddr(), addrs.local_addr);
        
        chksum.addWords(&addrs.remote_addr.data);
        ip4_header.set(Ip4Header::DstAddr(), addrs.remote_addr);
        
        // Save the partial header checksum.
        prep.partial_chksum_state = chksum.getState();
        
        return IpErr::SUCCESS;
    }
    
    /**
     * Send a datagram after preparation with @ref prepareSendIp4Dgram.
     * 
     * This sends a single datagram with header fields as specified in a previous
     * @ref prepareSendIp4Dgram call.
     * 
     * This function does not support fragmentation. If the packet would be too
     * large, the error @ref IpErr::FRAG_NEEDED is returned.
     * 
     * @param prep Structure with internal information that was filled in
     *             using @ref prepareSendIp4Dgram. Note that such information is
     *             only valid temporarily (see the note in @ref Ip4SendPrepared).
     * @param dgram The data to be sent. There must be space available before the
     *              data for the IPv4 header and lower-layer headers (reserving
     *              @ref HeaderBeforeIp4Dgram will suffice), and this must be the
     *              same buffer that was used in @ref prepareSendIp4Dgram via the
     *              header_end_ptr argument. The tot_len of the data must not
     *              exceed 2^16-1.
     * @param retryReq If not null, this may provide notification when to retry sending
     *                 after an unsuccessful attempt (notification is not guaranteed).
     * @return Success or error code.
     */
    AMBRO_ALWAYS_INLINE
    IpErr sendIp4DgramFast (Ip4SendPrepared const &prep, IpBufRef dgram,
                            IpSendRetry::Request *retryReq)
    {
        AMBRO_ASSERT(dgram.tot_len <= UINT16_MAX)
        AMBRO_ASSERT(dgram.offset >= Ip4Header::Size)
        
        // Reveal IP header.
        IpBufRef pkt = dgram.revealHeaderMust(Ip4Header::Size);
        
        // This function does not support fragmentation.
        if (AMBRO_UNLIKELY(pkt.tot_len > prep.route_info.iface->getMtu())) {
            return IpErr::FRAG_NEEDED;
        }
        
        // Write remaining IP header fields and continue calculating header checksum...
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        IpChksumAccumulator chksum(prep.partial_chksum_state);
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), pkt.tot_len);
        ip4_header.set(Ip4Header::TotalLen(), pkt.tot_len);
        
        uint16_t ident = m_next_id++; // generate identification number
        chksum.addWord(APrinter::WrapType<uint16_t>(), ident);
        ip4_header.set(Ip4Header::Ident(), ident);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        return prep.route_info.iface->driverSendIp4Packet(
            pkt, prep.route_info.addr, retryReq);
    }
    
    bool routeIp4 (Ip4Addr dst_addr, Ip4RouteInfo &route_info)
    {
        // Look for an interface where dst_addr is inside the local subnet
        // (and in case of multiple matches find a most specific one).
        // Also look for an interface with a gateway to use in case there
        // is no local subnet match.
        
        int best_prefix = -1;
        Iface *best_iface = nullptr;
        
        for (Iface *iface = m_iface_list.first(); iface != nullptr; iface = m_iface_list.next(*iface)) {
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
        
        route_info.iface = best_iface;
        route_info.addr = (best_prefix >= 0) ? dst_addr : best_iface->m_gateway;
        
        return true;
    }
    
    bool routeIp4ForceIface (Ip4Addr dst_addr, Iface *force_iface, Ip4RouteInfo &route_info)
    {
        AMBRO_ASSERT(force_iface != nullptr)
        
        // When an interface is forced the logic is almost the same except that only this
        // interface is considered and we also allow the all-ones broadcast address.
        
        if (dst_addr == Ip4Addr::AllOnesAddr() || force_iface->ip4AddrIsLocal(dst_addr)) {
            route_info.addr = dst_addr;
        }
        else if (force_iface->m_have_gateway) {
            route_info.addr = force_iface->m_gateway;
        }
        else {
            return false;
        }
        route_info.iface = force_iface;
        return true;
    }
    
    inline bool handleIcmpPacketTooBig (Ip4Addr remote_addr, uint16_t mtu_info)
    {
        return m_path_mtu_cache.handleIcmpPacketTooBig(remote_addr, mtu_info);
    }
    
    static bool checkUnicastSrcAddr (Ip4RxInfo const &ip_info)
    {
        return !ip_info.src_addr.isBroadcastOrMulticast() &&
               !ip_info.iface->ip4AddrIsLocalBcast(ip_info.src_addr);
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
        virtual bool recvIp4Dgram (Ip4RxInfo const &ip_info, IpBufRef dgram) = 0;
        
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
            m_stack->m_iface_list.prepend(*this);
        }
        
        void deinit ()
        {
            AMBRO_ASSERT(m_listeners_list.isEmpty())
            AMBRO_ASSERT(!m_state_observable.hasObservers())
            
            // Unregister interface.
            m_stack->m_iface_list.remove(*this);
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
            processRecvedIp4Packet(this, pkt);
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
        APrinter::LinkedListNode<IfaceLinkModel> m_iface_list_node;
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
    using IfaceList = APrinter::LinkedList<
        APRINTER_MEMBER_ACCESSOR_TN(&Iface::m_iface_list_node), IfaceLinkModel, false>;
    
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
    static void processRecvedIp4Packet (Iface *iface, IpBufRef pkt)
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
        Ip4TtlProto ttl_proto = ip4_header.get(Ip4Header::TtlProto());
        chksum.addWord(APrinter::WrapType<uint16_t>(), ttl_proto.value);
        
        // Read addresses and add to checksum
        Ip4Addr src_addr = ip4_header.get(Ip4Header::SrcAddr());
        chksum.addWords(&src_addr.data);
        Ip4Addr dst_addr = ip4_header.get(Ip4Header::DstAddr());
        chksum.addWords(&dst_addr.data);
        
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
            if (!iface->ip4AddrIsLocalAddr(dst_addr)) {
                return;
            }
            
            // Get the more-fragments flag and the fragment offset in bytes.
            bool more_fragments = (flags_offset & Ip4FlagMF) != 0;
            uint16_t fragment_offset = (flags_offset & Ip4OffsetMask) * 8;
            
            // Perform reassembly.
            if (!iface->m_stack->m_reassembly.reassembleIp4(
                ip4_header.get(Ip4Header::Ident()), src_addr, dst_addr,
                ttl_proto.proto(), ttl_proto.ttl(), more_fragments,
                fragment_offset, ip4_header.data, dgram))
            {
                return;
            }
            // Continue processing the reassembled datagram.
            // Note, dgram was modified pointing to the reassembled data.
        }
        
        // Do the real processing now that the datagram is complete and
        // sanity checked.
        recvIp4Dgram({src_addr, dst_addr, ttl_proto, iface}, dgram);
    }
    
    static void recvIp4Dgram (Ip4RxInfo ip_info, IpBufRef dgram)
    {
        uint8_t proto = ip_info.ttl_proto.proto();
        
        // Pass to interface listeners. If any listener accepts the
        // packet, inhibit further processing.
        for (IfaceListener *lis = ip_info.iface->m_listeners_list.first();
             lis != nullptr; lis = ip_info.iface->m_listeners_list.next(*lis))
        {
            if (lis->m_proto == proto) {
                if (AMBRO_UNLIKELY(lis->recvIp4Dgram(ip_info, dgram))) {
                    return;
                }
            }
        }
        
        // Handle using a protocol listener if existing.
        bool not_handled = APrinter::ListForBreak<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            if (proto == Helper::IpProtocolNumber::Value) {
                Helper::get(ip_info.iface->m_stack)->recvIp4Dgram(ip_info, dgram);
                return false;
            }
            return true;
        }));
        
        if (!not_handled) {
            return;
        }
        
        // Handle ICMP packets.
        if (proto == Ip4ProtocolIcmp) {
            return recvIcmp4Dgram(ip_info, dgram);
        }
    }
    
    static void recvIcmp4Dgram (Ip4RxInfo const &ip_info, IpBufRef const &dgram)
    {
        // Sanity check source address - reject broadcast addresses.
        if (AMBRO_UNLIKELY(!checkUnicastSrcAddr(ip_info))) {
            return;
        }
        
        // Check destination address.
        // Accept only: all-ones broadcast, subnet broadcast, unicast to interface address.
        if (AMBRO_UNLIKELY(
            !ip_info.iface->ip4AddrIsLocalAddr(ip_info.dst_addr) &&
            !ip_info.iface->ip4AddrIsLocalBcast(ip_info.dst_addr) &&
            ip_info.dst_addr != Ip4Addr::AllOnesAddr()))
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
        
        IpStack *stack = ip_info.iface->m_stack;
        
        if (type == Icmp4TypeEchoRequest) {
            // Got echo request, send echo reply.
            stack->sendIcmp4EchoReply(rest, icmp_data, ip_info.src_addr, ip_info.iface);
        }
        else if (type == Icmp4TypeDestUnreach) {
            stack->handleIcmp4DestUnreach(code, rest, icmp_data, ip_info.iface);
        }
    }
    
    void sendIcmp4EchoReply (Icmp4RestType rest, IpBufRef data, Ip4Addr dst_addr, Iface *iface)
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
        Ip4Addrs addrs{iface->m_addr.addr, dst_addr};
        sendIp4Dgram(addrs, {IcmpTTL, Ip4ProtocolIcmp}, dgram, iface, nullptr, 0);
    }
    
    void handleIcmp4DestUnreach (uint8_t code, Icmp4RestType rest, IpBufRef icmp_data, Iface *iface)
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
        
        // Create the Ip4RxInfo struct.
        Ip4RxInfo ip_info = {src_addr, dst_addr, ttl_proto, iface};
        
        // Get the included IP data.
        size_t data_len = APrinter::MinValueU(icmp_data.tot_len, total_len) - header_len;
        IpBufRef dgram_initial = icmp_data.hideHeader(header_len).subTo(data_len);
        
        // Dispatch based on the protocol.
        APrinter::ListForBreak<ProtocolHelpersList>([&] APRINTER_TL(Helper, {
            if (ip_info.ttl_proto.proto() == Helper::IpProtocolNumber::Value) {
                Helper::get(this)->handleIp4DestUnreach(du_meta, ip_info, dgram_initial);
                return false;
            }
            return true;
        }));
    }
    
private:
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
