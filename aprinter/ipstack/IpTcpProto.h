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
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/ipstack/Buf.h>
#include <aprinter/ipstack/IpAddr.h>
#include <aprinter/ipstack/IpStack.h>
#include <aprinter/ipstack/proto/Ip4Proto.h>
#include <aprinter/ipstack/proto/Tcp4Proto.h>
#include <aprinter/ipstack/proto/TcpUtils.h>

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
    APRINTER_USE_VALS(Arg::Params, (TcpTTL, NumTcpPcbs, NumOosSegs))
    APRINTER_USE_TYPES1(Arg, (Context, BufAllocator, TheIpStack))
    
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    APRINTER_USE_TYPE1(Context::EventLoop, TimedEvent)
    
    APRINTER_USE_TYPES1(TheIpStack, (Ip4DgramMeta, ProtoListener, Iface))
    
    static_assert(NumTcpPcbs > 0, "");
    static_assert(NumOosSegs > 0, "");
    
public:
    class TcpListener;
    class TcpConnection;
    
    APRINTER_USE_TYPES2(TcpUtils, (SeqType, PortType))
    
private:
    APRINTER_USE_TYPES2(TcpUtils, (TcpState))
    APRINTER_USE_VALS(TcpUtils, (state_is_active, accepting_data_in_state,
                                 can_output_in_state, snd_open_in_state))
    
    // PCB flags, see flags in TcpPcb.
    struct PcbFlags { enum : uint8_t {
        ACK_PENDING = 1 << 0, // ACK is needed; used in input processing
        OUT_PENDING = 1 << 1, // pcb_output is needed; used in input processing
        FIN_SENT    = 1 << 2, // A FIN has been sent, and is included in snd_nxt
        FIN_PENDING = 1 << 3, // A FIN is to be transmitted
        ABORTING    = 1 << 4, // The connectionAborted callback is being called
        RTT_PENDING = 1 << 5, // Round-trip-time is being measured
        RTT_VALID   = 1 << 6, // Round-trip-time is not in initial state
        OOSEQ_FIN   = 1 << 7, // Out-of-sequence FIN has been received
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
    
    struct OosSeg {
        SeqType start;
        SeqType end;
    };
    
    /**
     * A TCP Protocol Control Block.
     * These are maintained internally within the stack and may
     * survive deinit/reset of an associated TcpConnection object.
     */
    struct TcpPcb {
        // Timers.
        TimedEvent abrt_timer;   // timer for aborting PCB (TIME_WAIT, abandonment)
        TimedEvent output_timer; // timer for pcb_output after send buffer extension
        TimedEvent rtx_timer;    // timer for retransmissions and zero-window probes
        
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
        IpBufRef snd_buf;
        IpBufRef snd_buf_cur;
        size_t snd_psh_index;
        
        // Receiver variables.
        SeqType rcv_nxt;
        SeqType rcv_wnd;
        SeqType rcv_ann;
        SeqType rcv_ann_thres;
        IpBufRef rcv_buf;
        
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
        uint8_t flags;
        
        // Number of valid elements in ooseq_segs;
        uint8_t num_ooseq;
        
        // Convenience functions.
        inline bool hasFlag (uint8_t flag) { return (flags & flag) != 0; }
        inline void setFlag (uint8_t flag) { flags |= flag; }
        inline void clearFlag (uint8_t flag) { flags &= ~flag; }
        
        // Trampolines for timer handlers.
        void abrt_timer_handler (Context) { tcp->pcb_abrt_timer_handler(this); }
        void output_timer_handler (Context) { Output::pcb_output_timer_handler(this); }
        void rtx_timer_handler (Context) { Output::pcb_rtx_timer_handler(this); }
    };
    
private:
    // Default threshold for sending a window update (overridable by setWindowUpdateThreshold).
    static SeqType const DefaultWndAnnThreshold = 2700;
    
    // How old at most an ACK may be to be considered acceptable (MAX.SND.WND in RFC 5961).
    static SeqType const MaxAckBefore = UINT32_C(0xFFFF);
    
    // Don't allow the remote host to lower the MSS beyond this.
    static uint16_t const MinAllowedMss = 128;
    
    // SYN_RCVD state timeout.
    static TimeType const SynRcvdTimeoutTicks     = 20.0  * Clock::time_freq;
    
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
    
private:
    template <typename> friend class IpTcpProto_input;
    template <typename> friend class IpTcpProto_output;
    
    using Input = IpTcpProto_input<IpTcpProto>;
    using Output = IpTcpProto_output<IpTcpProto>;
    
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
        
        for (TcpPcb &pcb : m_pcbs) {
            pcb.abrt_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&TcpPcb::abrt_timer_handler, &pcb));
            pcb.output_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&TcpPcb::output_timer_handler, &pcb));
            pcb.rtx_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&TcpPcb::rtx_timer_handler, &pcb));
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
            pcb.rtx_timer.deinit(Context());
            pcb.output_timer.deinit(Context());
            pcb.abrt_timer.deinit(Context());
        }
        
        m_proto_listener.deinit();
    }
    
public:
    /**
     * Maximum allowed receive window.
     * 
     * The user must not set/extend the connection receive window beyond this.
     */
    static SeqType const MaxRcvWnd = UINT32_C(0x3fffffff);
    
    /**
     * Callback interface for TcpListener.
     */
    class TcpListenerCallback {
    public:
        /**
         * Called when a new connection has been established.
         * 
         * To accept the connection, the user should call TcpConnection::acceptConnection.
         * Note that there are no special restrictions regarding accessing the connection
         * from within this callback. It is also permissible to deinit/reset the listener.
         * 
         * If the connection is not accepted by acceptConnection within this callback,
         * it will be aborted.
         */
        virtual void connectionEstablished (TcpListener *lis) = 0;
    };
    
    /**
     * Represents listening for connections on a specific address and port.
     */
    class TcpListener {
        friend IpTcpProto;
        template <typename> friend class IpTcpProto_input;
        
    public:
        /**
         * Initialize the listener.
         * 
         * A callback listener must be provided, which is used to inform of
         * newly accepted connections. Upon init, the listener is in not-listening
         * state, and listenIp4 should be called to start listening.
         */
        void init (IpTcpProto *tcp, TcpListenerCallback *callback)
        {
            AMBRO_ASSERT(tcp != nullptr)
            AMBRO_ASSERT(callback != nullptr)
            
            m_tcp = tcp;
            m_callback = callback;
            m_initial_rcv_wnd = 0;
            m_accept_pcb = nullptr;
            m_listening = false;
        }
        
        /**
         * Deinitialize the listener.
         * 
         * All SYN_RCVD connections associated with this listener will be aborted
         * but any already established connection (those associated with a
         * TcpConnection object) will not be affected.
         */
        void deinit ()
        {
            reset();
        }
        
        /**
         * Reset the listener, bringing it to a non-listening state.
         * 
         * This is similar to deinit except that the listener remains initialzied
         * in a default non-listening state.
         */
        void reset ()
        {
            AMBRO_ASSERT(m_accept_pcb == nullptr)
            
            if (m_listening) {
                // Set not-listening, remove from listeners list.
                m_listening = false;
                m_tcp->m_listeners_list.remove(this);
                
                // Disassociate any PCBs associated with this listener,
                // and also abort any such PCBs in SYN_RCVD state (without RST).
                for (TcpPcb &pcb : m_tcp->m_pcbs) {
                    if (pcb.lis == this) {
                        pcb.lis = nullptr;
                        if (pcb.state == TcpState::SYN_RCVD) {
                            pcb_abort(&pcb, false);
                        }
                    }
                }
            }
            
            m_initial_rcv_wnd = 0;
        }
        
        /**
         * Return whether we are listening.
         */
        bool isListening ()
        {
            return m_listening;
        }
        
        /**
         * Return whether a connection is ready to be accepted.
         */
        bool hasAcceptPending ()
        {
            return m_accept_pcb != nullptr;
        }
        
        /**
         * Listen on an IPv4 address and port.
         * 
         * Listening on the all-zeros address listens on all local addresses.
         * Must not be called when already listening.
         * Return success/failure to start listening. It can fail only if there
         * is another listener listening on the same pair of address and port.
         */
        bool listenIp4 (Ip4Addr addr, PortType port, int max_pcbs)
        {
            AMBRO_ASSERT(!m_listening)
            AMBRO_ASSERT(max_pcbs > 0)
            
            // Check if there is an existing listener listning on this address+port.
            if (m_tcp->find_listener(addr, port) != nullptr) {
                return false;
            }
            
            // Set up listening.
            m_addr = addr;
            m_port = port;
            m_max_pcbs = max_pcbs;
            m_num_pcbs = 0;
            m_listening = true;
            m_tcp->m_listeners_list.prepend(this);
            
            return true;
        }
        
        /**
         * Set the initial receive window used for connections to this listener.
         * 
         * The default initial receive window is 0, which means that a newly accepted
         * connection will not receive data before the user extends he window using
         * extendReceiveWindow.
         * 
         * Note that the initial receive window is applied to a new connection when
         * the SYN is received, not when the connectionEstablished callback is called.
         * Hence the user should generaly use getReceiveWindow to determine the actual
         * receive window of a new connection.
         */
        void setInitialReceiveWindow (SeqType rcv_wnd)
        {
            AMBRO_ASSERT(rcv_wnd <= MaxRcvWnd)
            
            m_initial_rcv_wnd = rcv_wnd;
        }
        
    private:
        DoubleEndedListNode<TcpListener> m_listeners_node;
        IpTcpProto *m_tcp;
        TcpListenerCallback *m_callback;
        SeqType m_initial_rcv_wnd;
        TcpPcb *m_accept_pcb;
        Ip4Addr m_addr;
        PortType m_port;
        int m_max_pcbs;
        int m_num_pcbs;
        bool m_listening;
    };
    
    /**
     * Callback interface for TcpConnection.
     */
    class TcpConnectionCallback {
    public:
        /**
         * Called when the connection is aborted abnormally.
         * 
         * A connectionAborted callback implies a transition of the connection
         * object from connected to not-connected state.
         */
        virtual void connectionAborted () = 0;
        
        /**
         * Called when some data has been received.
         * 
         * Each dataReceived callback implies an automatic decrement of
         * the receive window by the amount of received data. The user should
         * generally call extendReceiveWindow after having processed the
         * received data to maintain reception.
         */
        virtual void dataReceived (size_t amount) = 0;
        
        /**
         * Called when the end of data has been received.
         * 
         * After this, there will be no further dataReceived or endReceived
         * callbacks.
         * 
         * Note that connection object may just have entered not-connected
         * state, if endSent has already been called.
         */
        virtual void endReceived () = 0;
        
        /**
         * Called when some data has been sent and acknowledged.
         */
        virtual void dataSent (size_t amount) = 0;
        
        /**
         * Called when the end of data has been sent and acknowledged.
         * 
         * Note that connection object may just have entered not-connected
         * state, if endReceived has already been called.
         */
        virtual void endSent () = 0;
    };
    
    /**
     * Represents a TCP connection.
     */
    class TcpConnection {
        friend IpTcpProto;
        
    public:
        /**
         * Initializes the connection object to a default not-connected state.
         * 
         * A callback interface must be provided which is used to inform the
         * user of various events related to the connection.
         */
        void init (IpTcpProto *tcp, TcpConnectionCallback *callback)
        {
            AMBRO_ASSERT(tcp != nullptr)
            AMBRO_ASSERT(callback != nullptr)
            
            m_tcp = tcp;
            m_callback = callback;
            m_pcb = nullptr;
        }
        
        /**
         * Deinitialzies the connection object, abandoning any associated connection.
         * 
         * This is permitted in any (initialized) state but the effects depend on
         * the states of the connection, and are not defined in detail here. In any
         * case, no more callbacks will be done.
         */
        void deinit ()
        {
            reset();
        }
        
        /**
         * Reset the connection object to its default not-connected state.
         * 
         * This is similar to deinit except that the connection object remains valid.
         */
        void reset ()
        {
            if (m_pcb != nullptr) {
                TcpPcb *pcb = m_pcb;
                AMBRO_ASSERT(pcb->con == this)
                
                // Disassociate with the PCB.
                pcb->con = nullptr;
                m_pcb = nullptr;
                
                // Handle abandonment of connection.
                m_tcp->pcb_con_abandoned(pcb);
            }
        }
        
        /**
         * Accepts a new connection from a listener into this connection object.
         * 
         * May only be called outside of the connectionEstablished callback of a
         * listener, and at most once.
         * May be called in not-connected state only.
         * 
         * After this, the connection object enters connected state. It remains in
         * connected state until the first of the following:
         * 1) The connectionAborted callback has been called.
         * 2) Both endReceived and endSent callbacks have been called.
         * 
         * Upon one of the above, the connection object automatically transitions
         * back to not-connected state. The transition happens after the callback
         * returns, which notably allows calling getSendBuf from within the callback.
         */
        void acceptConnection (TcpListener *lis)
        {
            AMBRO_ASSERT(m_pcb == nullptr)
            AMBRO_ASSERT(lis->m_accept_pcb != nullptr)
            AMBRO_ASSERT(lis->m_accept_pcb->lis == lis)
            AMBRO_ASSERT(lis->m_accept_pcb->con == nullptr)
            AMBRO_ASSERT(lis->m_accept_pcb->state == TcpState::ESTABLISHED)
            
            // Associate with the PCB.
            m_pcb = lis->m_accept_pcb;
            m_pcb->con = this;
            
            // Clear the PCB from the listener so pcb_input knows about the accept.
            lis->m_accept_pcb = nullptr;
        }
        
        /**
         * Move a connection from another TcpConnection object to this one.
         * 
         * This connection object must be in not-connected state.
         * The src_con must be in connected state.
         */
        void moveConnection (TcpConnection *src_con)
        {
            AMBRO_ASSERT(m_pcb == nullptr)
            AMBRO_ASSERT(src_con->m_pcb != nullptr)
            
            // Move the PCB association.
            m_pcb = src_con->m_pcb;
            m_pcb->con = this;
            src_con->m_pcb = nullptr;
        }
        
        /**
         * Returns whether the connection object is in connected state.
         */
        bool hasConnection ()
        {
            return m_pcb != nullptr;
        }
        
        /**
         * Set the threshold for sending window updates.
         * 
         * rcv_ann_thres must be positive and not exceed MaxRcvWnd.
         * May be called in connected state only.
         */
        void setWindowUpdateThreshold (SeqType rcv_ann_thres)
        {
            assert_pcb();
            AMBRO_ASSERT(rcv_ann_thres > 0)
            AMBRO_ASSERT(rcv_ann_thres <= MaxRcvWnd)
            
            // Set the threshold.
            m_pcb->rcv_ann_thres = rcv_ann_thres;
        }
        
        /**
         * Get the current receive window.
         * 
         * This is the number of data bytes that the connection is prepared to accept.
         * The connection starts with an initial receive window as per configuration
         * of the listener. The receive window is then managed entirely by the user
         * by extending it using extendReceiveWindow.
         * 
         * May be called in connected state only.
         */
        SeqType getReceiveWindow ()
        {
            assert_pcb();
            
            return m_pcb->rcv_wnd;
        }
        
        void setRecvBuf (IpBufRef rcv_buf)
        {
            assert_pcb();
            AMBRO_ASSERT(m_pcb->rcv_buf.tot_len == 0)
            AMBRO_ASSERT(rcv_buf.tot_len >= m_pcb->rcv_wnd)
            AMBRO_ASSERT(rcv_buf.tot_len <= MaxRcvWnd)
            
            m_pcb->rcv_buf = rcv_buf;
            m_pcb->rcv_wnd = rcv_buf.tot_len;
            
            if (rcv_buf.tot_len > 0) {
                Input::pcb_rcv_buf_extended(m_pcb);
            }
        }
        
        void extendRecvBuf (size_t amount)
        {
            assert_pcb();
            AMBRO_ASSERT(m_pcb->rcv_wnd == m_pcb->rcv_buf.tot_len)
            AMBRO_ASSERT(amount <= m_pcb->rcv_wnd - MaxRcvWnd)
            
            m_pcb->rcv_buf.tot_len += amount;
            m_pcb->rcv_wnd = m_pcb->rcv_buf.tot_len;
            
            if (amount > 0) {
                Input::pcb_rcv_buf_extended(m_pcb);
            }
        }
        
        IpBufRef getRecvBuf ()
        {
            assert_pcb();
            
            return m_pcb->rcv_buf;
        }
        
        /**
         * Returns the amount of send buffer space that may be unsent
         * indefinitely in the absence of sendPush.
         * 
         * The reason for this is that the TCP may delay transmission
         * until a full segment may be sent, to improve efficiency.
         * The value returned may change but it will never exceed the
         * initial value when a connection is established.
         * 
         * May be called in connected state only.
         */
        size_t getSndBufOverhead ()
        {
            assert_pcb();
            
            // Sending can be delayed for segmentation only when we have
            // less than the MSS data left to send, hence return mss-1.
            return m_pcb->snd_mss - 1;
        }
        
        /**
         * Set the send buffer for the connection.
         * 
         * This is only allowed when the send buffer of the connection is empty,
         * which is the initial state. In order to queue more data for sending
         * after some data is already queueued, the user should extend the pointed-to
         * IpBufNode structures then call extendSendBuf. To send data, it is only
         * really necessary to call setSendBuf once, since calling extendSendBuf is
         * still possible after all queued data has been sent.
         * 
         * The user is responsible for ensuring that any pointed-to IpBufRef and
         * data buffers remain valid until the associated that is reported as sent
         * by the dataSent callback. The exact meaning of this is not described here,
         * but essentially you must assume as if the TCP uses IpBufRef::skipBytes
         * to advance over sent data.
         * 
         * DANGER: Currently skipBytes is conservative in advancing to the next
         * buffer in the chain as it would only advance when it needs more data
         * not already when the current buffer is exhausted. So the TCP may
         * possibly still reference an IpBufNode even when all its data has been
         * reported as sent. You can only be sure that an IpBufNode is no longer
         * referenced after the connection reports having send more than the data
         * in that buffer.
         * 
         * May be called in connected state only.
         * Must not be called after endSending.
         */
        void setSendBuf (IpBufRef snd_buf)
        {
            assert_pcb_sending();
            AMBRO_ASSERT(m_pcb->snd_buf.tot_len == 0)
            AMBRO_ASSERT(m_pcb->snd_buf_cur.tot_len == 0) // implied
            
            // Set the send buffer references.
            m_pcb->snd_buf = snd_buf;
            m_pcb->snd_buf_cur = snd_buf;
            
            // Handle send buffer extension.
            if (snd_buf.tot_len > 0) {
                Output::pcb_snd_buf_extended(m_pcb);
            }
        }
        
        /**
         * Extend the send buffer for the connection.
         * 
         * This simply makes the TCP assume there is that much more data ready
         * for sending. See setSendBuf for more information about sending data.
         * 
         * May be called in connected state only.
         * Must not be called after endSending.
         */
        void extendSendBuf (size_t amount)
        {
            assert_pcb_sending();
            AMBRO_ASSERT(amount <= SIZE_MAX - m_pcb->snd_buf.tot_len)
            
            // Increment the amount of data in the send buffer.
            m_pcb->snd_buf.tot_len += amount;
            m_pcb->snd_buf_cur.tot_len += amount;
            
            // Handle send buffer extension.
            if (amount > 0) {
                Output::pcb_snd_buf_extended(m_pcb);
            }
        }
        
        /**
         * Get the current send buffer reference.
         * 
         * This references data which has been queued but not yet acknowledged.
         * 
         * May be called in connected state only.
         */
        IpBufRef getSendBuf ()
        {
            assert_pcb();
            
            return m_pcb->snd_buf;
        }
        
        /**
         * Report the end of the data stream.
         * 
         * After this is called, no further calls of setSendBuf, extendSendBuf
         * and endSending must be made.
         * Successful sending and acknowledgement of the end of data is reported
         * through the endSent callback. The endSent callback is only called
         * after all queued data has been reported sent using dataSent callbacks.
         * 
         * May be called in connected state only.
         * Must not be called after endSending.
         */
        void endSending ()
        {
            assert_pcb_sending();
            
            // Handle in private function.
            Output::pcb_end_sending(m_tcp, m_pcb);
        }
        
        /**
         * Push sending of data.
         * 
         * This will result in a PSH flag and also will prevent delay of sending
         * the current queued data due to waiting until a full segment can be sent.
         * 
         * May be called in connected state only.
         */
        void sendPush ()
        {
            assert_pcb();
            
            // Note that we ignore the request after endSending was called.
            // Because we pushed in pcb_end_sending, retransmissions of the
            // FIN will occur internally as needed.
            if (snd_open_in_state(m_pcb->state)) {
                Output::pcb_push_output(m_tcp, m_pcb);
            }
        }
        
    private:
        void assert_pcb ()
        {
            AMBRO_ASSERT(m_pcb != nullptr)
            AMBRO_ASSERT(m_pcb->con == this)
            AMBRO_ASSERT(state_is_active(m_pcb->state))
        }
        
        void assert_pcb_sending ()
        {
            assert_pcb();
            AMBRO_ASSERT(snd_open_in_state(m_pcb->state))
        }
        
    private:
        IpTcpProto *m_tcp;
        TcpConnectionCallback *m_callback;
        TcpPcb *m_pcb;
    };
    
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
            if (pcb.hasFlag(PcbFlags::ABORTING)) {
                // Ignore PCB being aborted in pcb_abort.
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
        bool send_rst = pcb->state != OneOf(TcpState::SYN_RCVD, TcpState::TIME_WAIT);
        pcb_abort(pcb, send_rst);
    }
    
    static void pcb_abort (TcpPcb *pcb, bool send_rst)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        AMBRO_ASSERT(!pcb->hasFlag(PcbFlags::ABORTING))
        
        // If there is an associated TcpConnection, call the connectionAborted
        // callback. During set the flag ABORTING durring the callback so we
        // can tell if we're being called back from here.
        if (pcb->con != nullptr) {
            pcb->setFlag(PcbFlags::ABORTING);
            pcb->con->m_callback->connectionAborted();
            pcb->clearFlag(PcbFlags::ABORTING);
        }
        
        // Send RST if desired.
        if (send_rst) {
            Output::pcb_send_rst(this, pcb);
        }
        
        // Disassociate any TcpConnection.
        pcb_unlink_con(pcb);
        
        // Disassociate any TcpListener.
        pcb_unlink_lis(pcb);
        
        // If this is called from input processing of this PCB,
        // clear m_current_pcb. This way, input processing can
        // detect aborts performed from within user callbacks.
        if (pcb->tcp->m_current_pcb == pcb) {
            pcb->tcp->m_current_pcb = nullptr;
        }
        
        // Reset PCB to initial state.
        pcb->abrt_timer.unset(Context());
        pcb->output_timer.unset(Context());
        pcb->rtx_timer.unset(Context());
        AMBRO_ASSERT(pcb->tcp == this)
        AMBRO_ASSERT(pcb->con == nullptr)
        AMBRO_ASSERT(pcb->lis == nullptr)
        pcb->state = TcpState::CLOSED;
    }
    
    static void pcb_go_to_time_wait (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_RCVD))
        
        // If there way any TcpConnection, disassociate it.
        // Note that there is no need for an aborted callbacks because the
        // user already knows that the connection is closed because they
        // have received both end-sent and end-received callbacks.
        pcb_unlink_con(pcb);
        
        // Disassociate any TcpListener.
        // We don't want abandoned connections to contributing to the
        // listener's PCB count and prevent new connections.
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
        
        if (pcb->con != nullptr) {
            TcpConnection *con = pcb->con;
            AMBRO_ASSERT(con->m_pcb == pcb)
            
            // Disassociate the TcpConnection and the PCB.
            con->m_pcb = nullptr;
            pcb->con = nullptr;
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
    
    void pcb_con_abandoned (TcpPcb *pcb)
    {
        AMBRO_ASSERT(state_is_active(pcb->state))
        AMBRO_ASSERT(pcb->con == nullptr)
        
        // Ignore this if it we're in the connectionAborted callback.
        if (pcb->hasFlag(PcbFlags::ABORTING)) {
            return;
        }
        
        // Disassociate any TcpListener.
        // We don't want abandoned connections to contributing to the
        // listener's PCB count and prevent new connections.
        pcb_unlink_lis(pcb);
        
        // Has a FIN not yet been sent and acknowledged?
        if (can_output_in_state(pcb->state)) {
            // If not all data has been sent we have to abort because we
            // may no longer reference the remaining data; send RST.
            if (pcb->snd_buf.tot_len > 0) {
                return pcb_abort(pcb, true);
            }
            
            // Assume end of data from user.
            if (snd_open_in_state(pcb->state)) {
                Output::pcb_end_sending(this, pcb);
            }
        }
        
        // If we haven't yet received a FIN, ensure we have nonzero window.
        // Also send a window update if we haven't advertised being able to
        // receive anything more.
        if (accepting_data_in_state(pcb->state)) {
            if (pcb->rcv_wnd == 0) {
                pcb->rcv_wnd++;
            }
            if (pcb->rcv_ann == pcb->rcv_nxt) {
                Output::pcb_need_ack(this, pcb);
            }
        }
        
        // Start the abort timeout.
        pcb->abrt_timer.appendAfter(Context(), AbandonedTimeoutTicks);
    }
    
    void pcb_abrt_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        pcb_abort(pcb);
    }
    
    // Call a TcpConnection callback, if there is any TcpConnection associated.
    // Returns false if the PCB was aborted, else true. This must be used from
    // pcb_input only as it relies on m_current_pcb to detect an abort.
    template <typename Callback>
    static bool pcb_callback (TcpPcb *pcb, Callback callback)
    {
        AMBRO_ASSERT(pcb->tcp->m_current_pcb == pcb)
        
        if (pcb->con != nullptr) {
            AMBRO_ASSERT(pcb->con->m_pcb == pcb)
            
            IpTcpProto *tcp = pcb->tcp;
            callback(pcb->con->m_callback);
            
            if (AMBRO_UNLIKELY(tcp->m_current_pcb == nullptr)) {
                return false;
            }
        }
        
        return true;
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
    
private:
    using ListenersList = DoubleEndedList<TcpListener, &TcpListener::m_listeners_node, false>;
    
    TheIpStack *m_stack;
    ProtoListener m_proto_listener;
    ListenersList m_listeners_list;
    TcpPcb *m_current_pcb;
    TcpPcb m_pcbs[NumTcpPcbs];
};

APRINTER_ALIAS_STRUCT_EXT(IpTcpProtoService, (
    APRINTER_AS_VALUE(uint8_t, TcpTTL),
    APRINTER_AS_VALUE(int, NumTcpPcbs),
    APRINTER_AS_VALUE(uint8_t, NumOosSegs)
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
