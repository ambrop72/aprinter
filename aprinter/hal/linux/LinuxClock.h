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

#ifndef APRINTER_LINUX_CLOCK_H
#define APRINTER_LINUX_CLOCK_H

#include <stdint.h>

#include <time.h>
#include <unistd.h>
#include <sys/timerfd.h>

#include <aprinter/platform/linux/linux_support.h>
#include <aprinter/base/Object.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/base/Callback.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename>
class LinuxClockInterruptTimer;

template <typename Arg>
class LinuxClock {
    APRINTER_USE_TYPE1(Arg, Context)
    APRINTER_USE_TYPE1(Arg, ParentObject)
    
    APRINTER_USE_VAL(Arg::Params, SubSecondBits)
    APRINTER_USE_VAL(Arg::Params, MaxTimers)
    
    static_assert(SubSecondBits >= 10 && SubSecondBits <= 21, "");
    static_assert(MaxTimers > 0 && MaxTimers <= 64, "");
    
    static long const NsecInSec = 1000000000;
    static int const NanosShift = 63 - SubSecondBits;
    static uint64_t const NanosMul = PowerOfTwo<uint64_t, SubSecondBits+NanosShift>::Value / NsecInSec;
    static uint32_t const SubSecondMask = PowerOfTwoMinusOne<uint32_t, SubSecondBits>::Value;
    
    template <typename> friend class LinuxClockInterruptTimer;
    
public:
    struct Object;
    using TimeType = uint32_t;
    
    static constexpr double time_freq = PowerOfTwo<uint32_t, SubSecondBits>::Value;
    static constexpr double time_unit = 1.0 / time_freq;
    
    static int const SecondBits = 32 - SubSecondBits;
    
private:
    using TheClockUtils = ClockUtilsForClock<LinuxClock>;
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        for (auto i : LoopRangeAuto(MaxTimers)) {
            o->m_timer_active[i] = false;
            o->m_timer_handler[i] = nullptr;
        }
        
        o->m_timer_fd = ::timerfd_create(CLOCK_MONOTONIC, 0);
        AMBRO_ASSERT_FORCE(o->m_timer_fd >= 0)
        
        o->m_timer_thread.start(APRINTER_CB_STATFUNC_T(&LinuxClock::timer_thread));
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        int res;
        
        // TODO: make thread terminate (deinit not used currently)
        
        o->m_timer_thread.join();
        
        res = ::close(o->m_timer_fd);
        AMBRO_ASSERT_FORCE(res == 0)
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        TheDebugObject::access(c);
        
        return timespecToTime(getTimespec(c));
    }
    
public:
    static void assert_timespec (struct timespec ts)
    {
        AMBRO_ASSERT(ts.tv_nsec >= 0)
        AMBRO_ASSERT(ts.tv_nsec < NsecInSec)
    }
    
    static struct timespec getTimespec (Context)
    {
        struct timespec ts;
        int res = clock_gettime(CLOCK_MONOTONIC, &ts);
        AMBRO_ASSERT_FORCE(res == 0)
        assert_timespec(ts);
        return ts;
    }
    
    static TimeType timespecToTime (struct timespec ts)
    {
        assert_timespec(ts);
        
        uint32_t t_subs = ((uint32_t)ts.tv_nsec * NanosMul) >> NanosShift;
        uint32_t t_sups = (uint32_t)ts.tv_sec << SubSecondBits;
        return t_subs + t_sups;
    }
    
    static struct timespec addTimeToTimespec (struct timespec ts, TimeType time)
    {
        assert_timespec(ts);
        
        long nsec = ((time & SubSecondMask) * (uint64_t)NsecInSec) >> SubSecondBits;
        time_t sec = time >> SubSecondBits;
        ts.tv_nsec += nsec;
        if (ts.tv_nsec >= NsecInSec) {
            ts.tv_nsec -= NsecInSec;
            ts.tv_sec++;
        }
        ts.tv_sec += sec;
        assert_timespec(ts);
        return ts;
    }
    
private:
    using InternalTimerHandlerType = void (*) (AtomicContext<Context>);
    
    static void timer_thread ()
    {
        Context c;
        auto *o = Object::self(c);
        
        while (true) {
            // Wait for the timerfd to expire.
            uint64_t expire_count = 0;
            ssize_t read_res = ::read(o->m_timer_fd, &expire_count, sizeof(expire_count));
            AMBRO_ASSERT_FORCE(read_res == sizeof(expire_count))
            AMBRO_ASSERT_FORCE(expire_count > 0)
            
            // Get the current time.
            struct timespec now_ts = getTimespec(c);
            TimeType now = timespecToTime(now_ts);
            
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                // Call handlers for all expired timers.
                for (auto i : LoopRangeAuto(MaxTimers)) {
                    if (o->m_timer_active[i] && TheClockUtils::timeGreaterOrEqual(now, o->m_timer_time[i])) {
                        o->m_timer_handler[i](lock_c);
                    }
                }
                
                // Arm (or disarm) the timerfd according to the latest timer states.
                configure_timerfd(lock_c, now_ts, now);
            }
        }
    }
    
    static void configure_timerfd (AtomicContext<Context> c, struct timespec now_ts, TimeType now)
    {
        auto *o = Object::self(c);
        
        bool have_first_time = false;
        TimeType first_time;
        
        for (auto i : LoopRangeAuto(MaxTimers)) {
            if (o->m_timer_active[i]) {
                TimeType tmr_time = o->m_timer_time[i];
                if (!TheClockUtils::timeGreaterOrEqual(tmr_time, now)) {
                    have_first_time = true;
                    first_time = now;
                    break;
                }
                if (!have_first_time || !TheClockUtils::timeGreaterOrEqual(tmr_time, first_time)) {
                    have_first_time = true;
                    first_time = tmr_time;
                }
            }
        }
        
        struct itimerspec itspec = {};
        if (have_first_time) {
            TimeType time_from_now = TheClockUtils::timeDifference(first_time, now);
            itspec.it_value = addTimeToTimespec(now_ts, time_from_now);
        }
        
        int res = ::timerfd_settime(o->m_timer_fd, TFD_TIMER_ABSTIME, &itspec, nullptr);
        AMBRO_ASSERT_FORCE(res == 0)
    }
    
    static void poke_timer_thread (AtomicContext<Context> c, struct timespec now_ts)
    {
        auto *o = Object::self(c);
        
        // This is used when a timer is started, to make the timer thread wake
        // up immediately and arm the timerfd taking the changed timer into account.
        // It has to be done within the lock, else the adjustment may be overridden
        // with one that does not account for the timer change.
        
        struct itimerspec itspec = {};
        itspec.it_value = now_ts;
        
        int res = ::timerfd_settime(o->m_timer_fd, TFD_TIMER_ABSTIME, &itspec, nullptr);
        AMBRO_ASSERT_FORCE(res == 0)
    }
    
public:
    struct Object : public ObjBase<LinuxClock, ParentObject, MakeTypeList<TheDebugObject>> {
        int m_timer_fd;
        LinuxRtThread m_timer_thread;
        bool m_timer_active[MaxTimers];
        TimeType m_timer_time[MaxTimers];
        InternalTimerHandlerType m_timer_handler[MaxTimers];
    };
};

APRINTER_ALIAS_STRUCT_EXT(LinuxClockService, (
    APRINTER_AS_VALUE(int, SubSecondBits),
    APRINTER_AS_VALUE(int, MaxTimers)
), (
    APRINTER_ALIAS_STRUCT_EXT(Clock, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(DummyTimersList)
    ), (
        using Params = LinuxClockService;
        APRINTER_DEF_INSTANCE(Clock, LinuxClock)
    ))
))

template <typename Arg>
class LinuxClockInterruptTimer {
    APRINTER_USE_TYPE1(Arg, Context)
    APRINTER_USE_TYPE1(Arg, ParentObject)
    APRINTER_USE_TYPE1(Arg, Handler)
    APRINTER_USE_TYPE1(Arg, Params)
    
    APRINTER_USE_VAL(Params, Index)
    APRINTER_USE_TYPE1(Params, ExtraClearance)
    
    // NOTE: We don't implement ExtraClearance currently.
    
public:
    struct Object;
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    using HandlerContext = AtomicContext<Context>;
    
private:
    static_assert(Index >= 0 && Index < Clock::MaxTimers, "");
    
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        auto *co = Clock::Object::self(c);
        AMBRO_ASSERT(!co->m_timer_active[Index])
        AMBRO_ASSERT(co->m_timer_handler[Index] == nullptr)
        
        co->m_timer_handler[Index] = LinuxClockInterruptTimer::timer_handler;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *co = Clock::Object::self(c);
        TheDebugObject::deinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            co->m_timer_active[Index] = false;
        }
        
        co->m_timer_handler[Index] = nullptr;
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *co = Clock::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!co->m_timer_active[Index])
        
        co->m_timer_time[Index] = time;
        
        struct timespec now_ts = Clock::getTimespec(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            co->m_timer_active[Index] = true;
            Clock::poke_timer_thread(lock_c, now_ts);
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        auto *co = Clock::Object::self(c);
        AMBRO_ASSERT(co->m_timer_active[Index])
        
        co->m_timer_time[Index] = time;
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        auto *co = Clock::Object::self(c);
        TheDebugObject::access(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            co->m_timer_active[Index] = false;
        }
    }
    
    template <typename ThisContext>
    static TimeType getLastSetTime (ThisContext c)
    {
        auto *co = Clock::Object::self(c);
        
        return co->m_timer_time[Index];
    }
    
private:
    static void timer_handler (AtomicContext<Context> c)
    {
        auto *co = Clock::Object::self(c);
        AMBRO_ASSERT(co->m_timer_active[Index])
        
        if (!Handler::call(c)) {
            co->m_timer_active[Index] = false;
        }
    }
    
public:
    struct Object : public ObjBase<LinuxClockInterruptTimer, ParentObject, MakeTypeList<TheDebugObject>> {};
};

APRINTER_ALIAS_STRUCT_EXT(LinuxClockInterruptTimerService, (
    APRINTER_AS_VALUE(int, Index),
    APRINTER_AS_TYPE(ExtraClearance)
), (
    APRINTER_ALIAS_STRUCT_EXT(InterruptTimer, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Handler)
    ), (
        using Params = LinuxClockInterruptTimerService;
        APRINTER_DEF_INSTANCE(InterruptTimer, LinuxClockInterruptTimer)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
