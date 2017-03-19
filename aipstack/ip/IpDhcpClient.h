/*
 * Copyright (c) 20176 Ambroz Bizjak
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
#include <aprinter/base/LoopUtils.h>
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

#include <aipstack/BeginNamespace.h>

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
    APRINTER_USE_ONEOF
    using TheClockUtils = APrinter::ClockUtils<Context>;
    
    APRINTER_USE_TIMERS_CLASS(IpDhcpClientTimers<Arg>, (DhcpTimer)) 
    
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
    
    // Helper function for calculating the size of a DHCP option.
    // OptDataType is the payload type declared with APRINTER_TSTRUCT.
    template <typename OptDataType>
    static constexpr size_t OptSize (OptDataType)
    {
        return DhcpOptionHeader::Size + OptDataType::Size;
    }
    
    // Maximum packet size that we could possibly transmit.
    static size_t const MaxDhcpSendMsgSize = UdpDhcpHeaderSize +
        // DHCP message type
        OptSize(DhcpOptMsgType()) +
        // requested IP address
        OptSize(DhcpOptAddr()) +
        // DHCP server identifier
        OptSize(DhcpOptServerId()) +
        // maximum message size
        OptSize(DhcpOptMaxMsgSize()) +
        // parameter request list
        DhcpOptionHeader::Size + 4 +
        // end
        1;
    
private:
    IpStack *m_ipstack;
    uint32_t m_xid;
    DhcpState m_state;
    uint8_t m_xid_reuse_count;
    uint8_t m_request_count;
    uint32_t m_time_left;
    struct {
        Ip4Addr yiaddr;
        uint32_t dhcp_server_identifier;
    } m_offered;
    struct {
        uint32_t ip_address_lease_time;
        Ip4Addr subnet_mask;
        MacAddr server_mac;
        bool have_router;
        uint8_t domain_name_servers_count;
        Ip4Addr router;
        Ip4Addr domain_name_servers[MaxDnsServers];
    } m_acked;
    
public:
    void init (IpStack *stack, Iface *iface)
    {
        // We only support Ethernet interfaces.
        AMBRO_ASSERT(iface->getHwType() == IpHwType::Ethernet)
        
        // Remember stuff.
        m_ipstack = stack;
        
        // Init resources.
        IfaceListener::init(iface, Ip4ProtocolUdp);
        tim(DhcpTimer()).init(Context());
        
        // Start discovery.
        start_discovery(true);
    }
    
    void deinit ()
    {
        // Remove any configuration that might have ben done.
        handle_dhcp_down();
        
        // Deinit resources.
        tim(DhcpTimer()).deinit(Context());
        IfaceListener::deinit();
    }
    
private:
    // Structure representing received DHCP options.
    struct DhcpRecvOptions {
        // Which options have been received.
        struct Have {
            bool dhcp_message_type : 1;
            bool dhcp_server_identifier : 1;
            bool ip_address_lease_time : 1;
            bool subnet_mask : 1;
            bool router : 1;
            uint8_t dns_servers;
        } have;
        
        // The option values (only options set in Have are initialized).
        DhcpMessageType dhcp_message_type;
        uint32_t dhcp_server_identifier;
        uint32_t ip_address_lease_time;
        Ip4Addr subnet_mask;
        Ip4Addr router;
        Ip4Addr dns_servers[MaxDnsServers];
    };
    
    // Structure representing DHCP options to be sent.
    struct DhcpSendOptions {
        // Default constructor clears the Have fields.
        inline DhcpSendOptions ()
        : have(Have{})
        {}
        
        // Which options to send.
        struct Have {
            bool requested_ip_address : 1;
            bool dhcp_server_identifier : 1;
        } have;
        
        // The option values (only options set in Have are relevant).
        Ip4Addr requested_ip_address;
        uint32_t dhcp_server_identifier;
    };
    
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
                m_time_left = m_acked.ip_address_lease_time - RenewTimeForLeaseTime(m_acked.ip_address_lease_time);
                
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
                    handle_lease_expired();
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
    
    void handle_lease_expired ()
    {
        // Restart discovery.
        start_discovery(true);
        
        // Handle down.
        handle_dhcp_down();
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
                chksum_accum.addWords(&ip_meta.remote_addr.data);
                chksum_accum.addWords(&ip_meta.local_addr.data);
                chksum_accum.addWord(APrinter::WrapType<uint16_t>(), Ip4ProtocolUdp);
                chksum_accum.addWord(APrinter::WrapType<uint16_t>(), udp_length);
                chksum_accum.addIpBuf(dgram);
                if (chksum_accum.getChksum() != 0) {
                    goto accept;
                }
            }
            
            // Process the DHCP payload.
            IpBufRef dhcp_msg = dgram.hideHeader(Udp4Header::Size);
            processReceivedDhcpMessage(ip_meta, dhcp_msg);
        }
        
    accept:
        // Inhibit further processing of packet.
        return true;
        
    reject:
        // Continue processing of packet.
        return false;
    }
    
    void processReceivedDhcpMessage (Ip4DgramMeta const &ip_meta, IpBufRef msg)
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
            dhcp_header.get(DhcpHeader::DhcpOp())    == APrinter::ToUnderlyingType(DhcpOp::BootReply) &&
            dhcp_header.get(DhcpHeader::DhcpHtype()) == APrinter::ToUnderlyingType(DhcpHwAddrType::Ethernet) &&
            dhcp_header.get(DhcpHeader::DhcpHlen())  == MacAddr::Size &&
            dhcp_header.get(DhcpHeader::DhcpXid())   == m_xid &&
            MacAddr::decode(dhcp_header.ref(DhcpHeader::DhcpChaddr())) == ethHw()->getMacAddr() &&
            dhcp_header.get(DhcpHeader::DhcpMagic()) == DhcpMagicNumber;
        if (!sane) {
            return;
        }
        
        // Parse DHCP options.
        DhcpRecvOptions opts;
        if (!parse_dhcp_options(msg, opts)) {
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
            if (opts.dhcp_server_identifier != m_offered.dhcp_server_identifier) {
                return;
            }
            
            DhcpState prev_state = m_state;
            
            // Set timeout to start discovery.
            tim(DhcpTimer()).appendAfter(Context(), ResetTimeoutTicks);
            
            // Going to Resetting state.
            m_state = DhcpState::Resetting;
            
            // If we had a lease, remove it.
            if (prev_state == OneOf(DhcpState::Finished, DhcpState::Renewing)) {
                handle_dhcp_down();
            }
            
            // Nothing else to do (further processing is for offer and ack).
            return;
        }
        
        // Get Your IP Address.
        Ip4Addr yiaddr = dhcp_header.get(DhcpHeader::DhcpYiaddr());
        
        // Sanity check configuration.
        if (!sanityCheckAddressInfo(yiaddr, opts)) {
            return;
        }
        
        // Handle received offer in SentDiscover state.
        if (m_state == DhcpState::SentDiscover &&
            opts.dhcp_message_type == DhcpMessageType::Offer)
        {
            // Remember offer.
            m_offered.yiaddr = yiaddr;
            m_offered.dhcp_server_identifier = opts.dhcp_server_identifier;
            
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
            // Sanity check against the offer.
            if (yiaddr != m_offered.yiaddr ||
                opts.dhcp_server_identifier != m_offered.dhcp_server_identifier)
            {
                return;
            }
            
            // Remember the lease time.
            m_acked.ip_address_lease_time = opts.ip_address_lease_time;
            
            // If we are in SentRequest state, remember lease information.
            // TODO: Handle possible changes of this info somehow.
            if (m_state == DhcpState::SentRequest) {
                m_acked.subnet_mask = opts.subnet_mask;
                m_acked.have_router = opts.have.router;
                m_acked.router = opts.have.router ? opts.router : Ip4Addr::ZeroAddr();
                m_acked.domain_name_servers_count = opts.have.dns_servers;
                ::memcpy(m_acked.domain_name_servers, opts.dns_servers, opts.have.dns_servers * sizeof(Ip4Addr));
                m_acked.server_mac = ethHw()->getRxEthHeader().get(EthHeader::SrcMac());
            }
            
            DhcpState prev_state = m_state;
            
            // Going to state Finished.
            m_state = DhcpState::Finished;
            
            // Start timeout for renewing.
            m_time_left = RenewTimeForLeaseTime(m_acked.ip_address_lease_time);
            set_timer_for_time_left(MaxTimerSeconds);
            
            // If this is a new lease, apply IP configuration etc.
            if (prev_state == DhcpState::SentRequest) {
                handle_dhcp_up();
            }
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
    
    void handle_dhcp_up ()
    {
        // Set IP address with prefix length.
        uint8_t prefix = m_acked.subnet_mask.countLeadingOnes();
        iface()->setIp4Addr(IpIfaceIp4AddrSetting{true, prefix, m_offered.yiaddr});
        
        // Set gateway if provided.
        if (m_acked.have_router) {
            iface()->setIp4Gateway(IpIfaceIp4GatewaySetting{true, m_acked.router});
        }
    }
    
    void handle_dhcp_down ()
    {
        // Remove gateway.
        iface()->setIp4Gateway(IpIfaceIp4GatewaySetting{false});
        
        // Remove IP address.
        iface()->setIp4Addr(IpIfaceIp4AddrSetting{false});
    }
    
    // Send a DHCP request message.
    void send_request (bool send_server_identifier)
    {
        DhcpSendOptions send_opts;
        send_opts.have.requested_ip_address = true;
        send_opts.requested_ip_address = m_offered.yiaddr;
        if (send_server_identifier) {
            send_opts.have.dhcp_server_identifier = true;
            send_opts.dhcp_server_identifier = m_offered.dhcp_server_identifier;
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
        dhcp_header.set(DhcpHeader::DhcpOp(),    APrinter::ToUnderlyingType(DhcpOp::BootRequest));
        dhcp_header.set(DhcpHeader::DhcpHtype(), APrinter::ToUnderlyingType(DhcpHwAddrType::Ethernet));
        dhcp_header.set(DhcpHeader::DhcpHlen(),  MacAddr::Size);
        dhcp_header.set(DhcpHeader::DhcpXid(),   xid);
        ethHw()->getMacAddr().encode(dhcp_header.ref(DhcpHeader::DhcpChaddr()));
        dhcp_header.set(DhcpHeader::DhcpMagic(), DhcpMagicNumber);
        
        // Write options; opt_writeptr will be incremented as options are written.
        // NOTE: If adding new options, adjust the MaxDhcpSendMsgSize definition.
        char *opt_writeptr = dgram_alloc.getPtr() + UdpDhcpHeaderSize;
        
        // DHCP message type
        write_option(opt_writeptr, DhcpOptionType::DhcpMessageType, [&](char *opt_data) {
            auto opt = DhcpOptMsgType::Ref(opt_data);
            opt.set(DhcpOptMsgType::MsgType(), APrinter::ToUnderlyingType(msg_type));
            return opt.Size();
        });
        
        // Requested IP address
        if (opts.have.requested_ip_address) {
            write_option(opt_writeptr, DhcpOptionType::RequestedIpAddress, [&](char *opt_data) {
                auto opt = DhcpOptAddr::Ref(opt_data);
                opt.set(DhcpOptAddr::Addr(), opts.requested_ip_address);
                return opt.Size();
            });
        }
        
        // DHCP server identifier
        if (opts.have.dhcp_server_identifier) {
            write_option(opt_writeptr, DhcpOptionType::DhcpServerIdentifier, [&](char *opt_data) {
                auto opt = DhcpOptServerId::Ref(opt_data);
                opt.set(DhcpOptServerId::ServerId(), opts.dhcp_server_identifier);
                return opt.Size();
            });
        }
        
        // Maximum message size
        write_option(opt_writeptr, DhcpOptionType::MaximumMessageSize, [&](char *opt_data) {
            auto opt = DhcpOptMaxMsgSize::Ref(opt_data);
            opt.set(DhcpOptMaxMsgSize::MaxMsgSize(), iface()->getMtu());
            return opt.Size();
        });
        
        // Parameter request list
        write_option(opt_writeptr, DhcpOptionType::ParameterRequestList, [&](char *opt_data) {
            DhcpOptionType opt[] = {
                DhcpOptionType::SubnetMask,
                DhcpOptionType::Router,
                DhcpOptionType::DomainNameServer,
                DhcpOptionType::IpAddressLeaseTime,
            };
            ::memcpy(opt_data, opt, sizeof(opt));
            return sizeof(opt);
        });
        
        // end option
        {
            DhcpOptionType end = DhcpOptionType::End;
            ::memcpy(opt_writeptr, &end, sizeof(end));
            opt_writeptr += sizeof(end);
        }
        
        // Calculate the UDP length.
        uint16_t udp_length = opt_writeptr - dgram_alloc.getPtr();
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
            Ip4Addr::ZeroAddr(),    // local_addr
            Ip4Addr::AllOnesAddr(), // remote_addr
            DhcpTTL,                // ttl
            Ip4ProtocolUdp,         // proto
            iface(),                // iface
        };
        
        // Calculate UDP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&ip_meta.local_addr.data);
        chksum_accum.addWords(&ip_meta.remote_addr.data);
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
    
    // Helper function for encoding DHCP options.
    template <typename PayloadFunc>
    void write_option (char *&out, DhcpOptionType opt_type, PayloadFunc payload_func)
    {
        auto oh = DhcpOptionHeader::Ref(out);
        oh.set(DhcpOptionHeader::OptType(), APrinter::ToUnderlyingType(opt_type));
        size_t opt_len = payload_func(out + DhcpOptionHeader::Size);
        oh.set(DhcpOptionHeader::OptLen(), opt_len);
        out += DhcpOptionHeader::Size + opt_len;
    }
    
    // Parse DHCP options from a buffer into DhcpRecvOptions.
    static bool parse_dhcp_options (IpBufRef data, DhcpRecvOptions &opts)
    {
        // Clear all the "have" fields.
        opts.have = typename DhcpRecvOptions::Have{};
        
        // Whether we have seen the end option.
        bool have_end = false;
        
        while (data.tot_len > 0) {
            // Read option type.
            DhcpOptionType opt_type = DhcpOptionType(data.takeByte());
            
            // Pad option?
            if (opt_type == DhcpOptionType::Pad) {
                continue;
            }
            
            // It is an error for options other than pad to follow
            // the end option.
            if (have_end) {
                return false;
            }
            
            // End option?
            if (opt_type == DhcpOptionType::End) {
                // Skip over any following pad options.
                have_end = true;
                continue;
            }
            
            // Read option length.
            if (data.tot_len == 0) {
                return false;
            }
            uint8_t opt_len = data.takeByte();
            
            // Check option length.
            if (opt_len > data.tot_len) {
                return false;
            }
            
            // Handle different options.
            switch (opt_type) {
                case DhcpOptionType::DhcpMessageType: {
                    if (opt_len != DhcpOptMsgType::Size) {
                        goto skip_data;
                    }
                    DhcpOptMsgType::Val val;
                    data.takeBytes(opt_len, val.data);
                    opts.have.dhcp_message_type = true;
                    opts.dhcp_message_type = DhcpMessageType(val.get(DhcpOptMsgType::MsgType()));
                } break;
                
                case DhcpOptionType::DhcpServerIdentifier: {
                    if (opt_len != DhcpOptServerId::Size) {
                        goto skip_data;
                    }
                    DhcpOptServerId::Val val;
                    data.takeBytes(opt_len, val.data);
                    opts.have.dhcp_server_identifier = true;
                    opts.dhcp_server_identifier = val.get(DhcpOptServerId::ServerId());
                } break;
                
                case DhcpOptionType::IpAddressLeaseTime: {
                    if (opt_len != DhcpOptTime::Size) {
                        goto skip_data;
                    }
                    DhcpOptTime::Val val;
                    data.takeBytes(opt_len, val.data);
                    opts.have.ip_address_lease_time = true;
                    opts.ip_address_lease_time = val.get(DhcpOptTime::Time());
                } break;
                
                case DhcpOptionType::SubnetMask: {
                    if (opt_len != DhcpOptAddr::Size) {
                        goto skip_data;
                    }
                    DhcpOptAddr::Val val;
                    data.takeBytes(opt_len, val.data);
                    opts.have.subnet_mask = true;
                    opts.subnet_mask = val.get(DhcpOptAddr::Addr());
                } break;
                
                case DhcpOptionType::Router: {
                    if (opt_len != DhcpOptAddr::Size) {
                        goto skip_data;
                    }
                    DhcpOptAddr::Val val;
                    data.takeBytes(opt_len, val.data);
                    opts.have.router = true;
                    opts.router = val.get(DhcpOptAddr::Addr());
                } break;
                
                case DhcpOptionType::DomainNameServer: {
                    if (opt_len % DhcpOptAddr::Size != 0) {
                        goto skip_data;
                    }
                    uint8_t num_servers = opt_len / DhcpOptAddr::Size;
                    for (auto server_index : APrinter::LoopRangeAuto(num_servers)) {
                        // Must consume all servers from data even if we can't save them.
                        DhcpOptAddr::Val val;
                        data.takeBytes(DhcpOptAddr::Size, val.data);
                        if (opts.have.dns_servers < MaxDnsServers) {
                            opts.dns_servers[opts.have.dns_servers++] = val.get(DhcpOptAddr::Addr());
                        }
                    }
                } break;
                
                // Unknown or bad option, consume the option data.
                skip_data:
                default: {
                    data.skipBytes(opt_len);
                } break;
            }
        }
        
        // Check that the end option was found.
        if (!have_end) {
            return false;
        }
        
        return true;
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
