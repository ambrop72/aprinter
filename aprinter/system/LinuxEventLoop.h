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

#ifndef APRINTER_LINUX_EVENT_LOOP_H
#define APRINTER_LINUX_EVENT_LOOP_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

#include <atomic>

#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename> class LinuxEventLoopQueuedEvent;
template <typename> class LinuxEventLoopTimedEvent;

template <typename Arg>
class LinuxEventLoop {
    using ParentObject = typename Arg::ParentObject;
    using ExtraDelay   = typename Arg::ExtraDelay;
    
    template <typename> friend class LinuxEventLoopQueuedEvent;
    template <typename> friend class LinuxEventLoopTimedEvent;
    
public:
    struct Object;
    using Context = typename Arg::Context;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TheClockUtils = ClockUtils<Context>;
    using QueuedEvent = LinuxEventLoopQueuedEvent<LinuxEventLoop>;
    using TimedEvent = LinuxEventLoopTimedEvent<LinuxEventLoop>;
    using FastHandlerType = void (*) (Context);
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    static int const NumEpollEvents = 10;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        int res;
        
        // Init event lists.
        o->m_queued_event_list.init();
        o->m_timed_event_list.init();
        o->m_timed_event_expired_list.init();
        
        // Initialize epoll events array state;
        o->m_num_epoll_events = 0;
        o->m_cur_epoll_event = 0;
        
        // Init the fastevents.
        for (auto i : LoopRangeAuto(Delay::Extra::NumFastEvents)) {
            Delay::extra(c)->m_event_pending[i] = false;
        }
        
        // Create the epoll instance.
        o->m_epoll_fd = epoll_create1(0);
        AMBRO_ASSERT_FORCE(o->m_epoll_fd >= 0)
        
        // Create the timerfd and add to epoll.
        o->m_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        AMBRO_ASSERT_FORCE(o->m_timer_fd >= 0)
        add_to_epoll(c, o->m_timer_fd, EPOLLIN, &o->m_timer_fd);
        
        // Create the eventfd and add to epoll.
        o->m_event_fd = eventfd(0, EFD_NONBLOCK);
        AMBRO_ASSERT_FORCE(o->m_event_fd >= 0)
        add_to_epoll(c, o->m_event_fd, EPOLLIN, &o->m_event_fd);
        
        TheDebugObject::init(c);
    }
    
    static void run (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        // Dispatch any initial queued events.
        dispatch_queued_events(c);
        
        // Get the current time.
        struct timespec now_ts = Clock::getTimespec(c);
        TimeType now = Clock::timespecToTime(now_ts);
        
        while (1) {
            // All previous events must have been processed.
            AMBRO_ASSERT(o->m_timed_event_expired_list.isEmpty())
            AMBRO_ASSERT(o->m_cur_epoll_event == o->m_num_epoll_events)
            
            // Configure timerfd to expire at the earliest timer time, or never.
            configure_timerfd(c, now_ts, now);
            
            // Do the epoll wait.
            int res = epoll_wait(o->m_epoll_fd, o->m_epoll_events, NumEpollEvents, -1);
            if (res < 0) {
                int err = errno;
                AMBRO_ASSERT_FORCE(err == EINTR) // nothign else should happen here
                continue;
            }
            AMBRO_ASSERT_FORCE(res <= NumEpollEvents)
            
            // Update the current time.
            now_ts = Clock::getTimespec(c);
            now = Clock::timespecToTime(now_ts);
            
            // Set the epoll event count and position.
            o->m_num_epoll_events = res;
            o->m_cur_epoll_event = 0;
            
            // Move expired timers to the expired list.
            move_expired_timers_to_expired(c, now);
            
            bool found_event = false;
            
            // Dispatch expired timers.
            while (TimedEvent *tev = o->m_timed_event_expired_list.first()) {
                AMBRO_ASSERT(!TimedEventList::isRemoved(tev))
                AMBRO_ASSERT(tev->m_expired)
                
                found_event = true;
                
                // Remove event from expired list.
                o->m_timed_event_expired_list.remove(tev);
                TimedEventList::markRemoved(tev);
                
                // Call the timer's handler.
                tev->m_handler(c);
                
                // Dispatch any queued events queued by the handler.
                dispatch_queued_events(c);
            }
            
            // Process epoll events.
            while (o->m_cur_epoll_event < o->m_num_epoll_events) {
                // Take an event.
                struct epoll_event *ev = &o->m_epoll_events[o->m_cur_epoll_event++];
                void *data_ptr = ev->data.ptr;
                
                if (data_ptr == &o->m_timer_fd) {
                    // It's our timerfd; no handling needed here.
                    continue;
                }
                else if (data_ptr == &o->m_event_fd) {
                    // It's our eventfd; need to read it and call any fastevent handlers.
                    
                    // Read the eventfd.
                    uint64_t event_count = 0;
                    ssize_t read_res = read(o->m_event_fd, &event_count, sizeof(event_count));
                    if (read_res < 0) {
                        // The only possibly expected error is that there are no events.
                        // But even this should not happen since the fd was found readable.
                        int err = errno;
                        AMBRO_ASSERT_FORCE(err == EAGAIN || err == EWOULDBLOCK)
                        continue;
                    }
                    
                    // If the read succeeds we are supposed to get a nonzero event count.
                    AMBRO_ASSERT_FORCE(read_res == sizeof(event_count))
                    AMBRO_ASSERT_FORCE(event_count > 0)
                    
                    // Dispatch any pending fastevents.
                    for (auto i : LoopRangeAuto(Delay::Extra::NumFastEvents)) {
                        // Try to consume the pending flag.
                        if (!Delay::extra(c)->m_event_pending[i].exchange(false)) {
                            continue;
                        }
                        
                        found_event = true;
                        
                        // Call the event's handler.
                        Delay::extra(c)->m_event_handler[i](c);
                        
                        // Dispatch any queued events queued by the handler.
                        dispatch_queued_events(c);
                    }
                }
                else {
                    
                    found_event = true;
                    
                    // TODO
                    fprintf(stderr, "unexpected fd event!?!?!?\n");
                    AMBRO_ASSERT_FORCE(false)
                }
            }
            
            if (!found_event) {
                fprintf(stderr, "epoll spurious wakeup\n");
            }
        }
    }
    
    template <typename Id>
    struct FastEventSpec {};
    
    template <typename EventSpec>
    static void initFastEvent (Context c, FastHandlerType handler)
    {
        TheDebugObject::access(c);
        
        int const index = Delay::Extra::template get_event_index<EventSpec>();
        Delay::extra(c)->m_event_handler[index] = handler;
    }
    
    template <typename EventSpec>
    static void resetFastEvent (Context c)
    {
        TheDebugObject::access(c);
        
        int const index = Delay::Extra::template get_event_index<EventSpec>();
        Delay::extra(c)->m_event_pending[index] = false;
    }
    
    template <typename EventSpec, typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void triggerFastEvent (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        int const index = Delay::Extra::template get_event_index<EventSpec>();
        
        // Set the pending flag and raise the eventfd if the flag was not already set.
        if (!Delay::extra(c)->m_event_pending[index].exchange(true)) {
            uint64_t event_count = 1;
            ssize_t write_res = write(o->m_event_fd, &event_count, sizeof(event_count));
            AMBRO_ASSERT(write_res == sizeof(event_count))
        }
    }
    
private:
    using QueuedEventList = DoubleEndedList<QueuedEvent, &QueuedEvent::m_list_node>;
    using TimedEventList = DoubleEndedList<TimedEvent, &TimedEvent::m_list_node>;
    
    struct Delay {
        using Extra = typename ExtraDelay::Type;
        static typename Extra::Object * extra (Context c) { return Extra::Object::self(c); }
    };
    
    static void add_to_epoll (Context c, int fd, uint32_t events, void *data_ptr)
    {
        auto *o = Object::self(c);
        
        struct epoll_event ev = {};
        ev.events = events;
        ev.data.ptr = data_ptr;
        int res = epoll_ctl(o->m_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
        AMBRO_ASSERT_FORCE(res == 0)
    }
    
    static void dispatch_queued_events (Context c)
    {
        auto *o = Object::self(c);
        
        // Dispatch queued events until there are no more.
        while (QueuedEvent *ev = o->m_queued_event_list.first()) {
            AMBRO_ASSERT(!QueuedEventList::isRemoved(ev))
            o->m_queued_event_list.remove(ev);
            QueuedEventList::markRemoved(ev);
            ev->m_handler(c);
        }
    }
    
    static void move_expired_timers_to_expired (Context c, TimeType now)
    {
        auto *o = Object::self(c);
        
        TimedEvent *tev = o->m_timed_event_list.first();
        
        while (tev != nullptr) {
            AMBRO_ASSERT(!TimedEventList::isRemoved(tev))
            AMBRO_ASSERT(!tev->m_expired)
            TimedEvent *next_tev = o->m_timed_event_list.next(tev);
            
            if (TheClockUtils::timeGreaterOrEqual(now, tev->m_time)) {
                tev->m_expired = true;
                o->m_timed_event_list.remove(tev);
                o->m_timed_event_expired_list.append(tev);
            }
            
            tev = next_tev;
        }
    }
    
    static void configure_timerfd (Context c, struct timespec now_ts, TimeType now)
    {
        auto *o = Object::self(c);
        
        bool have_first_time = false;
        TimeType first_time;
        
        for (TimedEvent *tev = o->m_timed_event_list.first(); tev != nullptr; tev = o->m_timed_event_list.next(tev)) {
            AMBRO_ASSERT(!TimedEventList::isRemoved(tev))
            AMBRO_ASSERT(!tev->m_expired)
            
            TimeType tev_time = tev->m_time;
            if (!TheClockUtils::timeGreaterOrEqual(tev_time, now)) {
                have_first_time = true;
                first_time = now;
                break;
            }
            
            if (!have_first_time || !TheClockUtils::timeGreaterOrEqual(tev_time, first_time)) {
                have_first_time = true;
                first_time = tev_time;
            }
        }
        
        struct itimerspec itspec = {};
        if (have_first_time) {
            TimeType time_from_now = TheClockUtils::timeDifference(first_time, now);
            itspec.it_value = Clock::addTimeToTimespec(now_ts, time_from_now);
        }
        
        int tfd_res = timerfd_settime(o->m_timer_fd, TFD_TIMER_ABSTIME, &itspec, nullptr);
        AMBRO_ASSERT_FORCE(tfd_res == 0)
    }
    
public:
    struct Object : public ObjBase<LinuxEventLoop, ParentObject, MakeTypeList<TheDebugObject>> {
        QueuedEventList m_queued_event_list;
        TimedEventList m_timed_event_list;
        TimedEventList m_timed_event_expired_list;
        int m_num_epoll_events;
        int m_cur_epoll_event;
        int m_epoll_fd;
        int m_timer_fd;
        int m_event_fd;
        struct epoll_event m_epoll_events[NumEpollEvents];
    };
};

APRINTER_ALIAS_STRUCT_EXT(LinuxEventLoopArg, (
    APRINTER_AS_TYPE(Context),
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(ExtraDelay)
), (
    APRINTER_DEF_INSTANCE(LinuxEventLoopArg, LinuxEventLoop)
))

template <typename Arg>
class LinuxEventLoopExtra {
    using ParentObject  = typename Arg::ParentObject;
    using Loop          = typename Arg::Loop;
    using FastEventList = typename Arg::FastEventList;
    
    friend Loop;
    
    static const int NumFastEvents = TypeListLength<FastEventList>::Value;
    using FastEventSizeType = ChooseInt<MaxValue(1, BitsInInt<NumFastEvents>::Value), false>;
    
    template <typename EventSpec>
    static constexpr FastEventSizeType get_event_index ()
    {
        return TypeListIndex<FastEventList, EventSpec>::Value;
    }
    
public:
    struct Object : public ObjBase<LinuxEventLoopExtra, ParentObject, EmptyTypeList> {
        std::atomic_bool m_event_pending[NumFastEvents];
        typename Loop::FastHandlerType m_event_handler[NumFastEvents];
    };
};

APRINTER_ALIAS_STRUCT_EXT(LinuxEventLoopExtraArg, (
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(Loop),
    APRINTER_AS_TYPE(FastEventList)
), (
    APRINTER_DEF_INSTANCE(LinuxEventLoopExtraArg, LinuxEventLoopExtra)
))

template <typename Loop>
class LinuxEventLoopQueuedEvent
: private SimpleDebugObject<typename Loop::Context>
{
    friend Loop;
    
public:
    using Context = typename Loop::Context;
    using TimeType = typename Loop::TimeType;
    using HandlerType = Callback<void(Context c)>;
    
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        m_handler = handler;
        Loop::QueuedEventList::markRemoved(this);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            lo->m_queued_event_list.remove(this);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            lo->m_queued_event_list.remove(this);
            Loop::QueuedEventList::markRemoved(this);
        }
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::QueuedEventList::isRemoved(this);
    }
    
    void appendNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        
        lo->m_queued_event_list.append(this);
    }
    
    void appendNow (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            lo->m_queued_event_list.remove(this);
        }
        lo->m_queued_event_list.append(this);
    }
    
    void prependNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        
        lo->m_queued_event_list.prepend(this);
    }
    
    void prependNow (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            lo->m_queued_event_list.remove(this);
        }
        lo->m_queued_event_list.prepend(this);
    }
    
private:
    DoubleEndedListNode<LinuxEventLoopQueuedEvent> m_list_node;
    HandlerType m_handler;
};

template <typename Loop>
class LinuxEventLoopTimedEvent
: private SimpleDebugObject<typename Loop::Context>
{
public:
    using Context = typename Loop::Context;
    using TimeType = typename Loop::TimeType;
    using HandlerType = Callback<void(Context c)>;
    
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        this->m_handler = handler;
        Loop::TimedEventList::markRemoved(this);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::TimedEventList::isRemoved(this)) {
            remove_from_list(c);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::TimedEventList::isRemoved(this)) {
            remove_from_list(c);
            Loop::TimedEventList::markRemoved(this);
        }
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return !Loop::TimedEventList::isRemoved(this);
    }
    
    void appendNowNotAlready (Context c)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::TimedEventList::isRemoved(this))
        
        add_to_list(c);
        m_time = Context::Clock::getTime(c);
    }
    
    void appendAt (Context c, TimeType time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        
        if (!Loop::TimedEventList::isRemoved(this)) {
            remove_from_list(c);
        }
        add_to_list(c);
        m_time = time;
    }
    
    void appendAfter (Context c, TimeType after_time)
    {
        appendAt(c, Context::Clock::getTime(c) + after_time);
    }
    
    void appendAfterNotAlready (Context c, TimeType after_time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::TimedEventList::isRemoved(this))
        
        add_to_list(c);
        m_time = Context::Clock::getTime(c) + after_time;
    }
    
    void appendAfterPrevious (Context c, TimeType after_time)
    {
        this->debugAccess(c);
        auto *lo = Loop::Object::self(c);
        AMBRO_ASSERT(Loop::TimedEventList::isRemoved(this))
        
        add_to_list(c);
        m_time += after_time;
    }
    
    TimeType getSetTime (Context c)
    {
        this->debugAccess(c);
        
        return m_time;
    }
    
private:
    void add_to_list (Context c)
    {
        auto *lo = Loop::Object::self(c);
        
        m_expired = false;
        lo->m_timed_event_list.append(this);
    }
    
    void remove_from_list (Context c)
    {
        auto *lo = Loop::Object::self(c);
        
        if (m_expired) {
            lo->m_timed_event_expired_list.remove(this);
        } else {
            lo->m_timed_event_list.remove(this);
        }
    }
    
    DoubleEndedListNode<LinuxEventLoopTimedEvent> m_list_node;
    HandlerType m_handler;
    TimeType m_time;
    bool m_expired;
};

#include <aprinter/EndNamespace.h>

#endif
