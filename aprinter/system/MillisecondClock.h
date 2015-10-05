/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_MILLISECOND_CLOCK_H
#define APRINTER_MILLISECOND_CLOCK_H

#include <stdint.h>
#include <math.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject>
class MillisecondClock {
public:
    struct Object;
    
    using TimeType = uint32_t;
    
    static constexpr double time_freq = 1000.0;
    static constexpr double time_unit = 1.0 / time_freq;
    
private:
    using SysClock = typename Context::Clock;
    using SysTimeType = typename SysClock::TimeType;
    using SysClockUtils = ClockUtilsForClock<SysClock>;
    
    static constexpr double systicks_to_ticks = time_freq / SysClock::time_freq;
    static SysTimeType const update_interval_ticks = (sizeof(double) == 8) ? UINT32_C(1073741824) : UINT32_C(8388608);
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        SysTimeType sys_time = SysClock::getTime(c);
        
        o->update_timer.init(c, APRINTER_CB_STATFUNC_T(&MillisecondClock::update_timer_handler));
        o->update_timer.appendAt(c, (SysTimeType)(sys_time + update_interval_ticks));
        
        o->ref_time = 0;
        o->ref_systime = sys_time;
        o->ref_remainder = 0.0;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        o->update_timer.deinit(c);
    }
    
    static TimeType getTime (Context c)
    {
        auto *o = Object::self(c);
        
        SysTimeType sys_time = SysClock::getTime(c);
        
        SysTimeType sys_time_diff = SysClockUtils::timeDifference(sys_time, o->ref_systime);
        TimeType time = o->ref_time + (TimeType)(o->ref_remainder + systicks_to_ticks * sys_time_diff);
        return time;
    }
    
private:
    static void update_timer_handler (Context c)
    {
        auto *o = Object::self(c);
        
        SysTimeType sys_time = SysClock::getTime(c);
        
        o->update_timer.appendAt(c, (SysTimeType)(sys_time + update_interval_ticks));
        
        SysTimeType sys_time_diff = SysClockUtils::timeDifference(sys_time, o->ref_systime);
        o->ref_systime = sys_time;
        double time_incr;
        o->ref_remainder = modf(o->ref_remainder + systicks_to_ticks * sys_time_diff, &time_incr);
        o->ref_time += (TimeType)time_incr;
    }
    
public:
    struct Object : public ObjBase<MillisecondClock, ParentObject, EmptyTypeList> {
        typename Context::EventLoop::TimedEvent update_timer;
        TimeType ref_time;
        SysTimeType ref_systime;
        double ref_remainder;
    };
};

#include <aprinter/EndNamespace.h>

#endif
