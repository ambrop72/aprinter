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
#include <aprinter/base/Assert.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/ipstack/misc/Buf.h>
#include <aprinter/ipstack/misc/SendRetry.h>
#include <aprinter/ipstack/proto/IpAddr.h>
#include <aprinter/ipstack/proto/Ip4Proto.h>
#include <aprinter/ipstack/proto/Tcp4Proto.h>
#include <aprinter/ipstack/proto/TcpUtils.h>
#include <aprinter/ipstack/ip/IpStack.h>

#include "IpTcpProto_api.h"
#include "IpTcpProto_input.h"
#include "IpTcpProto_output.h"

#include <aprinter/BeginNamespace.h>

/**
 * TCP protocol implementation.
 */
template <typename Arg>
class IpTcpProto :
    private Arg::TheIpStack::ProtoListenerCallback
{
    APRINTER_USE_VALS(Arg::Params, (TcpTTL, NumTcpPcbs, NumOosSegs,
                                    EphemeralPortFirst, EphemeralPortLast))
    APRINTER_USE_TYPES1(Arg, (Context, BufAllocator, TheIpStack))
    
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    APRINTER_USE_TYPE1(Context::EventLoop, TimedEvent)
    
    APRINTER_USE_TYPES1(TheIpStack, (Ip4DgramMeta, ProtoListener, Iface))
    
    static_assert(NumTcpPcbs > 0, "");
    static_assert(NumOosSegs > 0, "");
    static_assert(EphemeralPortFirst > 0, "");
    static_assert(EphemeralPortFirst <= EphemeralPortLast, "");
    
    template <typename> friend class IpTcpProto_api;
    template <typename> friend class IpTcpProto_input;
    template <typename> friend class IpTcpProto_output;
    
public:
    APRINTER_USE_TYPES2(TcpUtils, (SeqType, PortType))
    
    static SeqType const MaxRcvWnd = UINT32_C(0x3fffffff);
    
private:
    using Api = IpTcpProto_api<IpTcpProto>;
    using Input = IpTcpProto_input<IpTcpProto>;
    using Output = IpTcpProto_output<IpTcpProto>;
    
    APRINTER_USE_TYPES2(TcpUtils, (TcpState))
    APRINTER_USE_VALS(TcpUtils, (state_is_active, accepting_data_in_state,
                                 can_output_in_state, snd_open_in_state,
                                 seq_diff, seq_add))
    
    struct TcpPcb;
    
    // PCB flags, see flags in TcpPcb.
    using FlagsType = uint16_t;
    struct PcbFlags { enum : FlagsType {
        ACK_PENDING = 1 << 0, // ACK is needed; used in input processing
        OUT_PENDING = 1 << 1, // pcb_output is needed; used in input processing
        FIN_SENT    = 1 << 2, // A FIN has been sent, and is included in snd_nxt
        FIN_PENDING = 1 << 3, // A FIN is to be transmitted
        PROTECT     = 1 << 4, // Prevent allocate_pcb from taking this PCB
        RTT_PENDING = 1 << 5, // Round-trip-time is being measured
        RTT_VALID   = 1 << 6, // Round-trip-time is not in initial state
        OOSEQ_FIN   = 1 << 7, // Out-of-sequence FIN has been received
        CWND_INCRD  = 1 << 8, // cwnd has been increaded by snd_mss this round-trip
        RTX_ACTIVE  = 1 << 9, // A segment has been retransmitted and not yet acked
        RECOVER     = 1 << 10, // The recover variable valid (and >=snd_una)
        IDLE_TIMER  = 1 << 11, // If rtx_timer is running it is for idle timeout
        WND_SCALE   = 1 << 12, // Window scaling is used
    }; };
    
    // For retransmission time calculations we right-shift the Clock time
    // to obtain granularity between 1ms and 2ms.
    static int const RttShift = BitsInFloat(1e-3 / Clock::time_unit);
    static_assert(RttShift >= 0, "");
    static constexpr double RttTimeFreq = Clock::time_freq / PowerOfTwoFunc<double>(RttShift);
    
    // We store such scaled times in 16-bit variables.
    // This gives us a range of at least 65 seconds.
    using RttType = uint16_t;
    static RttType const RttTypeMax = (RttType)-1;
    static constexpr double RttTypeMaxDbl = RttTypeMax;
    
    // For intermediate RTT results we need a larger type.
    using RttNextType = uint32_t;
    
    // Number of ephemeral ports.
    static PortType const NumEphemeralPorts = EphemeralPortLast - EphemeralPortFirst + 1;
    
    // Represents a segment of contiguous out-of-sequence data.
    struct OosSeg {
        SeqType start;
        SeqType end;
    };
    
public:
    APRINTER_USE_TYPES1(Api, (TcpConnection, TcpConnectionCallback,
                              TcpListener, TcpListenerCallback))
    
private:
    /**
     * A TCP Protocol Control Block.
     * These are maintained internally within the stack and may
     * survive deinit/reset of an associated TcpConnection object.
     */
    struct TcpPcb : public IpSendRetry::Callback {
        // Timers.
        TimedEvent abrt_timer;   // timer for aborting PCB (TIME_WAIT, abandonment)
        TimedEvent output_timer; // timer for pcb_output after send buffer extension
        TimedEvent rtx_timer;    // timer for retransmission, window probe and cwnd idle reset
        
        // Send retry request.
        IpSendRetry::Request send_retry_request;
        
        // Basic stuff.
        IpTcpProto *tcp;    // pointer back to IpTcpProto
        TcpConnection *con; // pointer to any associated TcpConnection
        TcpListener *lis;   // pointer to any associated TcpListener
        TimeType last_time; // time when the last valid segment was received
        
        // Addresses and ports.
        Ip4Addr local_addr;
        Ip4Addr remote_addr;
        PortType local_port;
        PortType remote_port;
        
        // Sender variables.
        SeqType snd_una;
        SeqType snd_nxt;
        SeqType snd_wnd;
        SeqType snd_wl1;
        SeqType snd_wl2;
        IpBufRef snd_buf_cur;
        size_t snd_psh_index;
        
        // Congestion control variables.
        SeqType cwnd;
        SeqType ssthresh;
        SeqType cwnd_acked;
        SeqType recover;
        
        // Receiver variables.
        SeqType rcv_nxt;
        SeqType rcv_wnd;
        SeqType rcv_ann;
        SeqType rcv_ann_thres;
        
        // Out-of-sequence segment information.
        OosSeg ooseq[NumOosSegs];
        SeqType ooseq_fin;
        
        // Round-trip-time and retransmission time management.
        SeqType rtt_test_seq;
        TimeType rtt_test_time;
        RttType rttvar;
        RttType srtt;
        RttType rto;
        
        // MSSes
        uint16_t snd_mss; // NOTE: If updating this, consider invalidation of pcb_need_rtx_timer!
        uint16_t rcv_mss;
        
        // PCB state.
        TcpState state;
        
        // Flags (see comments in PcbFlags).
        FlagsType flags;
        
        // Number of valid elements in ooseq_segs;
        uint8_t num_ooseq;
        
        // Number of duplicate ACKs (>=FastRtxDupAcks means we're in fast recovery).
        uint8_t num_dupack;
        
        // Window shift values.
        uint8_t snd_wnd_shift;
        uint8_t rcv_wnd_shift;
        
        // Convenience functions for flags.
        inline bool hasFlag (FlagsType flag) { return (flags & flag) != 0; }
        inline void setFlag (FlagsType flag) { flags |= flag; }
        inline void clearFlag (FlagsType flag) { flags &= ~flag; }
        
        // Convenience functions for buffer length.
        inline size_t sndBufLen () { return (con != nullptr) ? con->m_snd_buf.tot_len : 0; }
        inline size_t rcvBufLen () { return (con != nullptr) ? con->m_rcv_buf.tot_len : 0; }
        
        // Trampolines for timer handlers.
        void abrt_timer_handler (Context) { pcb_abrt_timer_handler(this); }
        void output_timer_handler (Context) { Output::pcb_output_timer_handler(this); }
        void rtx_timer_handler (Context) { Output::pcb_rtx_timer_handler(this); }
        
        // Send retry callback.
        void retrySending () override final { Output::pcb_send_retry(this); }
    };
    
    // Default threshold for sending a window update (overridable by setWindowUpdateThreshold).
    static SeqType const DefaultWndAnnThreshold = 2700;
    
    // How old at most an ACK may be to be considered acceptable (MAX.SND.WND in RFC 5961).
    static SeqType const MaxAckBefore = UINT32_C(0xFFFF);
    
    // Don't allow the remote host to lower the MSS beyond this.
    static uint16_t const MinAllowedMss = 128;
    
    // SYN_RCVD state timeout.
    static TimeType const SynRcvdTimeoutTicks     = 20.0  * Clock::time_freq;
    
    // SYN_SENT state timeout.
    static TimeType const SynSentTimeoutTicks     = 30.0  * Clock::time_freq;
    
    // TIME_WAIT state timeout.
    static TimeType const TimeWaitTimeTicks       = 120.0 * Clock::time_freq;
    
    // Timeout to abort connection after it has been abandoned.
    static TimeType const AbandonedTimeoutTicks   = 30.0  * Clock::time_freq;
    
    // Time after the send buffer is extended to calling pcb_output.
    static TimeType const OutputTimerTicks        = 0.0005 * Clock::time_freq;
    
    // Initial retransmission time, before any round-trip-time measurement.
    static RttType const InitialRtxTime           = 1.0 * RttTimeFreq;
    
    // Minimum retransmission time.
    static RttType const MinRtxTime               = 0.25 * RttTimeFreq;
    
    // Maximum retransmission time (need care not to overflow RttType).
    static RttType const MaxRtxTime = MinValue(RttTypeMaxDbl, 60.0 * RttTimeFreq);
    
    // Number of duplicate ACKs to trigger fast retransmit/recovery.
    static uint8_t const FastRtxDupAcks = 3;
    
    // Maximum number of additional duplicate ACKs that will result in CWND increase.
    static uint8_t const MaxAdditionaDupAcks = 32;
    
    // Window scale shift count to send and use in outgoing ACKs.
    static uint8_t const RcvWndShift = 6;
    
public:
    /**
     * Initialize the TCP protocol implementation.
     * 
     * The TCP will register itself with the IpStack to receive incoming TCP packets.
     */
    void init (TheIpStack *stack)
    {
        AMBRO_ASSERT(stack != nullptr)
        
        m_stack = stack;
        m_proto_listener.init(m_stack, Ip4ProtocolTcp, this);
        m_listeners_list.init();
        m_current_pcb = nullptr;
        m_next_ephemeral_port = EphemeralPortFirst;
        
        for (TcpPcb &pcb : m_pcbs) {
            pcb.abrt_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&TcpPcb::abrt_timer_handler, &pcb));
            pcb.output_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&TcpPcb::output_timer_handler, &pcb));
            pcb.rtx_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&TcpPcb::rtx_timer_handler, &pcb));
            pcb.send_retry_request.init(&pcb);
            pcb.tcp = this;
            pcb.con = nullptr;
            pcb.lis = nullptr;
            pcb.state = TcpState::CLOSED;
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
            AMBRO_ASSERT(pcb.con == nullptr)
            pcb.send_retry_request.deinit();
            pcb.rtx_timer.deinit(Context());
            pcb.output_timer.deinit(Context());
            pcb.abrt_timer.deinit(Context());
        }
        
        m_proto_listener.deinit();
    }
    
private:
    void recvIp4Dgram (Ip4DgramMeta const &ip_meta, IpBufRef dgram) override
    {
        Input::recvIp4Dgram(this, ip_meta, dgram);
    }
    
    TcpPcb * allocate_pcb ()
    {
        TimeType now = Clock::getTime(Context());
        TcpPcb *the_pcb = nullptr;
        
        // Find a PCB to use, either a CLOSED one (preferably) or one which
        // has no associated TcpConnection. For the latter case use the least
        // recently used such PCB.
        for (TcpPcb &pcb : m_pcbs) {
            if (pcb.hasFlag(PcbFlags::PROTECT)) {
                // Don't take protected PCB.
                continue;
            }
            
            if (pcb.state == TcpState::CLOSED) {
                the_pcb = &pcb;
                break;
            }
            
            if (pcb.con == nullptr) {
                if (the_pcb == nullptr ||
                    (TimeType)(now - pcb.last_time) > (TimeType)(now - the_pcb->last_time))
                {
                    the_pcb = &pcb;
                }
            }
        }
        
        // No PCB available?
        if (the_pcb == nullptr) {
            return nullptr;
        }
        
        // Abort the PCB if it's not closed.
        if (the_pcb->state != TcpState::CLOSED) {
            pcb_abort(the_pcb);
        }
        
        // Set the last-time, since we already have the time here.
        the_pcb->last_time = now;
        
        AMBRO_ASSERT(!the_pcb->abrt_timer.isSet(Context()))
        AMBRO_ASSERT(!the_pcb->output_timer.isSet(Context()))
        AMBRO_ASSERT(!the_pcb->rtx_timer.isSet(Context()))
        AMBRO_ASSERT(the_pcb->tcp == this)
        AMBRO_ASSERT(the_pcb->con == nullptr)
        AMBRO_ASSERT(the_pcb->lis == nullptr)
        AMBRO_ASSERT(the_pcb->state == TcpState::CLOSED)
        
        return the_pcb;
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
        
        // Send RST if desired.
        if (send_rst) {
            Output::pcb_send_rst(pcb);
        }
        
        // Disassociate any TcpConnection. This will call the
        // connectionAborted callback if we do have a TcpConnection.
        pcb_unlink_con(pcb);
        
        // Disassociate any TcpListener.
        pcb_unlink_lis(pcb);
        
        // If this is called from input processing of this PCB,
        // clear m_current_pcb. This way, input processing can
        // detect aborts performed from within user callbacks.
        if (pcb->tcp->m_current_pcb == pcb) {
            pcb->tcp->m_current_pcb = nullptr;
        }
        
        // Reset other relevant fields to initial state.
        pcb->abrt_timer.unset(Context());
        pcb->output_timer.unset(Context());
        pcb->rtx_timer.unset(Context());
        pcb->send_retry_request.reset();
        pcb->state = TcpState::CLOSED;
    }
    
    static void pcb_go_to_time_wait (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_RCVD))
        
        // Disassociate any TcpConnection. This will call the
        // connectionAborted callback if we do have a TcpConnection.
        pcb_unlink_con(pcb);
        
        // Disassociate any TcpListener.
        pcb_unlink_lis(pcb);
        
        // Set snd_nxt to snd_una in order to not accept any more acknowledgements.
        // This is currently not necessary since we only enter TIME_WAIT after
        // having received a FIN, but in the future we might do some non-standard
        // transitions where this is not the case.
        pcb->snd_nxt = pcb->snd_una;
        
        // Change state.
        pcb->state = TcpState::TIME_WAIT;
        
        // Stop these timers due to asserts in their handlers.
        pcb->output_timer.unset(Context());
        pcb->rtx_timer.unset(Context());
        
        // Start the TIME_WAIT timeout.
        pcb->abrt_timer.appendAfter(Context(), TimeWaitTimeTicks);
    }
    
    static void pcb_unlink_con (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        AMBRO_ASSERT(!pcb->hasFlag(PcbFlags::PROTECT))
        
        if (pcb->con != nullptr) {
            // Use the protect flag to prevent allocate_pcb from taking the PCB.
            pcb->setFlag(PcbFlags::PROTECT);
            
            // Inform the connection object about the aborting.
            TcpConnection *con = pcb->con;
            AMBRO_ASSERT(con->m_pcb == pcb)
            con->pcb_aborted();
            
            AMBRO_ASSERT(pcb->con == nullptr)
            pcb->clearFlag(PcbFlags::PROTECT);
        }
    }
    
    static void pcb_unlink_lis (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        if (pcb->lis != nullptr) {
            TcpListener *lis = pcb->lis;
            
            // Decrement the listener's PCB count.
            AMBRO_ASSERT(lis->m_num_pcbs > 0)
            lis->m_num_pcbs--;
            
            // Remove a possible accept link.
            if (lis->m_accept_pcb == pcb) {
                lis->m_accept_pcb = nullptr;
            }
            
            pcb->lis = nullptr;
        }
    }
    
    static void pcb_con_abandoned (TcpPcb *pcb, bool snd_buf_nonempty)
    {
        AMBRO_ASSERT(pcb->state == TcpState::SYN_SENT || state_is_active(pcb->state))
        AMBRO_ASSERT(pcb->con == nullptr) // TcpConnection haa just disassociated itself
        AMBRO_ASSERT(snd_buf_nonempty || pcb->snd_buf_cur.tot_len == 0)
        
        // Abort if in SYN_SENT state or some data is queued.
        // The pcb_abort() will decide whether to send an RST
        // (no RST in SYN_SENT, RST otherwise).
        if (pcb->state == TcpState::SYN_SENT || snd_buf_nonempty) {
            return pcb_abort(pcb);
        }
        
        // Disassociate any TcpListener.
        // We don't want abandoned connections to contributing to the
        // listener's PCB count and prevent new connections.
        pcb_unlink_lis(pcb);
        
        // Arrange for sending the FIN.
        if (snd_open_in_state(pcb->state)) {
            Output::pcb_end_sending(pcb);
        }
        
        // If we haven't received a FIN, ensure that at least rcv_mss
        // window is advertised.
        if (accepting_data_in_state(pcb->state)) {
            if (pcb->rcv_wnd < pcb->rcv_mss) {
                pcb->rcv_wnd = pcb->rcv_mss;
            }
            if (seq_diff(pcb->rcv_ann, pcb->rcv_nxt) < pcb->rcv_mss) {
                Output::pcb_need_ack(pcb);
            }
        }
        
        // Start the abort timeout.
        pcb->abrt_timer.appendAfter(Context(), AbandonedTimeoutTicks);
    }
    
    static void pcb_abrt_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        pcb_abort(pcb);
    }
    
    // This is used from input processing to call one of the TcpConnection
    // private functions then check whether the PCB is still alive.
    template <typename Action>
    inline static bool pcb_event (TcpPcb *pcb, Action action)
    {
        AMBRO_ASSERT(pcb->tcp->m_current_pcb == pcb)
        AMBRO_ASSERT(pcb->con == nullptr || pcb->con->m_pcb == pcb)
        
        if (pcb->con == nullptr) {
            return true;
        }
        
        IpTcpProto *tcp = pcb->tcp;
        action(pcb->con);
        
        return tcp->m_current_pcb != nullptr;
    }
    
    static uint16_t get_iface_mss (Iface *iface)
    {
        size_t mtu = iface->getIp4DgramMtu();
        size_t mss = mtu - MinValue(mtu, Tcp4Header::Size);
        return MinValueU((uint16_t)-1, mss);
    }
    
    static inline SeqType make_iss ()
    {
        return Clock::getTime(Context());
    }
    
    TcpListener * find_listener (Ip4Addr addr, PortType port)
    {
        for (TcpListener *lis = m_listeners_list.first(); lis != nullptr; lis = m_listeners_list.next(lis)) {
            AMBRO_ASSERT(lis->m_listening)
            if (lis->m_addr == addr && lis->m_port == port) {
                return lis;
            }
        }
        return nullptr;
    }
    
    void unlink_listener (TcpListener *lis)
    {
        // Disassociate any PCBs associated with this listener,
        // and also abort any such PCBs in SYN_RCVD state (without RST).
        for (TcpPcb &pcb : m_pcbs) {
            if (pcb.lis == lis) {
                pcb.lis = nullptr;
                if (pcb.state == TcpState::SYN_RCVD) {
                    pcb_abort(&pcb, false);
                }
            }
        }
    }
    
    IpErr create_connection (TcpConnection *con, Ip4Addr remote_addr, PortType remote_port,
                             size_t user_rcv_wnd, TcpPcb *out_pcb)
    {
        AMBRO_ASSERT(con != nullptr)
        AMBRO_ASSERT(out_pcb != nullptr)
        
        // Determine the local IP address.
        Iface *iface;
        Ip4Addr local_addr;
        if (!m_stack->routeIp4(remote_addr, nullptr, &iface, &local_addr)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Determine the local port.
        PortType local_port = get_ephemeral_port(local_addr, remote_addr, remote_port);
        if (local_port == 0) {
            return IpErr::NO_PORT_AVAIL;
        }
        
        // Calculate the initial MSS.
        uint16_t iface_mss = get_iface_mss(iface);
        if (iface_mss < MinAllowedMss) {
            return IpErr::NO_HEADER_SPACE;
        }
        
        // Allocate the PCB.
        TcpPcb *pcb = allocate_pcb();
        if (pcb == nullptr) {
            return IpErr::NO_PCB_AVAIL;
        }
        
        // Generate an initial sequence number.
        SeqType iss = make_iss();
        
        // The initial receive window will be at least one
        // because we must be prepared to accept the SYN.
        SeqType rcv_wnd = 1 + MinValueU(seq_diff(MaxRcvWnd, 1), user_rcv_wnd);
        
        // Initialize most of the PCB.
        pcb->state = TcpState::SYN_SENT;
        pcb->flags = PcbFlags::WND_SCALE; // to send the window scale option
        pcb->con = con;
        pcb->local_addr = local_addr;
        pcb->remote_addr = remote_addr;
        pcb->local_port = local_port;
        pcb->remote_port = remote_port;
        pcb->rcv_nxt = 0; // it is sent in the SYN
        pcb->rcv_wnd = rcv_wnd;
        pcb->rcv_ann = pcb->rcv_nxt;
        pcb->rcv_ann_thres = DefaultWndAnnThreshold;
        pcb->rcv_mss = iface_mss;
        pcb->snd_una = iss;
        // TODO: Don't bump snd_nxt here.
        pcb->snd_nxt = seq_add(iss, 1);
        pcb->snd_buf_cur = IpBufRef{};
        pcb->snd_psh_index = 0;
        pcb->snd_mss = iface_mss;
        pcb->rto = InitialRtxTime;
        pcb->num_ooseq = 0;
        pcb->num_dupack = 0;
        pcb->snd_wnd_shift = 0;
        pcb->rcv_wnd_shift = RcvWndShift;
        
        // Start the connection timeout.
        pcb->abrt_timer.appendAfter(Context(), SynSentTimeoutTicks);
        
        // Start the retransmission timer.
        pcb->rtx_timer.appendAfter(Context(), Output::pcb_rto_time(pcb));
        
        // Send the SYN.
        pcb_send_syn(pcb, true);
        
        // Return the PCB.
        *out_pcb = pcb;
        return IpErr::SUCCESS;
    }
    
    PortType get_ephemeral_port (Ip4Addr local_addr, Ip4Addr remote_addr, PortType remote_port)
    {
        for (PortType i : LoopRangeAuto(NumEphemeralPorts)) {
            PortType port = m_next_ephemeral_port;
            m_next_ephemeral_port = (port < EphemeralPortLast) ? (port + 1) : EphemeralPortFirst;
            
            if (find_pcb_by_addr(local_addr, port, remote_addr, remote_port) == nullptr) {
                return port;
            }
        }
        
        return 0;
    }
    
    TcpPcb * find_pcb_by_addr (Ip4Addr local_addr, PortType local_port,
                               Ip4Addr remote_addr, PortType remote_port)
    {
        for (TcpPcb &pcb : m_pcbs) {
            if (pcb.state != TcpState::CLOSED  &&
                pcb.local_addr  == local_addr  &&
                pcb.remote_addr == remote_addr &&
                pcb.local_port  == local_port  &&
                pcb.remote_port == remote_port)
            {
                return &pcb;
            }
        }
        return nullptr;
    }
    
private:
    using ListenersList = DoubleEndedList<TcpListener, &TcpListener::m_listeners_node, false>;
    
    TheIpStack *m_stack;
    ProtoListener m_proto_listener;
    ListenersList m_listeners_list;
    TcpPcb *m_current_pcb;
    PortType m_next_ephemeral_port;
    TcpPcb m_pcbs[NumTcpPcbs];
};

APRINTER_ALIAS_STRUCT_EXT(IpTcpProtoService, (
    APRINTER_AS_VALUE(uint8_t, TcpTTL),
    APRINTER_AS_VALUE(int, NumTcpPcbs),
    APRINTER_AS_VALUE(uint8_t, NumOosSegs),
    APRINTER_AS_VALUE(uint16_t, EphemeralPortFirst),
    APRINTER_AS_VALUE(uint16_t, EphemeralPortLast)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(BufAllocator),
        APRINTER_AS_TYPE(TheIpStack)
    ), (
        using Params = IpTcpProtoService;
        APRINTER_DEF_INSTANCE(Compose, IpTcpProto)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
