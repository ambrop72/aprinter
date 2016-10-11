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

#ifndef APRINTER_IPSTACK_IP_TCP_PROTO_API_H
#define APRINTER_IPSTACK_IP_TCP_PROTO_API_H

#include <stddef.h>
#include <stdint.h>

#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/ipstack/Buf.h>
#include <aprinter/ipstack/IpAddr.h>
#include <aprinter/ipstack/proto/TcpUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename> class IpTcpProto;
template <typename> class IpTcpProto_input;

template <typename TcpProto>
class IpTcpProto_api
{
    APRINTER_USE_TYPES2(TcpUtils, (TcpState, PortType, SeqType))
    APRINTER_USE_VALS(TcpUtils, (state_is_active, snd_open_in_state))
    APRINTER_USE_TYPES1(TcpProto, (Context, TcpPcb, Input, Output))
    
public:
    class TcpConnection;
    class TcpListener;
    
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
        template <typename> friend class IpTcpProto;
        template <typename> friend class IpTcpProto_input;
        friend class TcpConnection;
        
    public:
        /**
         * Initialize the listener.
         * 
         * A callback listener must be provided, which is used to inform of
         * newly accepted connections. Upon init, the listener is in not-listening
         * state, and listenIp4 should be called to start listening.
         */
        void init (TcpListenerCallback *callback)
        {
            AMBRO_ASSERT(callback != nullptr)
            
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
                            TcpProto::pcb_abort(&pcb, false);
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
        bool listenIp4 (TcpProto *tcp, Ip4Addr addr, PortType port, int max_pcbs)
        {
            AMBRO_ASSERT(!m_listening)
            AMBRO_ASSERT(tcp != nullptr)
            AMBRO_ASSERT(max_pcbs > 0)
            
            // Check if there is an existing listener listning on this address+port.
            if (tcp->find_listener(addr, port) != nullptr) {
                return false;
            }
            
            // Set up listening.
            m_tcp = tcp;
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
            AMBRO_ASSERT(rcv_wnd <= TcpProto::MaxRcvWnd)
            
            m_initial_rcv_wnd = rcv_wnd;
        }
        
    private:
        DoubleEndedListNode<TcpListener> m_listeners_node;
        TcpProto *m_tcp;
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
         * Called when some data or FIN has been received.
         * 
         * Each callback corresponds to shifting of the receive
         * buffer by that amount. Zero amount indicates that FIN
         * was received.
         */
        virtual void dataReceived (size_t amount) = 0;
        
        /**
         * Called when some data or FIN has been acknowledged.
         * 
         * Each dataSent callback corresponds to shifting of the send buffer
         * by that amount. Zero amount indicates that FIN was acknowledged.
         */
        virtual void dataSent (size_t amount) = 0;
    };
    
    /**
     * Represents a TCP connection.
     */
    class TcpConnection {
        template <typename> friend class IpTcpProto;
        
    public:
        /**
         * Initializes the connection object to a default not-connected state.
         * 
         * A callback interface must be provided which is used to inform the
         * user of various events related to the connection.
         */
        void init (TcpConnectionCallback *callback)
        {
            AMBRO_ASSERT(callback != nullptr)
            
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
                TcpProto::pcb_con_abandoned(pcb);
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
         * 2) Both dataReceived(0) and dataSent(0) callbacks have been called.
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
            AMBRO_ASSERT(rcv_ann_thres <= TcpProto::MaxRcvWnd)
            
            // Set the threshold.
            m_pcb->rcv_ann_thres = rcv_ann_thres;
        }
        
        /**
         * Get the current receive window.
         * 
         * The intended use is that this is called when the connection is established
         * and the application sets a receive buffer using setRecvBuf for at least this
         * much data. Note that the value returned here otherwise might not exactly
         * match the size of the receive buffer.
         * 
         * May be called in connected state only.
         */
        SeqType getReceiveWindow ()
        {
            assert_pcb();
            
            return m_pcb->rcv_wnd;
        }
        
        /**
         * Set the receive buffer for the connection.
         * 
         * This is only allowed when the receive buffer is empty and really
         * should be called only once when the connection is established.
         * From that point on, extendRecvBuf should be used.
         * 
         * May be called in connected state only.
         */
        void setRecvBuf (IpBufRef rcv_buf)
        {
            assert_pcb();
            AMBRO_ASSERT(m_pcb->rcv_buf.tot_len == 0)
            
            m_pcb->rcv_buf = rcv_buf;
            Input::pcb_rcv_buf_extended(m_pcb);
        }
        
        /**
         * Extend the receive buffer.
         * 
         * This typically results in an extension of the receive window, but
         * note that the receive window is managed by the TCP and may not
         * directly correspond to the size of the receive buffer.
         * 
         * May be called in connected state only.
         */
        void extendRecvBuf (size_t amount)
        {
            assert_pcb();
            AMBRO_ASSERT(amount <= SIZE_MAX - m_pcb->rcv_buf.tot_len)
            
            m_pcb->rcv_buf.tot_len += amount;
            Input::pcb_rcv_buf_extended(m_pcb);
        }
        
        /**
         * Get the current receive buffer reference.
         * 
         * This references the memory which is ready to receive new data.
         * 
         * May be called in connected state only.
         */
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
         * through a dataSent(0) callback. The dataSent(0) callback is only called
         * after all queued data has been reported sent using dataSent(>0) callbacks.
         * 
         * May be called in connected state only.
         * Must not be called after endSending.
         */
        void endSending ()
        {
            assert_pcb_sending();
            
            // Handle in private function.
            Output::pcb_end_sending(m_pcb);
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
                Output::pcb_push_output(m_pcb);
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
        TcpConnectionCallback *m_callback;
        TcpPcb *m_pcb;
    };    
};

#include <aprinter/EndNamespace.h>

#endif
