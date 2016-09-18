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

#include <aprinter/base/Object.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class LinuxClock {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    
    static int const SubSecondBits = Arg::Params::SubSecondBits;
    
    static_assert(SubSecondBits >= 10, "");
    static_assert(SubSecondBits <= 21, "");
    
    static long const NsecInSec = 1000000000;
    static int const NanosShift = 63 - SubSecondBits;
    static uint64_t const NanosMul = ((uint64_t)1 << (SubSecondBits + NanosShift)) / NsecInSec;
    static uint32_t const SubSecondMask = ((uint32_t)1 << SubSecondBits) - 1;
    
public:
    struct Object;
    using TimeType = uint32_t;
    
    static constexpr double time_freq = (uint32_t)1 << SubSecondBits;
    static constexpr double time_unit = 1.0 / time_freq;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        TheDebugObject::access(c);
        
        struct timespec ts = getTimespec(c);
        return timespecToTime(ts);
    }
    
public:
    static void assert_timespec (struct timespec ts)
    {
        AMBRO_ASSERT(ts.tv_nsec >= 0)
        AMBRO_ASSERT(ts.tv_nsec < NsecInSec)
    }
    
    static struct timespec getTimespec (Context c)
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
    
public:
    struct Object : public ObjBase<LinuxClock, ParentObject, MakeTypeList<TheDebugObject>> {};
};

APRINTER_ALIAS_STRUCT_EXT(LinuxClockService, (
    APRINTER_AS_VALUE(int, SubSecondBits)
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
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using Handler      = typename Arg::Handler;
    using Params       = typename Arg::Params;
    
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = AtomicContext<Context>;
    
    static int const Index = Params::Index;
    using ExtraClearance = typename Params::ExtraClearance;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::init(c);
        
        o->m_running = false;
        
        // TODO
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        // TODO
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        
        o->m_time = time;
        o->m_running = true;
        
        // TODO
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        
        o->m_time = time;
        
        // TODO
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_running = false;
        
        // TODO
    }
    
    template <typename ThisContext>
    static TimeType getLastSetTime (ThisContext c)
    {
        auto *o = Object::self(c);
        
        return o->m_time;
    }
    
public:
    struct Object : public ObjBase<LinuxClockInterruptTimer, ParentObject, MakeTypeList<TheDebugObject>> {
        TimeType m_time;
        bool m_running;
    };
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
