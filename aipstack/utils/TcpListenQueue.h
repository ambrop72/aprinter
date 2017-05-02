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

#ifndef APRINTER_TCP_LISTEN_QUEUE_H
#define APRINTER_TCP_LISTEN_QUEUE_H

#include <stddef.h>

#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/misc/ClockUtils.h>

#include <aipstack/misc/Buf.h>

#include <aipstack/BeginNamespace.h>

template <
    typename Context,
    typename TcpProto,
    size_t RxBufferSize
>
class TcpListenQueue {
    using TimeType = typename Context::Clock::TimeType;
    using TheClockUtils = APrinter::ClockUtils<Context>;
    APRINTER_USE_TYPES1(TcpProto, (TcpListenParams, TcpListener,
                                   TcpListenerCallback, TcpConnection))
    
    static_assert(RxBufferSize > 0, "");
    
public:
    class QueuedListener;
    
    class ListenQueueEntry :
        private TcpConnection
    {
        friend class QueuedListener;
        
    private:
        void init (QueuedListener *listener)
        {
            m_listener = listener;
            TcpConnection::init();
            m_rx_buf_node = IpBufNode{m_rx_buf, RxBufferSize, nullptr};
        }
        
        void deinit ()
        {
            TcpConnection::deinit();
        }
        
        void accept_connection ()
        {
            AMBRO_ASSERT(TcpConnection::isInit())
            AMBRO_ASSERT(m_listener->m_queue_size > 0)
            
            if (!TcpConnection::acceptConnection(&m_listener->m_listener)) {
                return;
            }
            
            TcpConnection::setRecvBuf(IpBufRef{&m_rx_buf_node, 0, RxBufferSize});
            
            m_time = Context::Clock::getTime(Context());
            m_ready = false;
            
            // Added a not-ready connection -> update timeout.
            m_listener->update_timeout();
        }
        
        void reset_connection ()
        {
            AMBRO_ASSERT(!TcpConnection::isInit())
            
            TcpConnection::reset();
            
            if (!m_ready) {
                // Removed a not-ready connection -> update timeout.
                m_listener->update_timeout();
            }
        }
        
        IpBufRef get_received_data ()
        {
            AMBRO_ASSERT(!TcpConnection::isInit())
            
            size_t rx_buf_len = TcpConnection::getRecvBuf().tot_len;
            AMBRO_ASSERT(rx_buf_len <= RxBufferSize)
            size_t rx_len = RxBufferSize - rx_buf_len;
            return IpBufRef{&m_rx_buf_node, 0, rx_len};
        }
        
    private:
        void connectionAborted () override final
        {
            AMBRO_ASSERT(!TcpConnection::isInit())
            
            reset_connection();
        }
        
        void dataReceived (size_t amount) override final
        {
            AMBRO_ASSERT(!TcpConnection::isInit())
            
            // If we get a FIN without any data, abandon the connection.
            if (amount == 0 && TcpConnection::getRecvBuf().tot_len == RxBufferSize) {
                reset_connection();
                return;
            }
            
            if (!m_ready) {
                // Some data has been received, connection is now ready.
                m_ready = true;
                
                // Non-ready connection changed to ready -> update timeout.
                m_listener->update_timeout();
                
                // Try to hand over ready connections.
                m_listener->dequeue_event_handler(Context());
            }
        }
        
        void dataSent (size_t) override final
        {
            AMBRO_ASSERT(false) // nothing was sent so this cannot be called
        }
        
    private:
        QueuedListener *m_listener;
        TimeType m_time;
        IpBufNode m_rx_buf_node;
        bool m_ready;
        char m_rx_buf[RxBufferSize];
    };
    
    struct ListenQueueParams {
        size_t min_rcv_buf_size;
        int queue_size;
        TimeType queue_timeout;
        ListenQueueEntry *queue_entries;
    };
    
    class QueuedListenerCallback {
    public:
        virtual void connectionEstablished (QueuedListener *lis) = 0;
    };
    
    class QueuedListener :
        private TcpListenerCallback
    {
        friend class ListenQueueEntry;
        
    public:
        void init (QueuedListenerCallback *callback)
        {
            Context c;
            AMBRO_ASSERT(callback != nullptr)
            
            m_dequeue_event.init(c, APRINTER_CB_OBJFUNC_T(&QueuedListener::dequeue_event_handler, this));
            m_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&QueuedListener::timeout_event_handler, this));
            m_listener.init(this);
            m_callback = callback;
        }
        
        void deinit ()
        {
            deinit_queue();
            m_listener.deinit();
            m_timeout_event.deinit(Context());
            m_dequeue_event.deinit(Context());
        }
        
        void reset ()
        {
            deinit_queue();
            m_dequeue_event.unset(Context());
            m_timeout_event.unset(Context());
            m_listener.reset();
        }
        
        bool startListening (TcpProto *tcp, TcpListenParams const &params, ListenQueueParams const &q_params)
        {
            AMBRO_ASSERT(!m_listener.isListening())
            AMBRO_ASSERT(q_params.queue_size >= 0)
            AMBRO_ASSERT(q_params.queue_size == 0 || q_params.queue_entries != nullptr)
            AMBRO_ASSERT(q_params.queue_size == 0 || q_params.min_rcv_buf_size >= RxBufferSize)
            
            // Start listening.
            if (!m_listener.startListening(tcp, params)) {
                return false;
            }
            
            // Init queue variables.
            m_queue = q_params.queue_entries;
            m_queue_size = q_params.queue_size;
            m_queue_timeout = q_params.queue_timeout;
            m_queued_to_accept = nullptr;
            
            // Init queue entries.
            for (int i = 0; i < m_queue_size; i++) {
                m_queue[i].init(this);
            }
            
            // Set the initial receive window.
            size_t initial_rx_window = (m_queue_size == 0) ? q_params.min_rcv_buf_size : RxBufferSize;
            m_listener.setInitialReceiveWindow(initial_rx_window);
            
            return true;
        }
        
        void scheduleDequeue ()
        {
            AMBRO_ASSERT(m_listener.isListening())
            
            if (m_queue_size > 0) {
                m_dequeue_event.prependNow(Context());
            }
        }
        
        // NOTE: If m_queue_size>0, there are complications that you
        // must deal with:
        // - Any initial data which has already been received will be returned
        //   in initial_rx_data. You must copy this data immediately after this
        //   function returns and process it correctly.
        // - You must also immediately copy the contents of the existing remaining
        //   receive buffer (getRecvBuf) to your own receive buffer before calling
        //   setRecvBuf to set your receive buffer. This is because out-of-sequence
        //   data may have been stored there.
        // - A FIN may already have been received. If so you will not get a
        //   dataReceived(0) callback.
        bool acceptConnection (TcpConnection &dst_con, IpBufRef &initial_rx_data)
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(dst_con.isInit())
            
            if (m_queue_size == 0) {
                AMBRO_ASSERT(m_listener.hasAcceptPending())
                
                initial_rx_data = IpBufRef{};
                return dst_con.acceptConnection(&m_listener);
            } else {
                AMBRO_ASSERT(m_queued_to_accept != nullptr)
                AMBRO_ASSERT(!m_queued_to_accept->TcpConnection::isInit())
                AMBRO_ASSERT(m_queued_to_accept->m_ready)
                
                ListenQueueEntry *entry = m_queued_to_accept;
                m_queued_to_accept = nullptr;
                
                initial_rx_data = entry->get_received_data();
                dst_con.moveConnection(entry);
                return true;
            }
        }
        
    private:
        void connectionEstablished (TcpListener *lis) override final
        {
            AMBRO_ASSERT(lis == &m_listener)
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_listener.hasAcceptPending())
            
            if (m_queue_size == 0) {
                // Call the accept callback so the user can call acceptConnection.
                m_callback->connectionEstablished(this);
            } else {
                // Try to accept the connection into the queue.
                for (int i = 0; i < m_queue_size; i++) {
                    ListenQueueEntry &entry = m_queue[i];
                    if (entry.TcpConnection::isInit()) {
                        entry.accept_connection();
                        break;
                    }
                }
            }
            
            // If the connection was not accepted, it will be aborted.
        }
        
        void dequeue_event_handler (Context)
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            AMBRO_ASSERT(m_queued_to_accept == nullptr)
            
            // Try to dispatch the oldest ready connections.
            while (ListenQueueEntry *entry = find_oldest(true)) {
                AMBRO_ASSERT(!entry->TcpConnection::isInit())
                AMBRO_ASSERT(entry->m_ready)
                
                // Call the accept handler, while publishing the connection.
                m_queued_to_accept = entry;
                m_callback->connectionEstablished(this);
                m_queued_to_accept = nullptr;
                
                // If the connection was not taken, stop trying.
                if (!entry->TcpConnection::isInit()) {
                    break;
                }
            }
        }
        
        void update_timeout ()
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            ListenQueueEntry *entry = find_oldest(false);
            
            if (entry != nullptr) {
                TimeType expire_time = entry->m_time + m_queue_timeout;
                m_timeout_event.appendAt(Context(), expire_time);
            } else {
                m_timeout_event.unset(Context());
            }
        }
        
        void timeout_event_handler (Context)
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            // We must have a non-ready connection since we keep the timeout
            // always updated to expire for the oldest non-ready connection
            // (or not expire if there is none).
            ListenQueueEntry *entry = find_oldest(false);
            AMBRO_ASSERT(entry != nullptr)
            AMBRO_ASSERT(!entry->TcpConnection::isInit())
            AMBRO_ASSERT(!entry->m_ready)
            
            // Reset the oldest non-ready connection.
            entry->reset_connection();
        }
        
        void deinit_queue ()
        {
            if (m_listener.isListening()) {
                for (int i = 0; i < m_queue_size; i++) {
                    m_queue[i].deinit();
                }
            }
        }
        
        ListenQueueEntry * find_oldest (bool ready)
        {
            ListenQueueEntry *oldest_entry = nullptr;
            
            for (int i = 0; i < m_queue_size; i++) {
                ListenQueueEntry &entry = m_queue[i];
                if (!entry.TcpConnection::isInit() && entry.m_ready == ready &&
                    (oldest_entry == nullptr ||
                     !TheClockUtils::timeGreaterOrEqual(entry.m_time, oldest_entry->m_time)))
                {
                    oldest_entry = &entry;
                }
            }
            
            return oldest_entry;
        }
        
        typename Context::EventLoop::QueuedEvent m_dequeue_event;
        typename Context::EventLoop::TimedEvent m_timeout_event;
        QueuedListenerCallback *m_callback;
        TcpListener m_listener;
        ListenQueueEntry *m_queue;
        int m_queue_size;
        TimeType m_queue_timeout;
        ListenQueueEntry *m_queued_to_accept;
    };
};

#include <aipstack/EndNamespace.h>

#endif
