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

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aipstack/misc/Buf.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/TcpUtils.h>

#include <aipstack/BeginNamespace.h>

template <typename> class IpTcpProto;
template <typename> class IpTcpProto_input;
template <typename> class IpTcpProto_output;

template <typename TcpProto>
class IpTcpProto_api
{
    APRINTER_USE_TYPES1(TcpUtils, (TcpState, PortType, SeqType))
    APRINTER_USE_VALS(TcpUtils, (state_is_active, snd_open_in_state))
    APRINTER_USE_TYPES1(TcpProto, (Context, TcpPcb, Input, Output, Constants,
                                   PcbFlags))
    
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
         * A callback interface must be provided, which is used to inform of
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
            // Stop listening.
            if (m_listening) {
                m_tcp->m_listeners_list.remove(this);
                m_tcp->unlink_listener(this);
            }
            
            // Reset variables.
            m_initial_rcv_wnd = 0;
            m_accept_pcb = nullptr;
            m_listening = false;
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
            
            // Start listening.
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
         * connection will not receive data before the user extends the window using
         * extendReceiveWindow.
         * 
         * Note that the initial receive window is applied to a new connection when
         * the SYN is received, not when the connectionEstablished callback is called.
         * Hence the user should generaly use getReceiveWindow to determine the actual
         * receive window of a new connection. Further, the TCP may still use a
         * smaller initial receive window than configued with this function.
         */
        void setInitialReceiveWindow (size_t rcv_wnd)
        {
            m_initial_rcv_wnd = APrinter::MinValueU(Constants::MaxRcvWnd, rcv_wnd);
        }
        
    private:
        APrinter::DoubleEndedListNode<TcpListener> m_listeners_node;
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
         * Called when the connection is aborted.
         * This callback corresponds to a transition from CONNECTED
         * to CLOSED state, which happens just before the callback.
         */
        virtual void connectionAborted () = 0;
        
        /**
         * Called for actively opened connections when the connection
         * is established.
         */
        virtual void connectionEstablished () {}
        
        /**
         * Called when some data or FIN has been received.
         * 
         * Each callback corresponds to shifting of the receive
         * buffer by that amount. Zero amount indicates that FIN
         * was received.
         */
        virtual void dataReceived (size_t amount) = 0;
        
        /**
         * Called when some data or FIN has been sent and acknowledged.
         * 
         * Each dataSent callback corresponds to shifting of the send buffer
         * by that amount. Zero amount indicates that FIN was acknowledged.
         */
        virtual void dataSent (size_t amount) = 0;
    };
    
    /**
     * Represents a TCP connection.
     * Conceptually, the connection object has three main states:
     * - INIT: No connection has been made yet.
     * - CONNECTED: There is an active connection, or a connection attempt
     *              is in progress.
     * - CLOSED: There was a connection but is no more.
     */
    class TcpConnection {
        template <typename> friend class IpTcpProto;
        template <typename> friend class IpTcpProto_input;
        template <typename> friend class IpTcpProto_output;
        
        // Flag definitions for m_flags.
        struct Flags { enum : uint8_t {
            STARTED      = 1 << 0, // We are not in INIT state.
            SND_CLOSED   = 1 << 1, // Sending was closed.
            END_SENT     = 1 << 2, // FIN was sent and acked.
            END_RECEIVED = 1 << 3, // FIN was received.
        }; };
        
    public:
        /**
         * Initializes the connection object.
         * The object is initialized in INIT state.
         */
        void init (TcpConnectionCallback *callback)
        {
            AMBRO_ASSERT(callback != nullptr)
            
            m_callback = callback;
            m_pcb = nullptr;
            m_snd_buf = IpBufRef{};
            m_rcv_buf = IpBufRef{};
            m_flags = 0;
        }
        
        /**
         * Deinitializes the connection object.
         */
        void deinit ()
        {
            reset();
        }
        
        /**
         * Resets the connection object.
         * This brings the object to INIT state.
         */
        void reset ()
        {
            if (m_pcb != nullptr) {
                assert_started();
                
                TcpPcb *pcb = m_pcb;
                TcpProto *tcp = pcb->tcp;
                
                // Disassociate with the PCB.
                pcb->con = nullptr;
                m_pcb = nullptr;
                
                // Add the PCB to the unreferenced PCBs list.
                tcp->m_unrefed_pcbs_list.append(tcp->link_model_ref(*pcb), tcp->link_model_state());
                
                // Handle abandonment of connection.
                TcpProto::pcb_con_abandoned(pcb, m_snd_buf.tot_len > 0);
            }
            
            // Reset other variables.
            m_snd_buf = IpBufRef{};
            m_rcv_buf = IpBufRef{};
            m_flags = 0;
        }
        
        /**
         * Accepts a connection available on a listener.
         * This brings the object from the INIT to CONNECTED state.
         * May only be called in INIT state.
         */
        void acceptConnection (TcpListener *lis)
        {
            assert_init();
            AMBRO_ASSERT(lis->m_accept_pcb != nullptr)
            AMBRO_ASSERT(lis->m_accept_pcb->state == TcpState::ESTABLISHED)
            AMBRO_ASSERT(lis->m_accept_pcb->hasFlag(PcbFlags::LIS_LINK))
            AMBRO_ASSERT(lis->m_accept_pcb->lis == lis)
            
            TcpPcb *pcb = lis->m_accept_pcb;
            TcpProto *tcp = pcb->tcp;
            
            // Clear the m_accept_pcb link from the listener.
            lis->m_accept_pcb = nullptr;
            
            // Decrement the listener's PCB count.
            AMBRO_ASSERT(lis->m_num_pcbs > 0)
            lis->m_num_pcbs--;
            
            // Remove the PCB from the unreferenced PCBs list.
            tcp->m_unrefed_pcbs_list.remove(tcp->link_model_ref(*pcb), tcp->link_model_state());
            
            // Clear the LIS_LINK flag of the PCB since we have disassociated
            // it from the listener and will be associating it with this
            // PcbConnection.
            pcb->clearFlag(PcbFlags::LIS_LINK);
            
            // Associate with the PCB.
            m_pcb = pcb;
            pcb->con = this;
            
            // Set STARTED flag to indicate we're no longer in INIT state.
            m_flags = Flags::STARTED;
        }
        
        /**
         * Starts a connection attempt to the given address.
         * When connection is fully established, the connectionEstablished
         * callback will be called. But otherwise a connection that is
         * conneecting does not behave differently from a fully connected
         * connection and for that reason the connectionEstablished callback
         * need not be implemented.
         * On success, this brings the object from the INIT to CONNECTED state.
         * On failure, the object remains in INIT state.
         * May only be called in INIT state.
         */
        IpErr startConnection (TcpProto *tcp, Ip4Addr addr, PortType port, size_t rcv_wnd)
        {
            assert_init();
            
            // Create the PCB for the connection.
            TcpPcb *pcb = nullptr;
            IpErr err = TcpProto::create_connection(this, addr, port, rcv_wnd, &pcb);
            if (err != IpErr::SUCCESS) {
                return err;
            }
            
            // Remember the PCB (the link to us already exists).
            AMBRO_ASSERT(pcb != nullptr)
            AMBRO_ASSERT(pcb->con == this)
            m_pcb = pcb;
            
            // Set STARTED flag to indicate we're no longer in INIT state.
            m_flags = Flags::STARTED;
            
            return IpErr::SUCCESS;
        }
        
        /**
         * Move a connection from another connection object.
         * The source connection object must be in CONNECTED state.
         * This brings this object from the INIT to CONNECTED state.
         * May only be called in INIT state.
         */
        void moveConnection (TcpConnection *src_con)
        {
            assert_init();
            src_con->assert_connected();
            
            // Move the PCB association.
            m_pcb = src_con->m_pcb;
            m_pcb->con = this;
            src_con->m_pcb = nullptr;
            
            // Copy the other state.
            m_snd_buf = src_con->m_snd_buf;
            m_rcv_buf = src_con->m_rcv_buf;
            m_flags = src_con->m_flags;
            
            // Reset other variables in the source.
            src_con->m_snd_buf = IpBufRef{};
            src_con->m_rcv_buf = IpBufRef{};
            src_con->m_flags = 0;
        }
        
        /**
         * Returns whether the object is in INIT state.
         */
        inline bool isInit ()
        {
            return m_flags == 0;
        }
        
        /**
         * Returns whether the object is in CONNECTED state.
         */
        inline bool isConnected ()
        {
            return m_pcb != nullptr;
        }
        
        /**
         * Sets the window update threshold.
         * If the threshold is being raised outside of initializing a new
         * connection, is advised to then call extendRecvBuf(0) which will
         * ensure that a window update is sent if it is now needed.
         * May only be called in CONNECTED state.
         * The threshold value must be positive and not exceed MaxRcvWnd.
         */
        void setWindowUpdateThreshold (SeqType rcv_ann_thres)
        {
            assert_connected();
            AMBRO_ASSERT(rcv_ann_thres > 0)
            AMBRO_ASSERT(rcv_ann_thres <= Constants::MaxRcvWnd)
            
            m_pcb->rcv_ann_thres = rcv_ann_thres;
        }
        
        /**
         * Returns the current receive window.
         * May only be called in CONNECTED state.
         * This is intended to be used when a connection is accepted to determine
         * the minimum amount of receive buffer which must be available.
         */
        inline size_t getReceiveWindow ()
        {
            assert_connected();
            
            // In SYN_SENT we use one less because one was added
            // by create_connection for receiving the SYN.
            SeqType rcv_wnd = m_pcb->rcv_wnd;
            if (m_pcb->state == TcpState::SYN_SENT) {
                AMBRO_ASSERT(rcv_wnd > 0)
                rcv_wnd--;
            }
            
            AMBRO_ASSERT(rcv_wnd <= SIZE_MAX)
            return rcv_wnd;
        }
        
        /**
         * Sets the receive buffer.
         * Typically the application will call this once just after a connection
         * is established.
         * May only be called in CONNECTED or CLOSED state.
         * May only be called when the current receive buffer has zero length.
         */
        void setRecvBuf (IpBufRef rcv_buf)
        {
            assert_started();
            AMBRO_ASSERT(m_rcv_buf.tot_len == 0)
            
            // Set the receive buffer.
            m_rcv_buf = rcv_buf;
            
            if (m_pcb != nullptr) {
                // Inform the input code, so it can update rcv_wnd
                // and possibly send a window update.
                Input::pcb_rcv_buf_extended(m_pcb);
            }
        }
        
        /**
         * Extends the receive buffer for the specified amount.
         * May only be called in CONNECTED or CLOSED state.
         */
        void extendRecvBuf (size_t amount)
        {
            assert_started();
            AMBRO_ASSERT(amount <= SIZE_MAX - m_rcv_buf.tot_len)
            
            // Extend the receive buffer.
            m_rcv_buf.tot_len += amount;
            
            if (m_pcb != nullptr) {
                // Inform the input code, so it can update rcv_wnd
                // and possibly send a window update.
                Input::pcb_rcv_buf_extended(m_pcb);
            }
        }
        
        /**
         * Returns the current receive buffer.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline IpBufRef getRecvBuf ()
        {
            assert_started();
            
            return m_rcv_buf;
        }
        
        /**
         * Returns whether a FIN was received.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline bool wasEndReceived ()
        {
            assert_started();
            
            return (m_flags & Flags::END_RECEIVED) != 0;
        }
        
        /**
         * Returns the amount of send buffer that could remain unsent
         * indefinitely in the absence of sendPush or endSending.
         * Note: currently this does not change for for accepted
         * connections and only possibly decreases for initiated
         * connections, which is fine from a user perspective. However
         * when we implement Path-MTU, the issue of the MSS possibly
         * increasing should be addressed - we shouldn't silently
         * increase this value after returning a promise to the user.
         * May only be called in CONNECTED state.
         */
        inline size_t getSndBufOverhead ()
        {
            assert_connected();
            
            // Sending can be delayed for segmentation only when we have
            // less than the MSS data left to send, hence return mss-1.
            return m_pcb->snd_mss - 1;
        }
        
        /**
         * Sets the send buffer.
         * Typically the application will call this once just after a connection
         * is established.
         * May only be called in CONNECTED or CLOSED state.
         * May only be called before endSending is called.
         * May only be called when the current send buffer has zero length.
         */
        void setSendBuf (IpBufRef snd_buf)
        {
            assert_sending();
            AMBRO_ASSERT(m_snd_buf.tot_len == 0)
            AMBRO_ASSERT(m_pcb == nullptr || m_pcb->snd_buf_cur.tot_len == 0)
            
            // Set the send buffer.
            m_snd_buf = snd_buf;
            
            if (m_pcb != nullptr) {
                // Also update snd_buf_cur. It just needs to be set to the
                // same as we don't allow calling this with nonempty snd_buf.
                m_pcb->snd_buf_cur = snd_buf;
                
                // Inform the output code, so it may send the data.
                Output::pcb_snd_buf_extended(m_pcb);
            }
        }
        
        /**
         * Extends the send buffer for the specified amount.
         * May only be called in CONNECTED or CLOSED state.
         * May only be called before endSending is called.
         */
        void extendSendBuf (size_t amount)
        {
            assert_sending();
            AMBRO_ASSERT(amount <= SIZE_MAX - m_snd_buf.tot_len)
            AMBRO_ASSERT(m_pcb == nullptr || m_pcb->snd_buf_cur.tot_len <= m_snd_buf.tot_len)
            
            // Increment the amount of data in the send buffer.
            m_snd_buf.tot_len += amount;
            
            if (m_pcb != nullptr) {
                // Also adjust snd_buf_cur.
                m_pcb->snd_buf_cur.tot_len += amount;
                
                // Inform the output code, so it may send the data.
                Output::pcb_snd_buf_extended(m_pcb);
            }
        }
        
        /**
         * Returns the current send buffer.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline IpBufRef getSendBuf ()
        {
            assert_started();
            
            return m_snd_buf;
        }
        
        /**
         * Indicates the end of the data stream, i.e. queues a FIN.
         * May only be called in CONNECTED or CLOSED state.
         * May only be called before endSending is called.
         */
        void closeSending ()
        {
            assert_sending();
            
            // Remember that sending is closed.
            m_flags |= Flags::SND_CLOSED;
            
            if (m_pcb != nullptr) {
                // Inform the output code, e.g. to adjust the PCB state
                // and send a FIN. Except in SYN_SENT, in that case the
                // input code will take care of it when the SYN is received.
                if (m_pcb->state != TcpState::SYN_SENT) {
                    Output::pcb_end_sending(m_pcb);
                }
            }
        }
        
        /**
         * Returns whethercloseSending has been called.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline bool wasSendingClosed ()
        {
            assert_started();
            
            return (m_flags & Flags::SND_CLOSED) != 0;
        }
        
        /**
         * Returns whether a FIN was sent and acknowledged.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline bool wasEndSent ()
        {
            assert_started();
            
            return (m_flags & Flags::END_SENT) != 0;
        }
        
        /**
         * Push sending of currently queued data.
         * May only be called in CONNECTED or CLOSED state.
         */
        void sendPush ()
        {
            assert_started();
            
            // Tell the output code to push, if necessary.
            if (m_pcb != nullptr && (m_flags & Flags::SND_CLOSED) == 0) {
                Output::pcb_push_output(m_pcb);
            }
        }
        
    private:
        void assert_init ()
        {
            AMBRO_ASSERT(m_flags == 0)
            AMBRO_ASSERT(m_pcb == nullptr)
        }
        
        void assert_started ()
        {
            AMBRO_ASSERT((m_flags & Flags::STARTED) != 0)
            AMBRO_ASSERT(m_pcb == nullptr || m_pcb->con == this)
            AMBRO_ASSERT(m_pcb == nullptr || m_pcb->state == TcpState::SYN_SENT ||
                state_is_active(m_pcb->state))
            AMBRO_ASSERT(m_pcb == nullptr || m_pcb->state == TcpState::SYN_SENT ||
                snd_open_in_state(m_pcb->state) == ((m_flags & Flags::SND_CLOSED) == 0))
        }
        
        void assert_connected ()
        {
            assert_started();
            AMBRO_ASSERT(m_pcb != nullptr)
        }
        
        void assert_sending ()
        {
            assert_started();
            AMBRO_ASSERT((m_flags & Flags::SND_CLOSED) == 0)
        }
        
    private:
        // These are called by TCP internals when various things happen.
        
        // NOTE: This does not add the PCB to the unreferenced list.
        // It will be done afterward by the caller. This makes sure that
        // any allocate_pcb done from the user callback will not find
        // this PCB.
        void pcb_aborted ()
        {
            assert_connected();
            
            // Disassociate with the PCB.
            TcpPcb *pcb = m_pcb;
            pcb->con = nullptr;
            m_pcb = nullptr;
            
            // Call the application callback.
            m_callback->connectionAborted();
        }
        
        void connection_established ()
        {
            assert_connected();
            
            // Call the application callback.
            m_callback->connectionEstablished();
        }
        
        void data_sent (size_t amount)
        {
            assert_connected();
            AMBRO_ASSERT((m_flags & Flags::END_SENT) == 0)
            AMBRO_ASSERT(amount > 0)
            
            // Call the application callback.
            m_callback->dataSent(amount);
        }
        
        void end_sent ()
        {
            assert_connected();
            AMBRO_ASSERT((m_flags & Flags::END_SENT) == 0)
            AMBRO_ASSERT((m_flags & Flags::SND_CLOSED) != 0)
            
            // Remember that end was sent.
            m_flags |= Flags::END_SENT;
            
            // Call the application callback.
            m_callback->dataSent(0);
        }
        
        void data_received (size_t amount)
        {
            assert_connected();
            AMBRO_ASSERT((m_flags & Flags::END_RECEIVED) == 0)
            AMBRO_ASSERT(amount > 0)
            
            // Call the application callback.
            m_callback->dataReceived(amount);
        }
        
        void end_received ()
        {
            assert_connected();
            AMBRO_ASSERT((m_flags & Flags::END_RECEIVED) == 0)
            
            // Remember that end was received.
            m_flags |= Flags::END_RECEIVED;
            
            // Call the application callback.
            m_callback->dataReceived(0);
        }
        
    private:
        TcpConnectionCallback *m_callback;
        TcpPcb *m_pcb;
        IpBufRef m_snd_buf;
        IpBufRef m_rcv_buf;
        uint8_t m_flags;
    };
};

#include <aipstack/EndNamespace.h>

#endif
