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

#ifndef APRINTER_IPSTACK_IP_TCP_PROTO_H
#define APRINTER_IPSTACK_IP_TCP_PROTO_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BitsInFloat.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/system/MultiTimer.h>

#include <aipstack/misc/Buf.h>
#include <aipstack/misc/SendRetry.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Tcp4Proto.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/tcp/TcpUtils.h>
#include <aipstack/tcp/TcpOosBuffer.h>

#include "IpTcpProto_constants.h"
#include "IpTcpProto_api.h"
#include "IpTcpProto_input.h"
#include "IpTcpProto_output.h"

#include <aipstack/BeginNamespace.h>

/**
 * TCP protocol implementation.
 */
template <typename Arg>
class IpTcpProto
{
    APRINTER_USE_VALS(Arg::Params, (TcpTTL, NumTcpPcbs, NumOosSegs,
                                    EphemeralPortFirst, EphemeralPortLast,
                                    LinkWithArrayIndices))
    APRINTER_USE_TYPES1(Arg::Params, (PcbIndexService))
    APRINTER_USE_TYPES1(Arg, (Context, TheIpStack))
    
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    APRINTER_USE_ONEOF
    
    APRINTER_USE_TYPES1(TheIpStack, (Ip4DgramMeta, Iface, MtuRef))
    
    static_assert(NumTcpPcbs > 0, "");
    static_assert(NumOosSegs > 0 && NumOosSegs < 16, "");
    static_assert(EphemeralPortFirst > 0, "");
    static_assert(EphemeralPortFirst <= EphemeralPortLast, "");
    
    template <typename> friend class IpTcpProto_constants;
    template <typename> friend class IpTcpProto_api;
    template <typename> friend class IpTcpProto_input;
    template <typename> friend class IpTcpProto_output;
    
public:
    APRINTER_USE_TYPES1(TcpUtils, (SeqType, PortType))
    
private:
    using Constants = IpTcpProto_constants<IpTcpProto>;
    using Api = IpTcpProto_api<IpTcpProto>;
    using Input = IpTcpProto_input<IpTcpProto>;
    using Output = IpTcpProto_output<IpTcpProto>;
    
    APRINTER_USE_TYPES1(TcpUtils, (TcpState, TcpOptions, PcbKey, PcbKeyCompare))
    APRINTER_USE_VALS(TcpUtils, (state_is_active, accepting_data_in_state,
                                 can_output_in_state, snd_open_in_state,
                                 seq_diff))
    
    struct TcpPcb;
    
    // PCB flags, see flags in TcpPcb.
    using FlagsType = uint16_t;
    struct PcbFlags { enum : FlagsType {
        // ACK is needed; used in input processing
        ACK_PENDING = (FlagsType)1 << 0,
        // pcb_output_active/pcb_output_abandoned should be called at the end of
        // input processing. This flag must imply can_output_in_state and
        // pcb_has_snd_outstanding at the point in pcb_input where it is checked.
        // Any change that would break this implication must clear the flag.
        OUT_PENDING = (FlagsType)1 << 1,
        // A FIN was sent at least once and is included in snd_nxt
        FIN_SENT    = (FlagsType)1 << 2,
        // A FIN is to queued for sending
        FIN_PENDING = (FlagsType)1 << 3,
        // Round-trip-time is being measured
        RTT_PENDING = (FlagsType)1 << 4,
        // Round-trip-time is not in initial state
        RTT_VALID   = (FlagsType)1 << 5,
        // cwnd has been increaded by snd_mss this round-trip
        CWND_INCRD  = (FlagsType)1 << 6,
        // A segment has been retransmitted and not yet acked
        RTX_ACTIVE  = (FlagsType)1 << 7,
        // The recover variable valid (and >=snd_una)
        RECOVER     = (FlagsType)1 << 8,
        // If rtx_timer is running it is for idle timeout
        IDLE_TIMER  = (FlagsType)1 << 9,
        // Window scaling is used
        WND_SCALE   = (FlagsType)1 << 10,
        // Current cwnd is the initial cwnd
        CWND_INIT   = (FlagsType)1 << 11,
        // If OutputTimer is set it is for OutputRetry*Ticks
        OUT_RETRY   = (FlagsType)1 << 12,
        // rcv_ann_wnd needs update before sending a segment, implies con != nullptr
        RCV_WND_UPD = (FlagsType)1 << 13,
        // NOTE: Currently no more bits are available, see TcpPcb::flags.
    }; };
    
    // For retransmission time calculations we right-shift the Clock time
    // to obtain granularity between 1ms and 2ms.
    static int const RttShift = APrinter::BitsInFloat(1e-3 / Clock::time_unit);
    static_assert(RttShift >= 0, "");
    static constexpr double RttTimeFreq =
        Clock::time_freq / APrinter::PowerOfTwoFunc<double>(RttShift);
    
    // We store such scaled times in 16-bit variables.
    // This gives us a range of at least 65 seconds.
    using RttType = uint16_t;
    static RttType const RttTypeMax = (RttType)-1;
    static constexpr double RttTypeMaxDbl = RttTypeMax;
    
    // For intermediate RTT results we need a larger type.
    using RttNextType = uint32_t;
    
    // Number of ephemeral ports.
    static PortType const NumEphemeralPorts = EphemeralPortLast - EphemeralPortFirst + 1;
    
    // Unsigned integer type usable as an index for the PCBs array.
    // We use the largest value of that type as null (which cannot
    // be a valid PCB index).
    using PcbIndexType = APrinter::ChooseIntForMax<NumTcpPcbs, false>;
    static PcbIndexType const PcbIndexNull = PcbIndexType(-1);
    
    // Instantiate the out-of-sequence buffering.
    APRINTER_MAKE_INSTANCE(OosBuffer, (TcpOosBufferService<NumOosSegs>))
    
    struct PcbLinkModel;
    
    // Instantiate the PCB index.
    struct PcbIndexAccessor;
    using PcbIndexLookupKeyArg = PcbKey const &;
    struct PcbIndexKeyFuncs;
    APRINTER_MAKE_INSTANCE(PcbIndex, (PcbIndexService::template Index<
        PcbIndexAccessor, PcbIndexLookupKeyArg, PcbIndexKeyFuncs, PcbLinkModel>))
    
public:
    APRINTER_USE_TYPES1(Api, (TcpConnection, TcpListenParams, TcpListener,
                              TcpListenerCallback))
    
    static SeqType const MaxRcvWnd = Constants::MaxWindow;
    
private:
    // These TcpPcb fields are injected into MultiTimer to fill up what
    // would otherwise be holes in the layout, for better memory use.
    struct MultiTimerUserData {
        // The base send MSS. It is computed based on the interface
        // MTU and the MTU option provided by the peer.
        // In the SYN_SENT state this is set based on the interface MTU and
        // the calculation is completed at the transition to ESTABLISHED.
        uint16_t base_snd_mss;
    };
    
    /**
     * Timers:
     * AbrtTimer: for aborting PCB (TIME_WAIT, abandonment)
     * OutputTimer: for pcb_output after send buffer extension
     * RtxTimer: for retransmission, window probe and cwnd idle reset
     */
    struct AbrtTimer {};
    struct OutputTimer {};
    struct RtxTimer {};
    using PcbMultiTimer = APrinter::MultiTimer<
        typename Context::EventLoop::TimedEventNew, TcpPcb, MultiTimerUserData,
        AbrtTimer, OutputTimer, RtxTimer>;
    
    /**
     * A TCP Protocol Control Block.
     * These are maintained internally within the stack and may
     * survive deinit/reset of an associated TcpConnection object.
     */
    struct TcpPcb :
        // Send retry request (inherited for efficiency).
        public IpSendRetry::Request,
        // PCB timers.
        public PcbMultiTimer,
        // Local/remote IP address and port
        public PcbKey
    {
        // Node for the PCB index.
        typename PcbIndex::Node index_hook;
        
        // Node for the unreferenced PCBs list.
        // The function pcb_is_in_unreferenced_list specifies exactly when
        // a PCB is suposed to be in the unreferenced list. The only
        // exception to this is while pcb_unlink_con is during the callback
        // pcb_unlink_con-->pcb_aborted-->connectionAborted.
        APrinter::LinkedListNode<PcbLinkModel> unrefed_list_node;
        
        // Pointer back to IpTcpProto.
        IpTcpProto *tcp;    
        
        union {
            // Pointer to the associated TcpListener, if in SYN_RCVD.
            TcpListener *lis;
            
            // Pointer to any associated TcpConnection, otherwise.
            TcpConnection *con;
        };
        
        // Sender variables.
        SeqType snd_una;
        SeqType snd_nxt;
        
        // Receiver variables.
        SeqType rcv_nxt;
        SeqType rcv_ann_wnd; // ensured to fit in size_t (in case size_t is 16-bit)
        
        // Round-trip-time and retransmission time management.
        TimeType rtt_test_time;
        RttType rto;
        
        // The maximum segment size we will send.
        // This is dynamic based on Path MTU Discovery, but it will always
        // be between Constants::MinAllowedMss and base_snd_mss.
        // It is first properly initialized at the transition to ESTABLISHED
        // state, before that in SYN_SENT/SYN_RCVD is is used to store the
        // pmtu/iface_mss respectively.
        // Due to invariants and other requirements associated with snd_mss,
        // fixups must be performed when snd_mss is changed, specifically of
        // ssthresh, cwnd and rtx_timer (see pcb_pmtu_changed).
        uint16_t snd_mss;
        
        // NOTE: The following 5 fields are uint32_t to encourage compilers
        // to pack them into a single 32-bit word, if they were narrower
        // they may be packed less efficiently.
        
        // Flags (see comments in PcbFlags).
        uint32_t flags : 14;
        
        // PCB state.
        uint32_t state : TcpUtils::TcpStateBits;
        
        // Number of duplicate ACKs (>=FastRtxDupAcks means we're in fast recovery).
        uint32_t num_dupack : Constants::DupAckBits;
        
        // Window shift values.
        uint32_t snd_wnd_shift : 4;
        uint32_t rcv_wnd_shift : 4;
        
        // Convenience functions for flags.
        inline bool hasFlag (FlagsType flag) { return (flags & flag) != 0; }
        inline void setFlag (FlagsType flag) { flags |= flag; }
        inline void clearFlag (FlagsType flag) { flags &= ~flag; }
        
        // Check if a flag is set and clear it.
        inline bool hasAndClearFlag (FlagsType flag)
        {
            FlagsType the_flags = flags;
            if ((the_flags & flag) != 0) {
                flags = the_flags & ~flag;
                return true;
            }
            return false;
        }
        
        // Check if we are called from PCB input processing (pcb_input).
        inline bool inInputProcessing ()
        {
            return this == tcp->m_current_pcb;
        }
        
        // Apply delayed timer updates. This must be called after any PCB timer
        // has been changed before returning to the event loop.
        inline void doDelayedTimerUpdate ()
        {
            PcbMultiTimer::doDelayedUpdate(Context());
        }
        
        // Call doDelayedTimerUpdate if not called from input processing (pcb_input).
        // The update is not needed if called from pcb_input as it will be done at
        // return from pcb_input.
        inline void doDelayedTimerUpdateIfNeeded ()
        {
            if (!inInputProcessing()) {
                doDelayedTimerUpdate();
            }
        }
        
        // Trampolines for timer handlers.
        
        inline void timerExpired (AbrtTimer, Context)
        {
            pcb_abrt_timer_handler(this);
        }
        
        inline void timerExpired (OutputTimer, Context)
        {
            Output::pcb_output_timer_handler(this);
        }
        
        inline void timerExpired (RtxTimer, Context)
        {
            Output::pcb_rtx_timer_handler(this);
        }
        
        // Send retry callback.
        void retrySending () override final { Output::pcb_send_retry(this); }
    };
    
    // Define the hook accessor for the PCB index.
    struct PcbIndexAccessor : public APRINTER_MEMBER_ACCESSOR(&TcpPcb::index_hook) {};
    
public:
    /**
     * Initialize the TCP protocol implementation.
     * 
     * The TCP will register itself with the IpStack to receive incoming TCP packets.
     */
    void init (TheIpStack *stack)
    {
        AMBRO_ASSERT(stack != nullptr)
        
        // Remember things.
        m_stack = stack;
        
        // Initialize the list of listeners.
        m_listeners_list.init();
        
        // Clear m_current_pcb which tracks the current PCB being
        // processed by pcb_input().
        m_current_pcb = nullptr;
        
        // Set the initial counter for ephemeral ports.
        m_next_ephemeral_port = EphemeralPortFirst;
        
        // Initialize the list of unreferenced PCBs.
        m_unrefed_pcbs_list.init();
        
        // Initialize the PCB indices.
        m_pcb_index_active.init();
        m_pcb_index_timewait.init();
        
        for (TcpPcb &pcb : m_pcbs) {
            // Initialize the PCB timers.
            pcb.PcbMultiTimer::init(Context());
            
            // Initialize the send-retry Request object.
            pcb.IpSendRetry::Request::init();
            
            // Initialize some PCB variables.
            pcb.tcp = this;
            pcb.state = TcpState::CLOSED;
            pcb.con = nullptr;
            
            // Add the PCB to the list of unreferenced PCBs.
            m_unrefed_pcbs_list.prepend({pcb, *this}, *this);
        }
    }
    
    /**
     * Deinitialize the TCP protocol implementation.
     * 
     * Any TCP listeners and connections must have been deinited.
     * It is not permitted to call this from any TCP callbacks.
     */
    void deinit ()
    {
        AMBRO_ASSERT(m_listeners_list.isEmpty())
        AMBRO_ASSERT(m_current_pcb == nullptr)
        
        for (TcpPcb &pcb : m_pcbs) {
            AMBRO_ASSERT(pcb.state != TcpState::SYN_RCVD)
            AMBRO_ASSERT(pcb.con == nullptr)
            pcb.IpSendRetry::Request::deinit();
            pcb.PcbMultiTimer::deinit(Context());
        }
    }
    
    inline void recvIp4Dgram (Ip4DgramMeta const &ip_meta, IpBufRef dgram)
    {
        Input::recvIp4Dgram(this, ip_meta, dgram);
    }
    
    inline void handleIp4DestUnreach (Ip4DestUnreachMeta const &du_meta,
                Ip4DgramMeta const &ip_meta, IpBufRef const &dgram_initial)
    {
        Input::handleIp4DestUnreach(this, du_meta, ip_meta, dgram_initial);
    }
    
private:
    TcpPcb * allocate_pcb ()
    {
        // No PCB available?
        if (m_unrefed_pcbs_list.isEmpty()) {
            return nullptr;
        }
        
        // Get a PCB to use.
        TcpPcb *pcb = m_unrefed_pcbs_list.lastNotEmpty(*this);
        AMBRO_ASSERT(pcb_is_in_unreferenced_list(pcb))
        
        // Abort the PCB if it's not closed.
        if (pcb->state != TcpState::CLOSED) {
            pcb_abort(pcb);
        } else {
            pcb_assert_closed(pcb);
        }
        
        return pcb;
    }
    
    void pcb_assert_closed (TcpPcb *pcb)
    {
        AMBRO_ASSERT(!pcb->tim(AbrtTimer()).isSet(Context()))
        AMBRO_ASSERT(!pcb->tim(OutputTimer()).isSet(Context()))
        AMBRO_ASSERT(!pcb->tim(RtxTimer()).isSet(Context()))
        AMBRO_ASSERT(!pcb->IpSendRetry::Request::isActive())
        AMBRO_ASSERT(pcb->tcp == this)
        AMBRO_ASSERT(pcb->state == TcpState::CLOSED)
        AMBRO_ASSERT(pcb->con == nullptr)
    }
    
    inline static void pcb_abort (TcpPcb *pcb)
    {
        // This function aborts a PCB while sending an RST in
        // all states except these.
        bool send_rst = pcb->state !=
            OneOf(TcpState::SYN_SENT, TcpState::SYN_RCVD, TcpState::TIME_WAIT);
        
        pcb_abort(pcb, send_rst);
    }
    
    static void pcb_abort (TcpPcb *pcb, bool send_rst)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        IpTcpProto *tcp = pcb->tcp;
        
        // Send RST if desired.
        if (send_rst) {
            Output::pcb_send_rst(pcb);
        }
        
        if (pcb->state == TcpState::SYN_RCVD) {
            // Disassociate the TcpListener.
            pcb_unlink_lis(pcb);
        } else {
            // Disassociate any TcpConnection. This will call the
            // connectionAborted callback if we do have a TcpConnection.
            pcb_unlink_con(pcb, true);
        }
        
        // If this is called from input processing of this PCB,
        // clear m_current_pcb. This way, input processing can
        // detect aborts performed from within user callbacks.
        if (tcp->m_current_pcb == pcb) {
            tcp->m_current_pcb = nullptr;
        }
        
        // Remove the PCB from the index in which it is.
        if (pcb->state == TcpState::TIME_WAIT) {
            tcp->m_pcb_index_timewait.removeEntry(*tcp, {*pcb, *tcp});
        } else {
            tcp->m_pcb_index_active.removeEntry(*tcp, {*pcb, *tcp});
        }
        
        // Make sure the PCB is at the end of the unreferenced list.
        if (pcb != tcp->m_unrefed_pcbs_list.lastNotEmpty(*tcp)) {
            tcp->m_unrefed_pcbs_list.remove({*pcb, *tcp}, *tcp);
            tcp->m_unrefed_pcbs_list.append({*pcb, *tcp}, *tcp);
        }
        
        // Reset other relevant fields to initial state.
        pcb->PcbMultiTimer::unsetAll(Context());
        pcb->IpSendRetry::Request::reset();
        pcb->state = TcpState::CLOSED;
        
        tcp->pcb_assert_closed(pcb);
    }
    
    // NOTE: doDelayedTimerUpdate must be called after return.
    // We are okay because this is only called from pcb_input.
    static void pcb_go_to_time_wait (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_RCVD,
                                         TcpState::TIME_WAIT))
        
        // Disassociate any TcpConnection. This will call the
        // connectionAborted callback if we do have a TcpConnection.
        pcb_unlink_con(pcb, false);
        
        // Set snd_nxt to snd_una in order to not accept any more acknowledgements.
        // This is currently not necessary since we only enter TIME_WAIT after
        // having received a FIN, but in the future we might do some non-standard
        // transitions where this is not the case.
        pcb->snd_nxt = pcb->snd_una;
        
        // Change state.
        pcb->state = TcpState::TIME_WAIT;
        
        // Move the PCB from the active index to the time-wait index.
        IpTcpProto *tcp = pcb->tcp;
        tcp->m_pcb_index_active.removeEntry(*tcp, {*pcb, *tcp});
        tcp->m_pcb_index_timewait.addEntry(*tcp, {*pcb, *tcp});
        
        // Stop timers due to asserts in their handlers.
        pcb->tim(OutputTimer()).unset(Context());
        pcb->tim(RtxTimer()).unset(Context());
        
        // Clear the OUT_PENDING flag due to its preconditions.
        pcb->clearFlag(PcbFlags::OUT_PENDING);
        
        // Start the TIME_WAIT timeout.
        pcb->tim(AbrtTimer()).appendAfter(Context(), Constants::TimeWaitTimeTicks);
    }
    
    // NOTE: doDelayedTimerUpdate must be called after return.
    // We are okay because this is only called from pcb_input.
    static void pcb_go_to_fin_wait_2 (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state == TcpState::FIN_WAIT_1)
        
        // Change state.
        pcb->state = TcpState::FIN_WAIT_2;
        
        // Stop these timers due to asserts in their handlers.
        pcb->tim(OutputTimer()).unset(Context());
        pcb->tim(RtxTimer()).unset(Context());
        
        // Clear the OUT_PENDING flag due to its preconditions.
        pcb->clearFlag(PcbFlags::OUT_PENDING);
        
        // Reset the MTU reference.
        if (pcb->con != nullptr) {
            pcb->con->MtuRef::reset(pcb->tcp->m_stack);
        }
    }
    
    static void pcb_unlink_con (TcpPcb *pcb, bool closing)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_RCVD))
        
        if (pcb->con != nullptr) {
            // Inform the connection object about the aborting.
            // Note that the PCB is not yet on the list of unreferenced
            // PCBs, which protects it from being aborted by allocate_pcb
            // during this callback.
            TcpConnection *con = pcb->con;
            AMBRO_ASSERT(con->m_v.pcb == pcb)
            con->pcb_aborted();
            
            // The pcb->con has been cleared by con->pcb_aborted().
            AMBRO_ASSERT(pcb->con == nullptr)
            
            // Add the PCB to the list of unreferenced PCBs.
            IpTcpProto *tcp = pcb->tcp;
            if (closing) {
                tcp->m_unrefed_pcbs_list.append({*pcb, *tcp}, *tcp);
            } else {
                tcp->m_unrefed_pcbs_list.prepend({*pcb, *tcp}, *tcp);
            }
        }
    }
    
    static void pcb_unlink_lis (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state == TcpState::SYN_RCVD)
        AMBRO_ASSERT(pcb->lis != nullptr)
        
        TcpListener *lis = pcb->lis;
        
        // Decrement the listener's PCB count.
        AMBRO_ASSERT(lis->m_num_pcbs > 0)
        lis->m_num_pcbs--;
        
        // Is this a PCB which is being accepted?
        if (lis->m_accept_pcb == pcb) {
            // Break the link from the listener.
            lis->m_accept_pcb = nullptr;
            
            // The PCB was removed from the list of unreferenced
            // PCBs, so we have to add it back.
            IpTcpProto *tcp = pcb->tcp;
            tcp->m_unrefed_pcbs_list.append({*pcb, *tcp}, *tcp);
        }
        
        // Clear pcb->con since we will be going to CLOSED state
        // and it is was not undefined due to the union with pcb->lis.
        pcb->con = nullptr;
    }
    
    // This is called from TcpConnection::reset when the TcpConnection
    // is abandoning the PCB.
    static void pcb_abandoned (TcpPcb *pcb, bool snd_buf_nonempty, SeqType rcv_ann_thres)
    {
        AMBRO_ASSERT(pcb->state == TcpState::SYN_SENT || state_is_active(pcb->state))
        AMBRO_ASSERT(pcb->con == nullptr) // TcpConnection just cleared it
        IpTcpProto *tcp = pcb->tcp;
        
        // Add the PCB to the unreferenced PCBs list.
        // This has not been done by TcpConnection.
        tcp->m_unrefed_pcbs_list.append({*pcb, *tcp}, *tcp);
        
        // Clear any RTT_PENDING flag since we've lost the variables
        // needed for RTT measurement.
        pcb->clearFlag(PcbFlags::RTT_PENDING);
        
        // Clear RCV_WND_UPD flag since this flag must imply con != nullptr.
        pcb->clearFlag(PcbFlags::RCV_WND_UPD);
        
        // Abort if in SYN_SENT state or some data is queued.
        // The pcb_abort() will decide whether to send an RST
        // (no RST in SYN_SENT, RST otherwise).
        if (pcb->state == TcpState::SYN_SENT || snd_buf_nonempty) {
            return pcb_abort(pcb);
        }
        
        // Make sure any idle timeout is stopped, because pcb_rtx_timer_handler
        // requires the connection to not be abandoned when the idle timeout expires.
        if (pcb->hasFlag(PcbFlags::IDLE_TIMER)) {
            pcb->clearFlag(PcbFlags::IDLE_TIMER);
            pcb->tim(RtxTimer()).unset(Context());
        }
        
        // Arrange for sending the FIN.
        if (snd_open_in_state(pcb->state)) {
            Output::pcb_end_sending(pcb);
        }
        
        // If we haven't received a FIN, possibly announce more window
        // to encourage the peer to send its outstanding data/FIN.
        if (accepting_data_in_state(pcb->state)) {
            Input::pcb_update_rcv_wnd_after_abandoned(pcb, rcv_ann_thres);
        }
        
        // Start the abort timeout.
        pcb->tim(AbrtTimer()).appendAfter(Context(), Constants::AbandonedTimeoutTicks);
        
        pcb->doDelayedTimerUpdateIfNeeded();
    }
    
    static void pcb_abrt_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        // Abort the PCB.
        pcb_abort(pcb);
        
        // NOTE: A MultiTimer callback would normally need to call doDelayedTimerUpdate
        // before returning to the event loop but pcb_abort calls PcbMultiTimer::unsetAll
        // which is also sufficient.
    }
    
    // This is used to check within pcb_input if the PCB was aborted
    // while performing a user callback.
    inline static bool pcb_aborted_in_callback (TcpPcb *pcb)
    {
        // It is safe to read pcb->tcp since PCBs cannot just go away
        // while in input processing. If the PCB was aborted or even
        // reused, the tcp pointer must still be valid.
        IpTcpProto *tcp = pcb->tcp;
        AMBRO_ASSERT(tcp->m_current_pcb == pcb || tcp->m_current_pcb == nullptr)
        return tcp->m_current_pcb == nullptr;
    }
    
    static inline SeqType make_iss ()
    {
        return Clock::getTime(Context());
    }
    
    TcpListener * find_listener (Ip4Addr addr, PortType port)
    {
        for (TcpListener *lis = m_listeners_list.first();
             lis != nullptr; lis = m_listeners_list.next(lis))
        {
            AMBRO_ASSERT(lis->m_listening)
            if (lis->m_addr == addr && lis->m_port == port) {
                return lis;
            }
        }
        return nullptr;
    }
    
    void unlink_listener (TcpListener *lis)
    {
        // Abort any PCBs associated with the listener (without RST).
        for (TcpPcb &pcb : m_pcbs) {
            if (pcb.state == TcpState::SYN_RCVD && pcb.lis == lis) {
                pcb_abort(&pcb, false);
            }
        }
    }
    
    IpErr create_connection (TcpConnection *con, Ip4Addr remote_addr, PortType remote_port,
                             size_t user_rcv_wnd, uint16_t pmtu, TcpPcb **out_pcb)
    {
        AMBRO_ASSERT(con != nullptr)
        AMBRO_ASSERT(con->MtuRef::isSetup())
        AMBRO_ASSERT(out_pcb != nullptr)
        
        // Determine the local interface.
        Iface *iface;
        Ip4Addr route_addr;
        if (!m_stack->routeIp4(remote_addr, &iface, &route_addr)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Determine the local IP address.
        IpIfaceIp4AddrSetting addr_setting = iface->getIp4Addr();
        if (!addr_setting.present) {
            return IpErr::NO_IP_ROUTE;
        }
        Ip4Addr local_addr = addr_setting.addr;
        
        // Determine the local port.
        PortType local_port = get_ephemeral_port(local_addr, remote_addr, remote_port);
        if (local_port == 0) {
            return IpErr::NO_PORT_AVAIL;
        }
        
        // Calculate the MSS based on the interface MTU.
        uint16_t iface_mss = iface->getMtu() - Ip4TcpHeaderSize;
        
        // Allocate the PCB.
        TcpPcb *pcb = allocate_pcb();
        if (pcb == nullptr) {
            return IpErr::NO_PCB_AVAIL;
        }
        
        // NOTE: If another error case is added after this, make sure
        // to reset the MtuRef before abandoning the PCB!
        
        // Remove the PCB from the unreferenced PCBs list.
        m_unrefed_pcbs_list.remove({*pcb, *this}, *this);
        
        // Generate an initial sequence number.
        SeqType iss = make_iss();
        
        // The initial receive window will be at least one for the SYN and
        // at most 16-bit wide since SYN segments have unscaled window.
        // NOTE: rcv_ann_wnd after SYN-ACKSYN reception (-1) fits into size_t
        // as required since user_rcv_wnd is size_t.
        SeqType rcv_wnd = 1 + APrinter::MinValueU((uint16_t)(UINT16_MAX - 1), user_rcv_wnd);
        
        // Initialize most of the PCB.
        pcb->state = TcpState::SYN_SENT;
        pcb->flags = PcbFlags::WND_SCALE; // to send the window scale option
        pcb->con = con;
        pcb->local_addr = local_addr;
        pcb->remote_addr = remote_addr;
        pcb->local_port = local_port;
        pcb->remote_port = remote_port;
        pcb->rcv_nxt = 0; // it is sent in the SYN
        pcb->rcv_ann_wnd = rcv_wnd;
        pcb->snd_una = iss;
        pcb->snd_nxt = iss;
        pcb->snd_mss = pmtu; // store PMTU here temporarily
        pcb->base_snd_mss = iface_mss; // will be updated when the SYN-ACK is received
        pcb->rto = Constants::InitialRtxTime;
        pcb->num_dupack = 0;
        pcb->snd_wnd_shift = 0;
        pcb->rcv_wnd_shift = Constants::RcvWndShift;
        
        // Add the PCB to the active index.
        m_pcb_index_active.addEntry(*this, {*pcb, *this});
        
        // Start the connection timeout.
        pcb->tim(AbrtTimer()).appendAfter(Context(), Constants::SynSentTimeoutTicks);
        
        // Start the retransmission timer.
        pcb->tim(RtxTimer()).appendAfter(Context(), Output::pcb_rto_time(pcb));
        
        pcb->doDelayedTimerUpdate();
        
        // Send the SYN.
        Output::pcb_send_syn(pcb);
        
        // Return the PCB.
        *out_pcb = pcb;
        return IpErr::SUCCESS;
    }
    
    PortType get_ephemeral_port (Ip4Addr local_addr,
                                 Ip4Addr remote_addr, PortType remote_port)
    {
        for (PortType i : APrinter::LoopRangeAuto(NumEphemeralPorts)) {
            PortType port = m_next_ephemeral_port;
            m_next_ephemeral_port = (port < EphemeralPortLast) ?
                (port + 1) : EphemeralPortFirst;
            
            if (find_pcb({local_addr, remote_addr, port, remote_port}) == nullptr) {
                return port;
            }
        }
        
        return 0;
    }
    
    inline static bool pcb_is_in_unreferenced_list (TcpPcb *pcb)
    {
        return pcb->state == TcpState::SYN_RCVD ? pcb->lis->m_accept_pcb != pcb
                                                : pcb->con == nullptr;
    }
    
    void move_unrefed_pcb_to_front (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb_is_in_unreferenced_list(pcb))
        
        if (pcb != m_unrefed_pcbs_list.first(*this)) {
            m_unrefed_pcbs_list.remove({*pcb, *this}, *this);
            m_unrefed_pcbs_list.prepend({*pcb, *this}, *this);
        }
    }
    
    // Find a PCB by address tuple.
    TcpPcb * find_pcb (PcbKey const &key)
    {
        // Look in the active index first.
        TcpPcb *pcb = m_pcb_index_active.findEntry(*this, key);
        AMBRO_ASSERT(pcb == nullptr ||
                     pcb->state != OneOf(TcpState::CLOSED, TcpState::TIME_WAIT))
        
        // If not found, look in the time-wait index.
        if (AMBRO_UNLIKELY(pcb == nullptr)) {
            pcb = m_pcb_index_timewait.findEntry(*this, key);
            AMBRO_ASSERT(pcb == nullptr || pcb->state == TcpState::TIME_WAIT)
        }
        
        return pcb;
    }
    
    // Find a listener by local address and port. This also considers listeners bound
    // to wildcard address since it is used to associate received segments with a listener.
    TcpListener * find_listener_for_rx (Ip4Addr local_addr, PortType local_port)
    {
        for (TcpListener *lis = m_listeners_list.first();
             lis != nullptr; lis = m_listeners_list.next(lis))
        {
            AMBRO_ASSERT(lis->m_listening)
            if (lis->m_port == local_port &&
                (lis->m_addr == local_addr || lis->m_addr == Ip4Addr::ZeroAddr()))
            {
                return lis;
            }
        }
        return nullptr;
    }
    
    // This is used by the two PCB indexes to obtain the keys
    // defining the ordering of the PCBs and compare keys.
    // The key comparison functions are inherited from PcbKeyCompare.
    struct PcbIndexKeyFuncs : public PcbKeyCompare {
        inline static PcbKey const & GetKeyOfEntry (TcpPcb const &pcb)
        {
            // TcpPcb inherits PcbKey so just return pcb.
            return pcb;
        }
    };
    
    // Define the link model for data structures of PCBs.
    struct PcbArrayAccessor;
    struct PcbLinkModel : public APrinter::If<LinkWithArrayIndices,
        APrinter::ArrayLinkModelWithAccessor<
            TcpPcb, PcbIndexType, PcbIndexNull, IpTcpProto, PcbArrayAccessor>,
        APrinter::PointerLinkModel<TcpPcb>
    > {};
    APRINTER_USE_TYPES1(PcbLinkModel, (Ref, State))
    
private:
    using ListenersList = APrinter::DoubleEndedList<
        TcpListener, &TcpListener::m_listeners_node, false>;
    
    using UnrefedPcbsList = APrinter::LinkedList<
        APRINTER_MEMBER_ACCESSOR_TN(&TcpPcb::unrefed_list_node), PcbLinkModel, true>;
    
    TheIpStack *m_stack;
    ListenersList m_listeners_list;
    TcpPcb *m_current_pcb;
    IpBufRef m_received_opts_buf;
    TcpOptions m_received_opts;
    PortType m_next_ephemeral_port;
    UnrefedPcbsList m_unrefed_pcbs_list;
    typename PcbIndex::Index m_pcb_index_active;
    typename PcbIndex::Index m_pcb_index_timewait;
    TcpPcb m_pcbs[NumTcpPcbs];
    
    struct PcbArrayAccessor : public APRINTER_MEMBER_ACCESSOR(&IpTcpProto::m_pcbs) {};
};

APRINTER_ALIAS_STRUCT_EXT(IpTcpProtoService, (
    APRINTER_AS_VALUE(uint8_t, TcpTTL),
    APRINTER_AS_VALUE(int, NumTcpPcbs),
    APRINTER_AS_VALUE(uint8_t, NumOosSegs),
    APRINTER_AS_VALUE(uint16_t, EphemeralPortFirst),
    APRINTER_AS_VALUE(uint16_t, EphemeralPortLast),
    APRINTER_AS_TYPE(PcbIndexService),
    APRINTER_AS_VALUE(bool, LinkWithArrayIndices)
), (
    // This tells IpStack which IP protocol we receive packets for.
    using IpProtocolNumber = APrinter::WrapValue<uint8_t, Ip4ProtocolTcp>;
    
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(TheIpStack)
    ), (
        using Params = IpTcpProtoService;
        APRINTER_DEF_INSTANCE(Compose, IpTcpProto)
    ))
))

#include <aipstack/EndNamespace.h>

#endif
