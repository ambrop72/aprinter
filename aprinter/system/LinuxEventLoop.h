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
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <atomic>

#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/structure/LinkedHeap.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/TreeCompare.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename> class LinuxEventLoopQueuedEvent;
template <typename> class LinuxEventLoopTimedEvent;
template <typename> class LinuxEventLoopFdEvent;

template <typename Arg>
class LinuxEventLoop {
    APRINTER_USE_TYPE1(Arg, ParentObject)
    APRINTER_USE_TYPE1(Arg, ExtraDelay)
    
    template <typename> friend class LinuxEventLoopQueuedEvent;
    template <typename> friend class LinuxEventLoopTimedEvent;
    template <typename> friend class LinuxEventLoopFdEvent;
    
public:
    struct Object;
    
    APRINTER_USE_TYPE1(Arg, Context)
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    
    using QueuedEvent = LinuxEventLoopQueuedEvent<LinuxEventLoop>;
    using TimedEvent = LinuxEventLoopTimedEvent<LinuxEventLoop>;
    using FdEvent = LinuxEventLoopFdEvent<LinuxEventLoop>;
    
    using TimerLinkModel = PointerLinkModel<TimedEvent>;
    
    using FastHandlerType = void (*) (Context);
    
    struct FdEvFlags { enum {
        EV_READ  = 1 << 0,
        EV_WRITE = 1 << 1,
        EV_ERROR = 1 << 2,
        EV_HUP   = 1 << 3,
    }; };
    
private:
    using TheClockUtils = ClockUtils<Context>;
    using TheDebugObject = DebugObject<Context, Object>;
    
    static int const NumEpollEvents = 16;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        // Init event data structures.
        o->queued_event_list.init();
        o->timed_event_heap.init();
        o->timed_event_expired_list.init();
        o->timed_event_dispatch_list.init();
        
        // Initialize other event-related states;
        o->cur_epoll_event = 0;
        o->num_epoll_events = 0;
        o->timerfd_configured = false;
        o->timers_now = Clock::getTime(c);
        
        // Clear the fastevent pending flags.
        for (auto i : LoopRangeAuto(Extra<>::NumFastEvents)) {
            extra(c)->m_event_pending[i] = false;
        }
        
        // Create the epoll instance.
        o->epoll_fd = ::epoll_create1(0);
        AMBRO_ASSERT_FORCE(o->epoll_fd >= 0)
        
        // Create the timerfd and add to epoll.
        o->timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        AMBRO_ASSERT_FORCE(o->timer_fd >= 0)
        control_epoll(c, EPOLL_CTL_ADD, o->timer_fd, EPOLLIN, nullptr);
        
        // Create the eventfd and add to epoll.
        o->event_fd = ::eventfd(0, EFD_NONBLOCK);
        AMBRO_ASSERT_FORCE(o->event_fd >= 0)
        control_epoll(c, EPOLL_CTL_ADD, o->event_fd, EPOLLIN, &o->event_fd);
        
        TheDebugObject::init(c);
    }
    
    static void run (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        // Dispatch any initial queued events.
        dispatch_queued_events(c);
        
        struct timespec now_ts;
        TimeType now;
        
        while (true) {
            // Update the current time.
            now_ts = Clock::getTimespec(c);
            now = Clock::timespecToTime(now_ts);
            
            // Move expired timers to the dispatch list, update timers_now.
            update_timers_for_dispatch(c, now);
            
            // Dispatch expired timers.
            while (TimedEvent *tev = o->timed_event_dispatch_list.first()) {
                tev->debugAccess(c);
                AMBRO_ASSERT(tev->m_state == TimedEvent::State::DISPATCH)
                
                // Remove event from dispatch list, adjust state.
                o->timed_event_dispatch_list.removeFirst();
                tev->m_state = TimedEvent::State::IDLE;
                
                // Call the handler.
                tev->m_handler(c);
                dispatch_queued_events(c);
            }
            
            // Dispatch any pending fastevents.
            for (auto i : LoopRangeAuto(Extra<>::NumFastEvents)) {
                // Atomically set the pending flag to false and check if it was true.
                if (extra(c)->m_event_pending[i].exchange(false)) {
                    // Call the handler.
                    extra(c)->m_event_handler[i](c);
                    dispatch_queued_events(c);
                }
            }
            
            // Process epoll events.
            while (o->cur_epoll_event < o->num_epoll_events) {
                // Take an event.
                struct epoll_event *ev = &o->epoll_events[o->cur_epoll_event++];
                void *data_ptr = ev->data.ptr;
                
                if (data_ptr == &o->event_fd) {
                    // Consume the eventfd.
                    uint64_t event_count = 0;
                    ssize_t read_res = ::read(o->event_fd, &event_count, sizeof(event_count));
                    if (read_res < 0) {
                        // The only possibly expected error is that there are no events.
                        // But even this should not happen since the fd was found readable.
                        int err = errno;
                        AMBRO_ASSERT_FORCE(err == EAGAIN || err == EWOULDBLOCK)
                    } else {
                        // If the read succeeds we are supposed to get a nonzero event count.
                        AMBRO_ASSERT_FORCE(read_res == sizeof(event_count))
                        AMBRO_ASSERT_FORCE(event_count > 0)
                    }
                }
                else if (data_ptr != nullptr) {
                    // It must be for an FdEvent.
                    FdEvent *fdev = (FdEvent *)data_ptr;
                    fdev->debugAccess(c);
                    AMBRO_ASSERT(fdev->m_handler)
                    AMBRO_ASSERT(fdev->m_fd >= 0)
                    AMBRO_ASSERT(fd_req_events_valid(fdev->m_events))
                    
                    // Calculate events to report.
                    int events = get_fd_events_to_report(ev->events, fdev->m_events);
                    
                    if (events != 0) {
                        // Call the handler.
                        fdev->m_handler(c, events);
                        dispatch_queued_events(c);
                    }
                }
            }
            
            // All previous events must have been processed.
            AMBRO_ASSERT(o->timed_event_dispatch_list.isEmpty())
            AMBRO_ASSERT(o->cur_epoll_event == o->num_epoll_events)
            
            // Make sure the timerfd is set to expire according to the current timers.
            configure_timerfd(c, now_ts);
            
            // Wait for events with epoll.
            int wait_res;
            while (true) {
                wait_res = ::epoll_wait(o->epoll_fd, o->epoll_events, NumEpollEvents, -1);
                if (wait_res >= 0) {
                    break;
                }
                int err = errno;
                AMBRO_ASSERT_FORCE(err == EINTR) // nothign else should happen here
            }
            AMBRO_ASSERT_FORCE(wait_res <= NumEpollEvents)
            
            // Set the epoll event count and position.
            o->cur_epoll_event = 0;
            o->num_epoll_events = wait_res;
        }
    }
    
    template <typename Id>
    struct FastEventSpec {};
    
    template <typename EventSpec>
    static void initFastEvent (Context c, FastHandlerType handler)
    {
        TheDebugObject::access(c);
        AMBRO_ASSERT(handler)
        
        int const index = Extra<>::template get_event_index<EventSpec>();
        extra(c)->m_event_handler[index] = handler;
    }
    
    template <typename EventSpec>
    static void resetFastEvent (Context c)
    {
        TheDebugObject::access(c);
        
        int const index = Extra<>::template get_event_index<EventSpec>();
        extra(c)->m_event_pending[index] = false;
    }
    
    template <typename EventSpec, typename ThisContext>
    static void triggerFastEvent (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        int const index = Extra<>::template get_event_index<EventSpec>();
        
        // Set the pending flag and raise the eventfd if the flag was not already set.
        if (!extra(c)->m_event_pending[index].exchange(true)) {
            uint64_t event_count = 1;
            ssize_t write_res = ::write(o->event_fd, &event_count, sizeof(event_count));
#ifdef AMBROLIB_ASSERTIONS
            if (write_res < 0) {
                int err = errno;
                AMBRO_ASSERT(err == EAGAIN || err == EWOULDBLOCK)
            } else {
                AMBRO_ASSERT(write_res == sizeof(event_count))
            }
#endif
        }
    }
    
    static void setFdNonblocking (int fd)
    {
        int flags = ::fcntl(fd, F_GETFL, 0);
        AMBRO_ASSERT_FORCE(flags >= 0)
        
        int res = ::fcntl(fd, F_SETFL, flags|O_NONBLOCK);
        AMBRO_ASSERT_FORCE(res != -1)
    }
    
private:
    using QueuedEventList = DoubleEndedList<QueuedEvent, &QueuedEvent::m_list_node>;
    using TimedEventList = DoubleEndedList<TimedEvent, &TimedEvent::m_list_node>;
    
    using TimerHeapNodeAccessor = typename APRINTER_MEMBER_ACCESSOR(&TimedEvent::m_heap_node);
    struct TimerKeyFuncs;
    using TimerCompare = TreeCompare<TimerLinkModel, TimerKeyFuncs>;
    using TimedEventHeap = LinkedHeap<TimedEvent, TimerHeapNodeAccessor, TimerCompare, TimerLinkModel>;
    
    template <typename This=LinuxEventLoop>
    using Extra = typename This::ExtraDelay::Type;
    
    template <typename This=LinuxEventLoop>
    static typename Extra<This>::Object * extra (Context c) { return Extra<>::Object::self(c); }
    
    static void control_epoll (Context c, int op, int fd, uint32_t events, void *data_ptr)
    {
        auto *o = Object::self(c);
        
        struct epoll_event ev = {};
        ev.events = events;
        ev.data.ptr = data_ptr;
        
        int res = ::epoll_ctl(o->epoll_fd, op, fd, &ev);
        AMBRO_ASSERT_FORCE(res == 0)
    }
    
    static void dispatch_queued_events (Context c)
    {
        auto *o = Object::self(c);
        
        while (QueuedEvent *qev = o->queued_event_list.first()) {
            qev->debugAccess(c);
            AMBRO_ASSERT(qev->m_handler)
            AMBRO_ASSERT(!QueuedEventList::isRemoved(qev))
            
            o->queued_event_list.removeFirst();
            QueuedEventList::markRemoved(qev);
            
            qev->m_handler(c);
        }
    }
    
    static void update_timers_for_dispatch (Context c, TimeType now)
    {
        auto *o = Object::self(c);
        
        TimedEvent *tev;
        
        while ((tev = o->timed_event_expired_list.first()) != nullptr) {
            tev->debugAccess(c);
            AMBRO_ASSERT(tev->m_state == TimedEvent::State::EXPIRED)
            
            tev->m_state = TimedEvent::State::DISPATCH;
            o->timed_event_expired_list.removeFirst();
            o->timed_event_dispatch_list.append(tev);
        }
        
        bool force_expire = !TheClockUtils::timeGreaterOrEqual(now, o->timers_now);
        
        while ((tev = o->timed_event_heap.first().pointer()) != nullptr) {
            tev->debugAccess(c);
            AMBRO_ASSERT(tev->m_state == TimedEvent::State::QUEUED)
            AMBRO_ASSERT(TheClockUtils::timeGreaterOrEqual(tev->m_time, o->timers_now))
            
            if (!force_expire && !TheClockUtils::timeGreaterOrEqual(now, tev->m_time)) {
                break;
            }
            
            tev->m_state = TimedEvent::State::DISPATCH;
            o->timed_event_heap.remove(*tev);
            o->timed_event_dispatch_list.append(tev);
        }
        
        o->timers_now = now;
    }
    
    // now_ts MUST correspod to o->timers_now!
    static void configure_timerfd (Context c, struct timespec now_ts)
    {
        auto *o = Object::self(c);
        
        bool have_first_time = false;
        TimeType first_time;
        
        TimedEvent *tev;
        if (!o->timed_event_expired_list.isEmpty()) {
            have_first_time = true;
            first_time = o->timers_now;
        }
        else if ((tev = o->timed_event_heap.first().pointer()) != nullptr) {
            tev->debugAccess(c);
            AMBRO_ASSERT(tev->m_state == TimedEvent::State::QUEUED)
            AMBRO_ASSERT(TheClockUtils::timeGreaterOrEqual(tev->m_time, o->timers_now))
            
            have_first_time = true;
            first_time = tev->m_time;
        }
        
        struct itimerspec itspec = {};
        
        if (have_first_time) {
            // Avoid redundant timerfd_settime.
            time_t now_high_sec = now_ts.tv_sec >> Clock::SecondBits;
            if (o->timerfd_configured &&
                first_time == o->timerfd_time &&
                now_high_sec == o->timerfd_now_high_sec) {
                return;
            }
            
            // Compute the target timespec based on difference between first_time and now.
            TimeType time_from_now = TheClockUtils::timeDifference(first_time, o->timers_now);
            itspec.it_value = Clock::addTimeToTimespec(now_ts, time_from_now);
            
            o->timerfd_time = first_time;
            o->timerfd_now_high_sec = now_high_sec;
        } else {
            // Avoid redundant timerfd_settime.
            if (!o->timerfd_configured) {
                return;
            }
            
            // Leave itspec zeroed to disable the timer.
        }
        
        o->timerfd_configured = have_first_time;
        
        int res = ::timerfd_settime(o->timer_fd, TFD_TIMER_ABSTIME, &itspec, nullptr);
        AMBRO_ASSERT_FORCE(res == 0)
    }
    
    static uint32_t events_to_epoll (int events)
    {
        uint32_t epoll_events = 0;
        if ((events & FdEvFlags::EV_READ) != 0) {
            epoll_events |= EPOLLIN;
        }
        if ((events & FdEvFlags::EV_WRITE) != 0) {
            epoll_events |= EPOLLOUT;
        }
        return epoll_events;
    }
    
    static int get_fd_events_to_report (uint32_t epoll_events, int req_events)
    {
        int events = 0;
        if ((req_events & FdEvFlags::EV_READ) != 0 && (epoll_events & EPOLLIN) != 0) {
            events |= FdEvFlags::EV_READ;
        }
        if ((req_events & FdEvFlags::EV_WRITE) != 0 && (epoll_events & EPOLLOUT) != 0) {
            events |= FdEvFlags::EV_WRITE;
        }
        if ((epoll_events & EPOLLERR) != 0) {
            events |= FdEvFlags::EV_ERROR;
        }
        if ((epoll_events & EPOLLHUP) != 0) {
            events |= FdEvFlags::EV_HUP;
        }
        return events;
    }
    
    static void add_fd_event (Context c, FdEvent *fdev)
    {
        control_epoll(c, EPOLL_CTL_ADD, fdev->m_fd, events_to_epoll(fdev->m_events), fdev);
    }
    
    static void change_fd_event (Context c, FdEvent *fdev)
    {
        control_epoll(c, EPOLL_CTL_MOD, fdev->m_fd, events_to_epoll(fdev->m_events), fdev);
    }
    
    static void remove_fd_event (Context c, FdEvent *fdev)
    {
        auto *o = Object::self(c);
        
        control_epoll(c, EPOLL_CTL_DEL, fdev->m_fd, 0, nullptr);
        
        // Set the data pointer to null in any pending epoll events for this FdEvent.
        for (auto i : LoopRangeAuto(o->cur_epoll_event, o->num_epoll_events)) {
            struct epoll_event *ev = &o->epoll_events[i];
            if (ev->data.ptr == fdev) {
                ev->data.ptr = nullptr;
            }
        }
    }
    
    static bool fd_req_events_valid (int events)
    {
        return (events & ~(FdEvFlags::EV_READ|FdEvFlags::EV_WRITE)) == 0;
    }
    
    struct ComparableTime {
        TimeType value;
        
        inline bool operator== (ComparableTime const &other) const
        {
            return value == other.value;
        }
        
        inline bool operator< (ComparableTime const &other) const
        {
            return !TheClockUtils::timeGreaterOrEqual(value, other.value);
        }
    };
    
    struct TimerKeyFuncs {
        inline static ComparableTime GetKeyOfEntry (TimedEvent const &timer)
        {
            return ComparableTime{timer.m_time};
        }
    };
    
public:
    struct Object : public ObjBase<LinuxEventLoop, ParentObject, MakeTypeList<TheDebugObject>> {
        QueuedEventList queued_event_list;
        TimedEventHeap timed_event_heap;
        TimedEventList timed_event_expired_list;
        TimedEventList timed_event_dispatch_list;
        int cur_epoll_event;
        int num_epoll_events;
        int epoll_fd;
        int timer_fd;
        int event_fd;
        TimeType timers_now;
        TimeType timerfd_time;
        time_t timerfd_now_high_sec;
        bool timerfd_configured;
        struct epoll_event epoll_events[NumEpollEvents];
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
    APRINTER_USE_TYPE1(Arg, ParentObject)
    APRINTER_USE_TYPE1(Arg, Loop)
    APRINTER_USE_TYPE1(Arg, FastEventList)
    
    friend Loop;
    
    static int const NumFastEvents = TypeListLength<FastEventList>::Value;
    
    template <typename EventSpec>
    static constexpr int get_event_index ()
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
    APRINTER_USE_TYPE1(Loop, Context)
    APRINTER_USE_TYPE1(Loop, TimeType)
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
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            auto *lo = Loop::Object::self(c);
            lo->queued_event_list.remove(this);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        
        if (!Loop::QueuedEventList::isRemoved(this)) {
            auto *lo = Loop::Object::self(c);
            lo->queued_event_list.remove(this);
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
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        
        auto *lo = Loop::Object::self(c);
        lo->queued_event_list.append(this);
    }
    
    void appendNow (Context c)
    {
        this->debugAccess(c);
        
        auto *lo = Loop::Object::self(c);
        if (!Loop::QueuedEventList::isRemoved(this)) {
            lo->queued_event_list.remove(this);
        }
        lo->queued_event_list.append(this);
    }
    
    void prependNowNotAlready (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(Loop::QueuedEventList::isRemoved(this))
        
        auto *lo = Loop::Object::self(c);
        lo->queued_event_list.prepend(this);
    }
    
    void prependNow (Context c)
    {
        this->debugAccess(c);
        
        auto *lo = Loop::Object::self(c);
        if (!Loop::QueuedEventList::isRemoved(this)) {
            lo->queued_event_list.remove(this);
        }
        lo->queued_event_list.prepend(this);
    }
    
private:
    DoubleEndedListNode<LinuxEventLoopQueuedEvent> m_list_node;
    HandlerType m_handler;
};

template <typename Loop>
class LinuxEventLoopTimedEvent
: private SimpleDebugObject<typename Loop::Context>
{
    friend Loop;
    
    enum class State : uint8_t {IDLE, QUEUED, EXPIRED, DISPATCH};
    
public:
    APRINTER_USE_TYPE1(Loop, Context)
    APRINTER_USE_TYPE1(Loop, TimeType)
    using HandlerType = Callback<void(Context c)>;
    
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        m_handler = handler;
        m_state = State::IDLE;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        if (m_state != State::IDLE) {
            unlink_it(c);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        
        if (m_state != State::IDLE) {
            unlink_it(c);
            m_state = State::IDLE;
        }
    }
    
    bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return m_state != State::IDLE;
    }
    
    void appendAtNotAlready (Context c, TimeType time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == State::IDLE)
        
        m_time = time;
        link_it(c);
    }
    
    void appendAt (Context c, TimeType time)
    {
        this->debugAccess(c);
        
        if (m_state != State::IDLE) {
            unlink_it(c);
        }
        m_time = time;
        link_it(c);
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
    
    TimeType getSetTime (Context c)
    {
        this->debugAccess(c);
        
        return m_time;
    }
    
private:
    void link_it (Context c)
    {
        auto *lo = Loop::Object::self(c);
        
        if (Loop::TheClockUtils::timeGreaterOrEqual(m_time, lo->timers_now)) {
            lo->timed_event_heap.insert(*this);
            m_state = State::QUEUED;
        } else {
            lo->timed_event_expired_list.append(this);
            m_state = State::EXPIRED;
        }
    }
    
    void unlink_it (Context c)
    {
        auto *lo = Loop::Object::self(c);
        
        if (m_state == State::QUEUED) {
            lo->timed_event_heap.remove(*this);
        }
        else if (m_state == State::EXPIRED) {
            lo->timed_event_expired_list.remove(this);
        }
        else {
            AMBRO_ASSERT(m_state == State::DISPATCH)
            lo->timed_event_dispatch_list.remove(this);
        }
    }
    
    union {
        DoubleEndedListNode<LinuxEventLoopTimedEvent> m_list_node;
        LinkedHeapNode<typename Loop::TimerLinkModel> m_heap_node;
    };
    HandlerType m_handler;
    TimeType m_time;
    State m_state;
};

template <typename Loop>
class LinuxEventLoopFdEvent
: private SimpleDebugObject<typename Loop::Context>
{
    friend Loop;
    
public:
    APRINTER_USE_TYPE1(Loop, Context)
    using HandlerType = Callback<void(Context c, int events)>;
    
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        m_handler = handler;
        m_fd = -1;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        if (m_fd >= 0) {
            Loop::remove_fd_event(c, this);
        }
    }
    
    void reset (Context c)
    {
        this->debugAccess(c);
        
        if (m_fd >= 0) {
            Loop::remove_fd_event(c, this);
            m_fd = -1;
        }
    }
    
    void start (Context c, int fd, int events)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_fd == -1)
        AMBRO_ASSERT(fd >= 0)
        AMBRO_ASSERT(Loop::fd_req_events_valid(events))
        
        m_fd = fd;
        m_events = events;
        Loop::add_fd_event(c, this);
    }
    
    void changeEvents (Context c, int events)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_fd >= 0)
        AMBRO_ASSERT(Loop::fd_req_events_valid(events))
        
        if (m_events != events) {
            m_events = events;
            Loop::change_fd_event(c, this);
        }
    }
    
private:
    HandlerType m_handler;
    int m_fd;
    int m_events;
};

#include <aprinter/EndNamespace.h>

#endif
