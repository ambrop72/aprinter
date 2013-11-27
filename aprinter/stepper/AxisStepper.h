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

#ifndef AMBROLIB_AXIS_STEPPER_H
#define AMBROLIB_AXIS_STEPPER_H

#include <stdint.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/Options.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

#define AXIS_STEPPER_AMUL_EXPR(x, t, a) ((a).template shiftBits<(-2)>())
#define AXIS_STEPPER_V0_EXPR(x, t, a) (((x).toSigned() + (a)).toUnsignedUnsafe())
#define AXIS_STEPPER_V02_EXPR(x, t, a) ((AXIS_STEPPER_V0_EXPR((x), (t), (a)) * AXIS_STEPPER_V0_EXPR((x), (t), (a)))).toSigned()
#define AXIS_STEPPER_DISCRIMINANT_EXPR(x, t, a) (AXIS_STEPPER_V02_EXPR(x, t, a) + ((-AXIS_STEPPER_AMUL_EXPR(x, t, a) * (x)).template shift<2>()))
#define AXIS_STEPPER_TMUL_EXPR(x, t, a) ((t).template bitsTo<time_mul_bits>())

#define AXIS_STEPPER_AMUL_EXPR_HELPER(args) AXIS_STEPPER_AMUL_EXPR(args)
#define AXIS_STEPPER_V0_EXPR_HELPER(args) AXIS_STEPPER_V0_EXPR(args)
#define AXIS_STEPPER_DISCRIMINANT_EXPR_HELPER(args) AXIS_STEPPER_DISCRIMINANT_EXPR(args)
#define AXIS_STEPPER_TMUL_EXPR_HELPER(args) AXIS_STEPPER_TMUL_EXPR(args)

#define AXIS_STEPPER_DUMMY_VARS (StepFixedType()), (TimeFixedType()), (AccelFixedType())

template <
    template<typename, typename, typename> class TTimer
>
struct AxisStepperParams {
    template<typename X, typename Y, typename Z> using Timer = TTimer<X, Y, Z>;
};

template <typename TCommandCallback, typename TPrestepCallback>
struct AxisStepperConsumer {
    using CommandCallback = TCommandCallback;
    using PrestepCallback = TPrestepCallback;
};

template <typename Position, typename Context, typename Params, typename Stepper, typename GetStepper, typename ConsumersList>
class AxisStepper
: private DebugObject<Context, void>
{
private:
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_maybe_call_command_callback, maybe_call_command_callback)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_maybe_call_prestep_callback, maybe_call_prestep_callback)
    
    // DON'T TOUCH!
    // These were chosen carefully for speed, and some operations
    // were written in assembly specifically for use here.
    static const int step_bits = 11;
    static const int time_bits = 22;
    static const int q_div_shift = 16;
    static const int time_mul_bits = 24;
    
    struct TimerHandler;
    struct TimerPosition;
    
    static AxisStepper * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TimerInstance = typename Params::template Timer<TimerPosition, Context, TimerHandler>;
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using DirStepFixedType = FixedPoint<step_bits + 1, false, 0>;
    using SignedStepFixedType = FixedPoint<step_bits, true, 0>;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    using CommandCallbackContext = typename TimerInstance::HandlerContext;
    
    struct Command {
        union {
            DirStepFixedType dir_x;
            SignedStepFixedType x;
        };
        decltype(AXIS_STEPPER_DISCRIMINANT_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) discriminant;
        decltype(AXIS_STEPPER_AMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) a_mul;
        decltype(AXIS_STEPPER_TMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) t_mul;
    };
    
    static void generate_command (bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a, Command *cmd)
    {
        AMBRO_ASSERT(a >= -x)
        AMBRO_ASSERT(a <= x)
        
        // keep the order of the computation consistent with the dependencies between
        // these macros, to make it easier for the compiler to optimize
        cmd->dir_x = DirStepFixedType::importBits(x.bitsValue() | ((typename DirStepFixedType::IntType)dir << step_bits));
        cmd->a_mul = AXIS_STEPPER_AMUL_EXPR(x, t, a);
        cmd->discriminant = AXIS_STEPPER_DISCRIMINANT_EXPR(x, t, a);
        cmd->t_mul = AXIS_STEPPER_TMUL_EXPR(x, t, a);
    }
    
    static void init (Context c)
    {
        AxisStepper *o = self(c);
        
        o->m_timer.init(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        AxisStepper *o = self(c);
        o->debugDeinit(c);
        AMBRO_ASSERT(!o->m_running)
        
        o->m_timer.deinit(c);
    }
    
    static void setPrestepCallbackEnabled (Context c, bool enabled)
    {
        AxisStepper *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        
        o->m_prestep_callback_enabled = enabled;
    }
    
    template <typename TheConsumer>
    static void start (Context c, TimeType start_time, Command *first_command)
    {
        AxisStepper *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(first_command)
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        o->m_consumer_id = TypeListIndex<typename ConsumersList::List, IsEqualFunc<TheConsumer>>::value;
        o->m_current_command = first_command;
        o->m_time = o->m_current_command->t_mul.template bitsTo<time_bits>().bitsValue() + start_time;
        stepper(c)->setDir(c, o->m_current_command->dir_x.bitsValue() & ((typename DirStepFixedType::IntType)1 << step_bits));
        o->m_current_command->x = SignedStepFixedType::importBits(o->m_current_command->dir_x.bitsValue() & (((typename DirStepFixedType::IntType)1 << step_bits) - 1));
        o->m_v0 = (o->m_current_command->a_mul.template undoShiftBitsLeft<2>() + o->m_current_command->x).toUnsignedUnsafe();
        TimeType timer_t = (o->m_current_command->x.bitsValue() == 0) ? o->m_time : start_time;
        o->m_timer.setFirst(c, timer_t);
    }
    
    static void stop (Context c)
    {
        AxisStepper *o = self(c);
        o->debugAccess(c);
        
        o->m_timer.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    TimerInstance * getTimer ()
    {
        return &m_timer;
    }
    
#ifdef AMBROLIB_ASSERTIONS
    static bool isRunning (Context c)
    {
        AxisStepper *o = self(c);
        o->debugAccess(c);
        
        return o->m_running;
    }
#endif
    
private:
    static Stepper * stepper (Context c)
    {
        return GetStepper::call(c);
    }
    
    template <int ConsumerIndex>
    struct CallbackHelper {
        using TheConsumer = TypeListGet<typename ConsumersList::List, ConsumerIndex>;
        
        template <typename Ret, typename... Args>
        static bool maybe_call_command_callback (AxisStepper *o, uint8_t consumer_id, Ret *ret, Args... args)
        {
            if (AMBRO_LIKELY(consumer_id == ConsumerIndex || ConsumerIndex == TypeListLength<typename ConsumersList::List>::value - 1)) {
                *ret = TheConsumer::CommandCallback::call(args...);
                return false;
            }
            return true;
        }
        
        template <typename Ret, typename... Args>
        static bool maybe_call_prestep_callback (AxisStepper *o, uint8_t consumer_id, Ret *ret, Args... args)
        {
            if (AMBRO_LIKELY(consumer_id == ConsumerIndex || ConsumerIndex == TypeListLength<typename ConsumersList::List>::value - 1)) {
                *ret = TheConsumer::PrestepCallback::call(args...);
                return false;
            }
            return true;
        }
    };
    
    static bool timer_handler (TimerInstance *, typename TimerInstance::HandlerContext c)
    {
        AxisStepper *o = self(c);
        AMBRO_ASSERT(o->m_running)
        
        SignedStepFixedType x = o->m_current_command->x;
        if (AMBRO_LIKELY(x.bitsValue() == 0)) {
            IndexElemTuple<typename ConsumersList::List, CallbackHelper> dummy;
            bool res;
            TupleForEachForwardInterruptible(&dummy, Foreach_maybe_call_command_callback(), o, o->m_consumer_id, &res, c, &o->m_current_command);
            if (AMBRO_UNLIKELY(!res)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                return false;
            }
            
            o->m_time += o->m_current_command->t_mul.template bitsTo<time_bits>().bitsValue();
            stepper(c)->setDir(c, o->m_current_command->dir_x.bitsValue() & ((typename DirStepFixedType::IntType)1 << step_bits));
            x = SignedStepFixedType::importBits(o->m_current_command->dir_x.bitsValue() & (((typename DirStepFixedType::IntType)1 << step_bits) - 1));
            o->m_v0 = (o->m_current_command->a_mul.template undoShiftBitsLeft<2>() + x).toUnsignedUnsafe();
            
            if (AMBRO_UNLIKELY(x.bitsValue() == 0)) {
                o->m_current_command->x = SignedStepFixedType::importBits(0);
                o->m_timer.setNext(c, o->m_time);
                return true;
            }
        }
        
        o->m_current_command->x = SignedStepFixedType::importBits(x.bitsValue() - 1);
        
        if (AMBRO_UNLIKELY(o->m_prestep_callback_enabled)) {
            IndexElemTuple<typename ConsumersList::List, CallbackHelper> dummy;
            bool res;
            TupleForEachForwardInterruptible(&dummy, Foreach_maybe_call_prestep_callback(), o, o->m_consumer_id, &res, c);
            if (AMBRO_UNLIKELY(res)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                return false;
            }
        }
        
        stepper(c)->stepOn(c);
        
        o->m_current_command->discriminant.m_bits.m_int += o->m_current_command->a_mul.m_bits.m_int;
        AMBRO_ASSERT(o->m_current_command->discriminant.bitsValue() >= 0)
        
        auto q = (o->m_v0 + FixedSquareRoot<true>(o->m_current_command->discriminant, OptionForceInline())).template shift<-1>();
        
        auto t_frac = FixedFracDivide(StepFixedType::importBits(o->m_current_command->x.bitsValue()), q, OptionForceInline());
        
        TimeFixedType t = FixedResMultiply(o->m_current_command->t_mul, t_frac);
        
        stepper(c)->stepOff(c);
        
        o->m_timer.setNext(c, (TimeType)(o->m_time - t.bitsValue()));
        return true;
    }
    
    TimerInstance m_timer;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
    uint8_t m_consumer_id;
    Command *m_current_command;
    TimeType m_time;
    decltype(AXIS_STEPPER_V0_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) m_v0;
    bool m_prestep_callback_enabled;
    
    struct TimerHandler : public AMBRO_WFUNC_TD(&AxisStepper::timer_handler) {};
    struct TimerPosition : public MemberPosition<Position, TimerInstance, &AxisStepper::m_timer> {};
};

#include <aprinter/EndNamespace.h>

#endif
