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

#include <stdint.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Hints.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Params>
class SoftPwm {
private:
    struct TimerHandler;
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TheTimer = typename Params::TimerService::template InterruptTimer<Context, Object, TimerHandler>;
    
    struct DutyCycleData {
        TimeType on_time;
        uint8_t type;
    };
    
    static void init (Context c, TimeType start_time)
    {
        auto *o = Object::self(c);
        
        computeZeroDutyCycle(&o->m_duty);
        TheTimer::init(c);
        o->m_state = false;
        o->m_start_time = start_time;
        Context::Pins::template set<typename Params::Pin>(c, Params::Invert);
        Context::Pins::template setOutput<typename Params::Pin>(c);
        TheTimer::setFirst(c, start_time);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        TheTimer::deinit(c);
        Context::Pins::template set<typename Params::Pin>(c, Params::Invert);
    }
    
    static void computeZeroDutyCycle (DutyCycleData *duty)
    {
        duty->type = 0;
    }
    
    template <typename FpType>
    static void computeDutyCycle (FpType frac, DutyCycleData *duty)
    {
        if (!(frac > 0.005f)) {
            duty->type = 0;
        } else {
            if (!(frac < 0.995f)) {
                duty->type = 2;
            } else {
                duty->type = 1;
                duty->on_time = frac * (FpType)Interval;
            }
        }
    }
    
    template <typename ThisContext>
    static void setDutyCycle (ThisContext c, DutyCycleData duty)
    {
        auto *o = Object::self(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            o->m_duty = duty;
        }
    }
    
    template <typename FpType>
    static FpType getCurrentDutyFp (Context c)
    {
        auto *o = Object::self(c);
        
        DutyCycleData duty;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            duty = o->m_duty;
        }
        
        return (duty.type == 0) ? 0.0f :
               (duty.type == 2) ? 1.0f :
               (duty.on_time / (FpType)Interval);
    }
    
    static void emergency ()
    {
        Context::Pins::template emergencySet<typename Params::Pin>(Params::Invert);
    }
    
private:
    static TimeType const Interval = Params::PulseInterval::value() / Clock::time_unit;
    
    static bool timer_handler (typename TheTimer::HandlerContext c)
    {
        auto *o = Object::self(c);
        
        TimeType next_time;
        if (AMBRO_LIKELY(!o->m_state)) {
            DutyCycleData duty;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                duty = o->m_duty;
            }
            Context::Pins::template set<typename Params::Pin>(c, (duty.type != 0) != Params::Invert);
            if (AMBRO_LIKELY(duty.type == 1)) {
                next_time = o->m_start_time + duty.on_time;
                o->m_state = true;
            } else {
                o->m_start_time += Interval;
                next_time = o->m_start_time;
            }
        } else {
            Context::Pins::template set<typename Params::Pin>(c, Params::Invert);
            o->m_start_time += Interval;
            next_time = o->m_start_time;
            o->m_state = false;
        }
        TheTimer::setNext(c, next_time);
        return true;
    }
    
    struct TimerHandler : public AMBRO_WFUNC_TD(&SoftPwm::timer_handler) {};
    
public:
    struct Object : public ObjBase<SoftPwm, ParentObject, MakeTypeList<
        TheDebugObject,
        TheTimer
    >> {
        DutyCycleData m_duty;
        bool m_state;
        TimeType m_start_time;
    };
};

APRINTER_ALIAS_STRUCT_EXT(SoftPwmService, (
    APRINTER_AS_TYPE(Pin),
    APRINTER_AS_VALUE(bool, Invert),
    APRINTER_AS_TYPE(PulseInterval),
    APRINTER_AS_TYPE(TimerService)
), (
    template <typename Context, typename ParentObject>
    using Pwm = SoftPwm<Context, ParentObject, SoftPwmService>;
))

#include <aprinter/EndNamespace.h>

#endif
