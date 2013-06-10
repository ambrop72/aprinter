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

#ifndef AMBROLIB_EVENT_LOOP_H
#define AMBROLIB_EVENT_LOOP_H

#include <stdint.h>
#include <stddef.h>

#include <avr/interrupt.h>

#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename>
class EventLoopQueuedEvent;

template <typename Params>
class EventLoop
: private DebugObject<typename Params::Context, EventLoop<Params>>
{
public:
    typedef typename Params::Context Context;
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    
    void init (Context c)
    {
#ifdef AMBROLIB_SUPPORT_QUIT
        m_quitting = false;
#endif
        m_now = c.clock()->getTime(c);
        m_queued_event_list.init();
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AMBRO_ASSERT(m_queued_event_list.isEmpty())
    }
    
    void run (Context c)
    {
        this->debugAccess(c);
        
        while (1) {
            TimeType now = c.clock()->getTime(c);
            TimeType ref = now - Clock::past;
            
            cli();
            m_now = now;
            for (QueuedEvent *ev = m_queued_event_list.first(); ev; ev = m_queued_event_list.next(ev)) {
                AMBRO_ASSERT(!QueuedEventList::isRemoved(ev))
                if ((TimeType)(ev->m_time - ref) < Clock::past) {
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
    friend class EventLoopQueuedEvent;
    
    typedef EventLoopQueuedEvent<EventLoop> QueuedEvent;
    typedef DoubleEndedList<QueuedEvent, &QueuedEvent::m_list_node> QueuedEventList;
    
#ifdef AMBROLIB_SUPPORT_QUIT
    bool m_quitting;
#endif
    TimeType m_now;
    QueuedEventList m_queued_event_list;
};

template <typename Loop>
class EventLoopQueuedEvent
: private DebugObject<typename Loop::Context, EventLoopQueuedEvent<Loop>>
{
public:
    typedef typename Loop::Context Context;
    typedef typename Loop::TimeType TimeType;
    typedef void (*HandlerType) (EventLoopQueuedEvent *, Context);
    
    void init (Context c, HandlerType handler)
    {
        m_handler = handler;
        Loop::QueuedEventList::markRemoved(this);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        Loop *l = c.eventLoop();
        
        cli();
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
        }
        sei();
    }
    
    void appendAt (Context c, TimeType time, bool interrupt)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        if (!interrupt) cli();
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
        }
        l->m_queued_event_list.append(this);
        m_time = time;
        if (!interrupt) sei();
    }
    
    void appendNow (Context c, bool interrupt)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        if (!interrupt) cli();
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
        }
        l->m_queued_event_list.append(this);
        m_time = l->m_now;
        if (!interrupt) sei();
    }
    
    void prependAt (Context c, TimeType time, bool interrupt)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        if (!interrupt) cli();
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
        }
        l->m_queued_event_list.prepend(this);
        m_time = time;
        if (!interrupt) sei();
    }
    
    void prependNow (Context c, bool interrupt)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        if (!interrupt) cli();
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
        }
        l->m_queued_event_list.prepend(this);
        m_time = l->m_now;
        if (!interrupt) sei();
    }
    
    void unset (Context c, bool interrupt)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        if (!interrupt) cli();
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
            Loop::QueuedEventList::markRemoved(this);
        }
        if (!interrupt) sei();
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
    DoubleEndedListNode<EventLoopQueuedEvent> m_list_node;
};

#include <aprinter/EndNamespace.h>

#endif
