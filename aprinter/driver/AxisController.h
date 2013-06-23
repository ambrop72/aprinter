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

#ifndef AMBROLIB_AXIS_CONTROLLER_H
#define AMBROLIB_AXIS_CONTROLLER_H

#include <stdint.h>
#include <math.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/WrapCallback.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/WrapCallback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/structure/RingBuffer.h>
#include <aprinter/driver/AxisStepper.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename MyStepper, typename GetStepper, template<typename, typename> class StepperTimer, typename AvailHandler>
class AxisController : private DebugObject<Context, void> {
private:
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    
    static const int command_buffer_bits = 4;
    static const int stepper_command_buffer_bits = 4;
    static const int step_bits = 13 + 6;
    static const int time_bits = 22 + 6;
    
public:
    using TimeType = typename Clock::TimeType;
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    
private:
    struct Command {
        bool dir;
        StepFixedType x;
        AccelFixedType a;
        TimeFixedType t;
        StepFixedType x_pos;
        TimeFixedType t_pos;
    };
    
    struct AxisStepperAvailHandler;
    
    using MyAxisStepper = AxisStepper<Context, stepper_command_buffer_bits, MyStepper, GetStepper, StepperTimer, AxisStepperAvailHandler>;
    using CommandBuffer = RingBuffer<Context, Command, command_buffer_bits>;
    using RelStepType = typename MyAxisStepper::StepFixedType;
    using RelAccelType = typename MyAxisStepper::AccelFixedType;
    using RelTimeType = typename MyAxisStepper::TimeFixedType;
    
public:
    using BufferBoundedType = typename CommandBuffer::SizeType;
    
    void init (Context c)
    {
        m_axis_stepper.init(c);
        m_command_buffer.init(c);
        m_avail_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisController::m_avail_event, &AxisController::avail_event_handler));
        m_event_amount = BufferBoundedType::maxValue();
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        m_avail_event.deinit(c);
        m_command_buffer.deinit(c);
        m_axis_stepper.deinit(c);
    }
    
    void clearBuffer (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_axis_stepper.isRunning(c))
        
        m_axis_stepper.clearBuffer(c);
        m_command_buffer.clear(c);
    }
    
    BufferBoundedType writerGetAvail (Context c)
    {
        this->debugAccess(c);
        
        return m_command_buffer.writerGetAvail(c);
    }
    
    void bufferAddCommandTest (Context c, bool dir, float x, float t, float ha)
    {
        float step_length = 0.0125;
        writerAddCommand(c, dir, StepFixedType::importDouble(x / step_length), TimeFixedType::importDouble(t / Clock::time_unit), AccelFixedType::importDouble(ha / step_length));
    }
    
    void writerAddCommand (Context c, bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_command_buffer.writerGetAvail(c) > 0)
        AMBRO_ASSERT(a >= --x)
        AMBRO_ASSERT(a <= x)
        
        Command *cmd = m_command_buffer.writerGetPtr(c);
        cmd->dir = dir;
        cmd->x = x;
        cmd->a = a;
        cmd->t = t;
        cmd->x_pos = StepFixedType::importBits(0);
        cmd->t_pos = TimeFixedType::importBits(0);
        m_command_buffer.writerProvide(c);
        
        if (m_command_buffer.readerGetAvail(c).value() == 1) {
            m_axis_stepper.bufferRequestEvent(c, 1);
        }
    }
    
    void bufferRequestEvent (Context c, BufferBoundedType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount.value() > 0)
        
        if (m_command_buffer.writerGetAvail(c) >= min_amount) {
            m_event_amount = BufferBoundedType::maxValue();
            m_avail_event.appendNow(c);
        } else {
            m_event_amount = BoundedModuloSubtract(min_amount, BufferBoundedType::import(1));
            m_avail_event.unset(c);
        }
    }
    
    void start (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_axis_stepper.isRunning(c))
        
        m_axis_stepper.start(c, start_time);
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_axis_stepper.isRunning(c))
        
        m_axis_stepper.stop(c);
        m_avail_event.unset(c);
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_axis_stepper.isRunning(c);
    }
    
private:
    void axis_stepper_avail_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_axis_stepper.isRunning(c))
        AMBRO_ASSERT(m_axis_stepper.bufferQuery(c).value() > 0)
        AMBRO_ASSERT(m_command_buffer.readerGetAvail(c).value() > 0)
        
        Command *cmd = m_command_buffer.readerGetPtr(c);
        
        StepFixedType remain_x = StepFixedType::importBits(cmd->x.bitsValue() - cmd->x_pos.bitsValue());
        StepFixedType new_x;
        RelStepType rel_x;
        if (remain_x <= RelStepType::maxValue()) {
            new_x = cmd->x;
            rel_x = RelStepType::importBits(remain_x);
        } else {
            new_x = StepFixedType::importBits(cmd->x_pos.bitsValue() + RelStepType::maxIntValue());
            rel_x = RelStepType::maxIntValue();
        }
        
        auto v0 = (cmd->x - cmd->a).toUnsignedUnsafe();
        auto v02 = v0 * v0;
        auto s = v02 + (cmd->a * new_x).template shift<2>();
        AMBRO_ASSERT(s.bitsValue() >= 0)
        auto q = (v0 + FixedSquareRoot(s)).template shift<-1>();
        auto t_frac = FixedFracDivide(new_x, q);
        TimeFixedType new_t = FixedResMultiply(cmd->t, t_frac);
        
        RelTimeType rel_t = RelTimeType::importBits(new_t.bitsValue() - cmd->t_pos.bitsValue()); // TODO
        
        auto gt_frac = FixedFracDivide(rel_t, cmd->orig_t);
        AccelFixedType a = FixedResMultiply(cmd->a, gt_frac * gt_frac);
        
        RelAccelType rel_a = RelAccelType::importBits(a); // TODO
        
        m_axis_stepper.bufferProvide(c, cmd->dir, rel_x, rel_t, rel_a);
        
        cmd->x_pos = new_x;
        cmd->t_pos = new_t;
        
        if (cmd->x_pos == cmd->x) {
            m_command_buffer.readerConsume(c);
        }
        
        if (m_command_buffer.readerGetAvail(c).value() > 0) {
            m_axis_stepper.bufferRequestEvent(c, 1);
        }
        
        if (m_command_buffer.writerGetAvail(c) > m_event_amount) {
            m_event_amount = BufferBoundedType::maxValue();
            m_avail_event.appendNow(c);
        }
    }
    
    void avail_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_axis_stepper.isRunning(c))
        AMBRO_ASSERT(m_command_buffer.writerGetAvail(c).value() > 0)
        
        return AvailHandler::call(this, c);
    }
    
    /*
    static void solve_t (float rx, float v0, float a)
    {
        float d = v0 * v0 + 4 * a * rx;
        if (d < 0) {
            return 1;
        }
        return (rx / ((v0 + sqrtf(d)) / 2));
    }
    */
#if 0
    void axis_stepper_avail_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_axis_stepper.bufferQuery(c).value() > 0)
        AMBRO_ASSERT(m_command_buffer.readerGetAvail(c).value() > 0)
        
        Command *cmd = m_command_buffer.readerGetPtr(c);
        float v0 = cmd->dx - cmd->a;
        
        float rx = (cmd->dx > RelStepType::maxIntValue()) ? RelStepType::maxIntValue() : cmd->dx;
        
        float rt_frac = solve_t(rx, v0, cmd->a);
        float rt_temp = rt_frac * cmd->t;
        
        if (rt_temp > RelTimeType::maxIntValue()) {
            rt_temp = RelTimeType::maxIntValue();
            float rx_temp = v0 * rt_temp + cmd->a * (rt_temp * rt_temp);
            rx = RelStepType::import(
                (rx_temp < RelStepType::minIntValue()) ? RelStepType::minIntValue() :
                (rx_temp > RelStepType::maxIntValue()) ? RelStepType::maxIntValue() :
                rx_temp
            );
            rt_frac = solve_t(rx.value(), v0, cmd->a);
            rt_temp = rt_frac * cmd->t;
            if (rt_temp > RelTimeType::maxIntValue()) {
                rt_temp = RelTimeType::maxIntValue();
            }
        }
        
        float a = cmd->a * (rt_frac * rt_frac);
        RelAccelType ra = RelAccelType::import(
            (a < RelAccelType::minIntValue()) ? RelAccelType::minIntValue() :
            (a > RelAccelType::maxIntValue()) ? RelAccelType::maxIntValue() :
            a
        );
        
        m_axis_stepper.bufferProvide(c, cmd->dir, rx, rt, ra);
        
        if (m_command_buffer.readerGetAvail(c).value() > 0) {
            m_axis_stepper.bufferRequestEvent(c, 1);
        }
        
        cmd->x -= rx.value();
        cmd->a *= (1 - rt_frac) * (1 - rt_frac);
        cmd->t *= (1 - rt_frac);
        
        
        /*
            float a = cmd->a * (t_frac * t_frac);
            modff(a, &a);
            if (a > RelAccelType::maxIntValue()) {
                a = RelAccelType::maxIntValue();
            } else if (a < RelAccelType::minIntValue()) {
                a = RelAccelType::minIntValue();
            }
            ra = RelAccelType::import(a);
            */
        
        /*
            cmd->x -= rx.value();
            cmd->a *= (1 - t_frac) * (1 - t_frac);
            cmd->t *= (1 - t_frac);
            */
        
        m_command_buffer.readerConsume(c);

        
        RelTimeType rt = RelTimeType::import(rt_temp); // TODO
        
        
    }
#endif
    
    MyAxisStepper m_axis_stepper;
    CommandBuffer m_command_buffer;
    typename Loop::QueuedEvent m_avail_event;
    BufferBoundedType m_event_amount;
    
    struct AxisStepperAvailHandler : public AMBRO_WCALLBACK_TD(&AxisController::axis_stepper_avail_handler, &AxisController::m_axis_stepper) {};
};

#include <aprinter/EndNamespace.h>

#endif
