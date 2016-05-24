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

#ifndef AMBROLIB_AXIS_DRIVER_H
#define AMBROLIB_AXIS_DRIVER_H

#include <stdint.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/Options.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/ConstexprMath.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/math/StoredNumber.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/printer/actuators/AxisDriverConsumer.h>

#include <aprinter/BeginNamespace.h>

#define AXIS_STEPPER_AMUL_EXPR(x, t, a) ((a).template shiftBits<(-amul_shift)>())
#define AXIS_STEPPER_V0_EXPR(x, t, a) (((x) + (a).absVal()).template shiftBits<(-discriminant_prec)>())
#define AXIS_STEPPER_DISCRIMINANT_EXPR(x, t, a) ((((x).toSigned() - (a)).toUnsignedUnsafe() * ((x).toSigned() - (a)).toUnsignedUnsafe()).template shiftBits<(-2 * discriminant_prec)>())
#define AXIS_STEPPER_TMUL_EXPR(x, t, a) ((t).template bitsTo<time_mul_bits>())

#define AXIS_STEPPER_AMUL_EXPR_HELPER(args) AXIS_STEPPER_AMUL_EXPR(args)
#define AXIS_STEPPER_V0_EXPR_HELPER(args) AXIS_STEPPER_V0_EXPR(args)
#define AXIS_STEPPER_DISCRIMINANT_EXPR_HELPER(args) AXIS_STEPPER_DISCRIMINANT_EXPR(args)
#define AXIS_STEPPER_TMUL_EXPR_HELPER(args) AXIS_STEPPER_TMUL_EXPR(args)

#define AXIS_STEPPER_DUMMY_VARS (StepFixedType()), (TimeFixedType()), (AccelFixedType())

APRINTER_ALIAS_STRUCT(AxisDriverPrecisionParams, (
    APRINTER_AS_VALUE(int, step_bits),
    APRINTER_AS_VALUE(int, time_bits),
    APRINTER_AS_VALUE(int, time_mul_bits),
    APRINTER_AS_VALUE(int, discriminant_prec),
    APRINTER_AS_VALUE(int, rel_t_extra_prec),
    APRINTER_AS_VALUE(bool, preshift_accel)
))

using AxisDriverAvrPrecisionParams = AxisDriverPrecisionParams<11, 22, 24, 1, 0, true>;
using AxisDriverDuePrecisionParams = AxisDriverPrecisionParams<11, 28, 28, 3, 4, false>;

template <typename Arg>
class AxisDriver {
    using Context       = typename Arg::Context;
    using ParentObject  = typename Arg::ParentObject;
    using Stepper       = typename Arg::Stepper;
    using ConsumersList = typename Arg::ConsumersList;
    using Params        = typename Arg::Params;
    
private:
    static const int step_bits = Params::PrecisionParams::step_bits;
    static const int time_bits = Params::PrecisionParams::time_bits;
    static const int time_mul_bits = Params::PrecisionParams::time_mul_bits;
    static const int discriminant_prec = Params::PrecisionParams::discriminant_prec;
    static const int rel_t_extra_prec = Params::PrecisionParams::rel_t_extra_prec;
    static const int amul_shift = 2 * (1 + discriminant_prec);
    static bool const PreloadCommands = Params::PreloadCommands;
    
    struct TimerHandler;
    
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    APRINTER_MAKE_INSTANCE(TimerInstance, (Params::TimerService::template InterruptTimer<Context, Object, TimerHandler>))
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using DirStepFixedType = FixedPoint<step_bits + 2, false, 0>;
    using DirStepIntType = typename DirStepFixedType::IntType;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    using TimeMulFixedType = decltype(AXIS_STEPPER_TMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS));
    using CommandCallbackContext = typename TimerInstance::HandlerContext;
    using TMulStored = StoredNumber<TimeMulFixedType::num_bits, TimeMulFixedType::is_signed>;
    using DiscriminantType = decltype(AXIS_STEPPER_DISCRIMINANT_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS));
    using V0Type = decltype(AXIS_STEPPER_V0_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS));
    using AMulType = decltype(AXIS_STEPPER_AMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS));
    using ADiscShiftedType = decltype(AccelFixedType().template shiftBits<(-discriminant_prec)>());
    using DelayParams = typename Params::DelayParams;
    using StepContext = typename TimerInstance::HandlerContext;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    AMBRO_STRUCT_IF(AccelShiftMode, Params::PrecisionParams::preshift_accel) {
        using CommandAccelType = AMulType;
        
        struct ExtraMembers {};
        
        AMBRO_ALWAYS_INLINE
        static CommandAccelType make_command_accel (AccelFixedType a)
        {
            return a.template shiftBits<(-amul_shift)>();
        }
        
        AMBRO_ALWAYS_INLINE
        static ADiscShiftedType compute_accel_for_load (Context c, CommandAccelType accel)
        {
            return accel.template undoShiftBitsLeft<(amul_shift-discriminant_prec)>();
        }
        
        template <typename TheCommand>
        AMBRO_ALWAYS_INLINE
        static AMulType get_a_mul_for_step (Context c, TheCommand *command)
        {
            return command->accel;
        }
    }
    AMBRO_STRUCT_ELSE(AccelShiftMode) {
        using CommandAccelType = AccelFixedType;
        
        struct ExtraMembers {
            AMulType m_a_mul;
        };
        
        AMBRO_ALWAYS_INLINE
        static CommandAccelType make_command_accel (AccelFixedType a)
        {
            return a;
        }
        
        AMBRO_ALWAYS_INLINE
        static ADiscShiftedType compute_accel_for_load (Context c, CommandAccelType accel)
        {
            auto *o = Object::self(c);
            
            o->m_a_mul = accel.template shiftBits<(-amul_shift)>();
            return accel.template shiftBits<(-discriminant_prec)>();
        }
        
        template <typename TheCommand>
        AMBRO_ALWAYS_INLINE
        static AMulType get_a_mul_for_step (Context c, TheCommand *command)
        {
            auto *o = Object::self(c);
            
            return o->m_a_mul;
        }
    };
    
public:
    static constexpr double AsyncMinStepTime() { return DelayFeature::AsyncMinStepTime(); }
    static constexpr double SyncMinStepTime() { return DelayFeature::SyncMinStepTime(); }
    
    struct Command {
        DirStepFixedType dir_x;
        typename AccelShiftMode::CommandAccelType accel;
        TMulStored t_mul_stored;
    };
    
    AMBRO_ALWAYS_INLINE
    static void generate_command (bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a, Command *cmd)
    {
        AMBRO_ASSERT(a >= -x)
        AMBRO_ASSERT(a <= x)
        
        cmd->t_mul_stored = TMulStored::store(AXIS_STEPPER_TMUL_EXPR(x, t, a).m_bits.m_int);
        cmd->dir_x = DirStepFixedType::importBits(
            x.bitsValue() |
            ((DirStepIntType)dir << step_bits) |
            ((DirStepIntType)(a.bitsValue() >= 0) << (step_bits + 1))
        );
        cmd->accel = AccelShiftMode::make_command_accel(a);
    }
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TimerInstance::init(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
#ifdef AXISDRIVER_DETECT_OVERLOAD
        o->m_overload = false;
#endif
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        AMBRO_ASSERT(!o->m_running)
        
        TimerInstance::deinit(c);
    }
    
    static void setPrestepCallbackEnabled (Context c, bool enabled)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        
        o->m_prestep_callback_enabled = enabled;
    }
    
    template <typename TheConsumer>
    static void start (Context c, TimeType start_time, Command *first_command)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(first_command)
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
#ifdef AXISDRIVER_DETECT_OVERLOAD
        o->m_overload = false;
#endif
        o->m_consumer_id = TypeListIndex<typename ConsumersList::List, TheConsumer>::Value;
        o->m_time = start_time;
        
        bool command_completed = load_command(c, first_command);
        TimeType timer_t = (!PreloadCommands && command_completed) ? o->m_time : start_time;
        
        DelayFeature::set_step_timer_to_now(c);
        TimerInstance::setFirst(c, timer_t);
    }
    
    static void stop (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        TimerInstance::unset(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static StepFixedType getAbortedCmdSteps (Context c, bool *dir)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        
        *dir = (o->m_current_command->dir_x.bitsValue() & ((DirStepIntType)1 << step_bits));
        if (!o->m_notend) {
            return StepFixedType::importBits(0);
        } else {
            if (o->m_notdecel) {
                return StepFixedType::importBits(o->m_pos.bitsValue() + 1);
            } else {
                return StepFixedType::importBits((o->m_x.bitsValue() - o->m_pos.bitsValue()) + 1);
            }
        }
    }
    
    static StepFixedType getPendingCmdSteps (Context c, Command const *cmd, bool *dir)
    {
        *dir = (cmd->dir_x.bitsValue() & ((DirStepIntType)1 << step_bits));
        return StepFixedType::importBits(cmd->dir_x.bitsValue() & (((DirStepIntType)1 << step_bits) - 1));
    }
    
#ifdef AXISDRIVER_DETECT_OVERLOAD
    static bool overloadOccurred (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        
        return o->m_overload;
    }
#endif
    
    using GetTimer = TimerInstance;
    
private:
    template <int ConsumerIndex>
    struct CallbackHelper {
        using TheConsumer = TypeListGet<typename ConsumersList::List, ConsumerIndex>;
        
        template <typename... Args>
        AMBRO_ALWAYS_INLINE
        static bool call_command_callback (Args... args)
        {
            return TheConsumer::CommandCallback::call(args...);
        }
        
        template <typename... Args>
        AMBRO_ALWAYS_INLINE
        static bool call_prestep_callback (Args... args)
        {
            return TheConsumer::PrestepCallback::call(args...);
        }
    };
    
    template <typename This=AxisDriver>
    using CallbackHelperList = IndexElemList<typename This::ConsumersList::List, CallbackHelper>;
    
    template <typename T>
    inline static T volatile_read (T &x)
    {
        return *(T volatile *)&x;
    }
    
    template <typename T>
    inline static void volatile_write (T &x, T v)
    {
        *(T volatile *)&x = v;
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static bool load_command (ThisContext c, Command *command)
    {
        auto *o = Object::self(c);
        
        Stepper::setDir(c, command->dir_x.bitsValue()  & ((DirStepIntType)1 << step_bits));
        DelayFeature::set_dir_timer_for_step(c);
        
        // Below we do some volatile memory accesses, to guarantee that at least some
        // calculations are done after the setDir(). We want the new direction signal
        // to be applied for a sufficient time before the step signal is raised.
        
        DirStepFixedType dir_x = DirStepFixedType::importBits(volatile_read(command->dir_x.m_bits.m_int));
        
        o->m_current_command = command;
        o->m_notdecel = (dir_x.bitsValue() & ((DirStepIntType)1 << (step_bits + 1)));
        StepFixedType x = StepFixedType::importBits(dir_x.bitsValue() & (((DirStepIntType)1 << step_bits) - 1));
        o->m_notend = (x.bitsValue() != 0);
        
        if (AMBRO_UNLIKELY(!o->m_notend)) {
            o->m_time += TimeMulFixedType::importBits(TMulStored::retrieve(command->t_mul_stored)).template bitsTo<time_bits>().bitsValue();
            return true;
        }
        
        auto xs = x.toSigned().template shiftBits<(-discriminant_prec)>();
        auto command_accel = AccelShiftMode::CommandAccelType::importBits(volatile_read(command->accel.m_bits.m_int));
        ADiscShiftedType a = AccelShiftMode::compute_accel_for_load(c, command_accel);
        auto x_minus_a = (xs - a).toUnsignedUnsafe();
        if (AMBRO_LIKELY(o->m_notdecel)) {
            o->m_v0 = (xs + a).toUnsignedUnsafe();
            o->m_pos = StepFixedType::importBits(x.bitsValue() - 1);
            o->m_time += TimeMulFixedType::importBits(TMulStored::retrieve(command->t_mul_stored)).template bitsTo<time_bits>().bitsValue();
        } else {
            o->m_x = x;
            o->m_v0 = x_minus_a;
            o->m_pos = StepFixedType::importBits(1);
        }
        volatile_write(o->m_discriminant.m_bits.m_int, (x_minus_a * x_minus_a).bitsValue());
        
        return false;
    }
    
    static bool try_pull_command (StepContext c, Command **command)
    {
        auto *o = Object::self(c);
        
        bool res = ListForOne<CallbackHelperList<>, 0, bool>(o->m_consumer_id, [&] APRINTER_TL(helper, return helper::call_command_callback(c, command)));
        if (AMBRO_UNLIKELY(!res)) {
#ifdef AMBROLIB_ASSERTIONS
            o->m_running = false;
#endif
            return false;
        }
        
        return true;
    }
    
    static bool timer_handler (StepContext c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        
#ifdef AXISDRIVER_DETECT_OVERLOAD
        if ((TimeType)(Clock::getTime(c) - TimerInstance::getLastSetTime(c)) >= (TimeType)(0.001 * Clock::time_freq)) {
            o->m_overload = true;
        }
#endif
        
        Command *current_command = o->m_current_command;
        
        if (!PreloadCommands && AMBRO_LIKELY(!o->m_notend)) {
            if (!try_pull_command(c, &current_command)) {
                return false;
            }
            
            bool command_completed = load_command(c, current_command);
            if (command_completed) {
                DelayFeature::wait_for_step_low(c);
                TimerInstance::setNext(c, o->m_time);
                return true;
            }
            
#ifdef AMBROLIB_AVR
            // Force gcc to load the parameters we computed here (m_x, m_v0, m_pos)
            // when they're used below even though they could be kept in registers.
            // If we don't do that, these optimizations may use lots more registers
            // and we end up with slower code.
            asm volatile ("" ::: "memory");
#endif
        }
        
        TimeType next_time;
        
        if (!PreloadCommands || AMBRO_LIKELY(o->m_notend)) {
            if (AMBRO_UNLIKELY(o->m_prestep_callback_enabled)) {
                bool res = ListForOne<CallbackHelperList<>, 0, bool>(o->m_consumer_id, [&] APRINTER_TL(helper, return helper::call_prestep_callback(c)));
                if (AMBRO_UNLIKELY(res)) {
    #ifdef AMBROLIB_ASSERTIONS
                    o->m_running = false;
    #endif
                    return false;
                }
            }
            
            DelayFeature::wait_for_dir(c);
            
            DelayFeature::wait_for_step_low(c);
            Stepper::stepOn(c);
            DelayFeature::set_step_timer_for_high(c);
            
            // We need to ensure that the step signal is sufficiently long for the stepper driver
            // to register. To this end, we do the timely calculations in between stepOn and stepOff().
            // But to prevent the compiler from moving upwards any significant part of the calculation,
            // we do a volatile read of the discriminant (an input to the calculation).
            
            auto discriminant_bits = volatile_read(o->m_discriminant.m_bits.m_int);
            o->m_discriminant.m_bits.m_int = discriminant_bits + AccelShiftMode::get_a_mul_for_step(c, current_command).m_bits.m_int;
            AMBRO_ASSERT(o->m_discriminant.bitsValue() >= 0)
            
            auto q = (o->m_v0 + FixedSquareRoot<true>(o->m_discriminant, OptionForceInline())).template shift<-1>();
            
            auto t_frac = FixedFracDivide<rel_t_extra_prec>(o->m_pos, q, OptionForceInline());
            
            auto t_mul = TimeMulFixedType::importBits(TMulStored::retrieve(current_command->t_mul_stored));
            TimeFixedType t = FixedResMultiply(t_mul, t_frac);
            
            // Now make sure the calculations above happen before stepOff().
            volatile_write(o->m_dummy, (uint8_t)t.bitsValue());
            
            DelayFeature::wait_for_step_high(c);
            Stepper::stepOff(c);
            DelayFeature::set_step_timer_for_low(c);
            
            if (AMBRO_LIKELY(!o->m_notdecel)) {
                if (AMBRO_LIKELY(o->m_pos == o->m_x)) {
                    o->m_time += t_mul.template bitsTo<time_bits>().bitsValue();
                    o->m_notend = false;
                    next_time = o->m_time;
                } else {
                    o->m_pos.m_bits.m_int++;
                    next_time = (o->m_time + t.bitsValue());
                }
            } else {
                if (o->m_pos.bitsValue() == 0) {
                    o->m_notend = false;
                }
                o->m_pos.m_bits.m_int--;
                next_time = (o->m_time - t.bitsValue());
            }
        } else {
            DelayFeature::wait_for_step_low(c);
        }
        
        if (PreloadCommands && AMBRO_LIKELY(!o->m_notend)) {
            if (!try_pull_command(c, &current_command)) {
                return false;
            }
            
            next_time = o->m_time;
            
            load_command(c, current_command);
        }
        
        TimerInstance::setNext(c, next_time);
        return true;
    }
    struct TimerHandler : public AMBRO_WFUNC_TD(&AxisDriver::timer_handler) {};
    
    AMBRO_STRUCT_IF(DelayFeature, DelayParams::Enabled) {
        using DelayClockUtils = FastClockUtils<Context>;
        using DelayTimeType = typename DelayClockUtils::TimeType;
        
        static_assert(TimeFixedType::maxValue().bitsValue() * Clock::time_unit <= DelayClockUtils::WorkingTimeSpan, "Fast clock is too fast");
        
        // Note +1.99 to assure we do actually wait at least that much not lesser:
        // - +0.99 so that we effectively round up to an integer number of ticks (not down).
        // - +1.0 because waitSafe() only waits for the clock to increment by at least the
        //   requested number of ticks, which could takes less time than that number of
        //   clock periods.
        static DelayTimeType const MinDirSetTicks   = 1e-6 * DelayParams::DirSetTime::value()   * DelayClockUtils::time_freq + 1.99;
        static DelayTimeType const MinStepHighTicks = 1e-6 * DelayParams::StepHighTime::value() * DelayClockUtils::time_freq + 1.99;
        static DelayTimeType const MinStepLowTicks  = 1e-6 * DelayParams::StepLowTime::value()  * DelayClockUtils::time_freq + 1.99;
        
        static constexpr double MinStepTimeFactor = 1.2;
        
        static constexpr double AsyncMinStepTime ()
        {
            return MinStepTimeFactor * 1e-6 * ConstexprFmax(
                (PreloadCommands ? DelayParams::DirSetTime::value() : 0.0),
                DelayParams::StepLowTime::value()
            );
        }
        
        static constexpr double SyncMinStepTime ()
        {
            return MinStepTimeFactor * 1e-6 * (
                (!PreloadCommands ? DelayParams::DirSetTime::value() : 0.0) +
                DelayParams::StepHighTime::value()
            );
        }
        
        template <typename ThisContext>
        static void wait_for_dir (ThisContext c)
        {
            auto *o = Object::self(c);
            o->m_dir_timer.waitSafe(c, MinDirSetTicks);
        }
        
        template <typename ThisContext>
        static void wait_for_step_high (ThisContext c)
        {
            auto *o = Object::self(c);
            o->m_step_timer.waitSafe(c, MinStepHighTicks);
        }
        
        template <typename ThisContext>
        static void wait_for_step_low (ThisContext c)
        {
            auto *o = Object::self(c);
            o->m_step_timer.waitSafe(c, MinStepLowTicks);
        }
        
        template <typename ThisContext>
        static void set_dir_timer_for_step (ThisContext c)
        {
            auto *o = Object::self(c);
            o->m_dir_timer.setAfter(c, MinDirSetTicks);
        }
        
        template <typename ThisContext>
        static void set_step_timer_to_now (ThisContext c)
        {
            auto *o = Object::self(c);
            o->m_step_timer.setAfter(c, 0);
        }
        
        template <typename ThisContext>
        static void set_step_timer_for_high (ThisContext c)
        {
            auto *o = Object::self(c);
            o->m_step_timer.setAfter(c, MinStepHighTicks);
        }
        
        template <typename ThisContext>
        static void set_step_timer_for_low (ThisContext c)
        {
            auto *o = Object::self(c);
            o->m_step_timer.setAfter(c, MinStepLowTicks);
        }
        
        struct Object : public ObjBase<DelayFeature, typename AxisDriver::Object, EmptyTypeList> {
            typename DelayClockUtils::PollTimer m_dir_timer;
            typename DelayClockUtils::PollTimer m_step_timer;
        };
    }
    AMBRO_STRUCT_ELSE(DelayFeature) {
        static constexpr double AsyncMinStepTime () { return 0.0; }
        static constexpr double SyncMinStepTime () { return 0.0; }
        template <typename ThisContext> static void wait_for_dir (ThisContext c) {}
        template <typename ThisContext> static void wait_for_step_high (ThisContext c) {}
        template <typename ThisContext> static void wait_for_step_low (ThisContext c) {}
        template <typename ThisContext> static void set_dir_timer_for_step (ThisContext c) {}
        template <typename ThisContext> static void set_step_timer_to_now (ThisContext c) {}
        template <typename ThisContext> static void set_step_timer_for_high (ThisContext c) {}
        template <typename ThisContext> static void set_step_timer_for_low (ThisContext c) {}
        struct Object {};
    };
    
public:
    struct Object : public ObjBase<AxisDriver, ParentObject, MakeTypeList<
        TheDebugObject,
        TimerInstance,
        DelayFeature
    >>, public AccelShiftMode::ExtraMembers
    {
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
#ifdef AXISDRIVER_DETECT_OVERLOAD
        bool m_overload;
#endif
        bool m_prestep_callback_enabled;
        bool m_notend;
        bool m_notdecel;
        uint8_t m_consumer_id;
        uint8_t m_dummy;
        StepFixedType m_x;
        StepFixedType m_pos;
        V0Type m_v0;
        DiscriminantType m_discriminant;
        TimeType m_time;
        Command *m_current_command;
    };
};

struct AxisDriverNoDelayParams {
    static bool const Enabled = false;
};

APRINTER_ALIAS_STRUCT_EXT(AxisDriverDelayParams, (
    APRINTER_AS_TYPE(DirSetTime),
    APRINTER_AS_TYPE(StepHighTime),
    APRINTER_AS_TYPE(StepLowTime)
), (
    static bool const Enabled = true;
))

APRINTER_ALIAS_STRUCT_EXT(AxisDriverService, (
    APRINTER_AS_TYPE(TimerService),
    APRINTER_AS_TYPE(PrecisionParams),
    APRINTER_AS_VALUE(bool, PreloadCommands),
    APRINTER_AS_TYPE(DelayParams)
), (
    APRINTER_ALIAS_STRUCT_EXT(Driver, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Stepper),
        APRINTER_AS_TYPE(ConsumersList)
    ), (
        using Params = AxisDriverService;
        APRINTER_DEF_INSTANCE(Driver, AxisDriver)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
