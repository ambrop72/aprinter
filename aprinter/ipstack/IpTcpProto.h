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

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/ipstack/Buf.h>
#include <aprinter/ipstack/IpAddr.h>
#include <aprinter/ipstack/IpStack.h>
#include <aprinter/ipstack/Chksum.h>
#include <aprinter/ipstack/proto/Ip4Proto.h>
#include <aprinter/ipstack/proto/Tcp4Proto.h>
#include <aprinter/ipstack/proto/TcpUtils.h>

#include <aprinter/BeginNamespace.h>

/**
 * TCP protocol implementation.
 */
template <typename Arg>
class IpTcpProto :
    private Arg::TheIpStack::ProtoListenerCallback
{
    static uint8_t const TcpTTL     = Arg::Params::TcpTTL;
    static int const NumTcpPcbs     = Arg::Params::NumTcpPcbs;
    
    using Context      = typename Arg::Context;
    using BufAllocator = typename Arg::BufAllocator;
    using TheIpStack   = typename Arg::TheIpStack;
    
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TimedEvent = typename Context::EventLoop::TimedEvent;
    
    using Ip4DgramMeta  = typename TheIpStack::Ip4DgramMeta;
    using ProtoListener = typename TheIpStack::ProtoListener;
    using Iface         = typename TheIpStack::Iface;
    
    static size_t const HeaderBeforeIp4Dgram = TheIpStack::HeaderBeforeIp4Dgram;
    
public:
    class TcpListener;
    class TcpConnection;
    
    APRINTER_USE_T2(TcpUtils, SeqType)
    using PortType = uint16_t;
    
private:
    APRINTER_USE_T2(TcpUtils, FlagsType)
    APRINTER_USE_T2(TcpUtils, TcpState)
    
    APRINTER_USE_V(TcpUtils, seq_add)
    APRINTER_USE_V(TcpUtils, seq_diff)
    APRINTER_USE_V(TcpUtils, seq_lte)
    APRINTER_USE_V(TcpUtils, seq_lt)
    
    // PCB flags, see flags in TcpPcb.
    struct PcbFlags { enum : uint8_t {
        ACK_PENDING = 1 << 0,
        OUT_PENDING = 1 << 1,
        FIN_SENT    = 1 << 2,
        FIN_PENDING = 1 << 3,
        ZERO_WINDOW = 1 << 4,
        ABORTING    = 1 << 5,
    }; };
    
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
        
        // MSSes
        uint16_t snd_mss;
        uint16_t rcv_mss;
        
        // PCB state.
        TcpState state;
        
        // Flags:
        // ACK_PENDING - ACK is needed; used in input processing
        // OUT_PENDING - pcb_output is needed; used in input processing
        // FIN_SENT    - A FIN has been sent, and is included in snd_nxt
        // FIN_PENDING - A FIN is to be transmitted
        // ZERO_WINDOW - The rtx_timer has been set due to zero window
        // ABORTING    - The connectionAborted callback is being called
        uint8_t flags;
        
        // Trampolines for timer handlers.
        void abrt_timer_handler (Context) { tcp->pcb_abrt_timer_handler(this); }
        void output_timer_handler (Context) { tcp->pcb_output_timer_handler(this); }
        void rtx_timer_handler (Context) { tcp->pcb_rtx_timer_handler(this); }
    };
    
    struct TcpOptions;
    
    // Container for data in the TCP header (used both in RX and TX).
    struct TcpSegMeta {
        PortType local_port;
        PortType remote_port;
        SeqType seq_num;
        SeqType ack_num;
        uint16_t window_size;
        FlagsType flags;
        TcpOptions const *opts; // may be null for TX
    };
    
    // TCP options flags used in TcpOptions options_present.
    struct OptionFlags { enum : uint8_t {
        MSS = 1 << 0,
    }; };
    
    // Container for TCP options that we care about.
    struct TcpOptions {
        uint8_t options;
        uint16_t mss;
    };
    
private:
    // Default threshold for sending a window update (overridable by setWindowUpdateThreshold).
    static SeqType const DefaultWndAnnThreshold = 2700;
    
    // How old at most an ACK may be to be considered acceptable (MAX.SND.WND in RFC 5961).
    static SeqType const MaxAckBefore = UINT32_C(0xFFFF);
    
    // Don't allow the remote host to lower the MSS beyond this.
    static uint16_t const MinAllowedMss = 128;
    
    // TODO: Retransmission time management
    // TODO: Zero-window probe time management
    
    // Retransmission time (currently stupid and hardcoded).
    static TimeType const RetransmissionTimeTicks = 0.5   * Clock::time_freq;
    
    // Zero-window probe time (currently stupid and hardcoded).
    static TimeType const ZeroWindowTimeTicks     = 0.5   * Clock::time_freq;
    
    // SYN_RCVD state timeout.
    static TimeType const SynRcvdTimeoutTicks     = 20.0  * Clock::time_freq;
    
    // TIME_WAIT state timeout.
    static TimeType const TimeWaitTimeTicks       = 120.0 * Clock::time_freq;
    
    // Timeout to abort connection after it has been abandoned.
    static TimeType const AbandonedTimeoutTicks   = 30.0  * Clock::time_freq;
    
    // Time after the send buffer is extended to calling pcb_output.
    static TimeType const OutputTimerTicks        = 0.0005 * Clock::time_freq;
    
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
                            m_tcp->pcb_abort(&pcb, false);
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
         * 
         */
        virtual void dataReceived (IpBufRef data) = 0;
        
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
        
        /**
         * Extends the receive window.
         * 
         * wnd_ext is the number of bytes to extend the window by.
         * force_wnd_update will force sending a window update to the peer.
         * The receive window must not be extended beyond MaxRcvWnd.
         * 
         * May be called in connected state only.
         */
        void extendReceiveWindow (SeqType wnd_ext, bool force_wnd_update)
        {
            assert_pcb();
            AMBRO_ASSERT(m_pcb->rcv_wnd <= MaxRcvWnd)
            AMBRO_ASSERT(wnd_ext <= MaxRcvWnd - m_pcb->rcv_wnd)
            
            // Handle in private function.
            m_tcp->pcb_extend_receive_window(m_pcb, wnd_ext, force_wnd_update);
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
                m_tcp->pcb_snd_buf_extended(m_pcb);
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
                m_tcp->pcb_snd_buf_extended(m_pcb);
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
            m_tcp->pcb_end_sending(m_pcb);
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
            if (snd_not_closed_in_state(m_pcb->state)) {
                m_tcp->pcb_push_output(m_pcb);
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
            AMBRO_ASSERT(snd_not_closed_in_state(m_pcb->state))
        }
        
    private:
        IpTcpProto *m_tcp;
        TcpConnectionCallback *m_callback;
        TcpPcb *m_pcb;
    };
    
private:
    void recvIp4Dgram (Ip4DgramMeta const &ip_meta, IpBufRef dgram) override
    {
        // The local address must be the address of the incoming interface.
        if (!ip_meta.iface->ip4AddrIsLocalAddr(ip_meta.local_addr)) {
            return;
        }
        
        // Check header size, must fit in first buffer.
        if (!dgram.hasHeader(Tcp4Header::Size)) {
            return;
        }
        
        TcpSegMeta tcp_meta;
        
        // Read header fields.
        auto tcp_header = Tcp4Header::MakeRef(dgram.getChunkPtr());
        tcp_meta.remote_port = tcp_header.get(Tcp4Header::SrcPort());
        tcp_meta.local_port  = tcp_header.get(Tcp4Header::DstPort());
        tcp_meta.seq_num     = tcp_header.get(Tcp4Header::SeqNum());
        tcp_meta.ack_num     = tcp_header.get(Tcp4Header::AckNum());
        tcp_meta.flags       = tcp_header.get(Tcp4Header::OffsetFlags());
        tcp_meta.window_size = tcp_header.get(Tcp4Header::WindowSize());
        
        // Check data offset.
        uint8_t data_offset = (tcp_meta.flags >> TcpOffsetShift) * 4;
        if (data_offset < Tcp4Header::Size || data_offset > dgram.tot_len) {
            return;
        }
        
        // Check TCP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&ip_meta.local_addr.data);
        chksum_accum.addWords(&ip_meta.remote_addr.data);
        chksum_accum.addWord(WrapType<uint16_t>(), Ip4ProtocolTcp);
        chksum_accum.addWord(WrapType<uint16_t>(), dgram.tot_len);
        chksum_accum.addIpBuf(dgram);
        if (chksum_accum.getChksum() != 0) {
            return;
        }
        
        // Get a buffer reference starting at the option data and compute
        // how many bytes of options there options are.
        IpBufRef opts_and_data = dgram.hideHeader(Tcp4Header::Size);
        uint8_t opts_len = data_offset - Tcp4Header::Size;
        
        // Parse TCP options and get a reference to the data.
        TcpOptions tcp_opts;
        IpBufRef tcp_data;
        parse_options(opts_and_data, opts_len, &tcp_opts, &tcp_data);
        tcp_meta.opts = &tcp_opts;
        
        // Try to handle using a PCB.
        for (TcpPcb &pcb : m_pcbs) {
            if (pcb_try_input(&pcb, ip_meta, tcp_meta, tcp_data)) {
                return;
            }
        }
        
        // Try to handle using a listener.
        for (TcpListener *lis = m_listeners_list.first(); lis != nullptr; lis = m_listeners_list.next(lis)) {
            AMBRO_ASSERT(lis->m_listening)
            if (lis->m_port == tcp_meta.local_port &&
                (lis->m_addr == ip_meta.local_addr || lis->m_addr == Ip4Addr::ZeroAddr()))
            {
                return listen_input(lis, ip_meta, tcp_meta, tcp_data.tot_len);
            }
        }
        
        // Reply with RST, unless this is an RST.
        if ((tcp_meta.flags & Tcp4FlagRst) == 0) {
            send_rst_reply(ip_meta, tcp_meta, tcp_data.tot_len);
        }
    }
    
private:
    void listen_input (TcpListener *lis, Ip4DgramMeta const &ip_meta,
                       TcpSegMeta const &tcp_meta, size_t tcp_data_len)
    {
        do {
            // For a new connection we expect SYN flag and no FIN, RST, ACK.
            if ((tcp_meta.flags & Tcp4BasicFlags) != Tcp4FlagSyn) {
                // If the segment has no RST and has ACK, reply with RST; otherwise drop.
                // This includes dropping SYN+FIN packets though RFC 793 does not say this
                // should be done.
                if ((tcp_meta.flags & (Tcp4FlagRst|Tcp4FlagAck)) == Tcp4FlagAck) {
                    goto refuse;
                }
                return;
            }
            
            // Check maximum number of PCBs for this listener.
            if (lis->m_num_pcbs >= lis->m_max_pcbs) {
                goto refuse;
            }
            
            // Calculate MSSes.
            uint16_t iface_mss = get_iface_mss(ip_meta.iface);
            uint16_t snd_mss;
            if (!calc_snd_mss(iface_mss, *tcp_meta.opts, &snd_mss)) {
                goto refuse;
            }
            
            // Allocate a PCB.
            TcpPcb *pcb = allocate_pcb();
            if (pcb == nullptr) {
                goto refuse;
            }
            
            // Generate an initial sequence number.
            SeqType iss = make_iss();
            
            // Initialize the PCB.
            pcb->state = TcpState::SYN_RCVD;
            pcb->flags = 0;
            pcb->lis = lis;
            pcb->local_addr = ip_meta.local_addr;
            pcb->remote_addr = ip_meta.remote_addr;
            pcb->local_port = tcp_meta.local_port;
            pcb->remote_port = tcp_meta.remote_port;
            pcb->rcv_nxt = seq_add(tcp_meta.seq_num, 1);
            pcb->rcv_wnd = lis->m_initial_rcv_wnd;
            pcb->rcv_ann = pcb->rcv_nxt;
            pcb->rcv_ann_thres = DefaultWndAnnThreshold;
            pcb->rcv_mss = iface_mss;
            pcb->snd_una = iss;
            pcb->snd_nxt = seq_add(iss, 1);
            pcb->snd_buf = IpBufRef::NullRef();
            pcb->snd_buf_cur = IpBufRef::NullRef();
            pcb->snd_psh_index = 0;
            pcb->snd_mss = snd_mss;
            
            // These will be initialized at transition to ESTABLISHED:
            //pcb->snd_wnd
            //pcb->snd_wl1
            //pcb->snd_wl2
            
            // Increment the listener's PCB count.
            AMBRO_ASSERT(lis->m_num_pcbs < INT_MAX)
            lis->m_num_pcbs++;
            
            // Start the SYN_RCVD abort timeout.
            pcb->abrt_timer.appendAfter(Context(), SynRcvdTimeoutTicks);
            
            // Reply with a SYN+ACK.
            pcb_send_syn_ack(pcb);
            return;
        } while (false);
        
    refuse:
        // Refuse connection by RST.
        send_rst_reply(ip_meta, tcp_meta, tcp_data_len);
    }
    
    inline bool pcb_try_input (TcpPcb *pcb, Ip4DgramMeta const &ip_meta,
                               TcpSegMeta const &tcp_meta, IpBufRef tcp_data)
    {
        if (pcb->state != TcpState::CLOSED &&
            pcb->local_addr  == ip_meta.local_addr &&
            pcb->remote_addr == ip_meta.remote_addr &&
            pcb->local_port  == tcp_meta.local_port &&
            pcb->remote_port == tcp_meta.remote_port)
        {
            AMBRO_ASSERT(m_current_pcb == nullptr)
            m_current_pcb = pcb;
            pcb_input_core(pcb, tcp_meta, tcp_data);
            m_current_pcb = nullptr;
            return true;
        }
        return false;
    }
    
    void pcb_input_core (TcpPcb *pcb, TcpSegMeta const &tcp_meta, IpBufRef tcp_data)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_SENT))
        AMBRO_ASSERT(m_current_pcb == pcb)
        
        // Sequence length of segment (data+flags).
        size_t len = tcplen(tcp_meta.flags, tcp_data.tot_len);
        // Right edge of receive window.
        SeqType nxt_wnd = seq_add(pcb->rcv_nxt, pcb->rcv_wnd);
        
        // Handle uncommon flags.
        if (AMBRO_UNLIKELY((tcp_meta.flags & (Tcp4FlagRst|Tcp4FlagSyn|Tcp4FlagAck)) != Tcp4FlagAck)) {
            if ((tcp_meta.flags & Tcp4FlagRst) != 0) {
                // RST, handle as per RFC 5961.
                if (tcp_meta.seq_num == pcb->rcv_nxt) {
                    pcb_abort(pcb, false);
                }
                else if (seq_lte(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt)) {
                    // We're slightly violating RFC 5961 by allowing seq_num at nxt_wnd.
                    pcb_send_empty_ack(pcb);
                }
            }
            else if ((tcp_meta.flags & Tcp4FlagSyn) != 0) {
                // SYN, handle as per RFC 5961.
                if (pcb->state == TcpState::SYN_RCVD &&
                    tcp_meta.seq_num == seq_add(pcb->rcv_nxt, -1))
                {
                    // This seems to be a retransmission of the SYN, retransmit our
                    // SYN+ACK and bump the abort timeout.
                    pcb_send_syn_ack(pcb);
                    pcb->abrt_timer.appendAfter(Context(), SynRcvdTimeoutTicks);
                } else {
                    pcb_send_empty_ack(pcb);
                }
            }
            else {
                // Segment without none of RST, SYN and ACK should never be sent.
                // Just drop it here. Note that RFC 793 would have us check the
                // sequence number and possibly send an empty ACK if the segment
                // is outside the window, but we don't do that for perfomance.
            }
            return;
        }
        
        // Determine acceptability of segment.
        bool acceptable;
        bool left_edge_in_window;
        bool right_edge_in_window;
        if (len == 0) {
            // Empty segment is acceptable if the sequence number is within or at
            // the right edge of the receive window. Allowing the latter with
            // nonzero receive window violates RFC 793, but seems to make sense.
            acceptable = seq_lte(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt);
        } else {
            // Nonzero-length segment is acceptable if its left or right edge
            // is within the receive window. Except for SYN_RCVD, we are not expecting
            // any data to the left.
            SeqType last_seq = seq_add(tcp_meta.seq_num, seq_add(len, -1));
            left_edge_in_window = seq_lt(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt);
            right_edge_in_window = seq_lt(last_seq, nxt_wnd, pcb->rcv_nxt);
            if (AMBRO_UNLIKELY(pcb->state == TcpState::SYN_RCVD)) {
                acceptable = left_edge_in_window;
            } else {
                acceptable = left_edge_in_window || right_edge_in_window;
            }
        }
        
        // If not acceptable, send any appropriate response and drop.
        if (AMBRO_UNLIKELY(!acceptable)) {
            return pcb_send_empty_ack(pcb);
        }
        
        // Trim the segment on the left or right so that it fits into the receive window.
        SeqType eff_seq = tcp_meta.seq_num;
        FlagsType eff_flags = tcp_meta.flags;
        if (AMBRO_LIKELY(len > 0)) {
            SeqType left_keep;
            if (AMBRO_UNLIKELY(!left_edge_in_window)) {
                // The segment contains some already received data (seq_num < rcv_nxt).
                SeqType left_trim = seq_diff(pcb->rcv_nxt, tcp_meta.seq_num);
                AMBRO_ASSERT(left_trim > 0)   // because !left_edge_in_window
                AMBRO_ASSERT(left_trim < len) // because right_edge_in_window
                eff_seq = pcb->rcv_nxt;
                len -= left_trim;
                // No change to eff_flags: for SYN we'd have bailed out earlier,
                // and FIN could not be trimmed because left_trim < len.
                tcp_data.skipBytes(left_trim);
            }
            else if (AMBRO_UNLIKELY(len > (left_keep = seq_diff(nxt_wnd, tcp_meta.seq_num)))) {
                // The segment contains some extra data beyond the receive window.
                AMBRO_ASSERT(left_keep > 0)   // because left_edge_in_window
                AMBRO_ASSERT(left_keep < len) // because of condition
                len = left_keep;
                eff_flags &= ~Tcp4FlagFin; // a FIN would be outside the window
                tcp_data.tot_len = left_keep;
            }
        }
        
        // If after trimming the segment does not start at the left edge of the 
        // receive window, drop it but send an ACK. The spec does not permit us
        // to further process such an out-of-sequence segment at this time.
        // In the future we may implement queuing.
        if (AMBRO_UNLIKELY(eff_seq != pcb->rcv_nxt)) {
            return pcb_send_empty_ack(pcb);
        }
        
        // Check ACK validity as per RFC 5961.
        // For this arithemtic to work we're relying on snd_nxt not wrapping around
        // over snd_una-MaxAckBefore. Currently this cannot happen because we don't
        // support window scaling so snd_nxt-snd_una cannot even exceed 2^16-1.
        SeqType past_ack_num = seq_diff(pcb->snd_una, MaxAckBefore);
        bool valid_ack = seq_lte(tcp_meta.ack_num, pcb->snd_nxt, past_ack_num);
        if (AMBRO_UNLIKELY(!valid_ack)) {
            return pcb_send_empty_ack(pcb);
        }
        
        // Check if the ACK acknowledges anything new.
        bool new_ack = !seq_lte(tcp_meta.ack_num, pcb->snd_una, past_ack_num);
        
        // Bump the last-time of the PCB.
        pcb->last_time = Clock::getTime(Context());
        
        if (AMBRO_UNLIKELY(pcb->state == TcpState::SYN_RCVD)) {
            // If our SYN is not acknowledged, send RST and drop.
            // RFC 793 seems to allow ack_num==snd_una which doesn't make sense.
            if (!new_ack) {
                send_rst(pcb->local_addr, pcb->remote_addr, pcb->local_port, pcb->remote_port,
                         tcp_meta.ack_num, false, 0);
                return;
            }
            
            // In SYN_RCVD the remote acks only our SYN no more.
            // Otherwise we would have bailed out already (valid_ack, new_ack).
            AMBRO_ASSERT(tcp_meta.ack_num == seq_add(pcb->snd_una, 1))
            AMBRO_ASSERT(tcp_meta.ack_num == pcb->snd_nxt)
            
            // Stop the SYN_RCVD abort timer.
            pcb->abrt_timer.unset(Context());
            
            // Go to ESTABLISHED state.
            pcb->state = TcpState::ESTABLISHED;
            
            // Update snd_una due to one sequence count having been ACKed.
            pcb->snd_una = tcp_meta.ack_num;
            
            // Remember the initial send window.
            pcb->snd_wnd = tcp_meta.window_size;
            pcb->snd_wl1 = tcp_meta.seq_num;
            pcb->snd_wl2 = tcp_meta.ack_num;
            
            // We have a TcpListener (if it went away the PCB would have been aborted).
            // We also have no TcpConnection.
            TcpListener *lis = pcb->lis;
            AMBRO_ASSERT(lis != nullptr)
            AMBRO_ASSERT(lis->m_listening)
            AMBRO_ASSERT(lis->m_accept_pcb == nullptr)
            AMBRO_ASSERT(pcb->con == nullptr)
            
            // Inform the user that the connection has been established
            // and allow association with a TcpConnection to be done.
            lis->m_accept_pcb = pcb;
            lis->m_callback->connectionEstablished(lis);
            
            // Not referencing the listener after this, it might have been deinited from
            // the callback. In any case we will not leave lis->m_accept_pcb pointing
            // to this PCB, that is assured by acceptConnection or pcb_abort below.
            
            // Handle abort of PCB.
            if (AMBRO_UNLIKELY(m_current_pcb == nullptr)) {
                return;
            }
            
            // Possible transitions in callback (except to CLOSED):
            // - ESTABLISHED->FIN_WAIT_1
            
            // If the connection has not been accepted or has been accepted but
            // already abandoned, abort with RST.
            if (AMBRO_UNLIKELY(pcb->con == nullptr)) {
                return pcb_abort(pcb, true);
            }
        } else {
            // Handle new acknowledgments.
            if (new_ack) {
                // We can only get here if there was anything pending acknowledgement.
                AMBRO_ASSERT(can_output_in_state(pcb->state))
                
                // Calculate the amount of acknowledged sequence counts.
                // This can be data or FIN (but not SYN, as SYN_RCVD state is handled above).
                SeqType acked = seq_diff(tcp_meta.ack_num, pcb->snd_una);
                AMBRO_ASSERT(acked > 0)
                
                // Update snd_una due to sequences having been ACKed.
                pcb->snd_una = tcp_meta.ack_num;
                
                // Because snd_wnd is relative to snd_una, a matching decrease of snd_wnd
                // is neeeded.
                if (AMBRO_LIKELY(acked <= pcb->snd_wnd)) {
                    pcb->snd_wnd -= acked;
                } else {
                    // More has been ACKed than the send window. As snd_wnd annot be made
                    // negative, this is effectively an increase of the send window.
                    // This warrants a pcb_output but we're setting that up anyway next.
                    pcb->snd_wnd = 0;
                }
                
                // Clear the retransmission timeout and schedule pcb_output to
                // be done shortly, which will res-set the timeout as needed.
                pcb->rtx_timer.unset(Context());
                pcb_set_flag(pcb, PcbFlags::OUT_PENDING);
                
                // Check if our FIN has just been acked.
                bool fin_acked = pcb_fin_acked(pcb);
                
                // Calculate the amount of acknowledged data (without ACK of FIN).
                SeqType data_acked_seq = acked - (SeqType)fin_acked;
                AMBRO_ASSERT(data_acked_seq <= SIZE_MAX)
                size_t data_acked = data_acked_seq;
                
                if (data_acked > 0) {
                    AMBRO_ASSERT(data_acked <= pcb->snd_buf.tot_len)
                    AMBRO_ASSERT(pcb->snd_buf_cur.tot_len <= pcb->snd_buf.tot_len)
                    AMBRO_ASSERT(pcb->snd_psh_index <= pcb->snd_buf.tot_len)
                    
                    // Advance the send buffer.
                    size_t cur_offset = pcb_snd_cur_offset(pcb);
                    if (data_acked >= cur_offset) {
                        pcb->snd_buf_cur.skipBytes(data_acked - cur_offset);
                        pcb->snd_buf = pcb->snd_buf_cur;
                    } else {
                        pcb->snd_buf.skipBytes(data_acked);
                    }
                    
                    // Adjust the push index.
                    pcb->snd_psh_index -= MinValue(pcb->snd_psh_index, data_acked);
                    
                    // Report data-sent event to the user.
                    if (!pcb_callback(pcb, [&](auto cb) { cb->dataSent(data_acked); })) {
                        return;
                    }
                    // Possible transitions in callback (except to CLOSED):
                    // - ESTABLISHED->FIN_WAIT_1
                    // - CLOSE_WAIT->LAST_ACK
                }
                
                if (AMBRO_UNLIKELY(fin_acked)) {
                    // We must be in a state where we had queued a FIN for sending
                    // but have not send it or received its acknowledgement yet.
                    AMBRO_ASSERT(pcb->state == OneOf(TcpState::FIN_WAIT_1, TcpState::CLOSING,
                                                     TcpState::LAST_ACK))
                    
                    // Report end-sent event to the user.
                    if (!pcb_callback(pcb, [&](auto cb) { cb->endSent(); })) {
                        return;
                    }
                    // Possible transitions in callback (except to CLOSED): none.
                    
                    // In states where the endReceived has already been called
                    // (CLOSING and LAST_ACK), we need to now disassociate the PCB
                    // and the TcpConnection, using pcb_unlink_con.
                    
                    if (pcb->state == TcpState::FIN_WAIT_1) {
                        // FIN is accked in FIN_WAIT_1, transition to FIN_WAIT_2.
                        pcb->state = TcpState::FIN_WAIT_2;
                        // At this transition output_timer and rtx_timer must
                        // be unset due to assert in their handlers.
                        pcb->output_timer.unset(Context());
                        // rtx_timer was unset above already.
                    }
                    else if (pcb->state == TcpState::CLOSING) {
                        // Transition to TIME_WAIT; pcb_unlink_con is done by pcb_go_to_time_wait.
                        // Further processing below is not applicable so return here.
                        return pcb_go_to_time_wait(pcb);
                    }
                    else {
                        AMBRO_ASSERT(pcb->state == TcpState::LAST_ACK)
                        // Close the PCB; do pcb_unlink_con first to prevent connectionAborted.
                        pcb_unlink_con(pcb); 
                        return pcb_abort(pcb, false);
                    }
                }
            }
            
            // Handle window updates.
            // But inhibit for old acknowledgements (ack_num<snd_una). Note that
            // for new acknowledgements, snd_una has been bumped to ack_num.
            if (AMBRO_LIKELY(pcb->snd_una == tcp_meta.ack_num)) {
                // Compare sequence and ack numbers with respect to nxt_wnd+1
                // and snd_nxt+1 as the minimum value, respectively.
                // Note that we use the original non-trimmed sequence number.
                SeqType wnd_seq_ref = seq_add(nxt_wnd, 1);
                SeqType wnd_seq_seg = seq_diff(tcp_meta.seq_num, wnd_seq_ref);
                SeqType wnd_seq_old = seq_diff(pcb->snd_wl1, wnd_seq_ref);
                if (wnd_seq_seg > wnd_seq_old || (wnd_seq_seg == wnd_seq_old &&
                    seq_lte(pcb->snd_wl2, tcp_meta.ack_num, seq_add(pcb->snd_nxt, 1))))
                {
                    // Update the window.
                    SeqType old_snd_wnd = pcb->snd_wnd;
                    pcb->snd_wnd = tcp_meta.window_size;
                    pcb->snd_wl1 = tcp_meta.seq_num;
                    pcb->snd_wl2 = tcp_meta.ack_num;
                    
                    // If the window has increased, pcb_output is warranted.
                    if (pcb->snd_wnd > old_snd_wnd) {
                        pcb_set_flag(pcb, PcbFlags::OUT_PENDING);
                    }
                }
            }
        }
        
        if (AMBRO_LIKELY(accepting_data_in_state(pcb->state))) {
            // Any data or flags that don't fit in the receive window would have
            // been trimmed already. It's true that rcv_wnd might have been changed
            // from callbacks but it could only have been made larger.
            AMBRO_ASSERT(len <= pcb->rcv_wnd)
            
            // Check if we have just recevied a FIN.
            bool fin_recved = (eff_flags & Tcp4FlagFin) != 0;
            
            // This assert holds because of how "len" was initially calculated
            // and any trimming of the segment preserves this.
            AMBRO_ASSERT(tcp_data.tot_len + (size_t)fin_recved == len)
            
            // Accept anything newly received.
            if (len > 0) {
                // Adjust rcv_nxt and rcv_wnd due to newly received data.
                SeqType old_rcv_nxt = pcb->rcv_nxt;
                pcb->rcv_nxt = seq_add(pcb->rcv_nxt, len);
                pcb->rcv_wnd -= len;
                
                // Make sure we're not leaving rcv_ann behind rcv_nxt.
                // This can happen when the peer sends data before receiving
                // a window update permitting that.
                if (len > seq_diff(pcb->rcv_ann, old_rcv_nxt)) {
                    pcb->rcv_ann = pcb->rcv_nxt;
                }
                
                // Send an ACK (below).
                pcb_set_flag(pcb, PcbFlags::ACK_PENDING);
                
                // If a FIN was received, make appropriate state transitions, except the
                // FIN_WAIT_2->TIME_WAIT transition which is done after the callbacks.
                if (AMBRO_UNLIKELY(fin_recved)) {
                    if (pcb->state == TcpState::ESTABLISHED) {
                        pcb->state = TcpState::CLOSE_WAIT;
                    }
                    else if (pcb->state == TcpState::FIN_WAIT_1) {
                        pcb->state = TcpState::CLOSING;
                    }
                }
                
                // Reveived any actual data?
                if (tcp_data.tot_len > 0) {
                    // If the user has abandoned the connection, abort with RST.
                    if (pcb->con == nullptr) {
                        return pcb_abort(pcb, true);
                    }
                    
                    // Give any the data to the user.
                    if (!pcb_callback(pcb, [&](auto cb) { cb->dataReceived(tcp_data); })) {
                        return;
                    }
                    // Possible transitions in callback (except to CLOSED):
                    // - ESTABLISHED->FIN_WAIT_1
                    // - CLOSE_WAIT->LAST_ACK
                }
                
                if (AMBRO_UNLIKELY(fin_recved)) {
                    // Report end-received event to the user.
                    if (!pcb_callback(pcb, [&](auto cb) { cb->endReceived(); })) {
                        return;
                    }
                    // Possible transitions in callback (except to CLOSED):
                    // - CLOSE_WAIT->LAST_ACK
                    
                    // In FIN_WAIT_2 state we need to transition the PCB to TIME_WAIT.
                    // This includes pcb_unlink_con since both endSent and endReceived
                    // callbacks have now been called.
                    if (pcb->state == TcpState::FIN_WAIT_2) {
                        pcb_go_to_time_wait(pcb);
                    }
                }
            }
        }
        else if (pcb->state == TcpState::TIME_WAIT) {
            // Reply with an ACK, and restart the timeout.
            pcb_set_flag(pcb, PcbFlags::ACK_PENDING);
            pcb->abrt_timer.appendAfter(Context(), TimeWaitTimeTicks);
        }
        
        // Try to output if desired.
        if (pcb_has_flag(pcb, PcbFlags::OUT_PENDING)) {
            pcb_clear_flag(pcb, PcbFlags::OUT_PENDING);
            if (can_output_in_state(pcb->state)) {
                bool sent_something = pcb_output(pcb);
                if (sent_something) {
                    pcb_clear_flag(pcb, PcbFlags::ACK_PENDING);
                }
            }
        }
        
        // Send an empty ACK if desired.
        // Note, ACK_PENDING will have been cleared above if pcb_output sent anything,
        // in that case we don't need an empty ACK here.
        if (pcb_has_flag(pcb, PcbFlags::ACK_PENDING)) {
            pcb_clear_flag(pcb, PcbFlags::ACK_PENDING);
            pcb_send_empty_ack(pcb);
        }
    }
    
    TcpPcb * allocate_pcb ()
    {
        TimeType now = Clock::getTime(Context());
        TcpPcb *the_pcb = nullptr;
        
        // Find a PCB to use, either a CLOSED one (preferably) or one which
        // has no associated TcpConnection. For the latter case use the least
        // recently used such PCB.
        for (TcpPcb &pcb : m_pcbs) {
            if (pcb_has_flag(&pcb, PcbFlags::ABORTING)) {
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
    
    inline void pcb_abort (TcpPcb *pcb)
    {
        bool send_rst = pcb->state != OneOf(TcpState::SYN_RCVD, TcpState::TIME_WAIT);
        pcb_abort(pcb, send_rst);
    }
    
    void pcb_abort (TcpPcb *pcb, bool send_rst)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        AMBRO_ASSERT(!pcb_has_flag(pcb, PcbFlags::ABORTING))
        
        // If there is an associated TcpConnection, call the connectionAborted
        // callback. During set the flag ABORTING durring the callback so we
        // can tell if we're being called back from here.
        if (pcb->con != nullptr) {
            pcb_set_flag(pcb, PcbFlags::ABORTING);
            pcb->con->m_callback->connectionAborted();
            pcb_clear_flag(pcb, PcbFlags::ABORTING);
        }
        
        // Send RST if desired.
        if (send_rst) {
            pcb_send_rst(pcb);
        }
        
        // Disassociate any TcpConnection.
        pcb_unlink_con(pcb);
        
        // Disassociate any TcpListener.
        pcb_unlink_lis(pcb);
        
        // If this is called from input processing of this PCB,
        // clear m_current_pcb. This way, input processing can
        // detect aborts performed from within user callbacks.
        if (m_current_pcb == pcb) {
            m_current_pcb = nullptr;
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
        if (pcb_has_flag(pcb, PcbFlags::ABORTING)) {
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
            if (snd_not_closed_in_state(pcb->state)) {
                pcb_end_sending(pcb);
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
                pcb_need_ack(pcb);
            }
        }
        
        // Start the abort timeout.
        pcb->abrt_timer.appendAfter(Context(), AbandonedTimeoutTicks);
    }
    
    void pcb_extend_receive_window (TcpPcb *pcb, SeqType wnd_ext, bool force_wnd_update)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        // Extend the receive window.
        pcb->rcv_wnd += wnd_ext;
        
        // Don't do anything else if we're in the connectionAborted callback.
        if (pcb_has_flag(pcb, PcbFlags::ABORTING)) {
            return;
        }
        
        // If we already received a FIN, there's no need to send window updates.
        if (!accepting_data_in_state(pcb->state)) {
            return;
        }
        
        // Force window update if the threshold has exceeded.
        if (pcb_get_wnd_ann_incr(pcb) >= pcb->rcv_ann_thres) {
            force_wnd_update = true;
        }
        
        // Generate a window update if needed.
        if (force_wnd_update) {
            pcb_need_ack(pcb);
        }
    }
    
    void pcb_need_ack (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        if (m_current_pcb == pcb) {
            pcb_set_flag(pcb, PcbFlags::ACK_PENDING);
        } else {
            pcb_send_empty_ack(pcb);
        }
    }
    
    void pcb_snd_buf_extended (TcpPcb *pcb)
    {
        AMBRO_ASSERT(snd_not_closed_in_state(pcb->state))
        
        // Start the output timer if not running.
        if (!pcb->output_timer.isSet(Context())) {
            pcb->output_timer.appendAfter(Context(), OutputTimerTicks);
        }
    }
    
    void pcb_end_sending (TcpPcb *pcb)
    {
        AMBRO_ASSERT(snd_not_closed_in_state(pcb->state))
        
        // Make the appropriate state transition, effectively
        // queuing a FIN for sending.
        if (pcb->state == TcpState::ESTABLISHED) {
            pcb->state = TcpState::FIN_WAIT_1;
        } else {
            AMBRO_ASSERT(pcb->state == TcpState::CLOSE_WAIT)
            pcb->state = TcpState::LAST_ACK;
        }
        
        // Queue a FIN for sending.
        pcb_set_flag(pcb, PcbFlags::FIN_PENDING);
        
        // Push output.
        pcb_push_output(pcb);
    }
    
    void pcb_push_output (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // Ignore this if it we're in the connectionAborted callback.
        if (pcb_has_flag(pcb, PcbFlags::ABORTING)) {
            return;
        }
        
        // Set the push index to the end of the send buffer.
        pcb->snd_psh_index = pcb->snd_buf.tot_len;
        
        // Schedule a call to pcb_output soon.
        if (pcb == m_current_pcb) {
            pcb_set_flag(pcb, PcbFlags::OUT_PENDING);
        } else {
            if (!pcb->output_timer.isSet(Context())) {
                pcb->output_timer.appendAfter(Context(), OutputTimerTicks);
            }
        }
    }
    
    /**
     * Drives transmission of data including FIN.
     * Returns whether anything has been sent, excluding a possible zero-window probe.
     */
    bool pcb_output (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // If there is nothing to be sent or acknowledged, stop the retransmission
        // timer and return.
        if (pcb->snd_buf.tot_len == 0 && snd_not_closed_in_state(pcb->state)) {
            pcb->rtx_timer.unset(Context());
            return false;
        }
        
        // Figure out where to start and what can be sent.
        size_t offset = pcb_snd_cur_offset(pcb);
        SeqType seq_num = seq_add(pcb->snd_una, offset);
        SeqType rem_wnd = pcb->snd_wnd - MinValueU(pcb->snd_wnd, offset);
        
        // If we there is zero window:
        // - Pretend that there is one count available so that we allow ourselves
        //   to send a zero-window probe.
        // - Ensure that snd_buf_cur is at the start of data, since it may actually
        //   be beyond in case of window shrinking, and that would be a problem
        //   because we would not transmit anything at all.
        bool zero_window = (pcb->snd_wnd == 0);
        if (AMBRO_UNLIKELY(zero_window)) {
            rem_wnd = 1;
            pcb->snd_buf_cur = pcb->snd_buf;
        }
        
        // Will need to know if we sent anything.
        bool sent = false;
        
        // Loop while we have something to send and window is available.
        while ((pcb->snd_buf_cur.tot_len > 0 || pcb_has_flag(pcb, PcbFlags::FIN_PENDING)) && rem_wnd > 0) {
            // Determine segment data.
            auto min_wnd_mss = MinValueU(rem_wnd, pcb->snd_mss);
            size_t seg_data_len = MinValueU(min_wnd_mss, pcb->snd_buf_cur.tot_len);
            IpBufRef seg_data = pcb->snd_buf_cur.subTo(seg_data_len);
            
            // Check if the segment contains pushed data.
            bool push = pcb->snd_psh_index > offset;
            
            // Determine segment flags.
            FlagsType seg_flags = Tcp4FlagAck;
            if (seg_data_len == pcb->snd_buf_cur.tot_len && pcb_has_flag(pcb, PcbFlags::FIN_PENDING) && rem_wnd > seg_data_len) {
                seg_flags |= Tcp4FlagFin|Tcp4FlagPsh;
                push = true;
            }
            else if (push && pcb->snd_psh_index <= offset + seg_data_len) {
                seg_flags |= Tcp4FlagPsh;
            }
            
            // Avoid sending small segments for better efficiency.
            // This is tricky because we must prevent lockup:
            // - There must be no additional delay for any data before snd_psh_index.
            // - There must be no additional delay for the FIN.
            // - There must be progress even if the send window is always below snd_mss.
            if (!push && seg_data_len < min_wnd_mss) {
                break;
            }
            
            // Send the segment.
            TcpSegMeta tcp_meta = {pcb->local_port, pcb->remote_port, seq_num, pcb->rcv_nxt,
                                   pcb_ann_wnd(pcb), seg_flags};
            send_tcp(pcb->local_addr, pcb->remote_addr, tcp_meta, seg_data);
            
            // Calculate the sequence length of segment, update state due to sending FIN.
            SeqType seg_seqlen = seg_data_len;
            if ((seg_flags & Tcp4FlagFin) != 0) {
                seg_seqlen++;
                pcb_clear_flag(pcb, PcbFlags::FIN_PENDING);
                pcb_set_flag(pcb, PcbFlags::FIN_SENT);
            }
            
            // Bump snd_nxt if needed.
            SeqType seg_endseq = seq_add(seq_num, seg_seqlen);
            if (seq_lt(pcb->snd_nxt, seg_endseq, pcb->snd_una)) {
                pcb->snd_nxt = seg_endseq;
            }
            
            // Increment things.
            pcb->snd_buf_cur.skipBytes(seg_data_len);
            offset += seg_data_len;
            seq_num = seq_add(seq_num, seg_data_len);
            rem_wnd -= seg_data_len;
            
            sent = true;
        }
        
        if (AMBRO_UNLIKELY(zero_window)) {
            // Zero window => setup timer for zero window probe.
            if (!pcb->rtx_timer.isSet(Context()) || !pcb_has_flag(pcb, PcbFlags::ZERO_WINDOW)) {
                pcb->rtx_timer.appendAfter(Context(), ZeroWindowTimeTicks);
                pcb_set_flag(pcb, PcbFlags::ZERO_WINDOW);
            }
        }
        else if (pcb->snd_buf_cur.tot_len < pcb->snd_buf.tot_len || !snd_not_closed_in_state(pcb->state))
        {
            // Unacked data or sending closed => setup timer for retransmission.
            if (!pcb->rtx_timer.isSet(Context()) || pcb_has_flag(pcb, PcbFlags::ZERO_WINDOW)) {
                pcb->rtx_timer.appendAfter(Context(), RetransmissionTimeTicks);
                pcb_clear_flag(pcb, PcbFlags::ZERO_WINDOW);
            }
        }
        else {
            // No zero window, no unacked data, sending not closed => stop the timer.
            // This can only happen because we are delaying transmission in hope of
            // sending a larger segment later.
            
            // These asserts trivially follow from the non-taken ifs just above.
            AMBRO_ASSERT(pcb->snd_wnd > 0)
            AMBRO_ASSERT(pcb->snd_buf_cur.tot_len == pcb->snd_buf.tot_len)
            AMBRO_ASSERT(snd_not_closed_in_state(pcb->state))
            
            // These asserts are the important ones saying that we are allowed
            // to delay transmission here, because we have less than MSS worth
            // of data and none of this was pushed.
            AMBRO_ASSERT(pcb->snd_psh_index == 0)             // Statement (1)
            AMBRO_ASSERT(pcb->snd_buf.tot_len < pcb->snd_mss) // Statement (2)
            // Proof:
            // - If snd_buf.tot_len==0 then these statements are obviously
            //   true, so assume snd_buf.tot_len>0.
            // - Before start of the output loop, we had rem_wnd>0. This is
            //   because offset==0 so nothing was subtracted from snd_wnd>0.
            //   And offset==0 because snd_buf_cur.tot_len==snd_buf.tot_len.
            // - Before of output loop, we had snd_buf_cur.tot_len>0. This is
            //   because we had snd_buf_cur.tot_len==snd_buf.tot_len because
            //   we have that now.
            // - Therefore, there was at least one iteration of the output loop.
            // - In the first loop iteration we had seg_data_len>0 because that
            //   was a min of all >0 values.
            // - The first first iteration was interrupted at the break, because
            //   otherwise snd_buf_cur.tot_len would have been decreased which
            //   is contradictory to snd_buf_cur.tot_len==snd_buf.tot_len here.
            // - Statement (1) is true because !push and offset==0.
            // - Statement (2) is true because seg_data_len<min_wnd_mss
            //   which implies seg_data_len==snd_buf.tot_len (consider calculation
            //   of seg_data_len and snd_buf_cur.tot_len==snd_buf.tot_len).
            
            pcb->rtx_timer.unset(Context());
        }
        
        // We return whether we sent something except that we exclude zero-window
        // probes. This is because the return value is used to determine whether
        // we sent a valid ACK, but a zero-window probe will likely be discarded
        // by the received before ACK-processing.
        return (sent && !zero_window);
    }
    
    void pcb_abrt_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        pcb_abort(pcb);
    }
    
    void pcb_output_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // Drive the transmission.
        pcb_output(pcb);
    }
    
    void pcb_rtx_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // If this timeout was for a retransmission not a zero-window probe,
        // schedule retransmission of everything (data, FIN).
        // reset the send pointer to retransmit from the beginning.
        if (!pcb_has_flag(pcb, PcbFlags::ZERO_WINDOW)) {
            pcb->snd_buf_cur = pcb->snd_buf;
            if (!snd_not_closed_in_state(pcb->state)) {
                pcb_set_flag(pcb, PcbFlags::FIN_PENDING);
            }
            // TODO: We are supposed to retransmit only one segment not the
            // entire send queue.
        }
        
        // Drive the transmission.
        pcb_output(pcb);
    }
    
    inline static bool pcb_has_flag (TcpPcb *pcb, uint8_t flag)
    {
        return (pcb->flags & flag) != 0;
    }
    
    inline static void pcb_set_flag (TcpPcb *pcb, uint8_t flag)
    {
        pcb->flags |= flag;
    }
    
    inline static void pcb_clear_flag (TcpPcb *pcb, uint8_t flag)
    {
        pcb->flags &= ~flag;
    }
    
    // Determine how much new window would be anounced if we sent a window update.
    static SeqType pcb_get_wnd_ann_incr (TcpPcb *pcb)
    {
        uint16_t wnd_to_ann = MinValue(pcb->rcv_wnd, (SeqType)UINT16_MAX);
        SeqType new_rcv_ann = seq_add(pcb->rcv_nxt, wnd_to_ann);
        return seq_diff(new_rcv_ann, pcb->rcv_ann);
    }
    
    // Get a window size value to be put into a segment being sent.
    static uint16_t pcb_ann_wnd (TcpPcb *pcb)
    {
        uint16_t wnd_to_ann = MinValue(pcb->rcv_wnd, (SeqType)UINT16_MAX);
        pcb->rcv_ann = seq_add(pcb->rcv_nxt, wnd_to_ann);
        return wnd_to_ann;
    }
    
    // Check if our FIN has been ACKed.
    static bool pcb_fin_acked (TcpPcb *pcb)
    {
        return pcb_has_flag(pcb, PcbFlags::FIN_SENT) && pcb->snd_una == pcb->snd_nxt;
    }
    
    // Calculate the offset of the current send buffer position.
    static size_t pcb_snd_cur_offset (TcpPcb *pcb)
    {
        size_t buf_len = pcb->snd_buf.tot_len;
        size_t cur_len = pcb->snd_buf_cur.tot_len;
        AMBRO_ASSERT(cur_len <= buf_len)
        return (buf_len - cur_len);
    }
    
    // Send SYN+ACK packet in SYN_RCVD state, with MSS option.
    void pcb_send_syn_ack (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state == TcpState::SYN_RCVD)
        
        TcpOptions tcp_opts;
        tcp_opts.options = OptionFlags::MSS;
        tcp_opts.mss = pcb->rcv_mss;
        TcpSegMeta tcp_meta = {pcb->local_port, pcb->remote_port, pcb->snd_una, pcb->rcv_nxt,
                               pcb_ann_wnd(pcb), Tcp4FlagSyn|Tcp4FlagAck, &tcp_opts};
        send_tcp(pcb->local_addr, pcb->remote_addr, tcp_meta);
    }
    
    // Send an empty ACK (which may be a window update).
    void pcb_send_empty_ack (TcpPcb *pcb)
    {
        TcpSegMeta tcp_meta = {pcb->local_port, pcb->remote_port, pcb->snd_nxt, pcb->rcv_nxt,
                               pcb_ann_wnd(pcb), Tcp4FlagAck};
        send_tcp(pcb->local_addr, pcb->remote_addr, tcp_meta);
    }
    
    // Send an RST for this PCB.
    void pcb_send_rst (TcpPcb *pcb)
    {
        send_rst(pcb->local_addr, pcb->remote_addr, pcb->local_port, pcb->remote_port, pcb->snd_nxt, true, pcb->rcv_nxt);
    }
    
    // Call a TcpConnection callback, if there is any TcpConnection associated.
    // Returns false if the PCB was aborted, else true. This must be used from
    // pcb_input only as it relies on m_current_pcb to detect an abort.
    template <typename Callback>
    bool pcb_callback (TcpPcb *pcb, Callback callback)
    {
        AMBRO_ASSERT(m_current_pcb == pcb)
        
        if (pcb->con != nullptr) {
            AMBRO_ASSERT(pcb->con->m_pcb == pcb)
            callback(pcb->con->m_callback);
            if (m_current_pcb == nullptr) {
                return false;
            }
        }
        
        return true;
    }
    
    // Send an RST as a reply to a received segment.
    // This conforms to RFC 793 handling of segments not belonging to a known
    // connection.
    void send_rst_reply (Ip4DgramMeta const &ip_meta, TcpSegMeta const &tcp_meta, size_t tcp_data_len)
    {
        SeqType rst_seq_num;
        bool rst_ack;
        SeqType rst_ack_num;
        if ((tcp_meta.flags & Tcp4FlagAck) != 0) {
            rst_seq_num = tcp_meta.ack_num;
            rst_ack = false;
            rst_ack_num = 0;
        } else {
            rst_seq_num = 0;
            rst_ack = true;
            rst_ack_num = tcp_meta.seq_num + tcplen(tcp_meta.flags, tcp_data_len);
        }
        
        send_rst(ip_meta.local_addr, ip_meta.remote_addr, tcp_meta.local_port, tcp_meta.remote_port,
                 rst_seq_num, rst_ack, rst_ack_num);
    }
    
    void send_rst (Ip4Addr local_addr, Ip4Addr remote_addr,
                   PortType local_port, PortType remote_port,
                   SeqType seq_num, bool ack, SeqType ack_num)
    {
        FlagsType flags = Tcp4FlagRst | (ack ? Tcp4FlagAck : 0);
        TcpSegMeta tcp_meta = {local_port, remote_port, seq_num, ack_num, 0, flags};
        send_tcp(local_addr, remote_addr, tcp_meta);
    }
    
    void send_tcp (Ip4Addr local_addr, Ip4Addr remote_addr,
                   TcpSegMeta const &tcp_meta, IpBufRef data=IpBufRef{})
    {
        // Maximum length of TCP options.
        size_t const MaxOptsLen = TcpOptionLenMSS;
        
        // Compute length of TCP options.
        uint8_t options = (tcp_meta.opts != nullptr) ? tcp_meta.opts->options : 0;
        uint8_t opts_len = 0;
        if ((options & OptionFlags::MSS) != 0) {
            opts_len += TcpOptionLenMSS;
        }
        // NOTE: opts_len must be a multiple of 4, this must be considered
        // if any other options are implemented written in the future.
        
        // Allocate memory for headers.
        TxAllocHelper<BufAllocator, Tcp4Header::Size+MaxOptsLen, HeaderBeforeIp4Dgram>
            dgram_alloc(Tcp4Header::Size+opts_len);
        
        // Caculate the offset+flags field.
        FlagsType offset_flags = ((FlagsType)(5+opts_len/4) << TcpOffsetShift) | tcp_meta.flags;
        
        // Write the TCP header.
        auto tcp_header = Tcp4Header::MakeRef(dgram_alloc.getPtr());
        tcp_header.set(Tcp4Header::SrcPort(),     tcp_meta.local_port);
        tcp_header.set(Tcp4Header::DstPort(),     tcp_meta.remote_port);
        tcp_header.set(Tcp4Header::SeqNum(),      tcp_meta.seq_num);
        tcp_header.set(Tcp4Header::AckNum(),      tcp_meta.ack_num);
        tcp_header.set(Tcp4Header::OffsetFlags(), offset_flags);
        tcp_header.set(Tcp4Header::WindowSize(),  tcp_meta.window_size);
        tcp_header.set(Tcp4Header::Checksum(),    0);
        tcp_header.set(Tcp4Header::UrgentPtr(),   0);
        
        // Write any TCP options.
        char *opts_data = dgram_alloc.getPtr() + Tcp4Header::Size;
        if ((options & OptionFlags::MSS) != 0) {
            WriteBinaryInt<uint8_t,  BinaryBigEndian>(TcpOptionMSS,       opts_data + 0);
            WriteBinaryInt<uint8_t,  BinaryBigEndian>(TcpOptionLenMSS,    opts_data + 1);
            WriteBinaryInt<uint16_t, BinaryBigEndian>(tcp_meta.opts->mss, opts_data + 2);
            opts_data += TcpOptionLenMSS;
        }
        
        // Construct the datagram reference including any data.
        IpBufNode data_node;
        if (data.tot_len > 0) {
            data_node = data.toNode();
            dgram_alloc.setNext(&data_node, data.tot_len);
        }
        IpBufRef dgram = dgram_alloc.getBufRef();
        
        // Calculate TCP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&local_addr.data);
        chksum_accum.addWords(&remote_addr.data);
        chksum_accum.addWord(WrapType<uint16_t>(), Ip4ProtocolTcp);
        chksum_accum.addWord(WrapType<uint16_t>(), dgram.tot_len);
        chksum_accum.addIpBuf(dgram);
        uint16_t calc_chksum = chksum_accum.getChksum();
        tcp_header.set(Tcp4Header::Checksum(), calc_chksum);
        
        // Send the datagram.
        Ip4DgramMeta meta = {local_addr, remote_addr, TcpTTL, Ip4ProtocolTcp};
        m_stack->sendIp4Dgram(meta, dgram);
    }
    
    static void parse_options (IpBufRef buf, uint8_t opts_len, TcpOptions *out_opts, IpBufRef *out_data)
    {
        AMBRO_ASSERT(opts_len <= buf.tot_len)
        
        // Truncate the buffer temporarily while we're parsing the options.
        size_t data_len = buf.tot_len - opts_len;
        buf.tot_len = opts_len;
        
        // Clear options flags. Below we will set flags for options that we find.
        out_opts->options = 0;
        
        while (buf.tot_len > 0) {
            // Read the option kind.
            uint8_t kind = buf.takeByte();
            
            // Hanlde end option and nop option.
            if (kind == TcpOptionEnd) {
                break;
            }
            else if (kind == TcpOptionNop) {
                continue;
            }
            
            // Read the option length.
            if (buf.tot_len == 0) {
                break;
            }
            uint8_t length = buf.takeByte();
            
            // Check the option length.
            if (length < 2) {
                break;
            }
            uint8_t opt_data_len = length - 2;
            if (buf.tot_len < opt_data_len) {
                break;
            }
            
            // Handle different options, consume option data.
            switch (kind) {
                // Maximum Segment Size
                case TcpOptionMSS: {
                    if (opt_data_len != 2) {
                        goto skip_option;
                    }
                    char opt_data[2];
                    buf.takeBytes(opt_data_len, opt_data);
                    out_opts->options |= OptionFlags::MSS;
                    out_opts->mss = ReadBinaryInt<uint16_t, BinaryBigEndian>(opt_data);
                } break;
                
                // Unknown option (also used to handle bad options).
                skip_option:
                default: {
                    buf.skipBytes(opt_data_len);
                } break;
            }
        }
        
        // Skip any remaining option data (we might not have consumed all of it).
        buf.skipBytes(buf.tot_len);
        
        // The buf now points to the start of segment data but is empty.
        // Extend it to reference the entire segment data and return it to the caller.
        buf.tot_len = data_len;
        *out_data = buf;
    }
    
    static bool calc_snd_mss (uint16_t iface_mss, TcpOptions const &tcp_opts, uint16_t *out_mss)
    {
        uint16_t mss = iface_mss;
        if ((tcp_opts.options & OptionFlags::MSS) != 0) {
            mss = MinValue(mss, tcp_opts.mss);
        }
        if (mss < MinAllowedMss) {
            return false;
        }
        *out_mss = mss;
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
    
    static inline size_t tcplen (FlagsType flags, size_t tcp_data_len)
    {
        return tcp_data_len + ((flags & Tcp4SeqFlags) != 0);
    }
    
    static inline bool state_is_active (TcpState state)
    {
        return state != OneOf(TcpState::CLOSED, TcpState::SYN_SENT,
                              TcpState::SYN_RCVD, TcpState::TIME_WAIT);
    }
    
    static inline bool accepting_data_in_state (TcpState state)
    {
        return state == OneOf(TcpState::ESTABLISHED, TcpState::FIN_WAIT_1,
                              TcpState::FIN_WAIT_2);
    }
    
    static inline bool can_output_in_state (TcpState state)
    {
        return state == OneOf(TcpState::ESTABLISHED, TcpState::FIN_WAIT_1,
                              TcpState::CLOSING, TcpState::CLOSE_WAIT,
                              TcpState::LAST_ACK);
    }
    
    static inline bool snd_not_closed_in_state (TcpState state)
    {
        return state == OneOf(TcpState::ESTABLISHED, TcpState::CLOSE_WAIT);
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
    APRINTER_AS_VALUE(int, NumTcpPcbs)
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
