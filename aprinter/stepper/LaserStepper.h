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

#ifndef AMBROLIB_LASER_STEPPER_H
#define AMBROLIB_LASER_STEPPER_H

#include <stdint.h>
#include <math.h>

#include <aprinter/meta/Object.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Inline.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename FpType, typename PowerInterface, typename CommandCallback, typename Params>
class LaserStepper {
private:
    struct TimerCallback;
    
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using PowerFixedType = typename PowerInterface::PowerFixedType;
    using TheTimer = typename Params::InterruptTimerService::template InterruptTimer<Context, Object, TimerCallback>;
    using CommandCallbackContext = typename TheTimer::HandlerContext;
    
    struct Command {
        TimeType duration;
        float v_start;
        float v_end;
    };
    
    AMBRO_ALWAYS_INLINE
    static void generate_command (TimeType duration, FpType v_start, FpType v_end, Command *cmd)
    {
        AMBRO_ASSERT(FloatIsPosOrPosZero(v_start))
        AMBRO_ASSERT(FloatIsPosOrPosZero(v_end))
        
        cmd->duration = duration;
        cmd->v_start = v_start;
        cmd->v_end = v_end;
    }
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheTimer::init(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
        PowerInterface::setPower(c, PowerFixedType::importBits(0));
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        AMBRO_ASSERT(!o->m_running)
        
        TheTimer::deinit(c);
    }
    
    static void start (Context c, TimeType start_time, Command *first_command)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(first_command)
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        o->m_cmd = first_command;
        o->m_cmd_time = start_time;
        o->m_time = start_time;
        TheTimer::setFirst(c, o->m_time);
    }
    
    static void stop (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        TheTimer::unset(c);
        PowerInterface::setPower(c, PowerFixedType::importBits(0));
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
private:
    static TimeType const AdjustmentIntervalTicks = Params::AdjustmentInterval::value() / Clock::time_unit;
    
    static bool timer_callback (typename TheTimer::HandlerContext c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        
        TimeType rel_time = o->m_time - o->m_cmd_time;
        if (rel_time >= o->m_cmd->duration) {
            o->m_cmd_time += o->m_cmd->duration;
            rel_time = o->m_time - o->m_cmd_time;
            if (!CommandCallback::call(c, &o->m_cmd)) {
                PowerInterface::setPower(c, PowerFixedType::importBits(0));
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                return false;
            }
        }
        float frac = fminf(1.0f, (float)rel_time / o->m_cmd->duration);
        PowerFixedType power = PowerFixedType::importFpSaturatedRound((1.0f - frac) * o->m_cmd->v_start + frac * o->m_cmd->v_end);
        PowerInterface::setPower(c, power);
        o->m_time += AdjustmentIntervalTicks;
        TheTimer::setNext(c, o->m_time);
        return true;
    }
    
    struct TimerCallback : public AMBRO_WFUNC_TD(&LaserStepper::timer_callback) {};
    
public:
    struct Object : public ObjBase<LaserStepper, ParentObject, MakeTypeList<
        TheTimer
    >>,
        public DebugObject<Context, void>
    {
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
        Command *m_cmd;
        TimeType m_cmd_time;
        TimeType m_time;
    };
};

template <
    typename TInterruptTimerService,
    typename TAdjustmentInterval
>
struct LaserStepperService {
    using InterruptTimerService = TInterruptTimerService;
    using AdjustmentInterval = TAdjustmentInterval;
    
    template <typename Context, typename ParentObject, typename FpType, typename PowerInterface, typename CommandCallback>
    using LaserStepper = LaserStepper<Context, ParentObject, FpType, PowerInterface, CommandCallback, LaserStepperService>;
};

#include <aprinter/EndNamespace.h>

#endif
