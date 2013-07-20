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

template <int TCommandBufferBits, template<typename, typename> class TTimer>
struct AxisStepperParams {
    static const int CommandBufferBits = TCommandBufferBits;
    template<typename X, typename Y>
    using Timer = TTimer<X, Y>;
};

template <typename Context, typename Params, typename Stepper, typename GetStepper, typename PullCmdHandler, typename BufferFullHandler, typename BufferEmptyHandler>
class AxisStepper
: private DebugObject<Context, void>
{
    static_assert(Params::CommandBufferBits >= 2, "");
    
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
    
public:
    using TimerInstance = typename Params::template Timer<Context, TimerHandler>;
    using TimeType = typename Clock::TimeType;
    using BufferSizeType = BoundedInt<Params::CommandBufferBits, false>;
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    
    void init (Context c)
    {
        m_pull_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisStepper::m_pull_event, &AxisStepper::pull_event_handler));
        m_full_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisStepper::m_full_event, &AxisStepper::full_event_handler));
        m_empty_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisStepper::m_empty_event, &AxisStepper::empty_event_handler));
        m_lock.init(c);
        m_timer.init(c);
        m_running = false;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        m_timer.deinit(c);
        m_lock.deinit(c);
        m_empty_event.deinit(c);
        m_full_event.deinit(c);
        m_pull_event.deinit(c);
    }
    
    void start (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        m_running = true;
        m_stepping = false;
        m_pulling = false;
        m_start = BufferSizeType::import(0);
        m_end = BufferSizeType::import(0);
        m_commands[BoundedModuloDec(m_start).value()].clock_offset = start_time;
        m_pull_event.prependNowNotAlready(c);
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        m_timer.unset(c);
        m_empty_event.unset(c);
        m_full_event.unset(c);
        m_pull_event.unset(c);
        m_running = false;
    }
    
    void addTime (Context c, TimeType time_add)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_stepping)
        
        m_commands[BoundedModuloDec(m_start).value()].clock_offset += time_add;
        for (BufferSizeType i = m_start; i != m_end; i = BoundedModuloInc(i)) {
            m_commands[i.value()].clock_offset += time_add;
        }
    }
    
    void startStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_stepping)
        
        m_stepping = true;
        m_full_event.unset(c);
        
        if (m_start != m_end) {
            start_first_command(c);
        } else if (m_pulling) {
            m_empty_event.prependNowNotAlready(c);
        }
    }
    
    void stopStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_stepping)
        
        m_timer.unset(c);
        m_empty_event.unset(c);
        m_stepping = false;
        
        if (m_start != m_end) {
            static_cast<CurrentCommand &>(m_commands[m_start.value()]) = m_current_command;
        }
        
        if (BoundedModuloSubtract(m_start, m_end) == BufferSizeType::import(1)) {
            m_full_event.prependNowNotAlready(c);
        }
    }
    
    void commandDone (Context c, bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(!buffer_is_full(c))
        AMBRO_ASSERT(!m_pull_event.isSet(c))
        AMBRO_ASSERT(!m_full_event.isSet(c))
        AMBRO_ASSERT(a >= -x)
        AMBRO_ASSERT(a <= x)
        
        Command cmd;
        cmd.x = x;
        cmd.discriminant = AXIS_STEPPER_DISCRIMINANT_EXPR(x, t, a);
        cmd.a_mul = AXIS_STEPPER_AMUL_EXPR(x, t, a);
        cmd.v0 = AXIS_STEPPER_V0_EXPR(x, t, a);
        cmd.t_mul = AXIS_STEPPER_TMUL_EXPR(x, t, a);
        cmd.t_plain = t.bitsValue();
        cmd.dir = dir;
        cmd.clock_offset = m_commands[buffer_last(m_end).value()].clock_offset + cmd.t_plain;
        
        m_commands[m_end.value()] = cmd;
        
        bool was_empty;
        bool is_full;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_pulling = false;
            m_empty_event.unset(lock_c);
            was_empty = buffer_is_empty(lock_c);
            m_end = BoundedModuloInc(m_end);
            is_full = buffer_is_full(lock_c);
        });
        
        if (!is_full) {
            m_pull_event.prependNowNotAlready(c);
        } else if (!m_stepping) {
            m_full_event.prependNowNotAlready(c);
        }
        
        if (m_stepping && was_empty) {
            start_first_command(c);
        }
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_running;
    }
    
    bool isStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        return m_stepping;
    }
    
    bool isPulling (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        return m_pulling;
    }
    
    struct SteppingState {
        StepFixedType rem_extra_steps;
        BufferSizeType rem_cmds;
    };
    
    SteppingState getSteppingState (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_stepping)
        
        SteppingState st;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            if (!buffer_is_empty(lock_c)) {
                st.rem_extra_steps = m_commands[m_start.value()].x;
                st.rem_cmds = BoundedUnsafeDec(BoundedModuloSubtract(m_end, m_start));
            } else {
                st.rem_extra_steps = StepFixedType::importBits(0);
                st.rem_cmds = BufferSizeType::import(0);
            }
        });
        
        return st;
    }
    
    TimerInstance * getTimer ()
    {
        return &m_timer;
    }
    
private:
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
    
    static Stepper * stepper (AxisStepper *o)
    {
        return GetStepper::call(o);
    }
    
    static BufferSizeType buffer_last (BufferSizeType end)
    {
        return BoundedModuloSubtract(end, BufferSizeType::import(1));
    }
    
    template <typename ThisContext>
    bool buffer_is_full (ThisContext c)
    {
        bool res;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            res = (BoundedModuloSubtract(m_start, m_end) == BufferSizeType::import(1));
        });
        return res;
    }
    
    template <typename ThisContext>
    bool buffer_is_empty (ThisContext c)
    {
        bool res;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            res = (m_start == m_end);
        });
        return res;
    }
    
    void start_first_command (Context c)
    {
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_stepping)
        AMBRO_ASSERT(!buffer_is_empty(c))
        
        Command *cmd = &m_commands[m_start.value()];
        stepper(this)->setDir(c, cmd->dir);
        m_current_command = *cmd;
        TimeType timer_t = (m_current_command.x.bitsValue() == 0) ? m_current_command.clock_offset : (m_current_command.clock_offset - cmd->t_plain);
        m_timer.set(c, timer_t);
    }
    
    bool timer_handler (typename TimerInstance::HandlerContext c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_stepping)
        AMBRO_ASSERT(!buffer_is_empty(c))
        
        if (m_current_command.x.bitsValue() == 0) {
            bool run_out;
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                if (buffer_is_full(lock_c)) {
                    m_pull_event.appendNowNotAlready(lock_c);
                }
                m_start = BoundedModuloInc(m_start);
                if (m_pulling && buffer_is_empty(lock_c)) {
                    m_empty_event.appendNowNotAlready(lock_c);
                }
                run_out = (m_start == m_end);
            });
            
            if (AMBRO_UNLIKELY(run_out)) {
                return false;
            }
            
            Command *cmd = &m_commands[m_start.value()];
            stepper(this)->setDir(c, cmd->dir);
            CopyUnrolled<sizeof(CurrentCommand)>(&m_current_command, (CurrentCommand *)cmd);
            
            if (m_current_command.x.bitsValue() == 0) {
                TimeType timer_t = m_current_command.clock_offset;
                m_timer.set(c, timer_t);
                return true;
            }
        }
        
        stepper(this)->step(c);
        
        m_current_command.x.m_bits.m_int--;
        
        m_current_command.discriminant.m_bits.m_int -= m_current_command.a_mul.m_bits.m_int;
        AMBRO_ASSERT(m_current_command.discriminant.bitsValue() >= 0)
        
        auto q = (m_current_command.v0 + FixedSquareRoot(m_current_command.discriminant)).template shift<-1>();
        
        auto t_frac = FixedFracDivide(m_current_command.x, q);
        
        TimeFixedType t = FixedResMultiply(m_current_command.t_mul, t_frac);
        
        TimeType timer_t = m_current_command.clock_offset - t.bitsValue();
        m_timer.set(c, timer_t);
        return true;
    }
    
    void pull_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_pulling)
        AMBRO_ASSERT(!buffer_is_full(c))
        AMBRO_ASSERT(!m_full_event.isSet(c))
        AMBRO_ASSERT(!m_empty_event.isSet(c))
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_pulling = true;
            if (m_stepping && buffer_is_empty(lock_c)) {
                m_empty_event.prependNowNotAlready(lock_c);
            }
        });
        
        return PullCmdHandler::call(this, c);
    }
    
    void full_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_stepping)
        AMBRO_ASSERT(buffer_is_full(c))
        
        return BufferFullHandler::call(this, c);
    }
    
    void empty_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_stepping)
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(buffer_is_empty(c))
        
        return BufferEmptyHandler::call(this, c);
    }
    
    typename Loop::QueuedEvent m_pull_event;
    typename Loop::QueuedEvent m_full_event;
    typename Loop::QueuedEvent m_empty_event;
    Lock m_lock;
    TimerInstance m_timer;
    Command m_commands[(size_t)BufferSizeType::maxIntValue() + 1];
    BufferSizeType m_start;
    BufferSizeType m_end;
    CurrentCommand m_current_command;
    bool m_running;
    bool m_stepping;
    bool m_pulling;
    
    struct TimerHandler : public AMBRO_WCALLBACK_TD(&AxisStepper::timer_handler, &AxisStepper::m_timer) {};
};

#include <aprinter/EndNamespace.h>

#endif
