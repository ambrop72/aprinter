/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AMBROLIB_AVR_EVENT_LOOP_H
#define AMBROLIB_AVR_EVENT_LOOP_H

#include <stdint.h>
#include <stddef.h>

#include <avr/interrupt.h>

#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/system/AvrLock.h>

#include <aprinter/BeginNamespace.h>

template <typename>
class AvrEventLoopQueuedEvent;

template <typename Params>
class AvrEventLoop
: private DebugObject<typename Params::Context, AvrEventLoop<Params>>
{
public:
    typedef typename Params::Context Context;
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AvrEventLoopQueuedEvent<AvrEventLoop> QueuedEvent;
    
    void init (Context c)
    {
#ifdef AMBROLIB_SUPPORT_QUIT
        m_quitting = false;
#endif
        m_now = c.clock()->getTime(c);
        m_queued_event_list.init();
        m_lock.init(c);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        m_lock.deinit(c);
        AMBRO_ASSERT(m_queued_event_list.isEmpty())
    }
    
    void run (Context c)
    {
        this->debugAccess(c);
        
        while (1) {
            TimeType now = c.clock()->getTime(c);
            
            cli();
            m_now = now;
            for (QueuedEvent *ev = m_queued_event_list.first(); ev; ev = m_queued_event_list.next(ev)) {
                AMBRO_ASSERT(!QueuedEventList::isRemoved(ev))
                if ((TimeType)(now - ev->m_time) < UINT32_C(0x80000000)) {
                    m_queued_event_list.remove(ev);
                    QueuedEventList::markRemoved(ev);
                    sei();
                    ev->m_handler(ev, c);
#ifdef AMBROLIB_SUPPORT_QUIT
                    if (m_quitting) {
                        return;
                    }
#endif
                    break;
                }
            }
            sei();
        }
    }
    
#ifdef AMBROLIB_SUPPORT_QUIT
    void quit (Context c)
    {
        m_quitting = true;
    }
#endif
    
private:
    template <typename>
    friend class AvrEventLoopQueuedEvent;
    
    typedef DoubleEndedList<QueuedEvent, &QueuedEvent::m_list_node> QueuedEventList;
    
#ifdef AMBROLIB_SUPPORT_QUIT
    bool m_quitting;
#endif
    TimeType m_now;
    QueuedEventList m_queued_event_list;
    AvrLock<Context> m_lock;
};

template <typename Loop>
class AvrEventLoopQueuedEvent
: private DebugObject<typename Loop::Context, AvrEventLoopQueuedEvent<Loop>>
{
public:
    typedef typename Loop::Context Context;
    typedef typename Loop::TimeType TimeType;
    typedef void (*HandlerType) (AvrEventLoopQueuedEvent *, Context);
    
    void init (Context c, HandlerType handler)
    {
        m_handler = handler;
        Loop::QueuedEventList::markRemoved(this);
        
        this->debugInit(c);
    }
    
    template <typename ThisContext>
    void deinit (ThisContext c)
    {
        this->debugDeinit(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            if (!Loop::QueuedEventList::isRemoved(this)) {
                l->m_queued_event_list.remove(this);
            }
        });
    }
    
    template <typename ThisContext>
    void appendAt (ThisContext c, TimeType time)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            if (!Loop::QueuedEventList::isRemoved(this)) {
                l->m_queued_event_list.remove(this);
            }
            l->m_queued_event_list.append(this);
            m_time = time;
        });
    }
    
    template <typename ThisContext>
    void appendNow (ThisContext c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            if (!Loop::QueuedEventList::isRemoved(this)) {
                l->m_queued_event_list.remove(this);
            }
            l->m_queued_event_list.append(this);
            m_time = l->m_now;
        });
    }
    
    template <typename ThisContext>
    void appendNowNotAlready (ThisContext c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
            l->m_queued_event_list.append(this);
            m_time = l->m_now;
        });
    }
    
    template <typename ThisContext>
    void prependAt (ThisContext c, TimeType time)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            if (!Loop::QueuedEventList::isRemoved(this)) {
                l->m_queued_event_list.remove(this);
            }
            l->m_queued_event_list.prepend(this);
            m_time = time;
        });
    }
    
    template <typename ThisContext>
    void prependNow (ThisContext c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            if (!Loop::QueuedEventList::isRemoved(this)) {
                l->m_queued_event_list.remove(this);
            }
            l->m_queued_event_list.prepend(this);
            m_time = l->m_now;
        });
    }
    
    template <typename ThisContext>
    void prependNowNotAlready (ThisContext c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
            l->m_queued_event_list.prepend(this);
            m_time = l->m_now;
        });
    }
    
    template <typename ThisContext>
    void unset (ThisContext c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            if (!Loop::QueuedEventList::isRemoved(this)) {
                l->m_queued_event_list.remove(this);
                Loop::QueuedEventList::markRemoved(this);
            }
        });
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::QueuedEventList::isRemoved(this);
    }
    
    TimeType getSetTime (Context c)
    {
        this->debugAccess(c);
        
        return m_time;
    }
    
private:
    friend Loop;
    
    HandlerType m_handler;
    TimeType m_time;
    DoubleEndedListNode<AvrEventLoopQueuedEvent> m_list_node;
};

template <typename Context, typename Handler>
class AvrEventLoopTimer {
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef Context HandlerContext;
    
    void init (Context c)
    {
        m_queued_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AvrEventLoopTimer::m_queued_event, &AvrEventLoopTimer::queued_event_handler));
    }
    
    void deinit (Context c)
    {
        m_queued_event.deinit(c);
    }
    
    template <typename ThisContext>
    void set (ThisContext c, TimeType time)
    {
        m_queued_event.appendAt(c, time);
    }
    
    template <typename ThisContext>
    void unset (ThisContext c)
    {
        m_queued_event.unset(c);
    }
    
private:
    void queued_event_handler (Context c)
    {
        return Handler::call(this, c);
    }
    
    AvrEventLoopQueuedEvent<typename Context::EventLoop> m_queued_event;
};

#include <aprinter/EndNamespace.h>

#endif
