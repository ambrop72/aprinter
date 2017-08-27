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

#include <limits>

#include <stddef.h>
#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/InstantiateVariadic.h>
#include <aprinter/meta/MakeTupleOfSame.h>
#include <aprinter/meta/ResourceTuple.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/EnumBitfieldUtils.h>
#include <aprinter/base/NonCopyable.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/ObserverNotification.h>
#include <aprinter/structure/StructureRaiiWrapper.h>

#include <aipstack/misc/Err.h>
#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Chksum.h>
#include <aipstack/misc/SendRetry.h>
#include <aipstack/misc/TxAllocHelper.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpPathMtuCache.h>
#include <aipstack/ip/IpStackHelperTypes.h>
#include <aipstack/ip/hw/IpHwCommon.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

/**
 * IPv4 network layer implementation.
 * 
 * This class provides basic IPv4 services. It communicates with interface
 * drivers on one and and with protocol handlers on the other.
 * 
 * @tparam Arg Instantiation parameters (use via @ref IpStackService).
 */
template <typename Arg>
class IpStack :
    private APrinter::NonCopyable<IpStack<Arg>>
{
    APRINTER_USE_TYPES1(Arg, (Params, PlatformImpl, ProtocolServicesList))
    APRINTER_USE_VALS(Params, (HeaderBeforeIp, IcmpTTL, AllowBroadcastPing))
    APRINTER_USE_TYPES1(Params, (PathMtuParams, ReassemblyService))
    
    APRINTER_USE_VALS(APrinter, (EnumZero))
    
    using Platform = PlatformFacade<PlatformImpl>;
    APRINTER_USE_TYPE1(Platform, TimeType)
    
    APRINTER_MAKE_INSTANCE(Reassembly, (ReassemblyService::template Compose<PlatformImpl>))
    
    using PathMtuCacheService = IpPathMtuCacheService<PathMtuParams>;
    APRINTER_MAKE_INSTANCE(PathMtuCache, (PathMtuCacheService::template Compose<PlatformImpl, IpStack>))
    
    // Instantiate the protocols.
    template <int ProtocolIndex>
    struct ProtocolHelper {
        // Get the protocol service.
        using ProtocolService = APrinter::TypeListGet<ProtocolServicesList, ProtocolIndex>;
        
        // Expose the protocol number for TypeListGetMapped (GetProtocolType).
        using IpProtocolNumber = typename ProtocolService::IpProtocolNumber;
        
        // Instantiate the protocol.
        APRINTER_MAKE_INSTANCE(Protocol, (ProtocolService::template Compose<PlatformImpl, IpStack>))
        
        // Helper function to get the pointer to the protocol.
        inline static Protocol * get (IpStack *stack)
        {
            return &stack->m_protocols.template get<ProtocolIndex>();
        }
    };
    using ProtocolHelpersList = APrinter::IndexElemList<ProtocolServicesList, ProtocolHelper>;
    
    static int const NumProtocols = APrinter::TypeListLength<ProtocolHelpersList>::Value;
    
    // Create a list of the instantiated protocols, for the tuple.
    template <typename Helper>
    using ProtocolForHelper = typename Helper::Protocol;
    using ProtocolsList = APrinter::MapTypeList<ProtocolHelpersList, APrinter::TemplateFunc<ProtocolForHelper>>;
    
    // Helper to extract IpProtocolNumber from a ProtocolHelper.
    APRINTER_DEFINE_MEMBER_TYPE(MemberTypeIpProtocolNumber, IpProtocolNumber)
    
    using ProtocolHandlerArgs = IpProtocolHandlerArgs<Platform, IpStack>;
    
public:
    /**
     * Number of bytes which must be available in outgoing datagrams for headers.
     * 
     * Buffers passed to send functions such as @ref sendIp4Dgram and
     * @ref sendIp4DgramFast must have at least this much space available in the
     * first buffer node before the data. This space is used by the IP stack to write
     * the IP header and by lower-level protocols such as Ethernet for their own
     * headers.
     */
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
     * Construct the IP stack.
     */
    IpStack (Platform platform) :
        m_reassembly(platform),
        m_path_mtu_cache(platform, this),
        m_next_id(0),
        m_protocols(APrinter::ResourceTupleInitSame(), ProtocolHandlerArgs{platform, this})
    {
    }
    
    /**
     * Destruct the IP stack.
     * 
     * There must be no remaining interfaces associated with this stack
     * when the IP stack is destructed.
     */
    ~IpStack ()
    {
        AMBRO_ASSERT(m_iface_list.isEmpty())
    }
    
    /**
     * Get the protocol instance type for a protocol number.
     * 
     * @tparam ProtocolNumber The IP procol number to get the type for.
     *         It must be the number of one of the configured procotols.
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
     * @tparam Protocol The protocol instance type. It must be the
     *         instance type of one of the configured protocols.
     * @return Pointer to protocol instance.
     */
    template <typename Protocol>
    inline Protocol * getProtocol ()
    {
        static int const ProtocolIndex =
            APrinter::TypeListIndex<ProtocolsList, Protocol>::Value;
        return &m_protocols.template get<ProtocolIndex>();
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
                    Iface *iface, IpSendRetry::Request *retryReq, IpSendFlags send_flags)
    {
        AMBRO_ASSERT(dgram.tot_len <= UINT16_MAX)
        AMBRO_ASSERT(dgram.offset >= Ip4Header::Size)
        AMBRO_ASSERT((send_flags & ~IpSendFlags::AllFlags) == EnumZero)
        
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
            if (AMBRO_UNLIKELY((send_flags & IpSendFlags::DontFragmentFlag) != EnumZero)) {
                return IpErr::FRAG_NEEDED;
            }
            
            // Calculate length of first fragment.
            pkt_send_len = Ip4RoundFragLen(Ip4Header::Size, route_info.iface->getMtu());
            
            // Set the MoreFragments IP flag (will be cleared for the last fragment).
            send_flags |= IpSendFlags(Ip4FlagMF);
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
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), (uint16_t)send_flags);
        ip4_header.set(Ip4Header::FlagsOffset(), (uint16_t)send_flags);
        
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
        if (AMBRO_LIKELY((send_flags & IpSendFlags(Ip4FlagMF)) == EnumZero)) {
            return route_info.iface->driverSendIp4Packet(pkt, route_info.addr, retryReq);
        }
        
        // Slow path...
        return send_fragmented(pkt, route_info, send_flags, retryReq);
    }
    
private:
    IpErr send_fragmented (IpBufRef pkt, Ip4RouteInfo route_info,
                           IpSendFlags send_flags, IpSendRetry::Request *retryReq)
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
                send_flags &= ~IpSendFlags(Ip4FlagMF);
            }
            
            auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
            
            // Write the fragment-specific IP header fields.
            ip4_header.set(Ip4Header::TotalLen(),     pkt_send_len);
            uint16_t flags_offset = (uint16_t)send_flags | (fragment_offset / 8);
            ip4_header.set(Ip4Header::FlagsOffset(),  flags_offset);
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
            if ((send_flags & IpSendFlags(Ip4FlagMF)) == EnumZero ||
                AMBRO_UNLIKELY(err != IpErr::SUCCESS))
            {
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
        /**
         * Routing information (may be read externally if found useful).
         */
        Ip4RouteInfo route_info;
        
        /**
         * Partially calculated IP header checksum (should not be used externally).
         */
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
                        char *header_end_ptr, IpSendFlags send_flags, Ip4SendPrepared &prep)
    {
        AMBRO_ASSERT((send_flags & ~IpSendFlags::AllFlags) == EnumZero)
        
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
        
        chksum.addWord(APrinter::WrapType<uint16_t>(), (uint16_t)send_flags);
        ip4_header.set(Ip4Header::FlagsOffset(), (uint16_t)send_flags);
        
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
    
    /**
     * Determine routing for the given destination address.
     * 
     * Determines the interface and next hop address for sending a packet to
     * the given address. The logic is:
     * - If there is any interface with an address configured for which the
     *   destination address belongs to the subnet of the interface, the
     *   resulting interface is the most recently added interface out of
     *   such interfaces with the longest prefix length, and the resulting
     *   hop address is the destination address.
     * - Otherwise, if any interface has a gateway configured, the resulting
     *   interface is the most recently added such interface, and the
     *   resulting hop address is the gateway address of that interface.
     * - Otherwise, the function fails (returns false).
     * 
     * @param dst_addr Destination address to determine routing for.
     * @param route_info Routing information will be written here.
     * @return True on success (route_info was filled in),
     *         false on error (route_info was not changed).
     */
    bool routeIp4 (Ip4Addr dst_addr, Ip4RouteInfo &route_info)
    {
        int best_prefix = -1;
        Iface *best_iface = nullptr;
        
        for (Iface *iface = m_iface_list.first(); iface != nullptr;
             iface = m_iface_list.next(*iface))
        {
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
    
    /**
     * Determine routing for the given destination address through
     * the given interface.
     * 
     * This is like @ref routeIp4 restricted to one interface with the exception
     * that it also accepts the all-ones broadcast address. The logic is:
     * - If the destination address is all-ones, or the interface has an address
     *   configured and the destination address belongs to the subnet of the interface,
     *   the resulting hop address is the destination address (and the resulting
     *   interface is as given).
     * - Otherwise, if the interface has a gateway configured, the resulting
     *   hop address is the gateway address of the interface (and the resulting
     *   interface is as given).
     * - Otherwise, the function fails (returns false).
     * 
     * @param dst_addr Destination address to determine routing for.
     * @param iface Interface which is to be used.
     * @param route_info Routing information will be written here.
     * @return True on success (route_info was filled in),
     *         false on error (route_info was not changed).
     */
    bool routeIp4ForceIface (Ip4Addr dst_addr, Iface *iface, Ip4RouteInfo &route_info)
    {
        AMBRO_ASSERT(iface != nullptr)
        
        if (dst_addr == Ip4Addr::AllOnesAddr() || iface->ip4AddrIsLocal(dst_addr)) {
            route_info.addr = dst_addr;
        }
        else if (iface->m_have_gateway) {
            route_info.addr = iface->m_gateway;
        }
        else {
            return false;
        }
        route_info.iface = iface;
        return true;
    }
    
    /**
     * Handle an ICMP Packet Too Big message.
     * 
     * This function checks the Path MTU estimate for an address and lowers it
     * to min(interface_mtu, max(MinMTU, mtu_info)) if it is greater than that.
     * However, nothing is done if there is no existing Path MTU estimate for
     * the address. Also if there is no route for the address then the min is
     * not done.
     * 
     * If the Path MTU estimate was lowered, then all existing @ref MtuRef setup
     * for this address are notified (@ref MtuRef::pmtuChanged are called),
     * directly from this function.
     * 
     * @param remote_addr Address to which the ICMP message applies.
     * @param mtu_info The next-hop-MTU from the ICMP message.
     * @return True if the Path MTU estimate was lowered, false if not.
     */
    inline bool handleIcmpPacketTooBig (Ip4Addr remote_addr, uint16_t mtu_info)
    {
        return m_path_mtu_cache.handlePacketTooBig(remote_addr, mtu_info);
    }
    
    /**
     * Ensure that a Path MTU estimate does not exceed the interface MTU.
     * 
     * This is like @ref handleIcmpPacketTooBig except that it only considers the
     * interface MTU. This should be called by a protocol handler when it is using
     * Path MTU Discovery and sending fails with the @ref IpErr::FRAG_NEEDED error.
     * The intent is to handle the case when the MTU of the interface through which
     * the address is routed has changed, because this is a local issue and would
     * not be detected via an ICMP message.
     * 
     * If the Path MTU estimate was lowered, then all existing @ref MtuRef setup
     * for this address are notified (@ref MtuRef::pmtuChanged are called),
     * directly from this function.
     * 
     * @param remote_addr Address for which to check the Path MTU estimate.
     * @return True if the Path MTU estimate was lowered, false if not.
     */
    inline bool handleLocalPacketTooBig (Ip4Addr remote_addr)
    {
        return m_path_mtu_cache.handlePacketTooBig(
            remote_addr, std::numeric_limits<uint16_t>::max());
    }
    
    /**
     * Check if the source address of a received datagram appears to be
     * a unicast address.
     * 
     * Specifically, it checks that the source address is not all-ones or a
     * multicast address (@ref Ip4Addr::isBroadcastOrMulticast) and that it
     * is not the local broadcast address of the interface from which the
     * datagram was received.
     * 
     * @param ip_info Information about the received datagram.
     * @return True if the source address appears to be a unicast address,
     *         false if not.
     */
    static bool checkUnicastSrcAddr (Ip4RxInfo const &ip_info)
    {
        return !ip_info.src_addr.isBroadcastOrMulticast() &&
               !ip_info.iface->ip4AddrIsLocalBcast(ip_info.src_addr);
    }
    
    /**
     * Allows receiving and intercepting IP datagrams received through a specific
     * interface with a specific IP protocol.
     * 
     * This is a low-level interface designed to be used by the DHCP client
     * implementation. It may be removed at some point if a proper UDP protocol
     * handle is implemented that is usable for DHCP.
     */
    class IfaceListener :
        private APrinter::NonCopyable<IfaceListener>
    {
        friend IpStack;
        
    public:
        /**
         * Initialize the listener object and start listening.
         * 
         * Received datagrams with matching protocol number will be passed to
         * the @ref recvIp4Dgram callback.
         * 
         * @param iface The interface to listen for packets on. It is the
         *        responsibility of the user to ensure that the interface is
         *        not removed while this object is still initialized.
         * @param proto IP protocol number that the user is interested on.
         */
        IfaceListener (Iface *iface, uint8_t proto) :
            m_iface(iface),
            m_proto(proto)
        {
            m_iface->m_listeners_list.prepend(*this);
        }
        
        /**
         * Deinitialize the listener.
         * 
         * After this, @ref recvIp4Dgram will not be called.
         */
        ~IfaceListener ()
        {
            m_iface->m_listeners_list.remove(*this);
        }
        
        /**
         * Return the interface on which this object is listening.
         * 
         * @return Interface on which this object is listening.
         */
        inline Iface * getIface ()
        {
            return m_iface;
        }
        
    private:
        /**
         * Called when a matching datagram is received.
         * 
         * This is called before passing the datagram to any protocol handler
         * The return value allows inhibiting further processing of the datagram
         * (by other IfaceListener's, protocol handlers and built-in protocols
         * such as ICMP).
         * 
         * WARNING: It is not allowed to deinitialize this listener object from
         * this callback or to remove the interface through which the packet has
         * been received.
         * 
         * @param ip_info Information about the received datagram.
         * @param dgram Data of the received datagram.
         * @return True to inhibit further processing, false to continue.
         */
        virtual bool recvIp4Dgram (Ip4RxInfo const &ip_info, IpBufRef dgram) = 0;
        
    private:
        APrinter::LinkedListNode<IfaceListenerLinkModel> m_list_node;
        Iface *m_iface;
        uint8_t m_proto;
    };
    
    /**
     * Allows observing changes in the driver-reported state of an interface.
     * 
     * The driver-reported state can be queried using by @ref Iface::getDriverState.
     * This class can be used to receive a callback whenever the driver-reported
     * state may have changed.
     */
    class IfaceStateObserver :
        public APrinter::Observer<IfaceStateObserver>
    {
        friend IpStack;
        friend APrinter::Observable<IfaceStateObserver>;
        
    public:
        /**
         * Start observing an interface, making the observer active.
         * 
         * The observer must be inactive when this is called.
         * 
         * @param iface Interface to observe.
         */
        inline void observe (Iface &iface)
        {
            iface.m_state_observable.addObserver(*this);
        }
        
    protected:
        /**
         * Called when the driver-reported state of the interface may have changed.
         * 
         * It is not guaranteed that the state has actually changed, nor is it
         * guaranteed that the callback will be called immediately for every state
         * change (there may be just one callback for successive state changes).
         * 
         * WARNING: The callback must not do any potentially harmful actions such
         * as removing the interface. Removing this or other listeners and adding
         * other listeners is safe. Sending packets should be safe assuming this
         * is safe in the driver.
         */
        virtual void ifaceStateChanged () = 0;
    };
    
private:
    using IfaceListenerList = APrinter::LinkedList<
        APRINTER_MEMBER_ACCESSOR_TN(&IfaceListener::m_list_node), IfaceListenerLinkModel, false>;
    
public:
    /**
     * Encapsulates interface information passed to the @ref Iface constructor.
     */
    struct IfaceInitInfo {
        /**
         * The Maximum Transmission Unit (MTU), including the IP header.
         * 
         * It must be at least @ref MinMTU (this is an assert).
         */
        size_t ip_mtu = 0;
        
        /**
         * The type of the hardware-type-specific interface.
         * 
         * See @ref Iface::getHwType for an explanation of the hardware-type-specific
         * interface mechanism. If no hardware-type-specific interface is available,
         * use @ref IpHwType::Undefined.
         */
        IpHwType hw_type = IpHwType::Undefined;
        
        /**
         * Pointer to the hardware-type-specific interface.
         * 
         * If @ref hw_type is @ref IpHwType::Undefined, use null. Otherwise this must
         * point to an instance of the hardware-type-specific interface class
         * corresponding to @ref hw_type.
         */
        void *hw_iface = nullptr;
    };
    
    /**
     * A network interface.
     * 
     * This class is generally designed to be inherited and owned by the IP driver.
     * Virtual functions are used by the IP stack to request actions or information
     * from the driver (such as @ref driverSendIp4Packet to send a packet), while
     * protected non-virtual functions are to be used by the driver to request
     * service from the IP stack (such as @ref recvIp4PacketFromDriver to process
     * received packets).
     * 
     * The IP stack does not provide or impose any model for management of interfaces
     * and interface drivers. Such a system could be build on top if it is needed.
     */
    class Iface :
        private APrinter::NonCopyable<Iface>
    {
        friend IpStack;
        
    public:
        /**
         * The @ref IpStack type that this class is associated with.
         */
        using IfaceIpStack = IpStack;
        
        /**
         * Initialize the interface.
         * 
         * This should be used by the driver when the interface should start existing
         * from the perspective of the IP stack. After this, the various virtual
         * functions may be called.
         * 
         * @param stack Pointer to the IP stack.
         * @param info Interface information, see @ref IfaceInitInfo.
         */
        Iface (IpStack *stack, IfaceInitInfo const &info) :
            m_stack(stack),
            m_hw_iface(info.hw_iface),
            m_ip_mtu(APrinter::MinValueU((uint16_t)UINT16_MAX, info.ip_mtu)),
            m_hw_type(info.hw_type),
            m_have_addr(false),
            m_have_gateway(false)
        {
            AMBRO_ASSERT(stack != nullptr)
            AMBRO_ASSERT(m_ip_mtu >= MinMTU)
            
            // Initialize stuffs.
            m_listeners_list.init();
            
            // Register interface.
            m_stack->m_iface_list.prepend(*this);
        }
        
        /**
         * Deinitialize the interface.
         * 
         * This should be used by the driver when the interface should stop existing
         * from the perspective of the IP stack. After this, virtual functions will
         * not be called any more, nor will any virtual function be called from this
         * function.
         * 
         * When this is called, there must be no remaining @ref IfaceListener
         * objects listening on this interface or @ref IfaceStateObserver objects
         * observing this interface. Additionally, this must not be called in
         * potentially hazardous context with respect to IP processing, such as
         * from withing receive processing of this interface
         * (@ref recvIp4PacketFromDriver). For maximum safety this should be called
         * from a top-level event handler.
         */
        ~Iface ()
        {
            AMBRO_ASSERT(m_listeners_list.isEmpty())
            
            // Unregister interface.
            m_stack->m_iface_list.remove(*this);
        }
        
        /**
         * Set or remove the IP address and subnet prefix length.
         * 
         * @param value New IP address settings. If the "present" field is false
         *        then any existing assignment is removed. If the "present" field
         *        is true then the IP address in the "addr" field is assigned along
         *        with the subnet prefix length in the "prefix" field, overriding
         *        any existing assignment.
         */
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
        
        /**
         * Get the current IP address settings.
         * 
         * @return Current IP address settings. If the "present" field is false
         *         then no IP address is assigned, and the "addr" and "prefix" fields
         *         will be zero. If the "present" field is true then the "addr" and
         *         "prefix" fields contain the assigned address and subnet prefix
         *         length.
         */
        IpIfaceIp4AddrSetting getIp4Addr ()
        {
            IpIfaceIp4AddrSetting value = {m_have_addr};
            if (m_have_addr) {
                value.prefix = m_addr.prefix;
                value.addr = m_addr.addr;
            }
            return value;
        }
        
        /**
         * Set or remove the gateway address.
         * 
         * @param value New gateway address settings. If the "present" field is false
         *        then any existing gateway address is removed. If the "present" field
         *        is true then gateway address in the "addr" field is assigned,
         *        overriding any existing assignment.
         */
        void setIp4Gateway (IpIfaceIp4GatewaySetting value)
        {
            m_have_gateway = value.present;
            if (value.present) {
                m_gateway = value.addr;
            }
        }
        
        /**
         * Get the current gateway address settings.
         * 
         * @return Current gateway address settings. If the "present" field is false
         *         then no gateway address is assigned, and the "addr" field will be
         *         zero. If the "present" field is true then the "addr" field contains
         *         the assigned gateway address.
         */
        IpIfaceIp4GatewaySetting getIp4Gateway ()
        {
            IpIfaceIp4GatewaySetting value = {m_have_gateway};
            if (m_have_gateway) {
                value.addr = m_gateway;
            }
            return value;
        }
        
        /**
         * Get the type of the hardware-type-specific interface.
         * 
         * The can be used to check which kind of hardware-type-specific interface
         * is available via @ref getHwIface (if any). For example, if the result is
         * @ref IpHwType::Ethernet, then an interface of type @ref IpEthHw::HwIface
         * is available.
         * 
         * This function will return whatever was passed as @ref IfaceInitInfo::hw_type
         * when the interface was constructed.
         * 
         * This mechanism was created to support the DHCP client which requires
         * access to certain Ethernet/ARP-level functionality.
         * 
         * @return Type of hardware-type-specific interface.
         */
        inline IpHwType getHwType ()
        {
            return m_hw_type;
        }
        
        /**
         * Get the hardware-type-specific interface.
         * 
         * The HwIface type must correspond to the value returned by @ref getHwType.
         * If that value is @ref IpHwType::Undefined, then this function should not
         * be called at all.
         * 
         * This function will return whatever was passed as @ref IfaceInitInfo::hw_iface
         * when the interface was constructed.
         * 
         * @tparam HwIface Type of hardware-type-specific interface.
         * @return Pointer to hardware-type-specific interface.
         */
        template <typename HwIface>
        inline HwIface * getHwIface ()
        {
            return static_cast<HwIface *>(m_hw_iface);
        }
        
    public:
        /**
         * Check if an address belongs to the subnet of the interface.
         * 
         * @param addr Address to check.
         * @return True if the interface has an IP address assigned and the
         *         given address belongs to the associated subnet, false otherwise.
         */
        inline bool ip4AddrIsLocal (Ip4Addr addr)
        {
            return m_have_addr && (addr & m_addr.netmask) == m_addr.netaddr;
        }
        
        /**
         * Check if an address is the local broadcast address of the interface.
         * 
         * @param addr Address to check.
         * @return True if the interface has an IP address assigned and the
         *         given address is the associated local broadcast address,
         *         false otherwise.
         */
        inline bool ip4AddrIsLocalBcast (Ip4Addr addr)
        {
            return m_have_addr && addr == m_addr.bcastaddr;
        }
        
        /**
         * Check if an address is the address of the interface.
         * 
         * @param addr Address to check.
         * @return True if the interface has an IP address assigned and the
         *         assigned address is the given address, false otherwise.
         */
        inline bool ip4AddrIsLocalAddr (Ip4Addr addr)
        {
            return m_have_addr && addr == m_addr.addr;
        }
        
        /**
         * Return the IP level Maximum Transmission Unit of the interface.
         * 
         * @return MTU in bytes including the IP header. It will be at least
         *         @ref MinMTU.
         */
        inline uint16_t getMtu ()
        {
            return m_ip_mtu;
        }
        
        /**
         * Return the driver-provided interface state.
         * 
         * This directly queries the driver for the current state by calling the
         * virtual function @ref driverGetState. Use @ref IfaceStateObserver if
         * you need to be notified of changes of this state.
         * 
         * Currently, the driver-provided state indicates whether the link is up.
         * 
         * @return Driver-provided state.
         */
        inline IpIfaceDriverState getDriverState ()
        {
            return driverGetState();
        }
        
    protected:
        /**
         * Driver function used to send IPv4 packets through the interface.
         * 
         * This is called whenever an IPv4 packet needs to be sent. The driver should
         * copy the packet as needed because it must not access the referenced buffers
         * outside this function.
         * 
         * @param pkt Packet to send, this includes the IP header. It is guaranteed
         *        that its size does not exceed the MTU reported by the driver. The
         *        packet is expected to have HeaderBeforeIp bytes available before
         *        the IP header for link-layer protocol headers, but needed header
         *        space should still be checked since higher-layer prococols are
         *        responsible for allocating the buffers of packets they send.
         * @param ip_addr Next hop address.
         * @param sendRetryReq If sending fails and this is not null, the driver
         *        may use this to notify the requestor when sending should be retried.
         *        For example if the issue was that there is no ARP cache entry
         *        or similar entry for the given address, the notification should
         *        be done when the associated ARP query is successful.
         * @return Success or error code.
         */
        virtual IpErr driverSendIp4Packet (IpBufRef pkt, Ip4Addr ip_addr,
                                           IpSendRetry::Request *sendRetryReq) = 0;
        
        /**
         * Driver function to get the driver-provided interface state.
         * 
         * The driver should call @ref stateChangedFromDriver whenever the state
         * that would be returned here has changed.
         * 
         * @return Driver-provided-state (currently just the link-up flag).
         */
        virtual IpIfaceDriverState driverGetState () = 0;
        
        /**
         * Process a received IPv4 packet.
         * 
         * This function should be called by the driver when an IPv4 packet is
         * received (or what appears to be one at least).
         * 
         * The driver must support various driver functions being called from
         * within this, especially @ref driverSendIp4Packet.
         * 
         * @param pkt Received packet, presumably starting with the IP header.
         *            The referenced buffers will only be read from within this
         *            function call.
         */
        inline void recvIp4PacketFromDriver (IpBufRef pkt)
        {
            processRecvedIp4Packet(this, pkt);
        }
        
        /**
         * Return information about current IPv4 address assignment.
         * 
         * This can be used by the driver if it needs information about the
         * IPv4 address assigned to the interface, or other places where the
         * information is useful.
         * 
         * @return If no IPv4 address is assigned, then null. If an address is
         *         assigned, then a pointer to a structure providing information
         *         about the assignment (assigned address, network mask, network
         *         address, broadcast address, subnet prefix length). The pointer
         *         is only valid temporarily (it should not be cached).
         */
        inline IpIfaceIp4Addrs const * getIp4AddrsFromDriver ()
        {
            return m_have_addr ? &m_addr : nullptr;
        }
        
        /**
         * Notify that the driver-provided state may have changed.
         * 
         * This should be called by the driver after the values that would be
         * returned by  driverGetState have changed. It does not strictly have
         * to be called immediately after every change but it should be called
         * soon after a change.
         * 
         * The driver must support various driver functions being called from
         * within this, especially @ref driverSendIp4Packet.
         */
        void stateChangedFromDriver ()
        {
            m_state_observable.notifyKeepObservers([&](IfaceStateObserver &observer) {
                observer.ifaceStateChanged();
            });
        }
        
    private:
        APrinter::LinkedListNode<IfaceLinkModel> m_iface_list_node;
        IfaceListenerList m_listeners_list;
        APrinter::Observable<IfaceStateObserver> m_state_observable;
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
    /**
     * Allows keeping track of the Path MTU estimate for a remote address.
     */
    class MtuRef : private BaseMtuRef
    {
    public:
        /**
         * Initialize the MTU reference.
         * 
         * The object is initialized in not-setup state, that is without an
         * associated remote address. To set the remote address, call @ref setup.
         * This function must be called before any other function in this
         * class is called.
         */
        inline void init ()
        {
            return BaseMtuRef::init();
        }
        
        /**
         * Reset the MTU reference.
         * 
         * This resets the object to the not-setup state.
         *
         * NOTE: It is required to reset the object to not-setup state
         * before destructing it, if not already in not-setup state.
         * 
         * @param stack The IP stack.
         */
        inline void reset (IpStack *stack)
        {
            return BaseMtuRef::reset(mtu_cache(stack));
        }
        
        /**
         * Check if the MTU reference is in setup state.
         * 
         * @return True if in setup state, false if in not-setup state.
         */
        inline bool isSetup ()
        {
            return BaseMtuRef::isSetup();
        }
        
        /**
         * Setup the MTU reference for a specific remote address.
         * 
         * The object must be in not-setup state when this is called.
         * On success, the current PMTU estimate is provided and future PMTU
         * estimate changes will be reported via the @ref pmtuChanged callback.
         * 
         * WARNING: Do not destruct the object while it is in setup state.
         * First use @ref reset (or @ref moveFrom) to change the object to
         * not-setup state before destruction.
         * 
         * @param stack The IP stack.
         * @param remote_addr The remote address to observe the PMTU for.
         * @param iface NULL or the interface though which remote_addr would be
         *        routed, as an optimization.
         * @param out_pmtu On success, will be set to the current PMTU estimate
         *        (guaranteed to be at least MinMTU). On failure it will not be
         *        changed.
         * @return True on success (object enters setup state), false on failure
         *         (object remains in not-setup state).
         */
        inline bool setup (IpStack *stack, Ip4Addr remote_addr, Iface *iface, uint16_t &out_pmtu)
        {
            return BaseMtuRef::setup(mtu_cache(stack), remote_addr, iface, out_pmtu);
        }

        /**
         * Move an MTU reference from another object to this one.
         * 
         * This object must be in not-setup state. Upon return, the 'src' object
         * will be in not-setup state and this object will be in whatever state
         * the 'src' object was. If the 'src' object was in setup state, this object
         * will be setup with the same remote address.
         * 
         * @param src The object to move from.
         */
        inline void moveFrom (MtuRef &src)
        {
            return BaseMtuRef::moveFrom(src);
        }
        
    protected:
        /**
         * Callback which reports changes of the PMTU estimate.
         * 
         * This is called whenever the PMTU estimate changes,
         * and only in setup state.
         * 
         * WARNING: Do not change this object in any way from this callback,
         * specifically do not call @ref reset or @ref moveFrom. Note that the
         * implementation calls all these callbacks for the same remote address
         * in a loop, and that the callbacks may be called from within
         * @ref handleIcmpPacketTooBig and @ref handleLocalPacketTooBig.
         * 
         * @param pmtu The new PMTU estimate (guaranteed to be at least MinMTU).
         */
        virtual void pmtuChanged (uint16_t pmtu) = 0;
        
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
                Helper::get(ip_info.iface->m_stack)->recvIp4Dgram(
                    static_cast<Ip4RxInfo const &>(ip_info),
                    static_cast<IpBufRef>(dgram));
                return false;
            }
            return true;
        }));
        
        // If the packet was handled by a protocol handler, we are done.
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
        // Accept only: all-ones broadcast, subnet broadcast, interface address.
        bool is_broadcast_dst;
        if (AMBRO_LIKELY(ip_info.iface->ip4AddrIsLocalAddr(ip_info.dst_addr))) {
            is_broadcast_dst = false;
        } else {
            if (AMBRO_UNLIKELY(
                !ip_info.iface->ip4AddrIsLocalBcast(ip_info.dst_addr) &&
                ip_info.dst_addr != Ip4Addr::AllOnesAddr()))
            {
                return;
            }
            is_broadcast_dst = true;
        }
        
        // Check ICMP header length.
        if (AMBRO_UNLIKELY(!dgram.hasHeader(Icmp4Header::Size))) {
            return;
        }
        
        // Read ICMP header fields.
        auto icmp4_header = Icmp4Header::MakeRef(dgram.getChunkPtr());
        uint8_t type       = icmp4_header.get(Icmp4Header::Type());
        uint8_t code       = icmp4_header.get(Icmp4Header::Code());
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
            // But if this is a broadcast request, respond only if allowed.
            if (is_broadcast_dst && !AllowBroadcastPing) {
                return;
            }
            stack->sendIcmp4EchoReply(rest, icmp_data, ip_info.src_addr, ip_info.iface);
        }
        else if (type == Icmp4TypeDestUnreach) {
            stack->handleIcmp4DestUnreach(code, rest, icmp_data, ip_info.iface);
        }
    }
    
    void sendIcmp4EchoReply (Icmp4RestType rest, IpBufRef data, Ip4Addr dst_addr, Iface *iface)
    {
        // Can only reply when we have an address assigned.
        if (AMBRO_UNLIKELY(!iface->m_have_addr)) {
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
        Ip4Addrs addrs = {iface->m_addr.addr, dst_addr};
        sendIp4Dgram(addrs, {IcmpTTL, Ip4ProtocolIcmp}, dgram, iface, nullptr,
                     IpSendFlags());
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
                Helper::get(this)->handleIp4DestUnreach(
                    static_cast<Ip4DestUnreachMeta const &>(du_meta),
                    static_cast<Ip4RxInfo const &>(ip_info),
                    static_cast<IpBufRef>(dgram_initial));
                return false;
            }
            return true;
        }));
    }
    
private:
    Reassembly m_reassembly;
    PathMtuCache m_path_mtu_cache;
    APrinter::StructureRaiiWrapper<IfaceList> m_iface_list;
    uint16_t m_next_id;
    APrinter::InstantiateVariadic<APrinter::ResourceTuple, ProtocolsList> m_protocols;
};

/**
 * Service configuration class for @ref IpStack.
 * 
 * The template parameters of this class are "configuration". After these are
 * defined, use @ref APRINTER_MAKE_INSTANCE with @ref Compose to obtain the
 * @ref IpStack class type.
 * 
 * @tparam Param_HeaderBeforeIp Required space for headers before the IP header
 *         in outgoing packets. This should be the maximum of the required space
 *         of any IP interface driver that may be used.
 * @tparam Param_IcmpTTL TTL of outgoing ICMP packets.
 * @tparam Param_AllowBroadcastPing Whether to respond to broadcast pings
 *         (to local-broadcast or all-ones address).
 * @tparam Param_PathMtuParams Path MTU Discovery parameters,
 *         see @ref IpPathMtuParams.
 * @tparam Param_ReassemblyService Implementation/configuration of IP reassembly.
 *         This should be @ref IpReassemblyService instantiated with the desired
 *         template parameters (reassembly configuration).
 */
APRINTER_ALIAS_STRUCT_EXT(IpStackService, (
    APRINTER_AS_VALUE(size_t, HeaderBeforeIp),
    APRINTER_AS_VALUE(uint8_t, IcmpTTL),
    APRINTER_AS_VALUE(bool, AllowBroadcastPing),
    APRINTER_AS_TYPE(PathMtuParams),
    APRINTER_AS_TYPE(ReassemblyService)
), (
    /**
     * Template for use with @ref APRINTER_MAKE_INSTANCE to get the @ref IpStack type.
     * 
     * The template parameters of this class are "dependencies".
     * 
     * @tparam Param_Context Context class providing the event loop, clock etc..
     * @tparam Param_ProtocolServicesList List of IP protocol handler services.
     *         For example, to support only TCP, use
     *         APrinter::MakeTypeList\<IpTcpProtoService\<...\>\> with appropriate
     *         parameters passed to IpTcpProtoService.
     */
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(PlatformImpl),
        APRINTER_AS_TYPE(ProtocolServicesList)
    ), (
        using Params = IpStackService;
        APRINTER_DEF_INSTANCE(Compose, IpStack)
    ))
))

}

#endif
