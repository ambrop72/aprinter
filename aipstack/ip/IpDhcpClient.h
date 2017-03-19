/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef APRINTER_IPSTACK_IP_DHCP_CLIENT_H
#define APRINTER_IPSTACK_IP_DHCP_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <limits>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/EnumUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/system/TimedEventWrapper.h>

#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Chksum.h>
#include <aipstack/misc/Allocator.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Udp4Proto.h>
#include <aipstack/proto/DhcpProto.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/ip/hw/IpHwCommon.h>
#include <aipstack/ip/hw/IpEthHw.h>

#include <aipstack/ip/IpDhcpClient_options.h>

#include <aipstack/BeginNamespace.h>

// Interface for status callbacks from the DHCP client.
class IpDhcpClientCallback {
public:
    enum class LeaseEventType {LeaseObtained, LeaseRenewed, LeaseLost};
    
    virtual void dhcpLeaseEvent (LeaseEventType event_type) = 0;
};

template <typename Arg>
class IpDhcpClient;

template <typename Arg>
APRINTER_DECL_TIMERS_CLASS(IpDhcpClientTimers, typename Arg::Context, IpDhcpClient<Arg>, (DhcpTimer))

template <typename Arg>
class IpDhcpClient :
    private Arg::IpStack::IfaceListener,
    private IpDhcpClientTimers<Arg>::Timers
{
    APRINTER_USE_TYPES1(Arg, (Context, IpStack, BufAllocator))
    APRINTER_USE_VALS(Arg::Params, (DhcpTTL, MaxDnsServers))
    APRINTER_USE_TYPES1(Context, (Clock))
    APRINTER_USE_TYPES1(Clock, (TimeType))
    APRINTER_USE_TYPES1(IpStack, (Ip4DgramMeta, Iface, IfaceListener))
    APRINTER_USE_VALS(IpStack, (HeaderBeforeIp4Dgram))
    APRINTER_USE_TYPES2(IpDhcpClientCallback, (LeaseEventType))
    APRINTER_USE_ONEOF
    using TheClockUtils = APrinter::ClockUtils<Context>;
    
    APRINTER_USE_TIMERS_CLASS(IpDhcpClientTimers<Arg>, (DhcpTimer)) 
    
    using Options = IpDhcpClient_options<MaxDnsServers>;
    APRINTER_USE_TYPES1(Options, (DhcpRecvOptions, DhcpSendOptions))
    
    static_assert(MaxDnsServers > 0, "");
    
    // DHCP client states
    enum class DhcpState {
        // Resetting due to NAK after some time
        Resetting,
        // Send discover, waiting for offer
        SentDiscover,
        // Sent request after offer, waiting for ack
        SentRequest,
        // We have a lease, not trying to renew yet
        Finished,
        // We have a lease and we're trying to renew it
        Renewing,
    };
    
    // Maximum number of discovery attempts before generating a new XID.
    static uint8_t const XidReuseMax = 8;
    
    // Maximum number of requests to send after an offer before going back to discovery.
    static uint8_t const MaxRequests = 4;
    
    // Period for sending renew requests.
    static uint32_t const RenewRequestTimeoutSeconds = 20;
    static TimeType const RenewRequestTimeoutTicks = RenewRequestTimeoutSeconds * Clock::time_freq;
    
    // Time after no offer to retransit a discover.
    // Also time after a NAK to transmit a discover.
    static TimeType const ResetTimeoutTicks   = 4.0 * Clock::time_freq;
    
    // Time after no ack to retransmit a request.
    static TimeType const RequestTimeoutTicks = 3.0 * Clock::time_freq;
    
    // Maximum future time in seconds that the timer can be set to,
    // due to limited span of TimeType. For possibly longer periods
    // (start of renewal, lease timeout), multiple timer expirations
    // are used with keeping track of leftover seconds.
    static uint32_t const MaxTimerSeconds = TheClockUtils::WorkingTimeSpanTicks / (TimeType)Clock::time_freq;
    
    static_assert(RenewRequestTimeoutSeconds <= MaxTimerSeconds, "");
    
    // Determines the time after which we start trying to renew a lease.
    static constexpr uint32_t RenewTimeForLeaseTime (uint32_t lease_time)
    {
        return lease_time / 2;
    }
    
    // Combined size of UDP and TCP headers.
    static size_t const UdpDhcpHeaderSize = Udp4Header::Size + DhcpHeader::Size;
    
    // Maximum packet size that we could possibly transmit.
    static size_t const MaxDhcpSendMsgSize = UdpDhcpHeaderSize + Options::MaxOptionsSendSize;
    
public:
    // Contains all the information about a lease.
    struct LeaseInfo {
        // These two are set already when the offer is received.
        Ip4Addr ip_address;
        uint32_t dhcp_server_identifier;
        
        // The rest are set when the ack is received.
        uint32_t lease_time_s;
        Ip4Addr subnet_mask;
        MacAddr server_mac;
        bool have_router;
        uint8_t domain_name_servers_count;
        Ip4Addr router;
        Ip4Addr domain_name_servers[MaxDnsServers];
    };
    
private:
    IpStack *m_ipstack;
    IpDhcpClientCallback *m_callback;
    uint32_t m_xid;
    DhcpState m_state;
    uint8_t m_xid_reuse_count;
    uint8_t m_request_count;
    uint32_t m_time_left;
    LeaseInfo m_info;
    
public:
    void init (IpStack *stack, Iface *iface, IpDhcpClientCallback *callback)
    {
        // We only support Ethernet interfaces.
        AMBRO_ASSERT(iface->getHwType() == IpHwType::Ethernet)
        
        // Remember stuff.
        m_ipstack = stack;
        m_callback = callback;
        
        // Init resources.
        IfaceListener::init(iface, Ip4ProtocolUdp);
        tim(DhcpTimer()).init(Context());
        
        // Start discovery.
        start_discovery(true);
    }
    
    void deinit ()
    {
        // Remove any configuration that might have been done (no callback).
        handle_dhcp_down(false);
        
        // Deinit resources.
        tim(DhcpTimer()).deinit(Context());
        IfaceListener::deinit();
    }
    
    inline bool hasLease ()
    {
        return m_state == OneOf(DhcpState::Finished, DhcpState::Renewing);
    }
    
    inline LeaseInfo const & getLeaseInfoMustHaveLease ()
    {
        AMBRO_ASSERT(hasLease())
        
        return m_info;
    }
    
private:
    // Return the interface (stored by IfaceListener).
    inline Iface * iface ()
    {
        return IfaceListener::getIface();
    }
    
    // Return the IpEthHw interface for the interface.
    inline IpEthHw * ethHw ()
    {
        return iface()->template getHwIface<IpEthHw>();
    }
    
    // This is used to achieve longer effective timeouts than
    // can be achieved with the timers/clock directly.
    // To start a long timeout, m_time_left is first set to the
    // desired number of seconds, then this is called to set the
    // timer. The timer handler checks m_time_left; if it is zero
    // the desired time is expired, otherwise this is called
    // again to set the timer again.
    void set_timer_for_time_left (uint32_t max_timer_seconds)
    {
        AMBRO_ASSERT(max_timer_seconds <= MaxTimerSeconds)
        
        uint32_t seconds = APrinter::MinValue(m_time_left, max_timer_seconds);
        m_time_left -= seconds;
        tim(DhcpTimer()).appendAfter(Context(), seconds * (TimeType)Clock::time_freq);
    }
    
    // Starts discovery.
    void start_discovery (bool force_new_xid)
    {
        // Generate a new XID if forced or the reuse count was reached.
        if (force_new_xid || m_xid_reuse_count >= XidReuseMax) {
            m_xid = Clock::getTime(Context());
            m_xid_reuse_count = 0;
        }
        
        // Update the XID reuse count.
        m_xid_reuse_count++;
        
        // Send discover.
        DhcpSendOptions send_opts;
        send_dhcp_message(DhcpMessageType::Discover, m_xid, send_opts);
        
        // Set the timer to restart discovery if there is no offer.
        tim(DhcpTimer()).appendAfter(Context(), ResetTimeoutTicks);
        
        // Going to SentDiscover state.
        m_state = DhcpState::SentDiscover;
    }
    
    void timerExpired (DhcpTimer, Context)
    {
        switch (m_state) {
            // Timer is set for restarting discovery.
            case DhcpState::Resetting:
            case DhcpState::SentDiscover: {
                // Perform discovery.
                // If this is after a NAK then force a new XID.
                bool force_new_xid = m_state == DhcpState::Resetting;
                start_discovery(force_new_xid);
            } break;
            
            // Timer is set for retransmitting request.
            case DhcpState::SentRequest: {
                AMBRO_ASSERT(m_request_count >= 1)
                AMBRO_ASSERT(m_request_count <= MaxRequests)
                
                // If se sent enough requests, start discovery again.
                if (m_request_count >= MaxRequests) {
                    start_discovery(false);
                    return;
                }
                
                // Send request with server identifier.
                send_request(true);
                
                // Restart timer.
                tim(DhcpTimer()).appendAfter(Context(), RequestTimeoutTicks);
                
                // Increment request count.
                m_request_count++;
            } break;
            
            // Timer is set for starting renewal.
            case DhcpState::Finished: {
                // If we have more time left, restart timer.
                if (m_time_left > 0) {
                    set_timer_for_time_left(MaxTimerSeconds);
                    return;
                }
                
                // Set m_time_left to the time until the lease expires.
                m_time_left = m_info.lease_time_s - RenewTimeForLeaseTime(m_info.lease_time_s);
                
                // Send request without server identifier.
                send_request(false);
                
                // Start timer for sending request or lease timeout.
                set_timer_for_time_left(RenewRequestTimeoutSeconds);
                
                // Go to state Renewing.
                m_state = DhcpState::Renewing;
            } break;
            
            // Timer is set for sending another reqnewal request.
            case DhcpState::Renewing: {
                // Has the lease expired?
                if (m_time_left == 0) {
                    // Start discovery, remove IP configuration.
                    start_discovery(true);
                    handle_dhcp_down(true);
                    return;
                }
                
                // Send request without server identifier.
                send_request(false);
                
                // Restart timer for sending request or lease timeout.
                set_timer_for_time_left(RenewRequestTimeoutSeconds);
            } break;
            
            default:
                AMBRO_ASSERT(false);
        }
    }
    
    bool recvIp4Dgram (Ip4DgramMeta const &ip_meta, IpBufRef dgram) override final
    {
        {
            // Check that there is a UDP header.
            if (AMBRO_UNLIKELY(!dgram.hasHeader(Udp4Header::Size))) {
                goto reject;
            }
            
            auto udp_header = Udp4Header::MakeRef(dgram.getChunkPtr());
            
            // Check for expected source and destination port.
            if (AMBRO_LIKELY(udp_header.get(Udp4Header::SrcPort()) != DhcpServerPort)) {
                goto reject;
            }
            if (AMBRO_LIKELY(udp_header.get(Udp4Header::DstPort()) != DhcpClientPort)) {
                goto reject;
            }
            
            // Check UDP length.
            uint16_t udp_length = udp_header.get(Udp4Header::Length());
            if (AMBRO_UNLIKELY(udp_length < Udp4Header::Size || udp_length > dgram.tot_len)) {
                goto accept;
            }
            
            // Truncate data to UDP length.
            dgram = dgram.subTo(udp_length);
            
            // Check UDP checksum if provided.
            uint16_t checksum = udp_header.get(Udp4Header::Checksum());
            if (checksum != 0) {
                IpChksumAccumulator chksum_accum;
                chksum_accum.addWords(&ip_meta.src_addr.data);
                chksum_accum.addWords(&ip_meta.dst_addr.data);
                chksum_accum.addWord(APrinter::WrapType<uint16_t>(), Ip4ProtocolUdp);
                chksum_accum.addWord(APrinter::WrapType<uint16_t>(), udp_length);
                chksum_accum.addIpBuf(dgram);
                if (chksum_accum.getChksum() != 0) {
                    goto accept;
                }
            }
            
            // Process the DHCP payload.
            IpBufRef dhcp_msg = dgram.hideHeader(Udp4Header::Size);
            processReceivedDhcpMessage(dhcp_msg);
        }
        
    accept:
        // Inhibit further processing of packet.
        return true;
        
    reject:
        // Continue processing of packet.
        return false;
    }
    
    void processReceivedDhcpMessage (IpBufRef msg)
    {
        // In Resetting state we're not interested in any messages.
        if (m_state == DhcpState::Resetting) {
            return;
        }
        
        // Check that there is a DHCP header.
        if (msg.tot_len < DhcpHeader::Size) {
            return;
        }
        
        // Copy the DHCP header to contiguous memory (this moves msg forward).
        DhcpHeader::Val dhcp_header;
        msg.takeBytes(DhcpHeader::Size, dhcp_header.data);
        
        // Simple checks before further processing.
        bool sane =
            dhcp_header.get(DhcpHeader::DhcpOp())    == DhcpOp::BootReply &&
            dhcp_header.get(DhcpHeader::DhcpHtype()) == DhcpHwAddrType::Ethernet &&
            dhcp_header.get(DhcpHeader::DhcpHlen())  == MacAddr::Size &&
            dhcp_header.get(DhcpHeader::DhcpXid())   == m_xid &&
            MacAddr::decode(dhcp_header.ref(DhcpHeader::DhcpChaddr())) == ethHw()->getMacAddr() &&
            dhcp_header.get(DhcpHeader::DhcpMagic()) == DhcpMagicNumber;
        if (!sane) {
            return;
        }
        
        // Parse DHCP options.
        DhcpRecvOptions opts;
        if (!Options::parseOptions(msg, opts)) {
            return;
        }
        
        // Sanity check DHCP message type.
        if (!opts.have.dhcp_message_type || opts.dhcp_message_type !=
            OneOf(DhcpMessageType::Offer, DhcpMessageType::Ack, DhcpMessageType::Nak))
        {
            return;
        }
        
        // Check that there is a DHCP server identifier.
        if (!opts.have.dhcp_server_identifier) {
            return;
        }
        
        // Handle NAK message.
        if (opts.dhcp_message_type == DhcpMessageType::Nak) {
            // In Resetting and SentDiscover we do not care about NAK.
            if (m_state == OneOf(DhcpState::Resetting, DhcpState::SentDiscover)) {
                return;
            }
            
            // Ignore NAK if the DHCP server identifier does not match.
            if (opts.dhcp_server_identifier != m_info.dhcp_server_identifier) {
                return;
            }
            
            DhcpState prev_state = m_state;
            
            // Set timeout to start discovery.
            tim(DhcpTimer()).appendAfter(Context(), ResetTimeoutTicks);
            
            // Going to Resetting state.
            m_state = DhcpState::Resetting;
            
            // If we had a lease, remove it.
            if (prev_state == OneOf(DhcpState::Finished, DhcpState::Renewing)) {
                handle_dhcp_down(true);
            }
            
            // Nothing else to do (further processing is for offer and ack).
            return;
        }
        
        // Get Your IP Address.
        Ip4Addr ip_address = dhcp_header.get(DhcpHeader::DhcpYiaddr());
        
        // Sanity check configuration.
        if (!sanityCheckAddressInfo(ip_address, opts)) {
            return;
        }
        
        // Handle received offer in SentDiscover state.
        if (m_state == DhcpState::SentDiscover &&
            opts.dhcp_message_type == DhcpMessageType::Offer)
        {
            // Remember offer.
            m_info.ip_address = ip_address;
            m_info.dhcp_server_identifier = opts.dhcp_server_identifier;
            
            // Send request with server identifier.
            send_request(true);
            
            // Start timer for retransmitting request or reverting to discovery.
            tim(DhcpTimer()).appendAfter(Context(), RequestTimeoutTicks);
            
            // Going to state SentRequest.
            m_state = DhcpState::SentRequest;
            
            // Initialize the request count.
            m_request_count = 1;
        }
        // Handle received Ack in SentRequest or Renewing state.
        else if (m_state == OneOf(DhcpState::SentRequest, DhcpState::Renewing) &&
                 opts.dhcp_message_type == DhcpMessageType::Ack)
        {
            // Sanity check against the offer or existing lease.
            if (ip_address != m_info.ip_address ||
                opts.dhcp_server_identifier != m_info.dhcp_server_identifier)
            {
                return;
            }
            
            // Remember/update the lease information.
            m_info.lease_time_s = opts.ip_address_lease_time;
            m_info.subnet_mask = opts.subnet_mask;
            m_info.have_router = opts.have.router;
            m_info.router = opts.have.router ? opts.router : Ip4Addr::ZeroAddr();
            m_info.domain_name_servers_count = opts.have.dns_servers;
            ::memcpy(m_info.domain_name_servers, opts.dns_servers, opts.have.dns_servers * sizeof(Ip4Addr));
            m_info.server_mac = ethHw()->getRxEthHeader().get(EthHeader::SrcMac());
            
            bool renewed = m_state == DhcpState::Renewing;
            
            // Going to state Finished.
            m_state = DhcpState::Finished;
            
            // Start timeout for renewing.
            m_time_left = RenewTimeForLeaseTime(m_info.lease_time_s);
            set_timer_for_time_left(MaxTimerSeconds);
            
            // Apply IP configuration etc.
            handle_dhcp_up(renewed);
        }
    }
    
    // Checks received address information.
    // It may modify certain info in the opts that is
    // considered invalid but not fatal.
    static bool sanityCheckAddressInfo (Ip4Addr const &addr, DhcpRecvOptions &opts)
    {
        // Check that we have an IP Address lease time and a subnet mask.
        if (!opts.have.ip_address_lease_time || !opts.have.subnet_mask) {
            return false;
        }
        
        // Check that it's not all zeros or all ones.
        if (addr == Ip4Addr::ZeroAddr() || addr == Ip4Addr::AllOnesAddr()) {
            return false;
        }
        
        // Check that the subnet mask is sane.
        if (opts.subnet_mask != Ip4Addr::PrefixMask(opts.subnet_mask.countLeadingOnes())) {
            return false;
        }
        
        // Check that it's not a loopback address.
        if ((addr & Ip4Addr::PrefixMask<8>()) == Ip4Addr::FromBytes(127, 0, 0, 0)) {
            return false;
        }
        
        // Check that that it's not a multicast address.
        if ((addr & Ip4Addr::PrefixMask<4>()) == Ip4Addr::FromBytes(224, 0, 0, 0)) {
            return false;
        }
        
        // Check that it's not the local broadcast address.
        Ip4Addr local_bcast = Ip4Addr::Join(opts.subnet_mask, addr, Ip4Addr::AllOnesAddr());
        if (addr == local_bcast) {
            return false;
        }
        
        // If there is a router, check that it is within the subnet.
        if (opts.have.router) {
            if ((opts.router & opts.subnet_mask) != (addr & opts.subnet_mask)) {
                // Ignore bad router.
                opts.have.router = false;
            }
        }
        
        return true;
    }
    
    void handle_dhcp_up (bool renewed)
    {
        // Set IP address with prefix length.
        uint8_t prefix = m_info.subnet_mask.countLeadingOnes();
        iface()->setIp4Addr(IpIfaceIp4AddrSetting{true, prefix, m_info.ip_address});
        
        // Set gateway (or clear if none).
        iface()->setIp4Gateway(IpIfaceIp4GatewaySetting{m_info.have_router, m_info.router});
        
        // Call the callback if specified.
        if (m_callback != nullptr) {
            auto event_type = renewed ? LeaseEventType::LeaseRenewed : LeaseEventType::LeaseObtained;
            m_callback->dhcpLeaseEvent(event_type);
        }
    }
    
    void handle_dhcp_down (bool call_callback)
    {
        // Remove gateway.
        iface()->setIp4Gateway(IpIfaceIp4GatewaySetting{false});
        
        // Remove IP address.
        iface()->setIp4Addr(IpIfaceIp4AddrSetting{false});
        
        // Call the callback if specified and desired.
        if (m_callback != nullptr && call_callback) {
            m_callback->dhcpLeaseEvent(LeaseEventType::LeaseLost);
        }
    }
    
    // Send a DHCP request message.
    void send_request (bool send_server_identifier)
    {
        DhcpSendOptions send_opts;
        send_opts.have.requested_ip_address = true;
        send_opts.requested_ip_address = m_info.ip_address;
        if (send_server_identifier) {
            send_opts.have.dhcp_server_identifier = true;
            send_opts.dhcp_server_identifier = m_info.dhcp_server_identifier;
        }
        send_dhcp_message(DhcpMessageType::Request, m_xid, send_opts);
    }
    
    // Send a DHCP message.
    void send_dhcp_message (DhcpMessageType msg_type, uint32_t xid, DhcpSendOptions const &opts)
    {
        // Get a buffer for the message.
        using AllocHelperType = TxAllocHelper<BufAllocator, MaxDhcpSendMsgSize, HeaderBeforeIp4Dgram>;
        AllocHelperType dgram_alloc(MaxDhcpSendMsgSize);
        
        // Write the DHCP header.
        auto dhcp_header = DhcpHeader::MakeRef(dgram_alloc.getPtr() + Udp4Header::Size);
        ::memset(dhcp_header.data, 0, DhcpHeader::Size);
        dhcp_header.set(DhcpHeader::DhcpOp(),    DhcpOp::BootRequest);
        dhcp_header.set(DhcpHeader::DhcpHtype(), DhcpHwAddrType::Ethernet);
        dhcp_header.set(DhcpHeader::DhcpHlen(),  MacAddr::Size);
        dhcp_header.set(DhcpHeader::DhcpXid(),   xid);
        ethHw()->getMacAddr().encode(dhcp_header.ref(DhcpHeader::DhcpChaddr()));
        dhcp_header.set(DhcpHeader::DhcpMagic(), DhcpMagicNumber);
        
        // Write the DHCP options.
        char *opt_startptr = dgram_alloc.getPtr() + UdpDhcpHeaderSize;
        char *opt_endptr = Options::writeOptions(opt_startptr, msg_type, iface()->getMtu(), opts);
        
        // Calculate the UDP length.
        uint16_t udp_length = opt_endptr - dgram_alloc.getPtr();
        AMBRO_ASSERT(udp_length <= MaxDhcpSendMsgSize)
        
        // Write the UDP header.
        auto udp_header = Udp4Header::MakeRef(dgram_alloc.getPtr());
        udp_header.set(Udp4Header::SrcPort(),  DhcpClientPort);
        udp_header.set(Udp4Header::DstPort(),  DhcpServerPort);
        udp_header.set(Udp4Header::Length(),   udp_length);
        udp_header.set(Udp4Header::Checksum(), 0);
        
        // Construct the datagram reference.
        dgram_alloc.changeSize(udp_length);
        IpBufRef dgram = dgram_alloc.getBufRef();
        
        // Define information for IP.
        Ip4DgramMeta ip_meta = {
            Ip4Addr::ZeroAddr(),    // src_addr
            Ip4Addr::AllOnesAddr(), // dst_addr
            DhcpTTL,                // ttl
            Ip4ProtocolUdp,         // proto
            iface(),                // iface
        };
        
        // Calculate UDP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&ip_meta.src_addr.data);
        chksum_accum.addWords(&ip_meta.dst_addr.data);
        chksum_accum.addWord(APrinter::WrapType<uint16_t>(), ip_meta.proto);
        chksum_accum.addWord(APrinter::WrapType<uint16_t>(), udp_length);
        chksum_accum.addIpBuf(dgram);
        uint16_t checksum = chksum_accum.getChksum();
        if (checksum == 0) {
            checksum = std::numeric_limits<uint16_t>::max();
        }
        udp_header.set(Udp4Header::Checksum(), checksum);
        
        // Send the datagram.
        m_ipstack->sendIp4Dgram(ip_meta, dgram);
    }
};

APRINTER_ALIAS_STRUCT_EXT(IpDhcpClientService, (
    APRINTER_AS_VALUE(uint8_t, DhcpTTL),
    APRINTER_AS_VALUE(uint8_t, MaxDnsServers)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(IpStack),
        APRINTER_AS_TYPE(BufAllocator)
    ), (
        using Params = IpDhcpClientService;
        APRINTER_DEF_INSTANCE(Compose, IpDhcpClient)
    ))
))

#include <aipstack/EndNamespace.h>

#endif
