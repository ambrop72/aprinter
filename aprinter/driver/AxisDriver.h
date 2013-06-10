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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include <aprinter/meta/IsPowerOfTwo.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Modulo.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/system/EventLoop.h>

#include <aprinter/BeginNamespace.h>

#define HAMUL_EXPR(x, t, ha) ((ha).template shiftBits<(-2)>())
#define V0_EXPR(x, t, ha) (((x).toSigned() - (ha)).toUnsignedUnsafe())
#define V02_EXPR(x, t, ha) ((V0_EXPR((x), (t), (ha)) * V0_EXPR((x), (t), (ha))))
#define TMUL_EXPR(x, t, ha) ((t).template bitsTo<time_mul_bits>())

#define HAMUL_EXPR_HELPER(args) HAMUL_EXPR(args)
#define V0_EXPR_HELPER(args) V0_EXPR(args)
#define V02_EXPR_HELPER(args) V02_EXPR(args)
#define TMUL_EXPR_HELPER(args) TMUL_EXPR(args)

#define DUMMY_VARS (StepFixedType()), (TimeFixedType()), (AccelFixedType())

template <typename Context, typename CommandSizeType, CommandSizeType CommandBufferSize, typename GetStepper>
class AxisDriver
: private DebugObject<Context, AxisDriver<Context, CommandSizeType, CommandBufferSize, GetStepper>>
{
    static_assert(IsPowerOfTwo<uintmax_t, (uintmax_t)CommandBufferSize + 1>::value, "CommandBufferSize+1 must be a power of two");
    
private:
    typedef typename Context::EventLoop Loop;
    typedef typename Context::Clock Clock;
    
    struct D {
        static auto stepper (AxisDriver *o, Context c) -> decltype(GetStepper::call(o, c))
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
    static const int time_mul_bits = 15;
    
public:
    typedef typename Clock::TimeType TimeType;
    typedef int16_t StepType;
    typedef FixedPoint<step_bits, false, 0> StepFixedType;
    typedef FixedPoint<step_bits, true, 0> AccelFixedType;
    typedef FixedPoint<time_bits, false, 0> TimeFixedType;
    
    void init (Context c)
    {
        m_timer.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisDriver::m_timer, &AxisDriver::timer_handler));
        m_running = false;
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        m_timer.deinit(c);
    }
    
    void prepare (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        m_timer.unset(c, false);
        m_start = 0;
        m_end = 0;
    }
    
    CommandSizeType bufferQuery (Context c)
    {
        this->debugAccess(c);
        
        return buffer_avail(m_start, m_end);
    }
    
    void bufferProvide (Context c, bool dir, float x, float t, float ha)
    {
        float step_length = 0.0125;
        bufferProvideReal(c, dir, x / step_length, t / Clock::time_unit, ha / step_length);
    }
    
    void bufferProvideReal (Context c, bool dir, StepType x_arg, TimeType t_arg, StepType ha_arg)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(bufferQuery(c) >= 1)
        AMBRO_ASSERT(x_arg > 0)
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
        cmd.t = t;
        cmd.ha_mul = HAMUL_EXPR(x, t, ha);
        cmd.v0 = V0_EXPR(x, t, ha);
        cmd.v02 = V02_EXPR(x, t, ha);
        cmd.t_mul = TMUL_EXPR(x, t, ha);
        
        // add command to queue
        m_commands[m_end] = cmd;
        m_end = (CommandSizeType)(m_end + 1) % buffer_mod;
    }
    
    void start (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        AMBRO_ASSERT(m_start != m_end)
        
        TimeType now = c.clock()->getTime(c);
        
        // TODO compute this for new instructions added after starting
        TimeType prev_t = 0;
        for (CommandSizeType i = m_start; i != m_end; i = (CommandSizeType)(i + 1) % buffer_mod) {
            m_commands[i].clock_offset = (TimeType)(now + prev_t);
            prev_t += (TimeType)m_commands[i].t.bitsValue();
        }
        
        m_rel_x = 0;
        m_timer.prependNow(c, false);
        D::stepper(this, c)->enable(c, true);
        D::stepper(this, c)->setDir(c, m_commands[m_start].dir);
        m_running = true;
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        
        if (m_running) {
            D::stepper(this, c)->enable(c, false);
            m_timer.unset(c, false);
            m_running = false;
        }
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_running;
    }
    
private:
    static const size_t buffer_mod = (size_t)CommandBufferSize + 1;
    
    CommandSizeType buffer_avail (CommandSizeType start, CommandSizeType end)
    {
        return (CommandSizeType)((CommandSizeType)(start - 1) - end) % buffer_mod;
    }
    
    struct Command {
        bool dir;
        StepFixedType x;
        TimeFixedType t;
        decltype(HAMUL_EXPR_HELPER(DUMMY_VARS)) ha_mul;
        decltype(V0_EXPR_HELPER(DUMMY_VARS)) v0;
        decltype(V02_EXPR_HELPER(DUMMY_VARS)) v02;
        decltype(TMUL_EXPR_HELPER(DUMMY_VARS)) t_mul;
        TimeType clock_offset;
    };
    
    void perform_steps (Context c, StepType steps)
    {
        while (steps-- > 0) {
            D::stepper(this, c)->step(c);
        }
    }
    
    void timer_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_start != m_end)
        
    next_command:
        Command *cmd = &m_commands[m_start];
        {
            if (AMBRO_UNLIKELY(m_rel_x == cmd->x.bitsValue())) {
                goto finish_command;
            }
            
            // imcrement position and get it into a fixed type
            m_rel_x++;
            auto next_x_rel = StepFixedType::importBits(m_rel_x);
            
            // compute product part of discriminant
            auto s_prod = (cmd->ha_mul * next_x_rel.toSigned()).template shift<2>();
            
            // compute discriminant
            static_assert(decltype(cmd->v02.toSigned())::exp == decltype(s_prod)::exp, "slow shift");
            auto s = cmd->v02.toSigned() + s_prod;
            
            // we don't like negative discriminants
            if (AMBRO_UNLIKELY(s.bitsValue() < 0)) {
                perform_steps(c, cmd->x.bitsValue() - (m_rel_x - 1));
                goto finish_command;
            }
            
            // perform the step
            D::stepper(this, c)->step(c);
            
            // compute the thing with the square root
            static_assert(decltype(cmd->v0)::exp == decltype(s.squareRoot())::exp, "slow shift");
            auto q = (cmd->v0 + s.squareRoot()).template shift<-1>();
            
            // compute numerator for division
            static_assert(Modulo(q_div_shift, 8) == 0, "slow shift");
            auto numerator = next_x_rel.template shiftBits<(-q_div_shift)>();
            
            // compute solution as fraction of total time
            auto t_frac_comp = numerator / q; // TODO div0
            
            // we expect t_frac_comp to be approximately in [0, 1] so drop all but 1 highest non-fraction bits
            auto t_frac_drop = t_frac_comp.template dropBitsSaturated<(-decltype(t_frac_comp)::exp)>(); // TODO overflow
            
            // multiply by the time of this command
            auto t = t_frac_drop * cmd->t_mul;
            
            // drop all fraction bits, we don't need them
            static_assert(Modulo(decltype(t)::exp, 8) == 0, "slow shift");
            auto t_mul_drop = t.template bitsTo<(decltype(t)::num_bits + decltype(t)::exp)>();
            static_assert(decltype(t_mul_drop)::exp == 0, "");
            static_assert(!decltype(t_mul_drop)::is_signed, "");
            
            // schedule next step
            TimeType timer_t = cmd->clock_offset + t_mul_drop.bitsValue();
            m_timer.prependAt(c, timer_t, false);
            return;
        }
        
    finish_command:
        // move to next command and possibly report to user
        complete_command(c);
        
        // no commands left?
        if (AMBRO_UNLIKELY(m_start == m_end)) {
            D::stepper(this, c)->enable(c, false);
            m_running = false;
            return;
        }
        
        // continue with next command
        m_rel_x = 0;
        D::stepper(this, c)->setDir(c, m_commands[m_start].dir);
        goto next_command;
    }
    
    void complete_command (Context c)
    {
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_start != m_end)
        
        // consume it
        m_start = (CommandSizeType)(m_start + 1) % buffer_mod;
        
        // TODO report
    }
    
    EventLoopQueuedEvent<Loop> m_timer;
    Command m_commands[buffer_mod];
    CommandSizeType m_start;
    CommandSizeType m_end;
    typename StepFixedType::IntType m_rel_x;
    bool m_running;
};

#include <aprinter/EndNamespace.h>

#endif
