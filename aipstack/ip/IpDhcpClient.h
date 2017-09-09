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

#include <aprinter/meta/Instance.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>

#include <aipstack/common/Buf.h>
#include <aipstack/common/Chksum.h>
#include <aipstack/common/TxAllocHelper.h>
#include <aipstack/common/SendRetry.h>
#include <aipstack/common/Options.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/common/MemRef.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Udp4Proto.h>
#include <aipstack/proto/DhcpProto.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/ip/hw/IpHwCommon.h>
#include <aipstack/ip/hw/IpEthHw.h>
#include <aipstack/ip/IpDhcpClient_options.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/platform/TimerWrapper.h>

namespace AIpStack {

/**
 * Type of DHCP client event as reported by
 * @ref IpDhcpClientCallback::dhcpClientEvent.
 */
enum class IpDhcpClientEvent {
    /**
     * A lease has been obtained while no lease was owned.
     * 
     * This is reported just after addresses have been assigned no lease was
     * already owned (addresses were not assigned before).
     * 
     * This event can occur in the following contexts:
     * - When a lease is obtained after discovery.
     * - When a lease is obtained after the link was re-established via the
     *   REBOOTING state.
     */
    LeaseObtained,

    /**
     * A lease has been obtained while an existing leased was owned.
     * 
     * This is reported just after addresses have been assigned when an
     * existing lease was owned (addresses were already assigned). Note that
     * the new addresses may be different from those of the old lease.
     * 
     * This event occurs when a lease is obtained in the context of the
     * RENEWING or REBINDING state.
     */
    LeaseRenewed,

    /**
     * An existing lease has been lost.
     * 
     * This is reported just after existing address assignements have been
     * removed, except when due to the link going down (in that case the
     * LinkDown event is reported).
     * 
     * This event can occur in the following contexts:
     * - The lease has timed out.
     * - A NAK was received in reponse to a request in the constext of the
     *   RENEWING or REBINDING state.
     */
    LeaseLost,

    /**
     * The link went down while a lease was obtained.
     * 
     * This is reported when the link went down while a lease was owned, just
     * after the existing address assignements have been removed.
     * 
     * Note that the DHCP client has removed the address assignments because the
     * interface may later be reattached to a different network where these
     * assignements are not valid.
     * 
     * After the link goes up again, a subsequent LeaseObtained event indicates
     * that a lease has been re-obtained, regardless of whether this was via the
     * REBOOTING state or via discovery.
     */
    LinkDown,
};

/**
 * Interface for status callbacks from the DHCP client.
 */
class IpDhcpClientCallback {
public:
    /**
     * DHCP status callback.
     * 
     * It is not allowed to remove the interface (and therefore also the
     * DHCP client) from within the callback.
     * 
     * @param event_type Type of DHCP event.
     */
    virtual void dhcpClientEvent (IpDhcpClientEvent event_type) = 0;
};

/**
 * Initialization options for the DHCP client.
 */
class IpDhcpClientInitOptions {
public:
    /**
     * Constructor which sets default values.
     */
    inline IpDhcpClientInitOptions ()
    : client_id(MemRef::Null()),
      vendor_class_id(MemRef::Null()),
      request_ip_address(Ip4Addr::ZeroAddr())
    {}
    
    /**
     * Client identifier, empty/null to not send.
     * 
     * If given, the pointed-to memory must be valid as long as
     * the DHCP client is initialized.
     */
    MemRef client_id;
    
    /**
     * Vendor class identifier, empty/null to not send.
     * 
     * If given, the pointed-to memory must be valid as long as
     * the DHCP client is initialized.
     */
    MemRef vendor_class_id;
    
    /**
     * Address to request, zero for none.
     * 
     * If nonzero, then initially this address will be requested
     * through the REBOOTING state.
     */
    Ip4Addr request_ip_address;
};

template <typename Arg>
class IpDhcpClient;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template <typename Arg>
AIPSTACK_DECL_TIMERS_CLASS(IpDhcpClientTimers, typename Arg::PlatformImpl,
                           IpDhcpClient<Arg>, (DhcpTimer))
#endif

/**
 * DHCP client implementation.
 * 
 * @tparam Arg An instantiated @ref IpDhcpClientService::Compose template
 *         or a type derived from such; see @ref IpDhcpClientService.
 */
template <typename Arg>
class IpDhcpClient :
    private Arg::IpStack::IfaceListener,
    private Arg::IpStack::IfaceStateObserver,
    private IpDhcpClientTimers<Arg>::Timers,
    private IpSendRetryRequest,
    private IpEthHw::ArpObserver,
    private NonCopyable<IpDhcpClient<Arg>>
{
    APRINTER_USE_TYPES1(Arg, (PlatformImpl, IpStack, Params))
    APRINTER_USE_TYPES1(IpEthHw, (ArpObserver))
    using Platform = PlatformFacade<PlatformImpl>;
    APRINTER_USE_TYPES1(Platform, (TimeType))
    APRINTER_USE_TYPES1(IpStack, (Ip4RxInfo, Iface, IfaceListener, IfaceStateObserver))
    APRINTER_USE_VALS(IpStack, (HeaderBeforeIp4Dgram))
    AIPSTACK_USE_TIMERS_CLASS(IpDhcpClientTimers<Arg>, (DhcpTimer)) 
    using IpDhcpClientTimers<Arg>::Timers::platform;
    
    static_assert(Params::MaxDnsServers > 0 && Params::MaxDnsServers < 32, "");
    static_assert(Params::XidReuseMax >= 1 && Params::XidReuseMax <= 5, "");
    static_assert(Params::MaxRequests >= 1 && Params::MaxRequests <= 5, "");
    static_assert(Params::MaxRebootRequests >= 1 && Params::MaxRebootRequests <= 5, "");
    static_assert(Params::BaseRtxTimeoutSeconds >= 1 &&
                  Params::BaseRtxTimeoutSeconds <= 4, "");
    static_assert(Params::MaxRtxTimeoutSeconds >= Params::BaseRtxTimeoutSeconds &&
                  Params::MaxRtxTimeoutSeconds <= 255, "");
    static_assert(Params::ResetTimeoutSeconds >= 1 &&
                  Params::ResetTimeoutSeconds <= 128, "");
    static_assert(Params::MinRenewRtxTimeoutSeconds >= 10 &&
                  Params::MinRenewRtxTimeoutSeconds <= 255, "");
    static_assert(Params::ArpResponseTimeoutSeconds >= 1 &&
                  Params::ArpResponseTimeoutSeconds <= 5, "");
    static_assert(Params::NumArpQueries >= 1 && Params::NumArpQueries <= 10, "");
    
    // Message text to include in the DECLINE response if the address
    // was not used due to an ARP response (defined outside of class).
    static char const DeclineMessageArpResponse[];
    static uint8_t const MaxMessageSize = sizeof(DeclineMessageArpResponse) - 1;
    
    // Instatiate the options class with needed configuration.
    using Options = IpDhcpClient_options<
        Params::MaxDnsServers, Params::MaxClientIdSize,
        Params::MaxVendorClassIdSize, MaxMessageSize>;
    APRINTER_USE_TYPES1(Options, (DhcpRecvOptions, DhcpSendOptions))
    
    // DHCP client states
    enum class DhcpState {
        // Link is down
        LinkDown,
        // Resetting due to NAK after some time
        Resetting,
        // Trying to request a specific IP address
        Rebooting,
        // Send discover, waiting for offer
        Selecting,
        // Sent request after offer, waiting for ack
        Requesting,
        // Checking the address is available using ARP
        Checking,
        // We have a lease, not trying to renew yet
        Bound,
        // We have a lease and we're trying to renew it
        Renewing,
        // Like Renewing but requests are broadcast
        Rebinding,
    };
    
    // Maximum future time in seconds that the timer can be set to,
    // due to limited span of TimeType. For possibly longer periods
    // (start of renewal, lease timeout), multiple timer expirations
    // are used with keeping track of leftover seconds.
    static uint32_t const MaxTimerSeconds = MinValueU(
        std::numeric_limits<uint32_t>::max(),
        Platform::WorkingTimeSpanTicks / (TimeType)Platform::TimeFreq);
    
    static_assert(MaxTimerSeconds >= 255, "");
    
    // Determines the default renewal time if the server did not specify it.
    static constexpr uint32_t DefaultRenewTimeForLeaseTime (uint32_t lease_time_s)
    {
        return lease_time_s / 2;
    }
    
    // Determines the default rebinding time if the server did not specify it.
    static constexpr uint32_t DefaultRebindingTimeForLeaseTime (uint32_t lease_time_s)
    {
        return (uint64_t)lease_time_s * 7 / 8;
    }
    
    // Combined size of UDP and TCP headers.
    static size_t const UdpDhcpHeaderSize = Udp4Header::Size + DhcpHeaderSize;
    
    // Maximum packet size that we could possibly transmit.
    static size_t const MaxDhcpSendMsgSize =
        UdpDhcpHeaderSize + Options::MaxOptionsSendSize;
    
public:
    // Contains all the information about a lease.
    struct LeaseInfo {
        // These two are set already when the offer is received.
        Ip4Addr ip_address; // in LinkDown defines the address to reboot with or none
        uint32_t dhcp_server_identifier;
        
        // The rest are set when the ack is received.
        Ip4Addr dhcp_server_addr;
        uint32_t lease_time_s;
        uint32_t renewal_time_s;
        uint32_t rebinding_time_s;
        Ip4Addr subnet_mask;
        MacAddr server_mac;
        bool have_router;
        uint8_t domain_name_servers_count;
        Ip4Addr router;
        Ip4Addr domain_name_servers[Params::MaxDnsServers];
    };
    
private:
    IpStack *m_ipstack;
    IpDhcpClientCallback *m_callback;
    MemRef m_client_id;
    MemRef m_vendor_class_id;
    uint32_t m_xid;
    uint8_t m_rtx_timeout;
    DhcpState m_state;
    uint8_t m_request_count;
    uint32_t m_lease_time_passed;
    TimeType m_request_send_time;
    uint32_t m_request_send_time_passed;
    LeaseInfo m_info;
    
public:
    /**
     * Construct the DHCP client.
     * 
     * The interface must exist as long as the DHCP client exists.
     * The DHCP client assumes that it has exclusive control over the
     * IP address and gateway address assignement for the interface and
     * that both of these are initially unassigned.
     * 
     * @param platform The platform facade, should be the same as passed to
     *        the @ref IpStack::IpStack constructor.
     * @param stack The IP stack.
     * @param iface The interface to run on. It must be an Ethernet based interface
     *              and support the @ref IpHwType::Ethernet hardware-type
     *              specific interface (@ref IpEthHw).
     * @param opts Initialization options. This structure itself is copied but
     *             any referenced memory is not.
     * @param callback Object which will receive callbacks about the status
     *        of the lease, null for none.
     */
    IpDhcpClient (Platform platform, IpStack *stack, Iface *iface,
                  IpDhcpClientInitOptions const &opts, IpDhcpClientCallback *callback) :
        IfaceListener(iface, Ip4ProtocolUdp),
        IpDhcpClientTimers<Arg>::Timers(platform),
        m_ipstack(stack),
        m_callback(callback),
        m_client_id(opts.client_id),
        m_vendor_class_id(opts.vendor_class_id)
    {
        // We only support Ethernet interfaces.
        AMBRO_ASSERT(iface->getHwType() == IpHwType::Ethernet)
        
        // Start observing interface state.
        IfaceStateObserver::observe(*iface);
        
        // Remember any requested IP address for Rebooting.
        m_info.ip_address = opts.request_ip_address;
        
        if (iface->getDriverState().link_up) {
            // Start discovery/rebooting.
            start_discovery_or_rebooting();
        } else {
            // Remain inactive until the link is up.
            m_state = DhcpState::LinkDown;
        }
    }
    
    /**
     * Destruct the DHCP client.
     * 
     * This will remove any IP address or gateway address assignment
     * from the interface.
     */
    ~IpDhcpClient ()
    {
        // Remove any configuration that might have been done (no callback).
        handle_dhcp_down(/*call_callback=*/false, /*link_down=*/false);
    }
    
    /**
     * Check if a IP address lease is currently active.
     * 
     * @return True if a lease is active, false if not.
     */
    inline bool hasLease () const
    {
        return m_state == OneOf(DhcpState::Bound, DhcpState::Renewing,
                                DhcpState::Rebinding);
    }
    
    /**
     * Get information about the current IP address lease.
     * 
     * This may only be called when a lease is active (@ref hasLease
     * returns true).
     * 
     * @return Reference to lease information (valid only temporarily).
     */
    inline LeaseInfo const & getLeaseInfoMustHaveLease () const
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
    inline IpEthHw::HwIface * ethHw ()
    {
        return iface()->template getHwIface<IpEthHw::HwIface>();
    }
    
    // Convert seconds to ticks, requires seconds <= MaxTimerSeconds.
    inline static TimeType SecToTicks (uint32_t seconds)
    {
        AMBRO_ASSERT(seconds <= MaxTimerSeconds)
        return SecToTicksNoAssert(seconds);
    }
    
    // Same but without assert that seconds <= MaxTimerSeconds.
    inline static TimeType SecToTicksNoAssert (uint32_t seconds)
    {
        return seconds * (TimeType)Platform::TimeFreq;
    }
    
    // Convert ticks to seconds, rounding down.
    inline static TimeType TicksToSec (TimeType ticks)
    {
        return ticks / (TimeType)Platform::TimeFreq;
    }
    
    // Shortcut to last timer set time.
    inline TimeType getTimSetTime ()
    {
        return tim(DhcpTimer()).getSetTime();
    }
    
    // Set m_rtx_timeout to BaseRtxTimeoutSeconds.
    void reset_rtx_timeout ()
    {
        m_rtx_timeout = Params::BaseRtxTimeoutSeconds;
    }
    
    // Double m_rtx_timeout, but to no more than MaxRtxTimeoutSeconds.
    void double_rtx_timeout ()
    {
        m_rtx_timeout = (m_rtx_timeout > Params::MaxRtxTimeoutSeconds / 2) ?
            Params::MaxRtxTimeoutSeconds : (2 * m_rtx_timeout);
    }
    
    // Set the timer to expire after m_rtx_timeout.
    void set_timer_for_rtx ()
    {
        tim(DhcpTimer()).setAfter(SecToTicks(m_rtx_timeout));
    }
    
    // Start discovery process.
    void start_discovery_or_rebooting ()
    {
        // Generate an XID.
        new_xid();
        
        // Initialize the counter of discover/request messages.
        m_request_count = 1;
        
        if (m_info.ip_address == Ip4Addr::ZeroAddr()) {
            // Going to Selecting state.
            m_state = DhcpState::Selecting;
            
            // Send discover.
            send_discover();
        } else {
            // Go to Rebooting state.
            m_state = DhcpState::Rebooting;
            
            // Remember when the first request was sent.
            m_request_send_time = platform().getTime();
            
            // Send request.
            send_request();
        }
        
        // Set the timer for retransmission (or reverting from Rebooting to discovery).
        reset_rtx_timeout();
        set_timer_for_rtx();
    }
    
    // Start discovery (never rebooting).
    void start_discovery ()
    {
        // Clear ip_address to prevent Rebooting.
        m_info.ip_address = Ip4Addr::ZeroAddr();
        
        // Delegate to start_discovery_or_rebooting.
        start_discovery_or_rebooting();
    }
    
    void handle_expired_lease (bool had_lease)
    {
        // Start discovery.
        start_discovery();
        
        // If we had a lease, remove any IP configuration etc..
        if (had_lease) {
            return handle_dhcp_down(/*call_callback=*/true, /*link_down=*/false);
        }
    }
    
    void timerExpired (DhcpTimer)
    {
        switch (m_state) {
            case DhcpState::Resetting:
                return handleTimerResetting();
            case DhcpState::Selecting:
                return handleTimerSelecting();
            case DhcpState::Rebooting:
            case DhcpState::Requesting:
                return handleTimerRebootingRequesting();
            case DhcpState::Checking:
                return handleTimerChecking();
            case DhcpState::Bound:
            case DhcpState::Renewing:
            case DhcpState::Rebinding:
                return handleTimerBoundRenewingRebinding();
            default:
                AMBRO_ASSERT(false);
        }
    }
    
    void handleTimerResetting ()
    {
        // Timer was set for restarting discovery.
        
        start_discovery();
    }
    
    void handleTimerSelecting ()
    {
        // Timer was set for retransmitting discover.
        
        // Update request count, generate new XID if needed.
        if (m_request_count >= Params::XidReuseMax) {
            m_request_count = 1;
            new_xid();
        } else {
            m_request_count++;
        }
        
        // Send discover.
        send_discover();
        
        // Set the timer for another retransmission.
        double_rtx_timeout();
        set_timer_for_rtx();
    }
    
    void handleTimerRebootingRequesting ()
    {
        // Timer was set for retransmitting request.
        
        // If we sent enough requests, start discovery.
        auto limit = (m_state == DhcpState::Rebooting) ?
            Params::MaxRebootRequests : Params::MaxRequests;
        if (m_request_count >= limit) {
            start_discovery();
            return;
        }
        
        // Increment request count.
        m_request_count++;
        
        // NOTE: We do not update m_request_send_time, it remains set
        // to when the first request was sent. This is so that that times
        // for renewing, rebinding and lease timeout will be relative to
        // when the first request was sent.
        
        // Send request.
        send_request();
        
        // Restart timer with doubled retransmission timeout.
        double_rtx_timeout();
        set_timer_for_rtx();
    }
    
    void handleTimerChecking ()
    {
        // Timer was set to continue after no response to ARP query.
        
        if (m_request_count < Params::NumArpQueries) {
            // Increment the ARP query counter.
            m_request_count++;
            
            // Start the timeout.
            tim(DhcpTimer()).setAfter(SecToTicks(Params::ArpResponseTimeoutSeconds));
            
            // Send an ARP query.
            ethHw()->sendArpQuery(m_info.ip_address);
        } else {
            // Unsubscribe from ARP updates.
            ArpObserver::reset();
            
            // Bind the lease.
            return go_bound();
        }
    }
    
    void handleTimerBoundRenewingRebinding ()
    {
        // Timer was set for:
        // - Bound: transition to Renewing
        // - Renewing: retransmitting a request or transition to Rebinding
        // - Rebinding: retransmitting a request or lease timeout
        // Or it might have been set for earlier if that was too far in the
        // future. We anyway check how much time has actually passed and we
        // may also skip one or more states if more has passed than expected.
        
        AMBRO_ASSERT(m_lease_time_passed <= m_info.lease_time_s)
        
        TimeType now = platform().getTime();
        
        // Calculate how much time in seconds has passed since the time this timer
        // was set to expire at.
        uint32_t passed_sec = TicksToSec((TimeType)(now - getTimSetTime()));
        
        // Has the lease expired?
        if (passed_sec >= m_info.lease_time_s - m_lease_time_passed) {
            return handle_expired_lease(/*had_lease=*/true);
        }
        
        // Remember m_lease_time_passed (needed for seting the next timer).
        uint32_t prev_lease_time_passed = m_lease_time_passed;
        
        // Update m_lease_time_passed according to time passed so far.
        m_lease_time_passed += passed_sec;
        
        // Has the rebinding time expired?
        if (m_state != DhcpState::Rebinding &&
            m_lease_time_passed >= m_info.rebinding_time_s)
        {
            // Go to state Rebinding, generate XID.
            m_state = DhcpState::Rebinding;
            new_xid();
        }
        // Has the renewal time expired?
        else if (m_state == DhcpState::Bound &&
                 m_lease_time_passed >= m_info.renewal_time_s)
        {
            // Go to state Renewing, generate XID.
            m_state = DhcpState::Renewing;
            new_xid();
        }
        
        // We will choose after how many seconds the timer should next
        // expire, relative to the current m_lease_time_passed.
        uint32_t timer_rel_sec;
        
        if (m_state == DhcpState::Bound) {
            // Timer should expire at the renewal time.
            timer_rel_sec = m_info.renewal_time_s - m_lease_time_passed;
        } else {
            // Time to next state transition (Rebinding or lease timeout).
            uint32_t next_state_sec = (m_state == DhcpState::Renewing) ?
                m_info.rebinding_time_s : m_info.lease_time_s;
            uint32_t next_state_rel_sec = next_state_sec - m_lease_time_passed;
            
            // Time to next retransmission.
            // NOTE: Retransmission may actually be done earlier if this is
            // greater than MaxTimerSeconds, that is all right.
            uint32_t rtx_rel_sec = MaxValue(
                (uint32_t)Params::MinRenewRtxTimeoutSeconds,
                (uint32_t)(next_state_rel_sec / 2));
            
            // Timer should expire at the earlier of the above two.
            timer_rel_sec = MinValue(next_state_rel_sec, rtx_rel_sec);
            
            // Send a request.
            send_request();
            
            // Remember the time when the request was sent including the
            // m_lease_time_passed corresponding to this.
            m_request_send_time = now;
            m_request_send_time_passed = m_lease_time_passed;
        }
        
        // Limit to how far into the future the timer can be set.
        timer_rel_sec = MinValue(timer_rel_sec, MaxTimerSeconds);
        
        // Set the timer and update m_lease_time_passed as deciced above. Note that
        // we need to account for the extra time passed by which m_lease_time_passed
        // was incremented at the top.
        m_lease_time_passed += timer_rel_sec;
        TimeType timer_time = getTimSetTime() +
            SecToTicksNoAssert(m_lease_time_passed - prev_lease_time_passed);
        tim(DhcpTimer()).setAt(timer_time);
    }
    
    void retrySending () override final
    {
        // Retry sending a message after a send error, probably due to ARP cache miss.
        // To be complete we support retrying for all message types even broadcasts.
        
        // Note that send_dhcp_message calls IpSendRetryRequest::reset before
        // trying to send a message. This is enough to avoid spurious retransmissions,
        // because entry to all states which we handle here involves send_dhcp_message,
        // and we ignore this callback in other states.
        
        if (m_state == DhcpState::Selecting) {
            send_discover();
        }
        else if (m_state == OneOf(DhcpState::Requesting, DhcpState::Renewing,
                                  DhcpState::Rebinding, DhcpState::Rebooting)) {
            send_request();
        }
    }
    
    bool recvIp4Dgram (Ip4RxInfo const &ip_info, IpBufRef dgram) override final
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
            
            // Sanity check source address - reject broadcast addresses.
            if (AMBRO_UNLIKELY(!IpStack::checkUnicastSrcAddr(ip_info))) {
                goto accept;
            }
            
            // Check UDP length.
            uint16_t udp_length = udp_header.get(Udp4Header::Length());
            if (AMBRO_UNLIKELY(udp_length < Udp4Header::Size ||
                               udp_length > dgram.tot_len))
            {
                goto accept;
            }
            
            // Truncate data to UDP length.
            IpBufRef udp_data = dgram.subTo(udp_length);
            
            // Check UDP checksum if provided.
            uint16_t checksum = udp_header.get(Udp4Header::Checksum());
            if (checksum != 0) {
                IpChksumAccumulator chksum_accum;
                chksum_accum.addWords(&ip_info.src_addr.data);
                chksum_accum.addWords(&ip_info.dst_addr.data);
                chksum_accum.addWord(APrinter::WrapType<uint16_t>(), Ip4ProtocolUdp);
                chksum_accum.addWord(APrinter::WrapType<uint16_t>(), udp_length);
                if (chksum_accum.getChksum(udp_data) != 0) {
                    goto accept;
                }
            }
            
            // Process the DHCP payload.
            IpBufRef dhcp_msg = udp_data.hideHeader(Udp4Header::Size);
            processReceivedDhcpMessage(ip_info.src_addr, dhcp_msg);
        }
        
    accept:
        // Inhibit further processing of packet.
        return true;
        
    reject:
        // Continue processing of packet.
        return false;
    }
    
    void processReceivedDhcpMessage (Ip4Addr src_addr, IpBufRef msg)
    {
        // In these states we're not interested in any messages.
        if (m_state == OneOf(DhcpState::LinkDown, DhcpState::Resetting,
                             DhcpState::Checking, DhcpState::Bound))
        {
            return;
        }
        
        // Check that there is a DHCP header and that the first portion is contiguous.
        if (msg.tot_len < DhcpHeaderSize || !msg.hasHeader(DhcpHeader1::Size)) {
            return;
        }
        
        // Reference the first header part.
        auto dhcp_header1 = DhcpHeader1::MakeRef(msg.getChunkPtr());
        
        // Simple checks before further processing.
        // Note that we check that the XID matches the expected one here.
        bool sane =
            dhcp_header1.get(DhcpHeader1::DhcpOp())    == DhcpOp::BootReply &&
            dhcp_header1.get(DhcpHeader1::DhcpHtype()) == DhcpHwAddrType::Ethernet &&
            dhcp_header1.get(DhcpHeader1::DhcpHlen())  == MacAddr::Size &&
            dhcp_header1.get(DhcpHeader1::DhcpXid())   == m_xid &&
            MacAddr::decode(dhcp_header1.ref(DhcpHeader1::DhcpChaddr())) ==
                ethHw()->getMacAddr();
        if (!sane) {
            return;
        }
        
        // Skip the first header part.
        IpBufRef data = msg.hideHeader(DhcpHeader1::Size);
        
        // Get and skip the middle header part (sname and file).
        IpBufRef dhcp_header2 = data.subTo(DhcpHeader2::Size);
        data.skipBytes(DhcpHeader2::Size);
        
        // Read and skip the final header part (magic number).
        DhcpHeader3::Val dhcp_header3;
        data.takeBytes(DhcpHeader3::Size, dhcp_header3.data);
        
        // Check the magic number.
        if (dhcp_header3.get(DhcpHeader3::DhcpMagic()) != DhcpMagicNumber) {
            return;
        }
        
        // Parse DHCP options.
        DhcpRecvOptions opts;
        if (!Options::parseOptions(dhcp_header2, data, opts)) {
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
            // A NAK is only valid in states where we are expecting a reply to a request.
            if (m_state != OneOf(DhcpState::Requesting, DhcpState::Renewing,
                                 DhcpState::Rebinding, DhcpState::Rebooting)) {
                return;
            }
            
            // In Requesting state, verify the DHCP server identifier.
            if (m_state == DhcpState::Requesting) {
                if (opts.dhcp_server_identifier != m_info.dhcp_server_identifier) {
                    return;
                }
            }
            
            // Restart discovery. If in Requesting we go via Resetting state so that
            // a discover will be sent only after a delay. This prevents a tight loop
            // of discover-offer-request-NAK.
            bool discover_immediately = (m_state != DhcpState::Requesting);
            return go_resetting(discover_immediately);
            // Nothing else to do (further processing is for offer and ack).
        }
        
        // Get Your IP Address.
        Ip4Addr ip_address = dhcp_header1.get(DhcpHeader1::DhcpYiaddr());
        
        // Handle received offer in Selecting state.
        if (opts.dhcp_message_type == DhcpMessageType::Offer &&
            m_state == DhcpState::Selecting)
        {
            // Sanity check offer.
            if (!checkOffer(ip_address)) {
                return;
            }
            
            // Remember offer.
            m_info.ip_address = ip_address;
            m_info.dhcp_server_identifier = opts.dhcp_server_identifier;
            
            // Going to state Requesting.
            m_state = DhcpState::Requesting;
            
            // Leave existing XID because the request must use the XID of
            // the offer (which m_xid already is due to the check earlier).
            
            // Remember when the first request was sent.
            m_request_send_time = platform().getTime();
            
            // Send request.
            send_request();
            
            // Initialize the request count.
            m_request_count = 1;
            
            // Start timer for retransmitting request or reverting to discovery.
            reset_rtx_timeout();
            set_timer_for_rtx();
        }
        // Handle received ACK in Requesting/Renewing/Rebinding/Rebooting state.
        else if (opts.dhcp_message_type == DhcpMessageType::Ack &&
                 m_state == OneOf(DhcpState::Requesting, DhcpState::Renewing,
                                  DhcpState::Rebinding, DhcpState::Rebooting))
        {
            // Sanity check and fixup lease information.
            if (!checkAndFixupAck(ip_address, opts)) {
                return;
            }
            
            if (m_state == DhcpState::Requesting) {
                // In Requesting state, sanity check against the offer.
                if (ip_address != m_info.ip_address ||
                    opts.dhcp_server_identifier != m_info.dhcp_server_identifier)
                {
                    return;
                }
            }
            else if (m_state != DhcpState::Rebooting) {
                // In Renewing/Rebinding, check that not too much time has passed
                // that would make m_request_send_time invalid.
                // This check effectively means that the timer is still set for the
                // first expiration as set in request_in_renewing_or_rebinding and
                // not for a subsequent expiration due to needing a large delay.
                AMBRO_ASSERT(m_lease_time_passed >= m_request_send_time_passed)
                if (m_lease_time_passed - m_request_send_time_passed > MaxTimerSeconds) {
                    // Ignore the ACK. This should not be a problem because
                    // an ACK really should not arrive that long (MaxTimerSeconds)
                    // after a request was sent.
                    return;
                }
            }
            
            // Remember/update the lease information.
            m_info.ip_address = ip_address;
            m_info.dhcp_server_identifier = opts.dhcp_server_identifier;
            m_info.dhcp_server_addr = src_addr;
            m_info.lease_time_s = opts.ip_address_lease_time;
            m_info.renewal_time_s = opts.renewal_time;
            m_info.rebinding_time_s = opts.rebinding_time;
            m_info.subnet_mask = opts.subnet_mask;
            m_info.have_router = opts.have.router;
            m_info.router = opts.have.router ? opts.router : Ip4Addr::ZeroAddr();
            m_info.domain_name_servers_count = opts.have.dns_servers;
            ::memcpy(m_info.domain_name_servers, opts.dns_servers,
                     opts.have.dns_servers * sizeof(Ip4Addr));
            m_info.server_mac = ethHw()->getRxEthHeader().get(EthHeader::SrcMac());
            
            if (m_state == DhcpState::Requesting) {
                // In Requesting state, we need to do the ARP check first.
                go_checking();
            } else {
                // Bind the lease.
                return go_bound();
            }
        }
    }
    
    void ifaceStateChanged () override final
    {
        IpIfaceDriverState driver_state = iface()->getDriverState();
        
        if (m_state == DhcpState::LinkDown) {
            // If the link is now up, start discovery/rebooting.
            if (driver_state.link_up) {
                start_discovery_or_rebooting();
            }
        } else {
            // If the link is no longer up, revert everything.
            if (!driver_state.link_up) {
                bool had_lease = hasLease();
                
                // Prevant later requesting the m_info.ip_address via the REBOOTING
                // state if it is not actually assigned or being requested via the
                // REBOOTING state.
                if (!(had_lease || m_state == DhcpState::Rebooting)) {
                    m_info.ip_address = Ip4Addr::ZeroAddr();
                }
                
                // Go to state LinkDown.
                m_state = DhcpState::LinkDown;
                
                // Reset resources to prevent undesired callbacks.
                ArpObserver::reset();
                IpSendRetryRequest::reset();
                tim(DhcpTimer()).unset();
                
                // If we had a lease, unbind and notify user.
                if (had_lease) {
                    return handle_dhcp_down(/*call_callback=*/true, /*link_down=*/true);
                }
            }
        }
    }
    
    void arpInfoReceived (Ip4Addr ip_addr, MacAddr mac_addr) override final
    {
        AMBRO_ASSERT(m_state == DhcpState::Checking)
        
        // Is this an ARP message from the IP address we are checking?
        if (ip_addr == m_info.ip_address) {
            // Send a Decline.
            send_decline();
            
            // Unsubscribe from ARP updates.
            ArpObserver::reset();
            
            // Restart via Resetting state after a timeout.
            return go_resetting(false);
        }
    }
    
    // Do some sanity check of the offered IP address.
    static bool checkOffer (Ip4Addr addr)
    {
        // Check that it's not all zeros or all ones.
        if (addr == Ip4Addr::ZeroAddr() || addr == Ip4Addr::AllOnesAddr()) {
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
        
        return true;
    }
    
    // Checks received address information in an Ack.
    // This may modify certain fields in the opts that are considered
    // invalid but not fatal, or fill in missing fields.
    static bool checkAndFixupAck (Ip4Addr addr, DhcpRecvOptions &opts)
    {
        // Do the basic checks that apply to offers.
        if (!checkOffer(addr)) {
            return false;
        }
        
        // Check that we have an IP Address lease time.
        if (!opts.have.ip_address_lease_time) {
            return false;
        }
        
        // If there is no subnet mask, choose one based on the address class.
        if (!opts.have.subnet_mask) {
            if (addr < Ip4Addr::FromBytes(128, 0, 0, 0)) {
                // Class A.
                opts.subnet_mask = Ip4Addr::FromBytes(255, 0, 0, 0);
            }
            else if (addr < Ip4Addr::FromBytes(192, 0, 0, 0)) {
                // Class C.
                opts.subnet_mask = Ip4Addr::FromBytes(255, 255, 0, 0);
            }
            else if (addr < Ip4Addr::FromBytes(224, 0, 0, 0)) {
                // Class D.
                opts.subnet_mask = Ip4Addr::FromBytes(255, 255, 255, 0);
            }
            else {
                // Class D or E, considered invalid.
                return false;
            }
        }
        
        // Check that the subnet mask is sane.
        if (opts.subnet_mask != Ip4Addr::PrefixMask(opts.subnet_mask.countLeadingOnes())) {
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
        
        // If there is no renewal time, assume a default.
        if (!opts.have.renewal_time) {
            opts.renewal_time = DefaultRenewTimeForLeaseTime(opts.ip_address_lease_time);
        }
        // Make sure the renewal time does not exceed the lease time.
        opts.renewal_time =
            MinValue(opts.ip_address_lease_time, opts.renewal_time);
        
        // If there is no rebinding time, assume a default.
        if (!opts.have.rebinding_time) {
            opts.rebinding_time =
                DefaultRebindingTimeForLeaseTime(opts.ip_address_lease_time);
        }
        // Make sure the rebinding time is between the renewal time and the lease time.
        opts.rebinding_time =
            MaxValue(opts.renewal_time,
            MinValue(opts.ip_address_lease_time, opts.rebinding_time));
        
        return true;
    }
    
    void go_resetting (bool discover_immediately)
    {
        bool had_lease = hasLease();
        
        if (discover_immediately) {
            // Go directly to Selecting state without delay.
            start_discovery();
        } else {
            // Going to Resetting state.
            m_state = DhcpState::Resetting;
            
            // Set timeout to start discovery.
            tim(DhcpTimer()).setAfter(SecToTicks(Params::ResetTimeoutSeconds));
        }
        
        // If we had a lease, remove it.
        if (had_lease) {
            return handle_dhcp_down(/*call_callback=*/true, /*link_down=*/false);
        }
    }
    
    void go_checking ()
    {
        // Go to state Checking.
        m_state = DhcpState::Checking;
        
        // Initialize counter of ARP queries.
        m_request_count = 1;
        
        // Subscribe to receive ARP updates.
        // NOTE: This must not be called if already registered,
        // so we reset it when we no longer need it.
        ArpObserver::observe(*ethHw());
        
        // Start the timeout.
        tim(DhcpTimer()).setAfter(SecToTicks(Params::ArpResponseTimeoutSeconds));
        
        // Send an ARP query.
        ethHw()->sendArpQuery(m_info.ip_address);
    }
    
    void go_bound ()
    {
        AMBRO_ASSERT(m_state == OneOf(DhcpState::Checking, DhcpState::Renewing,
                                      DhcpState::Rebinding, DhcpState::Rebooting))
        
        bool had_lease = hasLease();
        TimeType now = platform().getTime();
        
        // Calculate how much time in seconds has passed since the request was sent
        // and set m_lease_time_passed accordingly. There is no need to limit to this
        // to lease_time_s since we check that just below.
        m_lease_time_passed = TicksToSec((TimeType)(now - m_request_send_time));
        
        // Has the lease expired already?
        if (m_lease_time_passed >= m_info.lease_time_s) {
            return handle_expired_lease(had_lease);
        }
        
        // Going to state Bound.
        // It is not necessary to check if we already need to go to Renewing
        // or Rebinding because if so the timer will take care of it.
        m_state = DhcpState::Bound;
        
        // Timer should expire at the renewal time.
        uint32_t timer_rel_sec;
        if (m_lease_time_passed <= m_info.renewal_time_s) {
            timer_rel_sec = m_info.renewal_time_s - m_lease_time_passed;
        } else {
            timer_rel_sec = 0;
        }
        
        // Limit to how far into the future the timer can be set.
        timer_rel_sec = MinValue(timer_rel_sec, MaxTimerSeconds);
        
        // Set the timer and update m_lease_time_passed to reflect the time
        // that the timer is being set for.
        m_lease_time_passed += timer_rel_sec;
        TimeType timer_time =
            m_request_send_time + SecToTicksNoAssert(m_lease_time_passed);
        tim(DhcpTimer()).setAt(timer_time);
        
        // Apply IP configuration etc..
        return handle_dhcp_up(had_lease);
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
            auto event_type = renewed ?
                IpDhcpClientEvent::LeaseRenewed : IpDhcpClientEvent::LeaseObtained;
            return m_callback->dhcpClientEvent(event_type);
        }
    }
    
    void handle_dhcp_down (bool call_callback, bool link_down)
    {
        // Remove gateway.
        iface()->setIp4Gateway(IpIfaceIp4GatewaySetting{false});
        
        // Remove IP address.
        iface()->setIp4Addr(IpIfaceIp4AddrSetting{false});
        
        // Call the callback if desired and specified.
        if (call_callback && m_callback != nullptr) {
            auto event_type = link_down ?
                IpDhcpClientEvent::LinkDown : IpDhcpClientEvent::LeaseLost;
            return m_callback->dhcpClientEvent(event_type);
        }
    }
    
    // Send a DHCP discover message.
    void send_discover ()
    {
        AMBRO_ASSERT(m_state == DhcpState::Selecting)
        
        DhcpSendOptions send_opts;
        send_dhcp_message(DhcpMessageType::Discover, send_opts,
                          Ip4Addr::ZeroAddr(), Ip4Addr::AllOnesAddr());
    }
    
    // Send a DHCP request message.
    void send_request ()
    {
        AMBRO_ASSERT(m_state == OneOf(DhcpState::Requesting, DhcpState::Renewing,
                                      DhcpState::Rebinding, DhcpState::Rebooting))
        
        DhcpSendOptions send_opts;
        Ip4Addr ciaddr = Ip4Addr::ZeroAddr();
        Ip4Addr dst_addr = Ip4Addr::AllOnesAddr();
        
        if (m_state == DhcpState::Requesting) {
            send_opts.have.dhcp_server_identifier = true;
            send_opts.dhcp_server_identifier = m_info.dhcp_server_identifier;
        }
        
        if (m_state == DhcpState::Renewing) {
            dst_addr = m_info.dhcp_server_addr;
        }
        
        if (m_state == OneOf(DhcpState::Requesting, DhcpState::Rebooting)) {
            send_opts.have.requested_ip_address = true;
            send_opts.requested_ip_address = m_info.ip_address;
        } else {
            ciaddr = m_info.ip_address;
        }
        
        send_dhcp_message(DhcpMessageType::Request, send_opts, ciaddr, dst_addr);
    }
    
    void send_decline ()
    {
        AMBRO_ASSERT(m_state == DhcpState::Checking)
        
        DhcpSendOptions send_opts;
        
        send_opts.have.dhcp_server_identifier = true;
        send_opts.dhcp_server_identifier = m_info.dhcp_server_identifier;
        
        send_opts.have.requested_ip_address = true;
        send_opts.requested_ip_address = m_info.ip_address;
        
        send_opts.have.message = true;
        send_opts.message = DeclineMessageArpResponse;
        
        send_dhcp_message(DhcpMessageType::Decline, send_opts,
                          Ip4Addr::ZeroAddr(), Ip4Addr::AllOnesAddr());
    }
    
    // Send a DHCP message.
    void send_dhcp_message (DhcpMessageType msg_type, DhcpSendOptions &opts,
                            Ip4Addr ciaddr, Ip4Addr dst_addr)
    {
        // Reset send-retry (not interested in retrying sending previous messages).
        IpSendRetryRequest::reset();
        
        // Add client identifier if configured.
        if (m_client_id.len > 0) {
            opts.have.client_identifier = true;
            opts.client_identifier = m_client_id;
        }
        
        // Add vendor class identifier if configured and not for Decline.
        if (m_vendor_class_id.len > 0 && msg_type != DhcpMessageType::Decline) {
            opts.have.vendor_class_identifier = true;
            opts.vendor_class_identifier = m_vendor_class_id;
        }
        
        // Max DHCP message size and parameter request list are present for
        // all messages except Decline.
        if (msg_type != DhcpMessageType::Decline) {
            opts.have.max_dhcp_message_size = true;
            opts.have.parameter_request_list = true;
        }
        
        // Get a buffer for the message.
        using AllocHelperType = TxAllocHelper<MaxDhcpSendMsgSize, HeaderBeforeIp4Dgram>;
        AllocHelperType dgram_alloc(MaxDhcpSendMsgSize);
        
        // Write the DHCP header.
        auto dhcp_header1 = DhcpHeader1::MakeRef(dgram_alloc.getPtr() + Udp4Header::Size);
        ::memset(dhcp_header1.data, 0, DhcpHeaderSize); // zero entire DHCP header
        dhcp_header1.set(DhcpHeader1::DhcpOp(),     DhcpOp::BootRequest);
        dhcp_header1.set(DhcpHeader1::DhcpHtype(),  DhcpHwAddrType::Ethernet);
        dhcp_header1.set(DhcpHeader1::DhcpHlen(),   MacAddr::Size);
        dhcp_header1.set(DhcpHeader1::DhcpXid(),    m_xid);
        dhcp_header1.set(DhcpHeader1::DhcpCiaddr(), ciaddr);
        ethHw()->getMacAddr().encode(dhcp_header1.ref(DhcpHeader1::DhcpChaddr()));
        auto dhcp_header3 = DhcpHeader3::MakeRef(
            dhcp_header1.data + DhcpHeader1::Size + DhcpHeader2::Size);
        dhcp_header3.set(DhcpHeader3::DhcpMagic(),  DhcpMagicNumber);
        
        // Write the DHCP options.
        char *opt_startptr = dgram_alloc.getPtr() + UdpDhcpHeaderSize;
        char *opt_endptr =
            Options::writeOptions(opt_startptr, msg_type, iface()->getMtu(), opts);
        
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
        
        // Calculate UDP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&ciaddr.data);
        chksum_accum.addWords(&dst_addr.data);
        chksum_accum.addWord(APrinter::WrapType<uint16_t>(), Ip4ProtocolUdp);
        chksum_accum.addWord(APrinter::WrapType<uint16_t>(), udp_length);
        uint16_t checksum = chksum_accum.getChksum(dgram);
        if (checksum == 0) {
            checksum = std::numeric_limits<uint16_t>::max();
        }
        udp_header.set(Udp4Header::Checksum(), checksum);
        
        // Send the datagram.
        Ip4Addrs addrs{ciaddr, dst_addr};
        m_ipstack->sendIp4Dgram(addrs, {Params::DhcpTTL, Ip4ProtocolUdp}, dgram, iface(),
                                this, IpSendFlags());
    }
    
    void new_xid ()
    {
        m_xid = platform().getTime();
    }
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template <typename Arg>
char const IpDhcpClient<Arg>::DeclineMessageArpResponse[] = "ArpResponse";
#endif

/**
 * Options for @ref IpDhcpClientService.
 */
struct IpDhcpClientOptions {
    /**
     * TTL of outgoing DHCP datagrams.
     */
    AIPSTACK_OPTION_DECL_VALUE(DhcpTTL, uint8_t, 64)
    
    /**
     * Maximum number of DNS servers that can be stored.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxDnsServers, uint8_t, 2)
    
    /**
     * Maximum size of client identifier that can be sent.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxClientIdSize, uint8_t, 16)
    
    /**
     * Maximum size of vendor class ID that can be sent.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxVendorClassIdSize, uint8_t, 16)
    
    /**
     * Maximum times that an XID will be reused.
     */
    AIPSTACK_OPTION_DECL_VALUE(XidReuseMax, uint8_t, 3)
    
    /**
     * Maximum times to send a request after an offer before reverting to discovery.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxRequests, uint8_t, 3)
    
    /**
     * Maximum times to send a request in REBOOTING state before revering to discovery.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxRebootRequests, uint8_t, 2)
    
    /**
     * Base retransmission time in seconds, before any backoff.
     */
    AIPSTACK_OPTION_DECL_VALUE(BaseRtxTimeoutSeconds, uint8_t, 3)
    
    /**
     * Maximum retransmission timeout (except in RENEWING or REBINDING states).
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxRtxTimeoutSeconds, uint8_t, 64)
    
    /**
     * Delay before sending a discover in certain error scenarios.
     *
     * This delay is used:
     * - after receing a NAK in response to a request following an offer,
     * - after receiving an ARP response while checking the offered address.
     */
    AIPSTACK_OPTION_DECL_VALUE(ResetTimeoutSeconds, uint8_t, 3)
    
    /**
     * Minimum request retransmission time when renewing a lease (in RENEWING or
     * REBINDING states).
     */
    AIPSTACK_OPTION_DECL_VALUE(MinRenewRtxTimeoutSeconds, uint8_t, 60)
    
    /**
     * How long to wait for a response to each ARP query when checking the address.
     */
    AIPSTACK_OPTION_DECL_VALUE(ArpResponseTimeoutSeconds, uint8_t, 1)
    
    /**
     * Number of ARP queries to send before proceeding with address assignment if no
     * response is received.
     * 
     * Normally when there is no response, ArpResponseTimeoutSeconds*NumArpQueries
     * will be spent for checking the address using ARP.
     */
    AIPSTACK_OPTION_DECL_VALUE(NumArpQueries, uint8_t, 2)
};

/**
 * Service definition for @ref IpDhcpClient.
 * 
 * The template parameters of this class are static configuration. After these
 * are defined, use @ref APRINTER_MAKE_INSTANCE with @ref Compose to obtain the
 * @ref IpDhcpClient class type, like this:
 * 
 * @code
 * using MyDhcpClientService = AIpStack::IpDhcpClientService<...>;
 * APRINTER_MAKE_INSTANCE(MyDhcpClient, (MyDhcpClientService::template Compose<
 *     PlatformImpl, MyIpStack>))
 * MyDhcpClient dhcp_client(...);
 * @endcode
 * 
 * The template parameters are assignments of options defined in
 * @ref IpDhcpClientOptions, for example:
 * AIpStack::IpDhcpClientOptions::DhcpTTL::Is\<16\>. The defaults (achieved with
 * an emtpy parameter list) should be suitable for most uses.
 * 
 * @tparam Options Assignments of options defined in @ref IpDhcpClientOptions.
 */
template <typename... Options>
class IpDhcpClientService {
    template <typename>
    friend class IpDhcpClient;
    
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, DhcpTTL)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxDnsServers)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxClientIdSize)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxVendorClassIdSize)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, XidReuseMax)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxRequests)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxRebootRequests)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, BaseRtxTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxRtxTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, ResetTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MinRenewRtxTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, ArpResponseTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, NumArpQueries)
    
public:
    /**
     * Template for use with @ref APRINTER_MAKE_INSTANCE to get an @ref IpDhcpClient type.
     * 
     * See @ref IpDhcpClientService for an example of instantiating the @ref IpDhcpClient.
     * 
     * @tparam PlatformImpl_ The platform implementation class, should be the same as
     *         passed to @ref IpStackService::Compose.
     * @tparam IpStack_ The @ref IpStack template class type.
     */
    template <typename PlatformImpl_, typename IpStack_>
    struct Compose {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        using PlatformImpl = PlatformImpl_;
        using IpStack = IpStack_;
        using Params = IpDhcpClientService;
        APRINTER_DEF_INSTANCE(Compose, IpDhcpClient)        
#endif
    };
};

}

#endif
