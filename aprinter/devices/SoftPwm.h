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

#ifndef AMBROLIB_SOFT_PWM_H
#define AMBROLIB_SOFT_PWM_H

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>

#include <aprinter/BeginNamespace.h>

template <typename Position, typename Context, typename Pin, typename PulseInterval, typename TimerCallback, template<typename, typename, typename> class TimerTemplate>
class SoftPwm
: private DebugObject<Context, void>
{
private:
    struct TimerHandler;
    struct TimerPosition;
    
    static SoftPwm * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TimerInstance = TimerTemplate<TimerPosition, Context, TimerHandler>;
    
    static void init (Context c, TimeType start_time)
    {
        SoftPwm *o = self(c);
        o->m_timer.init(c);
        o->m_state = false;
        o->m_start_time = start_time;
        c.pins()->template set<Pin>(c, false);
        c.pins()->template setOutput<Pin>(c);
        o->m_timer.set(c, start_time);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        SoftPwm *o = self(c);
        o->debugDeinit(c);
        
        o->m_timer.deinit(c);
        c.pins()->template set<Pin>(c, false);
    }
    
    TimerInstance * getTimer ()
    {
        return &m_timer;
    }
    
private:
    static const TimeType interval = PulseInterval::value() / Clock::time_unit;
    
    static bool timer_handler (TimerInstance *, typename TimerInstance::HandlerContext c)
    {
        SoftPwm *o = self(c);
        o->debugAccess(c);
        
        TimeType next_time;
        if (AMBRO_LIKELY(!o->m_state)) {
            auto frac = TimerCallback::call(c);
            c.pins()->template set<Pin>(c, (frac.bitsValue() > 0));
            if (AMBRO_LIKELY(frac.bitsValue() > 0 && frac < decltype(frac)::maxValue())) {
                auto res = FixedResMultiply(FixedPoint<32, false, 0>::importBits(interval), frac);
                next_time = o->m_start_time + (TimeType)res.bitsValue();
                o->m_state = true;
            } else {
                o->m_start_time += interval;
                next_time = o->m_start_time;
            }
        } else {
            c.pins()->template set<Pin>(c, false);
            o->m_start_time += interval;
            next_time = o->m_start_time;
            o->m_state = false;
        }
        o->m_timer.set(c, next_time);
        return true;
    }
    
    TimerInstance m_timer;
    bool m_state;
    TimeType m_start_time;
    
    struct TimerHandler : public AMBRO_WFUNC_TD(&SoftPwm::timer_handler) {};
    struct TimerPosition : public MemberPosition<Position, TimerInstance, &SoftPwm::m_timer> {};
};

#include <aprinter/EndNamespace.h>

#endif
