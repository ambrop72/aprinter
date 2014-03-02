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
#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Pin, bool Invert, typename PulseInterval, typename TimerCallback, template<typename, typename, typename> class TimerTemplate>
class SoftPwm {
private:
    struct TimerHandler;
    
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TimerInstance = TimerTemplate<Context, Object, TimerHandler>;
    
    struct PowerData {
        TimeType on_time;
        uint8_t type;
    };
    
    static void init (Context c, TimeType start_time)
    {
        auto *o = Object::self(c);
        TimerInstance::init(c);
        o->m_state = false;
        o->m_start_time = start_time;
        c.pins()->template set<Pin>(c, Invert);
        c.pins()->template setOutput<Pin>(c);
        TimerInstance::setFirst(c, start_time);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        TimerInstance::deinit(c);
        c.pins()->template set<Pin>(c, Invert);
    }
    
    using GetTimer = TimerInstance;
    
    static void computeZeroPowerData (PowerData *pd)
    {
        pd->type = 0;
    }
    
    template <typename FpType>
    static void computePowerData (FpType frac, PowerData *pd)
    {
        if (!(frac > 0.005f)) {
            pd->type = 0;
        } else {
            if (!(frac < 0.995f)) {
                pd->type = 2;
            } else {
                pd->type = 1;
                pd->on_time = frac * (FpType)interval;
            }
        }
    }
    
private:
    static const TimeType interval = PulseInterval::value() / Clock::time_unit;
    
    static bool timer_handler (typename TimerInstance::HandlerContext c)
    {
        auto *o = Object::self(c);
        
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
        TimerInstance::setNext(c, next_time);
        return true;
    }
    
    struct TimerHandler : public AMBRO_WFUNC_TD(&SoftPwm::timer_handler) {};
    
public:
    struct Object : public ObjBase<SoftPwm, ParentObject, MakeTypeList<
        TimerInstance
    >>,
        public DebugObject<Context, void>
    {
        bool m_state;
        TimeType m_start_time;
    };
};

#include <aprinter/EndNamespace.h>

#endif
