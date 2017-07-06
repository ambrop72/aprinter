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

#include <atomic>
#include <limits>

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
#include <aprinter/structure/LinkModel.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/system/TimedEventCompat.h>

#include <aprinter/BeginNamespace.h>

template <typename> class LinuxEventLoopQueuedEvent;
template <typename> class LinuxEventLoopTimedEvent;
template <typename> class LinuxEventLoopFdEvent;

template <typename Arg>
class LinuxEventLoop {
    APRINTER_USE_TYPES1(Arg, (ParentObject, ExtraDelay, TimersStructureService))
    
    template <typename> friend class LinuxEventLoopQueuedEvent;
    template <typename> friend class LinuxEventLoopTimedEvent;
    template <typename> friend class LinuxEventLoopFdEvent;
    
public:
    struct Object;
    
    APRINTER_USE_TYPE1(Arg, Context)
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    
    using QueuedEvent = LinuxEventLoopQueuedEvent<LinuxEventLoop>;
    using TimedEventNew = LinuxEventLoopTimedEvent<LinuxEventLoop>;
    using TimedEvent = TimedEventCompat<TimedEventNew>;
    using FdEvent = LinuxEventLoopFdEvent<LinuxEventLoop>;
    
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
    
    using TimerLinkModel = PointerLinkModel<TimedEventNew>;
    
    static int const NumEpollEvents = 16;
    
    // Number of lowest order bits in timer state which define the
    // order for the heap. This allows the TEMP_UNSET and TEMP_SET
    // states to the same heap order.
    static int const TimStateOrderBits = 3;
    
    // Mask for TimStateOrderBits.
    static uint8_t const TimStateOrderMask = (1 << TimStateOrderBits) - 1;
    
    // Timer states.
    // - IDLE timers are inactive and not in the heap.
    // - DISPATCH is means the timer is active, has expired and is
    //   about to be dispatched in this event loop iteration.
    //   For the heap these timers must be ordered lesser than timers in
    //   any other state for timers in the heap, so that timers can be
    //   dispatched by consuming the heap until there are no more timers
    //   in the heap or the least timer is not in DISPATCH state.
    // - TEMP_UNSET and TEMP_SET timers are inactive/active timers
    //   respectively which have just been dispatched and are in the
    //   heap but their heap positions do not correspond to the set-time.
    //   The TEMP_UNSET state is entered just as a timer is dispatched.
    //   If the timer is then set before the next event loop iteration
    //   it will transition to TEMP_SET and likewise it may transition
    //   back to TEMP_UNSET if it is subsequently unset. At the end of
    //   an event loop iteration, all TEMP-state timers will be collected
    //   and moved to appropriate non-TEMP states (IDLE, PAST, FUTURE).
    //   For the heap, the TEMP_UNSET and TEMP_SET timers are ordered
    //   equivalently (so transitions between these states need no heap
    //   fixup), are greater than DISPATCH timers (to not considered for
    //   dispatching) and are lesser than PAST/FUTURE timers (to allow
    //   collecting them).
    // - PAST timers are active timers that are considered expired but
    //   are not queued for dispatching. This state exists because the
    //   m_time value of the timers may no longer be valid for comparison
    //   (>=timers_now). Therefore for the heap PAST timers are considered
    //   lesser than FUTURE timers.
    // - FUTURE timers are those which are not considered expired and
    //   have a valid time (m_time>=timers_now).
    enum class TimState : uint8_t {
        IDLE       = 0,
        DISPATCH   = 1,
        TEMP_UNSET = 2,
        TEMP_SET   = 2 | (1 << TimStateOrderBits),
        PAST       = 3,
        FUTURE     = 4,
    };
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        // Init event data structures.
        o->queued_event_list.init();
        o->timed_event_heap.init();
        
        // Initialize other event-related states.
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
            
            // Mark expired timers for dispatch, update timers_now.
            update_timers_for_dispatch(c, now);
            
            // Dispatch timers marked for dispatch.
            while (TimedEventNew *tev = o->timed_event_heap.first()) {
                tev->debugAccess(c);
                AMBRO_ASSERT(tev->m_state == one_of_heap_timer_states())
                
                // If this is not a DISPATCH state timer, there are no more
                // DISPATCH timers, since DISPATCH timers are ordered smaller
                // than timers in any other state that can appear in the heap.
                if (tev->m_state != TimState::DISPATCH) {
                    break;
                }
                
                // Transition timer to TEMP_UNSET state, fixup the heap.
                tev->m_state = TimState::TEMP_UNSET;
                o->timed_event_heap.fixup(*tev);
                
                // Call the handler.
                tev->handleTimerExpired(c);
                dispatch_queued_events(c);
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
            
            // Dispatch any pending fastevents.
            // It is important to do this after reading the eventfd above.
            // If we did it before, we might miss an event that wrote into
            // the eventfd after checking.
            for (auto i : LoopRangeAuto(Extra<>::NumFastEvents)) {
                // Atomically set the pending flag to false and check if it was true.
                if (extra(c)->m_event_pending[i].exchange(false)) {
                    // Call the handler.
                    extra(c)->m_event_handler[i](c);
                    dispatch_queued_events(c);
                }
            }
            
            // All previous events must have been processed.
            AMBRO_ASSERT(!has_timers_for_dispatch(c))
            AMBRO_ASSERT(o->cur_epoll_event == o->num_epoll_events)
            
            // Adjust any TEMP_* state timers and make sure the
            // timerfd is set correctly for the current timers.
            prepare_timers_for_wait(c, now_ts);
            
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
    
    inline static TimeType getEventTime (Context c)
    {
        auto *o = Object::self(c);
        return o->timers_now;
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
    
    using TimerHeapNodeAccessor = typename APRINTER_MEMBER_ACCESSOR(&TimedEventNew::m_heap_node);
    class TimerCompare;
    using TimedEventHeap = typename TimersStructureService::template Structure<
        TimerHeapNodeAccessor, TimerCompare, TimerLinkModel>;
    
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
    
    // This is called after epoll_wait to transition to DISPATCH state
    // any timers which are expired and to update timers_now.
    // There MUST be no DISPATCH or TEMP_* timers in the heap.
    static void update_timers_for_dispatch (Context c, TimeType now)
    {
        auto *o = Object::self(c);
        
        // Determine the time based on which we consider timers expired.
        TimeType dispatch_time;
        if (TheClockUtils::timeGreaterOrEqual(now, o->timers_now)) {
            // Dispatching all timers <=now.
            dispatch_time = now;
        } else {
            // Clock seems to have overflowed, dispatching all timers.
            dispatch_time = o->timers_now + std::numeric_limits<TimeType>::max() / 2;
        }
        
        // Set all timers that have expired to DISPATCH state.
        // Because DISPATCH state timers are considered to be lesser than
        // timers in any other state, this does not break the heap property.
        // Because the traversal is pre-order, the heap property is preserved
        // even during this iteration, allowing the asserts in this heap code
        // to pass.
        TimedEventNew *tev = o->timed_event_heap.findFirstLesserOrEqual(dispatch_time);
        if (tev != nullptr) {
            do {
                tev->debugAccess(c);
                AMBRO_ASSERT(tev->m_state == OneOf(TimState::PAST, TimState::FUTURE))
                
                tev->m_state = TimState::DISPATCH;
                
                tev = o->timed_event_heap.findNextLesserOrEqual(dispatch_time, *tev);
            } while (tev != nullptr);
            
            // If the heap verification is enabled, verify here after
            // we changed the states.
            o->timed_event_heap.assertValidHeap();
        }
        
        // Update timers_now to the new now.
        // This preserves the invariant that all FUTURE state timers in the
        // heap have time >=timers_now, since any that do not would have been
        // moved to DISPATCH state.
        o->timers_now = now;
    }
    
    inline static bool has_timers_for_dispatch (Context c)
    {
        auto *o = Object::self(c);
        TimedEventNew *tev = o->timed_event_heap.first();
        return tev != nullptr && tev->m_state == TimState::DISPATCH;
    }
    
    // This is called before epoll_wait to transition any TEMP_* state
    // timers to other states and ensure that the timerfd is configured
    // correctly. Any DISPATCH state timers MUST have been dispatched and
    // now_ts MUST correspond to timers_now.
    static void prepare_timers_for_wait (Context c, struct timespec now_ts)
    {
        auto *o = Object::self(c);
        
        bool have_first_time = false;
        TimeType first_time;
        
        while (TimedEventNew *tev = o->timed_event_heap.first()) {
            tev->debugAccess(c);
            AMBRO_ASSERT(tev->m_state == OneOf(TimState::TEMP_UNSET, TimState::TEMP_SET,
                                               TimState::PAST, TimState::FUTURE))
            
            // If this is a TEMP_UNSET timer, remove it from the heap,
            // set it to IDLE state and proceed to look at the next timer.
            if (tev->m_state == TimState::TEMP_UNSET) {
                o->timed_event_heap.remove(*tev);
                tev->m_state = TimState::IDLE;
            }
            // If this is a TEMP_SET timer, set it to the right state,
            // fixup the heap and proceed to look again.
            else if (tev->m_state == TimState::TEMP_SET) {
                tev->m_state = tev->state_for_link(c);
                o->timed_event_heap.fixup(*tev);
            }
            // Otherwise it's the smallest PAST or FUTURE timer, and the
            // timerds is to be configured based on this timer.
            else {
                have_first_time = true;
                if (tev->m_state == TimState::FUTURE) {
                    AMBRO_ASSERT(TheClockUtils::timeGreaterOrEqual(tev->m_time, o->timers_now))
                    first_time = tev->m_time;
                } else {
                    AMBRO_ASSERT(tev->m_state == TimState::PAST)
                    first_time = o->timers_now;
                }
                break;
            }
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
    
    inline static auto one_of_heap_timer_states ()
    {
        return OneOf(TimState::DISPATCH, TimState::TEMP_UNSET, TimState::TEMP_SET,
                     TimState::PAST, TimState::FUTURE);
    }
    
    // This implements comparisons used by the timers heap.
    class TimerCompare {
        using State = typename TimerLinkModel::State;
        using Ref = typename TimerLinkModel::Ref;
        
    public:
        // Compare two timers.
        static int compareEntries (State, Ref ref1, Ref ref2)
        {
            Context c;
            auto *o = Object::self(c);
            TimedEventNew &tev1 = *ref1;
            TimedEventNew &tev2 = *ref2;
            AMBRO_ASSERT(tev1.m_state == one_of_heap_timer_states())
            AMBRO_ASSERT(tev2.m_state == one_of_heap_timer_states())
            AMBRO_ASSERT(tev1.m_state != TimState::FUTURE || TheClockUtils::timeGreaterOrEqual(tev1.m_time, o->timers_now))
            AMBRO_ASSERT(tev2.m_state != TimState::FUTURE || TheClockUtils::timeGreaterOrEqual(tev2.m_time, o->timers_now))
            
            uint8_t order1 = (uint8_t)tev1.m_state & TimStateOrderMask;
            uint8_t order2 = (uint8_t)tev2.m_state & TimStateOrderMask;
            
            if (order1 != order2) {
                return (order1 < order2) ? -1 : 1;
            }
            
            if (order1 != (uint8_t)TimState::FUTURE) {
                return 0;
            }
            
            TimeType time1 = tev1.m_time;
            TimeType time2 = tev2.m_time;
            
            return !TheClockUtils::timeGreaterOrEqual(time1, time2) ? -1 : (time1 == time2) ? 0 : 1;
        }
        
        // Compare a time (as if it was a FUTURE timer) and a timer.
        static int compareKeyEntry (State, TimeType time1, Ref ref2)
        {
            Context c;
            auto *o = Object::self(c);
            TimedEventNew &tev2 = *ref2;
            AMBRO_ASSERT(TheClockUtils::timeGreaterOrEqual(time1, o->timers_now))
            AMBRO_ASSERT(tev2.m_state == one_of_heap_timer_states())
            AMBRO_ASSERT(tev2.m_state != TimState::FUTURE || TheClockUtils::timeGreaterOrEqual(tev2.m_time, o->timers_now))
            
            uint8_t order1 = (uint8_t)TimState::FUTURE;
            uint8_t order2 = (uint8_t)tev2.m_state & TimStateOrderMask;
            
            if (order1 != order2) {
                return (order1 < order2) ? -1 : 1;
            }
            
            TimeType time2 = tev2.m_time;
            
            return !TheClockUtils::timeGreaterOrEqual(time1, time2) ? -1 : (time1 == time2) ? 0 : 1;
        }
    };
    
public:
    struct Object : public ObjBase<LinuxEventLoop, ParentObject, MakeTypeList<TheDebugObject>> {
        QueuedEventList queued_event_list;
        TimedEventHeap timed_event_heap;
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
    APRINTER_AS_TYPE(ExtraDelay),
    APRINTER_AS_TYPE(TimersStructureService)
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
    
    APRINTER_USE_TYPES1(Loop, (TimState, TimersStructureService, TimerLinkModel))
    
public:
    APRINTER_USE_TYPE1(Loop, Context)
    APRINTER_USE_TYPE1(Loop, TimeType)
    
    void init (Context c)
    {
        m_state = TimState::IDLE;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        if (m_state != TimState::IDLE) {
            unlink_it(c);
        }
    }
    
    void unset (Context c)
    {
        this->debugAccess(c);
        
        if (m_state == OneOf(TimState::TEMP_UNSET, TimState::TEMP_SET)) {
            m_state = TimState::TEMP_UNSET;
        } else {
            if (m_state != TimState::IDLE) {
                unlink_it(c);
                m_state = TimState::IDLE;
            }
        }
    }
    
    inline bool isSet (Context c)
    {
        this->debugAccess(c);
        
        return m_state != OneOf(TimState::IDLE, TimState::TEMP_UNSET);
    }
    
    inline TimeType getSetTime (Context c)
    {
        this->debugAccess(c);
        
        return m_time;
    }
    
    inline void appendAtNotAlready (Context c, TimeType time)
    {
        AMBRO_ASSERT(m_state == OneOf(TimState::IDLE, TimState::TEMP_UNSET))
        appendAt(c, time);
    }
    
    void appendAt (Context c, TimeType time)
    {
        auto *lo = Loop::Object::self(c);
        this->debugAccess(c);
        
        m_time = time;
        
        if (m_state == OneOf(TimState::TEMP_UNSET, TimState::TEMP_SET)) {
            m_state = TimState::TEMP_SET;
        } else {
            TimState old_state = m_state;
            m_state = state_for_link(c);
            
            if (old_state == TimState::IDLE) {
                lo->timed_event_heap.insert(*this);
            } else {
                lo->timed_event_heap.fixup(*this);
            }
        }
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
    
private:
    inline TimState state_for_link (Context c)
    {
        auto *lo = Loop::Object::self(c);
        
        if (Loop::TheClockUtils::timeGreaterOrEqual(m_time, lo->timers_now)) {
            return TimState::FUTURE;
        } else {
            return TimState::PAST;
        }
    }
    
    inline void unlink_it (Context c)
    {
        auto *lo = Loop::Object::self(c);
        lo->timed_event_heap.remove(*this);
    }
    
    typename TimersStructureService::template Node<TimerLinkModel> m_heap_node;
    TimeType m_time;
    TimState m_state;
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
