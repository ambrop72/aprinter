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
#include <aprinter/base/OffsetCallback.h>
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
    
    void init (Context c)
    {
#ifdef AMBROLIB_SUPPORT_QUIT
        m_quitting = false;
#endif
        m_now = c.clock()->getTime(c);
        m_queued_event_list.init();
        extra()->m_fast_event_pos = 0;
        for (typename Extra::FastEventSizeType i = 0; i < Extra::NumFastEvents; i++) {
            extra()->m_fast_events[i].not_triggered = true;
        }
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
            for (typename Extra::FastEventSizeType i = 0; i < Extra::NumFastEvents; i++) {
                extra()->m_fast_event_pos++;
                if (AMBRO_UNLIKELY(extra()->m_fast_event_pos == Extra::NumFastEvents)) {
                    extra()->m_fast_event_pos = 0;
                }
                cli();
                if (!extra()->m_fast_events[extra()->m_fast_event_pos].not_triggered) {
                    extra()->m_fast_events[extra()->m_fast_event_pos].not_triggered = true;
                    sei();
                    extra()->m_fast_events[extra()->m_fast_event_pos].handler(c);
                    break;
                }
                sei();
            }
            
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
    
    template <typename Id>
    struct FastEventSpec {};
    
    template <typename EventSpec>
    void initFastEvent (Context c, FastHandlerType handler)
    {
        this->debugAccess(c);
        
        extra()->m_fast_events[Extra::template get_event_index<EventSpec>()].handler = handler;
    }
    
    template <typename EventSpec>
    void resetFastEvent (Context c)
    {
        this->debugAccess(c);
        
        extra()->m_fast_events[Extra::template get_event_index<EventSpec>()].not_triggered = true;
    }
    
    template <typename EventSpec, typename ThisContext>
    void triggerFastEvent (ThisContext c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            extra()->m_fast_events[Extra::template get_event_index<EventSpec>()].not_triggered = false;
        });
    }
    
private:
    template <typename>
    friend class BusyEventLoopQueuedEvent;
    
    typedef DoubleEndedList<QueuedEvent, &QueuedEvent::m_list_node> QueuedEventList;
    
    Extra * extra ()
    {
        return PositionTraverse<Position, ExtraPosition>(this);
    }
    
#ifdef AMBROLIB_SUPPORT_QUIT
    bool m_quitting;
#endif
    TimeType m_now;
    QueuedEventList m_queued_event_list;
    InterruptLock<Context> m_lock;
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
    
    void mainOnlyAppendAfterPrevious (Context c, TimeType after_time)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        
        l->m_queued_event_list.append(this);
        m_time += after_time;
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
    void appendNowIfNotAlready (ThisContext c)
    {
        this->debugAccess(c);
        Loop *l = c.eventLoop();
        
        AMBRO_LOCK_T(l->m_lock, c, lock_c, {
            if (Loop::QueuedEventList::isRemoved(this)) {
                l->m_queued_event_list.append(this);
                m_time = l->m_now;
            }
        });
    }
    
    inline void appendNowIfNotAlready (InterruptContext<Context> c) __attribute__((always_inline));
    
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
    DoubleEndedListNode<BusyEventLoopQueuedEvent> m_list_node;
};

template <typename Loop>
inline void BusyEventLoopQueuedEvent<Loop>::appendNowIfNotAlready (InterruptContext<Context> c)
{
    this->debugAccess(c);
    Loop *l = c.eventLoop();
    
    if (AMBRO_LIKELY(Loop::QueuedEventList::isRemoved(this))) {
        l->m_queued_event_list.appendInline(this);
        m_time = l->m_now;
    }
}

template <typename Context, typename Handler>
class BusyEventLoopTimer {
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef Context HandlerContext;
    
    void init (Context c)
    {
        m_queued_event.init(c, AMBRO_OFFSET_CALLBACK_T(&BusyEventLoopTimer::m_queued_event, &BusyEventLoopTimer::queued_event_handler));
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
    
    BusyEventLoopQueuedEvent<typename Context::EventLoop> m_queued_event;
};

#include <aprinter/EndNamespace.h>

#endif
