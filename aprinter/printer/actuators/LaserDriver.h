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

#ifndef AMBROLIB_LASER_DRIVER_H
#define AMBROLIB_LASER_DRIVER_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/ChooseFixedForFloat.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Inline.h>

#include <aprinter/BeginNamespace.h>

APRINTER_ALIAS_STRUCT(LaserDriverPrecisionParams, (
    APRINTER_AS_VALUE(int, TimeBits),
    APRINTER_AS_VALUE(int, IntervalTimeBits)
))

using LaserDriverDefaultPrecisionParams = LaserDriverPrecisionParams<26, 32>;

template <typename Context, typename ParentObject, typename FpType, typename PowerInterface, typename CommandCallback, typename Params>
class LaserDriver {
private:
    struct TimerCallback;
    
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using PowerFixedType = typename PowerInterface::PowerFixedType;
    static_assert(!PowerFixedType::is_signed, "");
    using TheTimer = typename Params::InterruptTimerService::template InterruptTimer<Context, Object, TimerCallback>;
    using CommandCallbackContext = typename TheTimer::HandlerContext;
    using TimeFixedType = FixedPoint<Params::PrecisionParams::TimeBits, false, 0>;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using RequestedInterval = AMBRO_WRAP_DOUBLE(Params::AdjustmentInterval::value() / Clock::time_unit);
    static uintmax_t const MaxCount = 2 + (1.01 * TimeFixedType::maxValue().bitsValue() / RequestedInterval::value());
    using CountFixedType = FixedPoint<BitsInInt<MaxCount>::Value, false, 0>;
    using MaxIntervalTime = AMBRO_WRAP_DOUBLE(2.0 * RequestedInterval::value());
    using IntervalTimeFixedType = ChooseFixedForFloat<Params::PrecisionParams::IntervalTimeBits, false, MaxIntervalTime>;
    
public:
    struct Command {
        TimeFixedType duration;
        IntervalTimeFixedType interval_time;
        CountFixedType count;
        PowerFixedType power_end;
        PowerFixedType power_delta;
        bool is_accel;
    };
    
    AMBRO_ALWAYS_INLINE
    static void generate_command (TimeFixedType duration, FpType v_start, FpType v_end, Command *cmd)
    {
        AMBRO_ASSERT(FloatIsPosOrPosZero(v_start))
        AMBRO_ASSERT(FloatIsPosOrPosZero(v_end))
        
        cmd->duration = duration;
        cmd->count.m_bits.m_int = duration.bitsValue() * (FpType)(1.0 / RequestedInterval::value());
        if (cmd->count.m_bits.m_int == 0) {
            cmd->count.m_bits.m_int = 1;
        }
        cmd->power_end = PowerFixedType::importFpSaturatedRound(v_end);
        auto power_start = PowerFixedType::importFpSaturatedRound(v_start);
        cmd->is_accel = (cmd->power_end >= power_start);
        cmd->power_delta = PowerFixedType::importBits(cmd->is_accel ?
            (cmd->power_end.bitsValue() - power_start.bitsValue()) :
            (power_start.bitsValue() - cmd->power_end.bitsValue()));
        cmd->interval_time = IntervalTimeFixedType::importFpSaturatedRound((FpType)duration.bitsValue() / cmd->count.m_bits.m_int);
    }
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheTimer::init(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
        PowerInterface::setPower(c, PowerFixedType::importBits(0));
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        AMBRO_ASSERT(!o->m_running)
        
        TheTimer::deinit(c);
    }
    
    static void start (Context c, TimeType start_time, Command *first_command)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(first_command)
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        o->m_cmd = first_command;
        o->m_time = start_time + o->m_cmd->duration.bitsValue();
        o->m_pos = o->m_cmd->count;
        TheTimer::setFirst(c, start_time);
    }
    
    static void stop (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        TheTimer::unset(c);
        PowerInterface::setPower(c, PowerFixedType::importBits(0));
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
private:
    static bool timer_callback (typename TheTimer::HandlerContext c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        
        if (o->m_pos.m_bits.m_int == 0) {
            if (!CommandCallback::call(c, &o->m_cmd)) {
                PowerInterface::setPower(c, PowerFixedType::importBits(0));
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                return false;
            }
            o->m_time += o->m_cmd->duration.bitsValue();
            o->m_pos = o->m_cmd->count;
        }
        PowerFixedType rel_power = ((o->m_cmd->power_delta * o->m_pos) / o->m_cmd->count).template dropBitsUnsafe<PowerFixedType::num_bits>();
        PowerFixedType power = o->m_cmd->power_end;
        if (o->m_cmd->is_accel) {
            power.m_bits.m_int -= rel_power.m_bits.m_int;
        } else {
            power.m_bits.m_int += rel_power.m_bits.m_int;
        }
        PowerInterface::setPower(c, power);
        o->m_pos.m_bits.m_int--;
        TimeFixedType next_rel_time = FixedMin(o->m_cmd->duration, FixedResMultiply(o->m_pos, o->m_cmd->interval_time));
        TimeType next_time = o->m_time - next_rel_time.bitsValue();
        TheTimer::setNext(c, next_time);
        return true;
    }
    
    struct TimerCallback : public AMBRO_WFUNC_TD(&LaserDriver::timer_callback) {};
    
public:
    struct Object : public ObjBase<LaserDriver, ParentObject, MakeTypeList<
        TheDebugObject,
        TheTimer
    >> {
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
        Command *m_cmd;
        TimeType m_time;
        CountFixedType m_pos;
    };
};

APRINTER_ALIAS_STRUCT_EXT(LaserDriverService, (
    APRINTER_AS_TYPE(InterruptTimerService),
    APRINTER_AS_TYPE(AdjustmentInterval),
    APRINTER_AS_TYPE(PrecisionParams)
), (
    template <typename Context, typename ParentObject, typename FpType, typename PowerInterface, typename CommandCallback>
    using LaserDriver = LaserDriver<Context, ParentObject, FpType, PowerInterface, CommandCallback, LaserDriverService>;
))

#include <aprinter/EndNamespace.h>

#endif
