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
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Callback.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/TimedEventCompat.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename> class BusyEventLoopQueuedEvent;
template <typename> class BusyEventLoopTimedEvent;

template <typename Arg>
class BusyEventLoop {
    using ParentObject = typename Arg::ParentObject;
    using ExtraDelay   = typename Arg::ExtraDelay;
    
    template <typename> friend class BusyEventLoopQueuedEvent;
    template <typename> friend class BusyEventLoopTimedEvent;
    
public:
    struct Object;
    using Context = typename Arg::Context;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TheClockUtils = ClockUtils<Context>;
    using QueuedEvent = BusyEventLoopQueuedEvent<BusyEventLoop>;
    using TimedEventNew = BusyEventLoopTimedEvent<BusyEventLoop>;
    using TimedEvent = TimedEventCompat<TimedEventNew>;
    using FastHandlerType = void (*) (Context);
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->m_queued_event_list.init();
        o->m_timed_event_list.init();
        Delay::extra(c)->m_fast_event_pos = 0;
        for (typename Delay::Extra::FastEventSizeType i = 0; i < Delay::Extra::NumFastEvents; i++) {
            Delay::extra(c)->m_fast_events[i].not_triggered = true;
        }
#ifdef EVENTLOOP_BENCHMARK
        o->m_bench_time = 0;
#endif
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        AMBRO_ASSERT(o->m_queued_event_list.isEmpty())
        AMBRO_ASSERT(o->m_timed_event_list.isEmpty())
    }
    
    static void run (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        dispatch_queued_events(c);
        
        while (1) {
            for (typename Delay::Extra::FastEventSizeType i = 0; i < Delay::Extra::NumFastEvents; i++) {
                Delay::extra(c)->m_fast_event_pos++;
                if (AMBRO_UNLIKELY(Delay::extra(c)->m_fast_event_pos == Delay::Extra::NumFastEvents)) {
                    Delay::extra(c)->m_fast_event_pos = 0;
                }
                cli();
                if (!Delay::extra(c)->m_fast_events[Delay::extra(c)->m_fast_event_pos].not_triggered) {
                    Delay::extra(c)->m_fast_events[Delay::extra(c)->m_fast_event_pos].not_triggered = true;
                    sei();
                    bench_start_measuring(c);
                    Delay::extra(c)->m_fast_events[Delay::extra(c)->m_fast_event_pos].handler(c);
                    dispatch_queued_events(c);
                    c.check();
                    bench_stop_measuring(c);
                    break;
                }
                sei();
            }
            
            TimeType now = Clock::getTime(c);
            
            for (TimedEventNew *tev = o->m_timed_event_list.first(); tev; tev = o->m_timed_event_list.next(*tev)) {
                tev->debugAccess(c);
                AMBRO_ASSERT(!TimedEventList::isRemoved(*tev))
                
                if (TheClockUtils::timeGreaterOrEqual(now, tev->m_time)) {
                    o->m_timed_event_list.remove(*tev);
                    TimedEventList::markRemoved(*tev);
                    bench_start_measuring(c);
                    tev->handleTimerExpired(c);
                    dispatch_queued_events(c);
                    c.check();
                    bench_stop_measuring(c);
                    break;
                }
            }
        }
    }
    
#ifdef EVENTLOOP_BENCHMARK
    static void resetBenchTime (Context c)
    {
        auto *o = Object::self(c);
        o->m_bench_time = 0;
    }
    
    static TimeType getBenchTime (Context c)
    {
        auto *o = Object::self(c);
        return o->m_bench_time;
    }
#endif
    
    template <typename Id>
    struct FastEventSpec {};
    
    template <typename EventSpec>
    static void initFastEvent (Context c, FastHandlerType handler)
    {
        TheDebugObject::access(c);
        
        Delay::extra(c)->m_fast_events[Delay::Extra::template get_event_index<EventSpec>()].handler = handler;
    }
    
    template <typename EventSpec>
    static void resetFastEvent (Context c)
    {
        TheDebugObject::access(c);
        
        Delay::extra(c)->m_fast_events[Delay::Extra::template get_event_index<EventSpec>()].not_triggered = true;
    }
    
    template <typename EventSpec, typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void triggerFastEvent (ThisContext c)
    {
        TheDebugObject::access(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            Delay::extra(c)->m_fast_events[Delay::Extra::template get_event_index<EventSpec>()].not_triggered = false;
        }
    }
    
private:
    using QueuedEventList = LinkedList<QueuedEvent, typename APRINTER_MEMBER_ACCESSOR(&QueuedEvent::m_list_node),
                                       PointerLinkModel<QueuedEvent>, true>;
    
    using TimedEventList = LinkedList<TimedEventNew, typename APRINTER_MEMBER_ACCESSOR(&TimedEventNew::m_list_node),
                                      PointerLinkModel<TimedEventNew>, true>;
    
    struct Delay {
        using Extra = typename ExtraDelay::Type;
        static typename Extra::Object * extra (Context c) { return Extra::Object::self(c); }
    };
    
    static void bench_start_measuring (Context c)
    {
#ifdef EVENTLOOP_BENCHMARK
        auto *o = Object::self(c);
        o->m_bench_enter_time = Clock::getTime(c);
#endif
    }
    
    static void bench_stop_measuring (Context c)
    {
#ifdef EVENTLOOP_BENCHMARK
        auto *o = Object::self(c);
        o->m_bench_time += (TimeType)(Clock::getTime(c) - o->m_bench_enter_time);
#endif
    }
    
    static void dispatch_queued_events (Context c)
    {
        auto *o = Object::self(c);
        
        while (QueuedEvent *qev = o->m_queued_event_list.first()) {
            qev->debugAccess(c);
            AMBRO_ASSERT(qev->m_handler)
            AMBRO_ASSERT(!QueuedEventList::isRemoved(*qev))
            
            o->m_queued_event_list.removeFirst();
            QueuedEventList::markRemoved(*qev);
            
            qev->m_handler(c);
        }
    }
    
public:
    struct Object : public ObjBase<BusyEventLoop, ParentObject, MakeTypeList<TheDebugObject>> {
        QueuedEventList m_queued_event_list;
        TimedEventList m_timed_event_list;
#ifdef EVENTLOOP_BENCHMARK
        TimeType m_bench_time;
        TimeType m_bench_enter_time;
#endif
    };
};

APRINTER_ALIAS_STRUCT_EXT(BusyEventLoopArg, (
    APRINTER_AS_TYPE(Context),
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(ExtraDelay)
), (
    APRINTER_DEF_INSTANCE(BusyEventLoopArg, BusyEventLoop)
))

template <typename Arg>
class BusyEventLoopExtra {
    using ParentObject  = typename Arg::ParentObject;
    using Loop          = typename Arg::Loop;
    using FastEventList = typename Arg::FastEventList;
    
    friend Loop;
    
    static const int NumFastEvents = TypeListLength<FastEventList>::Value;
    using FastEventSizeType = ChooseInt<MaxValue(1, BitsInInt<NumFastEvents>::Value), false>;
    
    struct FastEventState {
        bool not_triggered;
        typename Loop::FastHandlerType handler;
    };
    
    template <typename EventSpec>
    static constexpr FastEventSizeType get_event_index ()
    {
        return TypeListIndex<FastEventList, EventSpec>::Value;
    }
    
public:
    struct Object : public ObjBase<BusyEventLoopExtra, ParentObject, EmptyTypeList> {
        FastEventSizeType m_fast_event_pos;
        FastEventState m_fast_events[NumFastEvents];
    };
};

APRINTER_ALIAS_STRUCT_EXT(BusyEventLoopExtraArg, (
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(Loop),
    APRINTER_AS_TYPE(FastEventList)
), (
    APRINTER_DEF_INSTANCE(BusyEventLoopExtraArg, BusyEventLoopExtra)
))

template <typename Loop>
class BusyEventLoopQueuedEvent
: private SimpleDebugObject<typename Loop::Context>
{
    friend Loop;
    
public:
    using Context = typename Loop::Context;
    using TimeType = typename Loop::TimeType;
    using HandlerType = Callback<void(Context c)>;
    
private:
    LinkedListNode<PointerLinkModel<BusyEventLoopQueuedEvent>> m_list_node;
    HandlerType m_handler;
    
public:
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        m_handler = handler;
        Loop::QueuedEventList::markRemoved(*this);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(*this)) {
            lo->m_queued_event_list.remove(*this);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(*this)) {
            lo->m_queued_event_list.remove(*this);
            Loop::QueuedEventList::markRemoved(*this);
        }
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::QueuedEventList::isRemoved(*this);
    }
    
    void appendNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(*this))
        
        lo->m_queued_event_list.append(*this);
    }
    
    void appendNow (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(*this)) {
            lo->m_queued_event_list.remove(*this);
        }
        lo->m_queued_event_list.append(*this);
    }
    
    void prependNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(*this))
        
        lo->m_queued_event_list.prepend(*this);
    }
    
    void prependNow (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(*this)) {
            lo->m_queued_event_list.remove(*this);
        }
        lo->m_queued_event_list.prepend(*this);
    }
};

template <typename Loop>
class BusyEventLoopTimedEvent
: private SimpleDebugObject<typename Loop::Context>
{
    friend Loop;
    
public:
    using Context = typename Loop::Context;
    using TimeType = typename Loop::TimeType;
    
private:
    LinkedListNode<PointerLinkModel<BusyEventLoopTimedEvent>> m_list_node;
    TimeType m_time;
    
public:
    void init (Context c)
    {
        Loop::TimedEventList::markRemoved(*this);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::TimedEventList::isRemoved(*this)) {
            lo->m_timed_event_list.remove(*this);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::TimedEventList::isRemoved(*this)) {
            lo->m_timed_event_list.remove(*this);
            Loop::TimedEventList::markRemoved(*this);
        }
    }
    
    inline bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::TimedEventList::isRemoved(*this);
    }
    
    inline TimeType getSetTime (Context c)
    {
        this->debugAccess(c);
        
        return m_time;
    }
    
    void appendAtNotAlready (Context c, TimeType time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::TimedEventList::isRemoved(*this))
        
        lo->m_timed_event_list.append(*this);
        m_time = time;
    }
    
    void appendAt (Context c, TimeType time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::TimedEventList::isRemoved(*this)) {
            lo->m_timed_event_list.remove(*this);
        }
        lo->m_timed_event_list.append(*this);
        m_time = time;
    }
    
    void appendNowNotAlready (Context c)
    {
        appendAtNotAlready(c, Context::Clock::getTime(c));
    }
    
    void appendAfter (Context c, TimeType after_time)
    {
        appendAt(c, Context::Clock::getTime(c) + after_time);
    }
    
    void appendAfterNotAlready (Context c, TimeType after_time)
    {
        appendAtNotAlready(c, Context::Clock::getTime(c) + after_time);
    }
    
    void appendAfterPrevious (Context c, TimeType after_time)
    {
        appendAtNotAlready(c, m_time + after_time);
    }
    
protected:
    virtual void handleTimerExpired (Context c) = 0;
};

#include <aprinter/EndNamespace.h>

#endif
