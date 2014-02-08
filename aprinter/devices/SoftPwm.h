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

template <typename Position, typename Context, typename Pin, bool Invert, typename PulseInterval, typename TimerCallback, template<typename, typename, typename> class TimerTemplate>
class SoftPwm
: private DebugObject<Context, void>
{
private:
    AMBRO_MAKE_SELF(Context, SoftPwm, Position)
    struct TimerHandler;
    struct TimerPosition;
    
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TimerInstance = TimerTemplate<TimerPosition, Context, TimerHandler>;
    
    struct PowerData {
        TimeType on_time;
        uint8_t type;
    };
    
    static void init (Context c, TimeType start_time)
    {
        SoftPwm *o = self(c);
        o->m_timer.init(c);
        o->m_state = false;
        o->m_start_time = start_time;
        c.pins()->template set<Pin>(c, Invert);
        c.pins()->template setOutput<Pin>(c);
        o->m_timer.setFirst(c, start_time);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        SoftPwm *o = self(c);
        o->debugDeinit(c);
        
        o->m_timer.deinit(c);
        c.pins()->template set<Pin>(c, Invert);
    }
    
    TimerInstance * getTimer ()
    {
        return &m_timer;
    }
    
    template <typename FracFixedType>
    static void computePowerData (FracFixedType frac, PowerData *pd)
    {
        if (frac.bitsValue() <= 0) {
            pd->type = 0;
        } else {
            if (frac >= FracFixedType::maxValue()) {
                pd->type = 2;
            } else {
                pd->type = 1;
                pd->on_time = FixedResMultiply(FixedPoint<32, false, 0>::importBits(interval), frac).bitsValue();
            }
        }
    }
    
private:
    static const TimeType interval = PulseInterval::value() / Clock::time_unit;
    
    static bool timer_handler (TimerInstance *, typename TimerInstance::HandlerContext c)
    {
        SoftPwm *o = self(c);
        
        TimeType next_time;
        if (AMBRO_LIKELY(!o->m_state)) {
            PowerData pd;
            TimerCallback::call(c, &pd);
            c.pins()->template set<Pin>(c, (pd.type != 0) != Invert);
            if (AMBRO_LIKELY(pd.type == 1)) {
                next_time = o->m_start_time + pd.on_time;
                o->m_state = true;
            } else {
                o->m_start_time += interval;
                next_time = o->m_start_time;
            }
        } else {
            c.pins()->template set<Pin>(c, Invert);
            o->m_start_time += interval;
            next_time = o->m_start_time;
            o->m_state = false;
        }
        o->m_timer.setNext(c, next_time);
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
