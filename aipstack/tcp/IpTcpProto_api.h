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
#include <string.h>

#include <limits>
#include <type_traits>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/NonCopyable.h>
#include <aprinter/structure/LinkedList.h>

#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Err.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/tcp/TcpUtils.h>

namespace AIpStack {

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template <typename> class IpTcpProto;
template <typename> class IpTcpProto_input;
template <typename> class IpTcpProto_output;
#endif

template <typename TcpProto>
class IpTcpProto_api
{
    APRINTER_USE_TYPES1(TcpUtils, (TcpState, PortType, SeqType))
    APRINTER_USE_VALS(TcpUtils, (state_is_active, snd_open_in_state))
    APRINTER_USE_TYPES1(TcpProto, (TcpPcb, Input, Output, Constants, MtuRef, OosBuffer,
                                   RttType))
    
public:
    class TcpConnection;
    class TcpListener;
    
    /**
     * Structure for listening parameters.
     */
    struct TcpListenParams {
        Ip4Addr addr;
        PortType port;
        int max_pcbs;
    };
    
    /**
     * Represents listening for connections on a specific address and port.
     */
    class TcpListener :
        private APrinter::NonCopyable<TcpListener>
    {
        template <typename> friend class IpTcpProto;
        template <typename> friend class IpTcpProto_input;
        friend class TcpConnection;
        
    public:
        /**
         * Initialize the listener.
         * 
         * Upon init, the listener is in not-listening state, and listenIp4 should
         * be called to start listening.
         */
        TcpListener () :
            m_initial_rcv_wnd(0),
            m_accept_pcb(nullptr),
            m_listening(false)
        {}
        
        /**
         * Deinitialize the listener.
         * 
         * All SYN_RCVD connections associated with this listener will be aborted
         * but any already established connection (those associated with a
         * TcpConnection object) will not be affected.
         */
        ~TcpListener ()
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
                m_tcp->m_listeners_list.remove(*this);
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
         * Return a reference to the TCP protocol.
         * 
         * May only be called when listening.
         */
        TcpProto & getTcp ()
        {
            AMBRO_ASSERT(isListening())
            
            return *m_tcp;
        }
        
        /**
         * Listen on an IPv4 address and port.
         * 
         * Listening on the all-zeros address listens on all local addresses.
         * Must not be called when already listening.
         * Return success/failure to start listening. It can fail only if there
         * is another listener listening on the same pair of address and port.
         */
        bool startListening (TcpProto *tcp, TcpListenParams const &params)
        {
            AMBRO_ASSERT(!m_listening)
            AMBRO_ASSERT(tcp != nullptr)
            AMBRO_ASSERT(params.max_pcbs > 0)
            
            // Check if there is an existing listener listning on this address+port.
            if (tcp->find_listener(params.addr, params.port) != nullptr) {
                return false;
            }
            
            // Start listening.
            m_tcp = tcp;
            m_addr = params.addr;
            m_port = params.port;
            m_max_pcbs = params.max_pcbs;
            m_num_pcbs = 0;
            m_listening = true;
            m_tcp->m_listeners_list.prepend(*this);
            
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
         * Hence the user should generaly use getAnnouncedRcvWnd to determine the
         * initially announced receive window of a new connection. Further, the TCP
         * may still use a smaller initial receive window than configued with this
         * function.
         */
        void setInitialReceiveWindow (size_t rcv_wnd)
        {
            m_initial_rcv_wnd = APrinter::MinValueU(Constants::MaxWindow, rcv_wnd);
        }
        
    protected:
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
        virtual void connectionEstablished () = 0;
        
    private:
        APrinter::LinkedListNode<typename TcpProto::ListenerLinkModel> m_listeners_node;
        TcpProto *m_tcp;
        SeqType m_initial_rcv_wnd;
        TcpPcb *m_accept_pcb;
        Ip4Addr m_addr;
        PortType m_port;
        int m_max_pcbs;
        int m_num_pcbs;
        bool m_listening;
    };
    
    /**
     * Represents a TCP connection.
     * Conceptually, the connection object has three main states:
     * - INIT: No connection has been made yet.
     * - CONNECTED: There is an active connection, or a connection attempt
     *              is in progress.
     * - CLOSED: There was a connection but is no more.
     */
    class TcpConnection :
        private APrinter::NonCopyable<TcpConnection>,
        // MTU reference.
        // It is setup if and only if SYN_SENT or (PCB referenced and can_output_in_state).
        private MtuRef
    {
        template <typename> friend class IpTcpProto;
        template <typename> friend class IpTcpProto_input;
        template <typename> friend class IpTcpProto_output;
        
    public:
        /**
         * Initializes the connection object.
         * The object is initialized in INIT state.
         */
        TcpConnection ()
        {
            m_v.pcb = nullptr;
            reset_flags();
        }
        
        /**
         * Deinitializes the connection object.
         */
        ~TcpConnection ()
        {
            reset();
        }
        
        /**
         * Resets the connection object.
         * This brings the object to INIT state.
         */
        void reset ()
        {
            if (m_v.pcb != nullptr) {
                assert_started();
                
                TcpPcb *pcb = m_v.pcb;
                
                // Reset the MtuRef.
                MtuRef::reset(pcb->tcp->m_stack);
                
                // Disassociate with the PCB.
                pcb->con = nullptr;
                m_v.pcb = nullptr;
                
                // Handle abandonment of connection.
                TcpProto::pcb_abandoned(pcb, m_v.snd_buf.tot_len > 0, m_v.rcv_ann_thres);
            }
            
            reset_flags();
        }
        
        /**
         * Accepts a connection available on a listener.
         * On success, this brings the object from the INIT to CONNECTED state.
         * On failure, the object remains in INIT state.
         * May only be called in INIT state.
         */
        IpErr acceptConnection (TcpListener *lis)
        {
            assert_init();
            AMBRO_ASSERT(lis->m_accept_pcb != nullptr)
            AMBRO_ASSERT(lis->m_accept_pcb->state == TcpState::SYN_RCVD)
            AMBRO_ASSERT(lis->m_accept_pcb->lis == lis)
            
            TcpPcb *pcb = lis->m_accept_pcb;
            TcpProto *tcp = pcb->tcp;
            
            // Setup the MTU reference.
            uint16_t pmtu;
            if (!MtuRef::setup(tcp->m_stack, pcb->remote_addr, nullptr, pmtu)) {
                return IpErr::NO_IPMTU_AVAIL;
            }
            
            // Clear the m_accept_pcb link from the listener.
            lis->m_accept_pcb = nullptr;
            
            // Decrement the listener's PCB count.
            AMBRO_ASSERT(lis->m_num_pcbs > 0)
            lis->m_num_pcbs--;
            
            // Note that the PCB has already been removed from the list of
            // unreferenced PCBs, so we must not try to remove it here.
            
            // Set the PCB state to ESTABLISHED.
            pcb->state = TcpState::ESTABLISHED;
            
            // Associate with the PCB.
            m_v.pcb = pcb;
            pcb->con = this;
            
            // Initialize TcpConnection variables, set STARTED flag.
            setup_common_started();
            
            // Initialize certain sender variables.
            Input::pcb_complete_established_transition(pcb, pmtu);
            
            return IpErr::SUCCESS;
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
            
            // Setup the MTU reference.
            uint16_t pmtu;
            if (!MtuRef::setup(tcp->m_stack, addr, nullptr, pmtu)) {
                return IpErr::NO_IPMTU_AVAIL;
            }
            
            // Create the PCB for the connection.
            TcpPcb *pcb = nullptr;
            IpErr err = tcp->create_connection(this, addr, port, rcv_wnd, pmtu, &pcb);
            if (err != IpErr::SUCCESS) {
                MtuRef::reset(tcp->m_stack);
                return err;
            }
            
            // Remember the PCB (the link to us already exists).
            AMBRO_ASSERT(pcb != nullptr)
            AMBRO_ASSERT(pcb->con == this)
            m_v.pcb = pcb;
            
            // Initialize TcpConnection variables, set STARTED flag.
            setup_common_started();
            
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
            
            static_assert(std::is_trivially_copy_constructible<OosBuffer>::value, "");
            
            // Byte-copy the whole m_v.
            ::memcpy(&m_v, &src_con->m_v, sizeof(m_v));
            
            // Update the PCB association.
            m_v.pcb->con = this;
            
            // Move the MtuRef setup.
            MtuRef::moveFrom(*src_con);
            
            // Reset the source.
            src_con->m_v.pcb = nullptr;
            src_con->reset_flags();
        }
        
        /**
         * Returns whether the object is in INIT state.
         */
        inline bool isInit ()
        {
            return !m_v.started;
        }
        
        /**
         * Returns whether the object is in CONNECTED state.
         */
        inline bool isConnected ()
        {
            return m_v.pcb != nullptr;
        }
        
        /**
         * Return a reference to the TCP protocol.
         * 
         * May only be called in CONNECTED state.
         */
        TcpProto & getTcp ()
        {
            AMBRO_ASSERT(isConnected())
            
            return *m_v.pcb->tcp;
        }
        
        /**
         * Sets the window update threshold.
         * If the threshold is being raised outside of initializing a new
         * connection, is advised to then call extendRecvBuf(0) which will
         * ensure that a window update is sent if it is now needed.
         * May only be called in CONNECTED or CLOSED state.
         * The threshold value must be positive and not exceed MaxRcvWnd.
         */
        void setWindowUpdateThreshold (SeqType rcv_ann_thres)
        {
            assert_started();
            AMBRO_ASSERT(rcv_ann_thres > 0)
            AMBRO_ASSERT(rcv_ann_thres <= Constants::MaxWindow)
            
            m_v.rcv_ann_thres = rcv_ann_thres;
        }
        
        /**
         * Returns the last announced receive window.
         * May only be called in CONNECTED state.
         * This is intended to be used when a connection is accepted to determine
         * the minimum amount of receive buffer which must be available.
         */
        size_t getAnnouncedRcvWnd ()
        {
            assert_connected();
            
            SeqType ann_wnd = m_v.pcb->rcv_ann_wnd;
            
            // In SYN_SENT we subtract one because one was added
            // by create_connection for receiving the SYN.
            if (m_v.pcb->state == TcpState::SYN_SENT) {
                AMBRO_ASSERT(ann_wnd > 0)
                ann_wnd--;
            }
            
            AMBRO_ASSERT(ann_wnd <= std::numeric_limits<size_t>::max())
            return ann_wnd;
        }
        
        /**
         * Sets the receive buffer.
         * Typically the application will call this once just after a connection
         * is established.
         * May only be called in CONNECTED or CLOSED state.
         * If a receive buffer has already been set than the new buffer must be
         * at least as large as the old one and the leading portion corresponding
         * to the old size must have been copied (because it may contain buffered
         * out-of-sequence data).
         */
        void setRecvBuf (IpBufRef rcv_buf)
        {
            assert_started();
            AMBRO_ASSERT(rcv_buf.tot_len >= m_v.rcv_buf.tot_len)
            
            // Set the receive buffer.
            m_v.rcv_buf = rcv_buf;
            
            if (m_v.pcb != nullptr) {
                // Inform the input code, so it can send a window update.
                Input::pcb_rcv_buf_extended(m_v.pcb);
            }
        }
        
        /**
         * Extends the receive buffer for the specified amount.
         * May only be called in CONNECTED or CLOSED state.
         */
        void extendRecvBuf (size_t amount)
        {
            assert_started();
            AMBRO_ASSERT(amount <= std::numeric_limits<size_t>::max() - m_v.rcv_buf.tot_len)
            
            // Extend the receive buffer.
            m_v.rcv_buf.tot_len += amount;
            
            if (m_v.pcb != nullptr) {
                // Inform the input code, so it can send a window update.
                Input::pcb_rcv_buf_extended(m_v.pcb);
            }
        }
        
        /**
         * Returns the current receive buffer.
         * May only be called in CONNECTED or CLOSED state.
         * 
         * When the stack shifts the receive buffer it will move to
         * subsequent buffer nodes eagerly (see IpBufRef::processBytes).
         * This is convenient when using a ring buffer as it guarantees
         * that the offset will remain less than the buffer size.
         */
        inline IpBufRef getRecvBuf ()
        {
            assert_started();
            
            return m_v.rcv_buf;
        }
        
        /**
         * Returns whether a FIN was received.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline bool wasEndReceived ()
        {
            assert_started();
            
            return m_v.end_received;
        }
        
        /**
         * Returns the amount of send buffer that could remain unsent
         * indefinitely in the absence of sendPush or endSending.
         * 
         * For accepted connections, this does not change, and for
         * initiated connections, it only possibly decreases when the
         * connection is established.
         */
        inline size_t getSndBufOverhead ()
        {
            assert_connected();
            
            // Sending can be delayed for segmentation only when we have
            // less than the MSS data left to send, hence return mss-1.
            return m_v.pcb->base_snd_mss - 1;
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
            AMBRO_ASSERT(m_v.snd_buf.tot_len == 0)
            AMBRO_ASSERT(m_v.snd_buf_cur.tot_len == 0)
            
            // Set the send buffer.
            m_v.snd_buf = snd_buf;
            
            // Also update snd_buf_cur. It just needs to be set to the
            // same as we don't allow calling this with nonempty snd_buf.
            m_v.snd_buf_cur = snd_buf;
            
            if (AMBRO_LIKELY(m_v.pcb != nullptr && m_v.snd_buf.tot_len > 0)) {
                // Inform the output code, so it may send the data.
                Output::pcb_snd_buf_extended(m_v.pcb);
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
            AMBRO_ASSERT(amount <= std::numeric_limits<size_t>::max() - m_v.snd_buf.tot_len)
            AMBRO_ASSERT(m_v.snd_buf_cur.tot_len <= m_v.snd_buf.tot_len)
            
            // Increment the amount of data in the send buffer.
            m_v.snd_buf.tot_len += amount;
            
            // Also adjust snd_buf_cur.
            m_v.snd_buf_cur.tot_len += amount;
        
            if (AMBRO_LIKELY(m_v.pcb != nullptr && m_v.snd_buf.tot_len > 0)) {
                // Inform the output code, so it may send the data.
                Output::pcb_snd_buf_extended(m_v.pcb);
            }
        }
        
        /**
         * Returns the current send buffer.
         * May only be called in CONNECTED or CLOSED state.
         * 
         * When the stack shifts the send buffer it will move to
         * subsequent buffer nodes eagerly (see IpBufRef::processBytes).
         * This is convenient when using a ring buffer as it guarantees
         * that the offset will remain less than the buffer size.
         */
        inline IpBufRef getSendBuf ()
        {
            assert_started();
            
            return m_v.snd_buf;
        }
        
        /**
         * Indicates the end of the data stream, i.e. queues a FIN.
         * May only be called in CONNECTED or CLOSED state.
         * May only be called before endSending is called.
         */
        void closeSending ()
        {
            assert_sending();
            
            // Set the push index to the end of the send buffer.
            m_v.snd_psh_index = m_v.snd_buf.tot_len;
            
            // Remember that sending is closed.
            m_v.snd_closed = true;
            
            // Inform the output code, e.g. to adjust the PCB state
            // and send a FIN. But not in SYN_SENT, in that case the
            // input code will take care of it when the SYN is received.
            if (m_v.pcb != nullptr && m_v.pcb->state != TcpState::SYN_SENT) {
                Output::pcb_end_sending(m_v.pcb);
            }
        }
        
        /**
         * Returns whethercloseSending has been called.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline bool wasSendingClosed ()
        {
            assert_started();
            
            return m_v.snd_closed;
        }
        
        /**
         * Returns whether a FIN was sent and acknowledged.
         * May only be called in CONNECTED or CLOSED state.
         */
        inline bool wasEndSent ()
        {
            assert_started();
            
            return m_v.end_sent;
        }
        
        /**
         * Push sending of currently queued data.
         * May only be called in CONNECTED or CLOSED state.
         */
        void sendPush ()
        {
            assert_started();
            
            // No need to do anything after closeSending.
            if (m_v.snd_closed) {
                return;
            }
            
            // Set the push index to the current send buffer size.
            m_v.snd_psh_index = m_v.snd_buf.tot_len;
            
            // Tell the output code to push, if necessary.
            if (m_v.pcb != nullptr && snd_open_in_state(m_v.pcb->state) &&
                m_v.snd_buf.tot_len > 0)
            {
                Output::pcb_push_output(m_v.pcb);
            }
        }
        
    protected:
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
        
    private:
        void assert_init ()
        {
            AMBRO_ASSERT(!m_v.started && !m_v.snd_closed &&
                         !m_v.end_sent && !m_v.end_received)
            AMBRO_ASSERT(m_v.pcb == nullptr)
        }
        
        void assert_started ()
        {
            AMBRO_ASSERT(m_v.started)
            AMBRO_ASSERT(m_v.pcb == nullptr || m_v.pcb->state == TcpState::SYN_SENT ||
                state_is_active(m_v.pcb->state))
            AMBRO_ASSERT(m_v.pcb == nullptr || m_v.pcb->con == this)
            AMBRO_ASSERT(m_v.pcb == nullptr || m_v.pcb->state == TcpState::SYN_SENT ||
                snd_open_in_state(m_v.pcb->state) == !m_v.snd_closed)
        }
        
        void assert_connected ()
        {
            assert_started();
            AMBRO_ASSERT(m_v.pcb != nullptr)
        }
        
        void assert_sending ()
        {
            assert_started();
            AMBRO_ASSERT(!m_v.snd_closed)
        }
        
        void setup_common_started ()
        {
            // Clear buffer variables.
            m_v.snd_buf = IpBufRef{};
            m_v.rcv_buf = IpBufRef{};
            m_v.snd_buf_cur = IpBufRef{};
            m_v.snd_psh_index = 0;
            
            // Initialize rcv_ann_thres.
            m_v.rcv_ann_thres = Constants::DefaultWndAnnThreshold;
            
            // Initialize the out-of-sequence information.
            m_v.ooseq.init();
            
            // Set STARTED flag to indicate we're no longer in INIT state.
            m_v.started = true;
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
            
            TcpPcb *pcb = m_v.pcb;
            
            // Reset the MtuRef.
            MtuRef::reset(pcb->tcp->m_stack);
            
            // Disassociate with the PCB.
            pcb->con = nullptr;
            m_v.pcb = nullptr;
            
            // Call the application callback.
            connectionAborted();
        }
        
        void connection_established ()
        {
            assert_connected();
            
            // Call the application callback.
            connectionEstablished();
        }
        
        void data_sent (size_t amount)
        {
            assert_connected();
            AMBRO_ASSERT(!m_v.end_sent)
            AMBRO_ASSERT(amount > 0)
            
            // Call the application callback.
            dataSent(amount);
        }
        
        void end_sent ()
        {
            assert_connected();
            AMBRO_ASSERT(!m_v.end_sent)
            AMBRO_ASSERT(m_v.snd_closed)
            
            // Remember that end was sent.
            m_v.end_sent = true;
            
            // Call the application callback.
            dataSent(0);
        }
        
        void data_received (size_t amount)
        {
            assert_connected();
            AMBRO_ASSERT(!m_v.end_received)
            AMBRO_ASSERT(amount > 0)
            
            // Call the application callback.
            dataReceived(amount);
        }
        
        void end_received ()
        {
            assert_connected();
            AMBRO_ASSERT(!m_v.end_received)
            
            // Remember that end was received.
            m_v.end_received = true;
            
            // Call the application callback.
            dataReceived(0);
        }
        
        // Callback from MtuRef when the PMTU changes.
        void pmtuChanged (uint16_t pmtu) override final
        {
            assert_connected();
            
            Output::pcb_pmtu_changed(m_v.pcb, pmtu);
        }
        
        void reset_flags ()
        {
            m_v.started      = false;
            m_v.snd_closed   = false;
            m_v.end_sent     = false;
            m_v.end_received = false;
        }
        
    private:
        struct Vars {
            TcpPcb *pcb;
            IpBufRef snd_buf;
            IpBufRef rcv_buf;
            IpBufRef snd_buf_cur;
            SeqType snd_wnd : 30;
            SeqType started : 1;
            SeqType snd_closed : 1;
            SeqType cwnd;
            SeqType ssthresh;
            SeqType cwnd_acked;
            SeqType recover;
            SeqType rcv_ann_thres : 30;
            SeqType end_sent : 1;
            SeqType end_received : 1;
            SeqType rtt_test_seq;
            RttType rttvar;
            RttType srtt;
            OosBuffer ooseq;
            size_t snd_psh_index;
        };
        
        Vars m_v;
    };
};

}

#endif
