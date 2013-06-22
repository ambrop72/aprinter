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

#include <stddef.h>
#include <stdint.h>

#include <aprinter/meta/IsPowerOfTwo.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Modulo.h>
#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/meta/WrapCallback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Lock.h>

#include <aprinter/BeginNamespace.h>

#define AXIS_STEPPER_HAMUL_EXPR(x, t, ha) ((ha).template shiftBits<(-2)>())
#define AXIS_STEPPER_V0_EXPR(x, t, ha) (((x).toSigned() - (ha)).toUnsignedUnsafe())
#define AXIS_STEPPER_V02_EXPR(x, t, ha) ((AXIS_STEPPER_V0_EXPR((x), (t), (ha)) * AXIS_STEPPER_V0_EXPR((x), (t), (ha))))
#define AXIS_STEPPER_TMUL_EXPR(x, t, ha) ((t).template bitsTo<time_mul_bits>())

#define AXIS_STEPPER_HAMUL_EXPR_HELPER(args) AXIS_STEPPER_HAMUL_EXPR(args)
#define AXIS_STEPPER_V0_EXPR_HELPER(args) AXIS_STEPPER_V0_EXPR(args)
#define AXIS_STEPPER_V02_EXPR_HELPER(args) AXIS_STEPPER_V02_EXPR(args)
#define AXIS_STEPPER_TMUL_EXPR_HELPER(args) AXIS_STEPPER_TMUL_EXPR(args)

#define AXIS_STEPPER_DUMMY_VARS (StepFixedType()), (TimeFixedType()), (AccelFixedType())

template <typename Context, typename CommandSizeType, CommandSizeType CommandBufferSize, typename GetStepper, template<typename, typename> class Timer, typename AvailHandler>
class AxisStepper
: private DebugObject<Context, AxisStepper<Context, CommandSizeType, CommandBufferSize, GetStepper, Timer, AvailHandler>>
{
    static_assert(!IntTypeInfo<CommandSizeType>::is_signed, "CommandSizeType must be unsigned");
    static_assert(IsPowerOfTwo<uintmax_t, (uintmax_t)CommandBufferSize + 1>::value, "CommandBufferSize+1 must be a power of two");
    
private:
    typedef typename Context::EventLoop Loop;
    typedef typename Context::Clock Clock;
    typedef typename Context::Lock Lock;
    
    struct D {
        static auto stepper (AxisStepper *o, Context c) -> decltype(GetStepper::call(o, c))
        {
            return GetStepper::call(o, c);
        }
    };
    
    // WARNING: these were very carefully chosen.
    // An attempt was made to:
    // - avoid 64-bit multiplications,
    // - avoid bit shifts or keep them close to being a multiple of 8,
    // - provide acceptable precision.
    // The maximum time and distance for a single move is relatively small for these
    // reasons. If longer or larger moves are desired they need to be split prior
    // to feeding them to this driver.
    static const int step_bits = 13;
    static const int time_bits = 22;
    static const int q_div_shift = 16;
    static const int time_mul_bits = 23;
    
    struct TimerHandler;
    
public:
    typedef Timer<Context, TimerHandler> TimerInstance;
    typedef typename Clock::TimeType TimeType;
    typedef int16_t StepType;
    typedef FixedPoint<step_bits, false, 0> StepFixedType;
    typedef FixedPoint<step_bits, true, 0> AccelFixedType;
    typedef FixedPoint<time_bits, false, 0> TimeFixedType;
    
    void init (Context c)
    {
        m_timer.init(c);
        m_avail_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisStepper::m_avail_event, &AxisStepper::avail_event_handler));
        m_running = false;
        m_start = 0;
        m_end = 0;
        m_event_amount = CommandBufferSize;
        m_current_command = &m_commands[m_start];
        m_lock.init(c);
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        m_lock.deinit(c);
        m_avail_event.deinit(c);
        m_timer.deinit(c);
    }
    
    void clearBuffer (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        m_start = 0;
        m_end = 0;
        m_current_command = &m_commands[m_start];
    }
    
    CommandSizeType bufferQuery (Context c)
    {
        this->debugAccess(c);
        
        CommandSizeType start;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            start = m_start;
        });
        
        return buffer_avail(start, m_end);
    }
    
    void bufferProvideTest (Context c, bool dir, float x, float t, float ha)
    {
        float step_length = 0.0125;
        bufferProvide(c, dir, x / step_length, t / Clock::time_unit, ha / step_length);
    }
    
    void bufferProvide (Context c, bool dir, StepType x_arg, TimeType t_arg, StepType ha_arg)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(bufferQuery(c) >= 1)
        AMBRO_ASSERT(x_arg >= 0)
        AMBRO_ASSERT(x_arg <= StepFixedType::BoundedIntType::maxValue())
        AMBRO_ASSERT(t_arg > 0)
        AMBRO_ASSERT(t_arg <= TimeFixedType::BoundedIntType::maxValue())
        AMBRO_ASSERT(ha_arg >= -x_arg)
        AMBRO_ASSERT(ha_arg <= x_arg)
        
        auto x = StepFixedType::importBits(x_arg);
        auto t = TimeFixedType::importBits(t_arg);
        auto ha = AccelFixedType::importBits(ha_arg);
        
        // compute the command parameters
        Command cmd;
        cmd.dir = dir;
        cmd.x = x;
        cmd.t_plain = t_arg;
        cmd.ha_mul = AXIS_STEPPER_HAMUL_EXPR(x, t, ha);
        cmd.v0 = AXIS_STEPPER_V0_EXPR(x, t, ha);
        cmd.v02 = AXIS_STEPPER_V02_EXPR(x, t, ha);
        cmd.t_mul = AXIS_STEPPER_TMUL_EXPR(x, t, ha);
        
        // compute the clock offset based on the last command. If not running start() will do it.
        if (m_running) {
            Command *last_cmd = &m_commands[buffer_last(m_end)];
            cmd.clock_offset = last_cmd->clock_offset + last_cmd->t_plain;
        }
        
        // add command to queue
        m_commands[m_end] = cmd;
        bool was_empty;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            was_empty = (m_start == m_end);
            m_end = (CommandSizeType)(m_end + 1) % buffer_mod;
        });
        
        // if we have run out of commands, continue motion
        if (m_running && was_empty) {
            D::stepper(this, c)->setDir(c, cmd.dir);
            TimeType timer_t = (cmd.x.bitsValue() == 0) ? (cmd.clock_offset + cmd.t_plain) : cmd.clock_offset;
            m_timer.set(c, timer_t);
        }
    }
    
    void bufferRequestEvent (Context c, CommandSizeType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount > 0)
        AMBRO_ASSERT(min_amount <= CommandBufferSize)
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            if (buffer_avail(m_start, m_end) >= min_amount) {
                m_event_amount = CommandBufferSize;
                m_avail_event.appendNow(lock_c);
            } else {
                m_event_amount = min_amount - 1;
                m_avail_event.unset(lock_c);
            }
        });
    }
    
    void start (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        // compute clock offsets for commands
        if (m_start == m_end) {
            Command *last_cmd = &m_commands[buffer_last(m_end)];
            last_cmd->clock_offset = start_time;
            last_cmd->t_plain = 0;
        } else {
            TimeType clock_offset = start_time;
            for (CommandSizeType i = m_start; i != m_end; i = (CommandSizeType)(i + 1) % buffer_mod) {
                m_commands[i].clock_offset = clock_offset;
                clock_offset += m_commands[i].t_plain;
            }
        }
        
        m_running = true;
        m_rel_x = 0;
        
        // unless we don['t have any commands, begin motion
        if (m_start != m_end) {
            Command *cmd = &m_commands[m_start];
            D::stepper(this, c)->setDir(c, cmd->dir);
            TimeType timer_t = (cmd->x.bitsValue() == 0) ? (cmd->clock_offset + cmd->t_plain) : cmd->clock_offset;
            m_timer.set(c, timer_t);
        }
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        m_timer.unset(c);
        m_avail_event.unset(c);
        m_running = false;
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_running;
    }
    
    TimerInstance * getTimer ()
    {
        return &m_timer;
    }
    
private:
    static const size_t buffer_mod = (size_t)CommandBufferSize + 1;
    
    static CommandSizeType buffer_avail (CommandSizeType start, CommandSizeType end)
    {
        return (CommandSizeType)((CommandSizeType)(start - 1) - end) % buffer_mod;
    }
    
    static CommandSizeType buffer_last (CommandSizeType end)
    {
        return (CommandSizeType)(end - 1) % buffer_mod;
    }
    
    struct Command {
        bool dir;
        StepFixedType x;
        TimeType t_plain;
        decltype(AXIS_STEPPER_HAMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) ha_mul;
        decltype(AXIS_STEPPER_V0_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) v0;
        decltype(AXIS_STEPPER_V02_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) v02;
        decltype(AXIS_STEPPER_TMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) t_mul;
        TimeType clock_offset;
    };
    
    template <typename ThisContext>
    void perform_steps (ThisContext c, StepType steps)
    {
        while (steps-- > 0) {
            D::stepper(this, c)->step(c);
        }
    }
    
    void timer_handler (typename TimerInstance::HandlerContext c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            AMBRO_ASSERT(m_start != m_end)
        });
        
        while (1) {
            do {
                // is command finished?
                if (m_rel_x == m_current_command->x.bitsValue()) {
                    break;
                }
                
                // imcrement position and get it into a fixed type
                m_rel_x++;
                
                // perform the step
                D::stepper(this, c)->step(c);
                
                // compute product part of discriminant
                auto s_prod = (m_current_command->ha_mul * StepFixedType::importBits(m_rel_x)).template shift<2>();
                
                // compute discriminant
                static_assert(decltype(m_current_command->v02.toSigned())::exp == decltype(s_prod)::exp, "slow shift");
                auto s = m_current_command->v02.toSigned() + s_prod;
                
                // we don't like negative discriminants
                if (s.bitsValue() < 0) {
                    perform_steps(c, m_current_command->x.bitsValue() - m_rel_x);
                    break;
                }
                
                // compute the thing with the square root
                static_assert(decltype(m_current_command->v0)::exp == decltype(s.squareRoot())::exp, "slow shift");
                auto q = (m_current_command->v0 + s.squareRoot()).template shift<-1>();
                
                // compute solution as fraction of total time
                static_assert(Modulo(q_div_shift, 8) == 0, "slow shift");
                static const int div_res_sat_bits = -(StepFixedType::exp - decltype(q)::exp - q_div_shift);
                auto t_frac = FixedDivide<q_div_shift, div_res_sat_bits>(StepFixedType::importBits(m_rel_x), q); // TODO div0
                
                // multiply by the time of this command, and drop fraction bits at the same time
                typedef decltype(m_current_command->t_mul * t_frac) ProdType;
                static_assert(Modulo(ProdType::exp, 8) == 0, "slow shift");
                auto t = FixedMultiply<(-ProdType::exp)>(m_current_command->t_mul, t_frac);
                
                // schedule next step
                static_assert(decltype(t)::exp == 0, "");
                static_assert(!decltype(t)::is_signed, "");
                TimeType timer_t = m_current_command->clock_offset + t.bitsValue();
                m_timer.set(c, timer_t);
                return;
            } while (0);
            
            // reset step counter for next command
            m_rel_x = 0;
            
            bool run_out;
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                // consume command
                m_start = (CommandSizeType)(m_start + 1) % buffer_mod;
                m_current_command = &m_commands[m_start];
                
                // report avail event if we have enough buffer space
                CommandSizeType avail = buffer_avail(m_start, m_end);
                if (avail > m_event_amount) {
                    m_event_amount = CommandBufferSize;
                    m_avail_event.appendNow(lock_c);
                }
                
                run_out = (m_start == m_end);
            });
            
            // have we run out of commands?
            if (run_out) {
                return;
            }
            
            // continue with next command
            D::stepper(this, c)->setDir(c, m_current_command->dir);
            
            // if this is a motionless command, wait
            if (m_current_command->x.bitsValue() == 0) {
                TimeType timer_t = m_current_command->clock_offset + m_current_command->t_plain;
                m_timer.set(c, timer_t);
                return;
            }
        }
    }
    
    void avail_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        return AvailHandler::call(this, c);
    }
    
    TimerInstance m_timer;
    typename Loop::QueuedEvent m_avail_event;
    Command m_commands[buffer_mod];
    CommandSizeType m_start;
    CommandSizeType m_end;
    CommandSizeType m_event_amount;
    Command *m_current_command;
    typename StepFixedType::IntType m_rel_x;
    bool m_running;
    Lock m_lock;
    
    struct TimerHandler : public AMBRO_WCALLBACK_TD(&AxisStepper::timer_handler, &AxisStepper::m_timer) {};
};

#include <aprinter/EndNamespace.h>

#endif
