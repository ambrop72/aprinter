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

#include <aipstack/BeginNamespace.h>

template <
    typename Context,
    typename TcpProto
>
class TcpListenQueue {
    using TimeType = typename Context::Clock::TimeType;
    using TheClockUtils = APrinter::ClockUtils<Context>;
    APRINTER_USE_TYPES1(TcpProto, (TcpListenParams, TcpListener,
                                   TcpListenerCallback, TcpConnection,
                                   TcpConnectionCallback))
    
public:
    class QueuedListener;
    
    class ListenQueueEntry :
        private TcpConnectionCallback
    {
        friend class QueuedListener;
        
    private:
        void connectionAborted () override final
        {
            AMBRO_ASSERT(!m_connection.isInit())
            
            m_connection.reset();
            m_listener->update_timeout();
        }
        
        void dataReceived (size_t) override final
        {
            AMBRO_ASSERT(false) // zero window
        }
        
        void dataSent (size_t) override final
        {
            AMBRO_ASSERT(false) // nothing sent
        }
        
    private:
        QueuedListener *m_listener;
        TcpConnection m_connection;
        TimeType m_time;
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
                ListenQueueEntry *entry = &m_queue[i];
                entry->m_listener = this;
                entry->m_connection.init(entry);
            }
            
            // If there is no queue, raise the initial receive window.
            // If there is a queue, we have to leave it at zero.
            if (m_queue_size == 0) {
                m_listener.setInitialReceiveWindow(q_params.min_rcv_buf_size);
            }
            
            return true;
        }
        
        void scheduleDequeue ()
        {
            AMBRO_ASSERT(m_listener.isListening())
            
            if (m_queue_size > 0) {
                m_dequeue_event.prependNow(Context());
            }
        }
        
        void acceptConnection (TcpConnection *dst_con)
        {
            AMBRO_ASSERT(m_listener.isListening())
            
            if (m_queued_to_accept != nullptr) {
                ListenQueueEntry *entry = m_queued_to_accept;
                dst_con->moveConnection(&entry->m_connection);
            } else {
                dst_con->acceptConnection(&m_listener);
            }
        }
        
    private:
        void connectionEstablished (TcpListener *lis) override final
        {
            AMBRO_ASSERT(lis == &m_listener)
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_listener.hasAcceptPending())
            AMBRO_ASSERT(m_queued_to_accept == nullptr)
            
            // Call the accept callback so the user can call acceptConnection.
            m_callback->connectionEstablished(this);
            
            // If the user did not accept the connection, try to queue it.
            if (m_listener.hasAcceptPending()) {
                for (int i = 0; i < m_queue_size; i++) {
                    ListenQueueEntry *entry = &m_queue[i];
                    if (entry->m_connection.isInit()) {
                        entry->m_connection.acceptConnection(&m_listener);
                        entry->m_time = Context::Clock::getTime(Context());
                        update_timeout();
                        break;
                    }
                }
            }
        }
        
        void dequeue_event_handler (Context)
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            AMBRO_ASSERT(m_queued_to_accept == nullptr)
            
            bool queue_changed = false;
            
            // Find the oldest queued connection.
            ListenQueueEntry *oldest_entry = find_oldest_queued_pcb();
            
            while (oldest_entry != nullptr) {
                AMBRO_ASSERT(!oldest_entry->m_connection.isInit())
                
                // Call the accept handler, while publishing the connection.
                m_queued_to_accept = oldest_entry;
                m_callback->connectionEstablished(this);
                m_queued_to_accept = nullptr;
                
                // If the connection was not taken, stop trying.
                if (!oldest_entry->m_connection.isInit()) {
                    break;
                }
                
                queue_changed = true;
                
                // Refresh the oldest entry.
                oldest_entry = find_oldest_queued_pcb();
            }
            
            // Update the dequeue timeout if we dequeued any connection.
            if (queue_changed) {
                update_timeout(oldest_entry);
            }
        }
        
        void update_timeout ()
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            update_timeout(find_oldest_queued_pcb());
        }
        
        void update_timeout (ListenQueueEntry *oldest_entry)
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            if (oldest_entry != nullptr) {
                TimeType expire_time = oldest_entry->m_time + m_queue_timeout;
                m_timeout_event.appendAt(Context(), expire_time);
            } else {
                m_timeout_event.unset(Context());
            }
        }
        
        void timeout_event_handler (Context)
        {
            AMBRO_ASSERT(m_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            // The oldest queued connection has expired, close it.
            ListenQueueEntry *entry = find_oldest_queued_pcb();
            AMBRO_ASSERT(entry != nullptr)
            entry->m_connection.reset();
            update_timeout();
        }
        
        void deinit_queue ()
        {
            if (m_listener.isListening()) {
                for (int i = 0; i < m_queue_size; i++) {
                    ListenQueueEntry *entry = &m_queue[i];
                    entry->m_connection.deinit();
                }
            }
        }
        
        ListenQueueEntry * find_oldest_queued_pcb ()
        {
            ListenQueueEntry *oldest_entry = nullptr;
            for (int i = 0; i < m_queue_size; i++) {
                ListenQueueEntry *entry = &m_queue[i];
                if (!entry->m_connection.isInit() &&
                    (oldest_entry == nullptr ||
                     !TheClockUtils::timeGreaterOrEqual(entry->m_time, oldest_entry->m_time)))
                {
                    oldest_entry = entry;
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
