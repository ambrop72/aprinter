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
#include <aprinter/meta/WrapCallback.h>
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

#define AXIS_STEPPER_AMUL_EXPR(x, t, a) (-(a).template shiftBits<(-2)>())
#define AXIS_STEPPER_V0_EXPR(x, t, a) (((x).toSigned() + (a)).toUnsignedUnsafe())
#define AXIS_STEPPER_V02_EXPR(x, t, a) ((AXIS_STEPPER_V0_EXPR((x), (t), (a)) * AXIS_STEPPER_V0_EXPR((x), (t), (a)))).toSigned()
#define AXIS_STEPPER_DISCRIMINANT_EXPR(x, t, a) (AXIS_STEPPER_V02_EXPR(x, t, a) + ((AXIS_STEPPER_AMUL_EXPR(x, t, a) * (x)).template shift<2>()))
#define AXIS_STEPPER_TMUL_EXPR(x, t, a) ((t).template bitsTo<time_mul_bits>())

#define AXIS_STEPPER_AMUL_EXPR_HELPER(args) AXIS_STEPPER_AMUL_EXPR(args)
#define AXIS_STEPPER_V0_EXPR_HELPER(args) AXIS_STEPPER_V0_EXPR(args)
#define AXIS_STEPPER_DISCRIMINANT_EXPR_HELPER(args) AXIS_STEPPER_DISCRIMINANT_EXPR(args)
#define AXIS_STEPPER_TMUL_EXPR_HELPER(args) AXIS_STEPPER_TMUL_EXPR(args)

#define AXIS_STEPPER_DUMMY_VARS (StepFixedType()), (TimeFixedType()), (AccelFixedType())

template <
    template<typename, typename> class TTimer
>
struct AxisStepperParams {
    template<typename X, typename Y> using Timer = TTimer<X, Y>;
};

template <typename TPosition, typename TCommandCallback, typename TPrestepCallback>
struct AxisStepperConsumer {
    using Position = TPosition;
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
    static const int step_bits = 12;
    static const int time_bits = 22;
    static const int q_div_shift = 16;
    static const int time_mul_bits = 24;
    
    struct TimerHandler;
    
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TimerInstance = typename Params::template Timer<Context, TimerHandler>;
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    using CommandCallbackContext = typename TimerInstance::HandlerContext;
    
    struct Command {
        StepFixedType x;
        decltype(AXIS_STEPPER_DISCRIMINANT_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) discriminant;
        decltype(AXIS_STEPPER_AMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) a_mul;
        decltype(AXIS_STEPPER_V0_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) v0;
        decltype(AXIS_STEPPER_TMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) t_mul;
        bool dir;
    };
    
    static void generate_command (bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a, Command *cmd)
    {
        AMBRO_ASSERT(a >= -x)
        AMBRO_ASSERT(a <= x)
        
        // keep the order of the computation consistent with the dependencies between
        // these macros, to make it easier for the compiler to optimize
        cmd->x = x;
        cmd->v0 = AXIS_STEPPER_V0_EXPR(x, t, a);
        cmd->a_mul = AXIS_STEPPER_AMUL_EXPR(x, t, a);
        cmd->discriminant = AXIS_STEPPER_DISCRIMINANT_EXPR(x, t, a);
        cmd->t_mul = AXIS_STEPPER_TMUL_EXPR(x, t, a);
        cmd->dir = dir;
    }
    
    void init (Context c)
    {
        m_timer.init(c);
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AMBRO_ASSERT(!m_running)
        
        m_timer.deinit(c);
    }
    
    void setPrestepCallbackEnabled (Context c, bool enabled)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        m_prestep_callback_enabled = enabled;
    }
    
    template <typename TheConsumer>
    void start (Context c, TimeType start_time, Command *first_command)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        AMBRO_ASSERT(first_command)
        
#ifdef AMBROLIB_ASSERTIONS
        m_running = true;
#endif
        m_consumer_id = TypeListIndex<typename ConsumersList::List, IsEqualFunc<TheConsumer>>::value;
        m_current_command = first_command;
        m_time = m_current_command->t_mul.template bitsTo<time_bits>().bitsValue() + start_time;
        stepper(this)->setDir(c, m_current_command->dir);
        TimeType timer_t = (m_current_command->x.bitsValue() == 0) ? m_time : start_time;
        m_timer.set(c, timer_t);
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        
        m_timer.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
    }
    
    TimerInstance * getTimer ()
    {
        return &m_timer;
    }
    
#ifdef AMBROLIB_ASSERTIONS
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_running;
    }
#endif
    
private:
    static Stepper * stepper (AxisStepper *o)
    {
        return GetStepper::call(o);
    }
    
    template <int ConsumerIndex>
    struct CallbackHelper {
        using TheConsumer = TypeListGet<typename ConsumersList::List, ConsumerIndex>;
        using ConsumerPosition = typename TheConsumer::Position;
        
        template <typename Ret, typename... Args>
        void maybe_call_command_callback (AxisStepper *o, uint8_t consumer_id, Ret *ret, Args... args)
        {
            if (consumer_id == ConsumerIndex) {
                *ret = TheConsumer::CommandCallback::call(PositionTraverse<Position, ConsumerPosition>(o), args...);
            }
        }
        
        template <typename Ret, typename... Args>
        void maybe_call_prestep_callback (AxisStepper *o, uint8_t consumer_id, Ret *ret, Args... args)
        {
            if (consumer_id == ConsumerIndex) {
                *ret = TheConsumer::PrestepCallback::call(PositionTraverse<Position, ConsumerPosition>(o), args...);
            }
        }
    };
    
    bool timer_handler (typename TimerInstance::HandlerContext c)
    {
        AMBRO_ASSERT(m_running)
        
        if (AMBRO_LIKELY(m_current_command->x.bitsValue() == 0)) {
            IndexElemTuple<typename ConsumersList::List, CallbackHelper> dummy;
            TupleForEachForward(&dummy, Foreach_maybe_call_command_callback(), this, m_consumer_id, &m_current_command, c);
            if (AMBRO_UNLIKELY(!m_current_command)) {
#ifdef AMBROLIB_ASSERTIONS
                m_running = false;
#endif
                return false;
            }
            
            m_time += m_current_command->t_mul.template bitsTo<time_bits>().bitsValue();
            stepper(this)->setDir(c, m_current_command->dir);
            
            if (AMBRO_UNLIKELY(m_current_command->x.bitsValue() == 0)) {
                TimeType timer_t = m_time;
                m_timer.set(c, timer_t);
                return true;
            }
        }
        
        m_current_command->x.m_bits.m_int--;
        
        if (AMBRO_UNLIKELY(m_prestep_callback_enabled)) {
            IndexElemTuple<typename ConsumersList::List, CallbackHelper> dummy;
            bool res;
            TupleForEachForward(&dummy, Foreach_maybe_call_prestep_callback(), this, m_consumer_id, &res, c);
            if (AMBRO_UNLIKELY(res)) {
#ifdef AMBROLIB_ASSERTIONS
                m_running = false;
#endif
                return false;
            }
        }
        
        stepper(this)->step(c);
        
        m_current_command->discriminant.m_bits.m_int -= m_current_command->a_mul.m_bits.m_int;
        AMBRO_ASSERT(m_current_command->discriminant.bitsValue() >= 0)
        
        auto q = (m_current_command->v0 + FixedSquareRoot(m_current_command->discriminant, OptionForceInline())).template shift<-1>();
        
        auto t_frac = FixedFracDivide(m_current_command->x, q, OptionForceInline());
        
        TimeFixedType t = FixedResMultiply(m_current_command->t_mul, t_frac);
        
        TimeType timer_t = m_time - t.bitsValue();
        m_timer.set(c, timer_t);
        return true;
    }
    
    TimerInstance m_timer;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
    uint8_t m_consumer_id;
    Command *m_current_command;
    TimeType m_time;
    bool m_prestep_callback_enabled;
    
    struct TimerHandler : public AMBRO_WCALLBACK_TD(&AxisStepper::timer_handler, &AxisStepper::m_timer) {};
};

#include <aprinter/EndNamespace.h>

#endif
