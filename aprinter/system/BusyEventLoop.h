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
#include <aprinter/base/Object.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/WrapType.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Inline.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename, typename, typename> class BusyEventLoopExtra;
template <typename> class BusyEventLoopQueuedEvent;
template <typename> class BusyEventLoopTimedEvent;

template <typename TContext, typename ParentObject, typename ExtraDelay>
class BusyEventLoop {
public:
    struct Object;
    using Context = TContext;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TheClockUtils = ClockUtils<Context>;
    using QueuedEvent = BusyEventLoopQueuedEvent<BusyEventLoop>;
    using TimedEvent = BusyEventLoopTimedEvent<BusyEventLoop>;
    using FastHandlerType = void (*) (Context);
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
#ifdef AMBROLIB_SUPPORT_QUIT
        o->m_quitting = false;
#endif
        o->m_event_list.init();
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
        AMBRO_ASSERT(o->m_event_list.isEmpty())
    }
    
    static void run (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
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
                    c.check();
                    bench_stop_measuring(c);
                    break;
                }
                sei();
            }
            
        again:;
            TimeType now = Clock::getTime(c);
            for (BaseEventStruct *ev = o->m_event_list.first(); ev; ev = o->m_event_list.next(ev)) {
                AMBRO_ASSERT(!EventList::isRemoved(ev))
                if (ev->handler_or_hack || TheClockUtils::timeGreaterOrEqual(now, static_cast<TimedEventStruct *>(ev)->time)) {
                    o->m_event_list.remove(ev);
                    EventList::markRemoved(ev);
                    bench_start_measuring(c);
                    if (ev->handler_or_hack) {
                        ev->handler_or_hack(c);
                    } else {
                        auto handler = EventHandlerType::Make(static_cast<TimedEventStruct *>(ev)->handler_func, ev->handler_or_hack.m_arg);
                        handler(c);
                    }
                    c.check();
                    bench_stop_measuring(c);
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
    
#ifdef AMBROLIB_SUPPORT_QUIT
    static void quit (Context c)
    {
        auto *o = Object::self(c);
        o->m_quitting = true;
    }
#endif
    
    template <typename Id>
    struct FastEventSpec {};
    
    template <typename EventSpec>
    static void initFastEvent (Context c, FastHandlerType handler)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        Delay::extra(c)->m_fast_events[Delay::Extra::template get_event_index<EventSpec>()].handler = handler;
    }
    
    template <typename EventSpec>
    static void resetFastEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        Delay::extra(c)->m_fast_events[Delay::Extra::template get_event_index<EventSpec>()].not_triggered = true;
    }
    
    template <typename EventSpec, typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void triggerFastEvent (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            Delay::extra(c)->m_fast_events[Delay::Extra::template get_event_index<EventSpec>()].not_triggered = false;
        }
    }
    
private:
    template <typename> friend class BusyEventLoopQueuedEvent;
    template <typename> friend class BusyEventLoopTimedEvent;
    
    using EventHandlerType = Callback<void(Context)>;
    
    struct BaseEventStruct {
        EventHandlerType handler_or_hack;
        DoubleEndedListNode<BaseEventStruct> list_node;
    };
    
    struct TimedEventStruct : public BaseEventStruct {
        typename EventHandlerType::FuncType handler_func;
        TimeType time;
    };
    
    using EventList = DoubleEndedList<BaseEventStruct, &BaseEventStruct::list_node>;
    
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
    
public:
    struct Object : public ObjBase<BusyEventLoop, ParentObject, MakeTypeList<TheDebugObject>> {
#ifdef AMBROLIB_SUPPORT_QUIT
        bool m_quitting;
#endif
        EventList m_event_list;
#ifdef EVENTLOOP_BENCHMARK
        TimeType m_bench_time;
        TimeType m_bench_enter_time;
#endif
    };
};

template <typename ParentObject, typename Loop, typename FastEventList>
class BusyEventLoopExtra {
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

template <typename Loop>
class BusyEventLoopQueuedEvent
: private SimpleDebugObject<typename Loop::Context>, private Loop::BaseEventStruct
{
public:
    using Context = typename Loop::Context;
    using TimeType = typename Loop::TimeType;
    using HandlerType = Callback<void(Context c)>;
    
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        this->handler_or_hack = handler;
        Loop::EventList::markRemoved(this);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::EventList::isRemoved(this)) {
            lo->m_event_list.remove(this);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::EventList::isRemoved(this)) {
            lo->m_event_list.remove(this);
            Loop::EventList::markRemoved(this);
        }
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::EventList::isRemoved(this);
    }
    
    void appendNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::EventList::isRemoved(this))
        
        lo->m_event_list.append(this);
    }
    
    void prependNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::EventList::isRemoved(this))
        
        lo->m_event_list.prepend(this);
    }
    
    void prependNow (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::EventList::isRemoved(this)) {
            lo->m_event_list.remove(this);
        }
        lo->m_event_list.prepend(this);
    }
};

template <typename Loop>
class BusyEventLoopTimedEvent
: private SimpleDebugObject<typename Loop::Context>, private Loop::TimedEventStruct
{
public:
    using Context = typename Loop::Context;
    using TimeType = typename Loop::TimeType;
    using HandlerType = Callback<void(Context c)>;
    
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        this->handler_or_hack = HandlerType::Make(nullptr, handler.m_arg);
        Loop::EventList::markRemoved(this);
        this->handler_func = handler.m_func;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::EventList::isRemoved(this)) {
            lo->m_event_list.remove(this);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::EventList::isRemoved(this)) {
            lo->m_event_list.remove(this);
            Loop::EventList::markRemoved(this);
        }
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::EventList::isRemoved(this);
    }
    
    void appendNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::EventList::isRemoved(this))
        
        lo->m_event_list.append(this);
        this->time = Context::Clock::getTime(c);
    }
    
    void appendAt (Context c, TimeType time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::EventList::isRemoved(this)) {
            lo->m_event_list.remove(this);
        }
        lo->m_event_list.append(this);
        this->time = time;
    }
    
    void appendAfter (Context c, TimeType after_time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::EventList::isRemoved(this))
        
        lo->m_event_list.append(this);
        this->time = Context::Clock::getTime(c) + after_time;
    }
    
    void appendAfterPrevious (Context c, TimeType after_time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::EventList::isRemoved(this))
        
        lo->m_event_list.append(this);
        this->time += after_time;
    }
    
    TimeType getSetTime (Context c)
    {
        this->debugAccess(c);
        
        return this->time;
    }
};

#include <aprinter/EndNamespace.h>

#endif
