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

#ifndef AMBROLIB_BUSY_EVENT_LOOP_H
#define AMBROLIB_BUSY_EVENT_LOOP_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/Position.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename, typename, typename>
class BusyEventLoopExtra;

template <typename>
class BusyEventLoopQueuedEvent;

template <typename Position, typename ExtraPosition, typename TContext, typename Extra>
class BusyEventLoop
: private DebugObject<TContext, void>
{
public:
    using Context = TContext;
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef BusyEventLoopQueuedEvent<BusyEventLoop> QueuedEvent;
    using FastHandlerType = void (*) (Context);
    
private:
    static BusyEventLoop * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    static void init (Context c)
    {
        BusyEventLoop *o = self(c);
#ifdef AMBROLIB_SUPPORT_QUIT
        o->m_quitting = false;
#endif
        o->m_now = c.clock()->getTime(c);
        o->m_queued_event_list.init();
        extra(c)->m_fast_event_pos = 0;
        for (typename Extra::FastEventSizeType i = 0; i < Extra::NumFastEvents; i++) {
            extra(c)->m_fast_events[i].not_triggered = true;
        }
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        BusyEventLoop *o = self(c);
        o->debugDeinit(c);
        AMBRO_ASSERT(o->m_queued_event_list.isEmpty())
    }
    
    static void run (Context c)
    {
        BusyEventLoop *o = self(c);
        o->debugAccess(c);
        
        while (1) {
            for (typename Extra::FastEventSizeType i = 0; i < Extra::NumFastEvents; i++) {
                extra(c)->m_fast_event_pos++;
                if (AMBRO_UNLIKELY(extra(c)->m_fast_event_pos == Extra::NumFastEvents)) {
                    extra(c)->m_fast_event_pos = 0;
                }
                cli();
                if (!extra(c)->m_fast_events[extra(c)->m_fast_event_pos].not_triggered) {
                    extra(c)->m_fast_events[extra(c)->m_fast_event_pos].not_triggered = true;
                    sei();
                    extra(c)->m_fast_events[extra(c)->m_fast_event_pos].handler(c);
                    break;
                }
                sei();
            }
            
        again:;
            TimeType now = c.clock()->getTime(c);
            o->m_now = now;
            for (QueuedEvent *ev = o->m_queued_event_list.first(); ev; ev = o->m_queued_event_list.next(ev)) {
                AMBRO_ASSERT(!QueuedEventList::isRemoved(ev))
                if ((TimeType)(now - ev->m_time) < UINT32_C(0x80000000)) {
                    o->m_queued_event_list.remove(ev);
                    QueuedEventList::markRemoved(ev);
                    ev->m_handler(ev, c);
#ifdef AMBROLIB_SUPPORT_QUIT
                    if (o->m_quitting) {
                        return;
                    }
#endif
                    goto again;
                }
            }
        }
    }
    
#ifdef AMBROLIB_SUPPORT_QUIT
    static void quit (Context c)
    {
        BusyEventLoop *o = self(c);
        o->m_quitting = true;
    }
#endif
    
    template <typename Id>
    struct FastEventSpec {};
    
    template <typename EventSpec>
    static void initFastEvent (Context c, FastHandlerType handler)
    {
        BusyEventLoop *o = self(c);
        o->debugAccess(c);
        
        extra(c)->m_fast_events[Extra::template get_event_index<EventSpec>()].handler = handler;
    }
    
    template <typename EventSpec>
    static void resetFastEvent (Context c)
    {
        BusyEventLoop *o = self(c);
        o->debugAccess(c);
        
        extra(c)->m_fast_events[Extra::template get_event_index<EventSpec>()].not_triggered = true;
    }
    
    template <typename EventSpec, typename ThisContext>
    static void triggerFastEvent (ThisContext c)
    {
        BusyEventLoop *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            extra(c)->m_fast_events[Extra::template get_event_index<EventSpec>()].not_triggered = false;
        }
    }
    
private:
    template <typename>
    friend class BusyEventLoopQueuedEvent;
    
    typedef DoubleEndedList<QueuedEvent, &QueuedEvent::m_list_node> QueuedEventList;
    
    static Extra * extra (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, ExtraPosition>(c.root());
    }
    
#ifdef AMBROLIB_SUPPORT_QUIT
    bool m_quitting;
#endif
    TimeType m_now;
    QueuedEventList m_queued_event_list;
};

template <typename Position, typename Loop, typename FastEventList>
class BusyEventLoopExtra {
    friend Loop;
    
    static const int NumFastEvents = TypeListLength<FastEventList>::value;
    using FastEventSizeType = typename ChooseInt<BitsInInt<NumFastEvents>::value, false>::Type;
    
    struct FastEventState {
        bool not_triggered;
        typename Loop::FastHandlerType handler;
    };
    
    template <typename EventSpec>
    static constexpr FastEventSizeType get_event_index ()
    {
        return TypeListIndex<FastEventList, IsEqualFunc<EventSpec>>::value;
    }
    
    FastEventSizeType m_fast_event_pos;
    FastEventState m_fast_events[NumFastEvents];
};

template <typename Loop>
class BusyEventLoopQueuedEvent
: private DebugObject<typename Loop::Context, BusyEventLoopQueuedEvent<Loop>>
{
public:
    typedef typename Loop::Context Context;
    typedef typename Loop::TimeType TimeType;
    typedef void (*HandlerType) (BusyEventLoopQueuedEvent *, Context);
    
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
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
        }
    }
    
    void appendAt (Context c, TimeType time)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
        }
        l->m_queued_event_list.append(this);
        m_time = time;
    }
    
    void appendAfterPrevious (Context c, TimeType after_time)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        
        l->m_queued_event_list.append(this);
        m_time += after_time;
    }
    
    void appendNowNotAlready (Context c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        l->m_queued_event_list.append(this);
        m_time = l->m_now;
    }
    
    void prependNowNotAlready (Context c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        l->m_queued_event_list.prepend(this);
        m_time = l->m_now;
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            l->m_queued_event_list.remove(this);
            Loop::QueuedEventList::markRemoved(this);
        }
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::QueuedEventList::isRemoved(this);
    }
    
private:
    friend Loop;
    
    HandlerType m_handler;
    TimeType m_time;
    DoubleEndedListNode<BusyEventLoopQueuedEvent> m_list_node;
};

#include <aprinter/EndNamespace.h>

#endif
