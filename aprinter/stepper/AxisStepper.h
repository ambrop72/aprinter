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

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Modulo.h>
#include <aprinter/meta/WrapCallback.h>
#include <aprinter/meta/CopyUnrolled.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Lock.h>

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

template <typename Context, int CommandBufferBits, typename Stepper, typename GetStepper, template<typename, typename> class Timer, typename AvailHandler>
class AxisStepper
: private DebugObject<Context, AxisStepper<Context, CommandBufferBits, Stepper, GetStepper, Timer, AvailHandler>>
{
    static_assert(CommandBufferBits >= 2, "");
    
private:
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    using Lock = typename Context::Lock;
    
    // DON'T TOUCH!
    // These were chosen carefully for speed, and some operations
    // were written in assembly specifically for use here.
    static const int step_bits = 13;
    static const int time_bits = 22;
    static const int q_div_shift = 16;
    static const int time_mul_bits = 23;
    
    struct TimerHandler;
    
    using TimerInstance = Timer<Context, TimerHandler>;
    
public:
    using TimeType = typename Clock::TimeType;
    using BufferSizeType = BoundedInt<CommandBufferBits, false>;
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    
    void init (Context c)
    {
        m_timer.init(c);
        m_avail_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisStepper::m_avail_event, &AxisStepper::avail_event_handler));
        m_running = false;
        m_start = BufferSizeType::import(0);
        m_end = BufferSizeType::import(0);
        m_event = m_end;
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
        AMBRO_ASSERT(m_event == m_end)
        AMBRO_ASSERT(!m_avail_event.isSet(c))
        
        m_start = BufferSizeType::import(0);
        m_end = BufferSizeType::import(0);
        m_event = m_end;
    }
    
    BufferSizeType bufferGetAvail (Context c)
    {
        this->debugAccess(c);
        
        BufferSizeType start;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            start = m_start;
        });
        
        return buffer_avail(start, m_end);
    }
    
    void bufferProvideTest (Context c, bool dir, float x, float t, float ha)
    {
        float step_length = 0.0125;
        bufferProvide(c, dir, StepFixedType::importDouble(x / step_length), TimeFixedType::importDouble(t / Clock::time_unit), AccelFixedType::importDouble(ha / step_length));
    }
    
    void bufferProvide (Context c, bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_event == m_end)
        AMBRO_ASSERT(!m_avail_event.isSet(c))
        AMBRO_ASSERT(bufferQuery(c).value() > 0)
        AMBRO_ASSERT(a >= -x)
        AMBRO_ASSERT(a <= x)
        
        // compute the command parameters
        Command cmd;
        cmd.x = x;
        cmd.discriminant = AXIS_STEPPER_DISCRIMINANT_EXPR(x, t, a);
        cmd.a_mul = AXIS_STEPPER_AMUL_EXPR(x, t, a);
        cmd.v0 = AXIS_STEPPER_V0_EXPR(x, t, a);
        cmd.t_mul = AXIS_STEPPER_TMUL_EXPR(x, t, a);
        cmd.t_plain = t.bitsValue();
        cmd.dir = dir;
        
        // compute the clock offset based on the last command. If not running start() will do it.
        if (m_running) {
            Command *last_cmd = &m_commands[buffer_last(m_end).value()];
            cmd.clock_offset = last_cmd->clock_offset + cmd.t_plain;
        }
        
        // add command to queue
        m_commands[m_end.value()] = cmd;
        bool was_empty;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            was_empty = (m_start == m_end);
            m_end = BoundedModuloInc(m_end);
            m_event = m_end;
        });
        
        // if we have run out of commands, continue motion
        if (m_running && was_empty) {
            Command *cmd = &m_commands[m_start.value()];
            stepper(this)->setDir(c, cmd->dir);
            m_current_command = *cmd;
            TimeType timer_t = (m_current_command.x.bitsValue() == 0) ? m_current_command.clock_offset : (m_current_command.clock_offset - cmd->t_plain);
            m_timer.set(c, timer_t);
        }
    }
    
    void bufferRequestEvent (Context c, BufferSizeType min_amount = BufferSizeType::import(1))
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount.value() > 0)
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            if (buffer_avail(m_start, m_end) >= min_amount) {
                m_event = m_end;
                m_avail_event.prependNow(lock_c);
            } else {
                m_event = BoundedModuloAdd(m_end, min_amount);
                m_avail_event.unset(lock_c);
            }
        });
    }
    
    void bufferCancelEvent (Context c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_event = m_end;
            m_avail_event.unset(lock_c);
        });
    }
    
    void start (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        // compute clock offsets for commands
        if (m_start == m_end) {
            Command *last_cmd = &m_commands[buffer_last(m_end).value()];
            last_cmd->clock_offset = start_time;
            last_cmd->t_plain = 0;
        } else {
            TimeType clock_offset = start_time;
            for (BufferSizeType i = m_start; i != m_end; i = BoundedModuloInc(i)) {
                clock_offset += m_commands[i.value()].t_plain;
                m_commands[i.value()].clock_offset = clock_offset;
            }
        }
        
        m_running = true;
        
        // unless we don't have any commands, begin motion
        if (m_start != m_end) {
            Command *cmd = &m_commands[m_start.value()];
            stepper(this)->setDir(c, cmd->dir);
            m_current_command = *cmd;
            TimeType timer_t = (m_current_command.x.bitsValue() == 0) ? m_current_command.clock_offset : (m_current_command.clock_offset - cmd->t_plain);
            m_timer.set(c, timer_t);
        }
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        m_timer.unset(c);
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
    static Stepper * stepper (AxisStepper *o)
    {
        return GetStepper::call(o);
    }
    
    static BufferSizeType buffer_avail (BufferSizeType start, BufferSizeType end)
    {
        return BoundedModuloSubtract(BoundedModuloSubtract(start, BufferSizeType::import(1)), end);
    }
    
    static BufferSizeType buffer_last (BufferSizeType end)
    {
        return BoundedModuloSubtract(end, BufferSizeType::import(1));
    }
    
    struct CurrentCommand {
        StepFixedType x;
        decltype(AXIS_STEPPER_DISCRIMINANT_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) discriminant;
        decltype(AXIS_STEPPER_AMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) a_mul;
        decltype(AXIS_STEPPER_V0_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) v0;
        decltype(AXIS_STEPPER_TMUL_EXPR_HELPER(AXIS_STEPPER_DUMMY_VARS)) t_mul;
        TimeType clock_offset;
    };
    
    struct Command : public CurrentCommand {
        TimeType t_plain;
        bool dir;
    };
    
    bool timer_handler (typename TimerInstance::HandlerContext c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            AMBRO_ASSERT(m_start != m_end)
        });
        
        if (m_current_command.x.bitsValue() == 0) {
            bool run_out;
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                // report avail event if we'll have enough buffer space
                if (m_start == m_event) {
                    m_event = m_end;
                    m_avail_event.appendNowNotAlready(lock_c);
                }
                
                // consume command
                m_start = BoundedModuloInc(m_start);
                
                run_out = (m_start == m_end);
            });
            
            // have we run out of commands?
            if (AMBRO_UNLIKELY(run_out)) {
                return false;
            }
            
            // continue with next command
            Command *cmd = &m_commands[m_start.value()];
            stepper(this)->setDir(c, cmd->dir);
            CopyUnrolled<sizeof(CurrentCommand)>(&m_current_command, (CurrentCommand *)cmd);
            
            // if this is a motionless command, wait
            if (m_current_command.x.bitsValue() == 0) {
                TimeType timer_t = m_current_command.clock_offset;
                m_timer.set(c, timer_t);
                return true;
            }
        }
        
        // perform the step
        stepper(this)->step(c);
        
        // increment position
        m_current_command.x.m_bits.m_int--;
        
        // increment discriminant. It's not zero because of constraints on acceleration.
        m_current_command.discriminant.m_bits.m_int -= m_current_command.a_mul.m_bits.m_int;
        AMBRO_ASSERT(m_current_command.discriminant.bitsValue() >= 0)
        
        // compute the thing with the square root. It can be proved it's not zero.
        auto q = (m_current_command.v0 + FixedSquareRoot(m_current_command.discriminant)).template shift<-1>();
        AMBRO_ASSERT(q.bitsValue() > 0)
        
        // compute solution as fraction of total time
        auto t_frac = FixedFracDivide(m_current_command.x, q);
        
        // multiply by the time of this command, and drop fraction bits at the same time
        TimeFixedType t = FixedResMultiply(m_current_command.t_mul, t_frac);
        
        // schedule next step
        TimeType timer_t = m_current_command.clock_offset - t.bitsValue();
        m_timer.set(c, timer_t);
        return true;
    }
    
    void avail_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(buffer_avail(m_start, m_end).value() > 0)
        AMBRO_ASSERT(m_event == m_end)
        
        return AvailHandler::call(this, c);
    }
    
    TimerInstance m_timer;
    typename Loop::QueuedEvent m_avail_event;
    Command m_commands[(size_t)BufferSizeType::maxIntValue() + 1];
    BufferSizeType m_start;
    BufferSizeType m_end;
    BufferSizeType m_event;
    CurrentCommand m_current_command;
    bool m_running;
    Lock m_lock;
    
    struct TimerHandler : public AMBRO_WCALLBACK_TD(&AxisStepper::timer_handler, &AxisStepper::m_timer) {};
};

#include <aprinter/EndNamespace.h>

#endif
