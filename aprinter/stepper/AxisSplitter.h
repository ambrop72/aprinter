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

#ifndef AMBROLIB_AXIS_SPLITTER_H
#define AMBROLIB_AXIS_SPLITTER_H

#include <stdint.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/WrapCallback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/structure/RingBuffer.h>
#include <aprinter/stepper/AxisStepper.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, int BufferBits, int StepperBufferBits, typename MyStepper, typename GetStepper, template<typename, typename> class StepperTimer, typename AvailHandler>
class AxisSplitter : private DebugObject<Context, void> {
private:
    using Loop = typename Context::EventLoop;
    
    struct StepperAvailHandler;
    struct MyGetStepper;
    
public:
    using MyAxisStepper = AxisStepper<Context, StepperBufferBits, MyStepper, MyGetStepper, StepperTimer, StepperAvailHandler>;
    
private:
    static const int step_bits = MyAxisStepper::StepFixedType::num_bits + 4;
    static const int time_bits = MyAxisStepper::TimeFixedType::num_bits + 6;
    static const int gt_frac_square_bits = step_bits + 1;
    
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    
private:
    using StepperBufferSizeType = typename MyAxisStepper::BufferSizeType;
    using StepperStepType = typename MyAxisStepper::StepFixedType;
    using StepperAccelType = typename MyAxisStepper::AccelFixedType;
    using StepperTimeType = typename MyAxisStepper::TimeFixedType;
    using VelocityType = FixedPoint<step_bits + 1, false, 0>;
    using Velocity2Type = FixedPoint<(2 * (step_bits + 1)), false, 0>;
    
    struct Command {
        bool dir;
        AccelFixedType a;
        VelocityType v0;
        Velocity2Type v02;
        TimeFixedType all_t;
        StepFixedType x;
        TimeFixedType t;
        StepperBufferSizeType num_cmds;
    };
    
    using CommandBuffer = RingBuffer<Context, Command, BufferBits>;
    
public:
    using BufferSizeType = typename CommandBuffer::SizeType;
    
    void init (Context c)
    {
        m_axis_stepper.init(c);
        m_command_buffer.init(c);
        m_avail_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AxisSplitter::m_avail_event, &AxisSplitter::avail_event_handler));
        m_event_amount = BufferSizeType::maxValue();
        m_backlog = BufferSizeType::import(0);
        m_stepper_avail = StepperBufferSizeType::maxValue();
        
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
        AMBRO_ASSERT(m_event_amount == BufferSizeType::maxValue())
        AMBRO_ASSERT(!m_avail_event.isSet(c))
        
        m_axis_stepper.clearBuffer(c);
        m_command_buffer.clear(c);
        m_backlog = BufferSizeType::import(0);
        m_stepper_avail = StepperBufferSizeType::maxValue();
    }
    
    BufferSizeType bufferGetAvail (Context c)
    {
        this->debugAccess(c);
        
        return m_command_buffer.writerGetAvail(c);
    }
    
    void bufferAddCommandTest (Context c, bool dir, float x, float t, float a)
    {
        float step_length = 0.0125;
        bufferAddCommand(c, dir, StepFixedType::importDouble(x / step_length), TimeFixedType::importDouble(t / Clock::time_unit), AccelFixedType::importDouble(a / step_length));
    }
    
    void bufferAddCommand (Context c, bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_event_amount == BufferSizeType::maxValue())
        AMBRO_ASSERT(!m_avail_event.isSet(c))
        AMBRO_ASSERT(m_command_buffer.writerGetAvail(c).value() > 0)
        AMBRO_ASSERT(a >= -x)
        AMBRO_ASSERT(a <= x)
        
        bool was_empty = (m_backlog == m_command_buffer.readerGetAvail(c));
        
        Command *cmd = m_command_buffer.writerGetPtr(c);
        cmd->dir = dir;
        cmd->a = -a;
        cmd->v0 = (x - cmd->a).toUnsignedUnsafe();
        cmd->v02 = cmd->v0 * cmd->v0;
        cmd->all_t = t;
        cmd->x = x;
        cmd->t = t;
        cmd->num_cmds = StepperBufferSizeType::import(0);
        m_command_buffer.writerProvide(c);
        
        if (m_axis_stepper.isRunning(c) && was_empty) {
            m_axis_stepper.bufferRequestEvent(c);
        }
    }
    
    void bufferRequestEvent (Context c, BufferSizeType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount.value() > 0)
        
        if (m_command_buffer.writerGetAvail(c) >= min_amount) {
            m_event_amount = BufferSizeType::maxValue();
            m_avail_event.prependNow(c);
        } else {
            m_event_amount = BoundedModuloDec(min_amount);
            m_avail_event.unset(c);
        }
    }
    
    void bufferCancelEvent (Context c)
    {
        this->debugAccess(c);
        
        m_event_amount = BufferSizeType::maxValue();
        m_avail_event.unset(c);
    }
    
    void start (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_axis_stepper.isRunning(c))
        
        // fill stepper command buffer
        while (m_backlog < m_command_buffer.readerGetAvail(c) && m_axis_stepper.bufferGetAvail(c).value() > 0) {
            send_stepper_command(c);
        }
        
        m_axis_stepper.start(c, start_time);
        m_axis_stepper.bufferRequestEvent(c);
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_axis_stepper.isRunning(c))
        
        m_axis_stepper.bufferCancelEvent(c);
        m_axis_stepper.stop(c);
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_axis_stepper.isRunning(c);
    }
    
    MyAxisStepper * getAxisStepper ()
    {
        return &m_axis_stepper;
    }
    
private:
    void send_stepper_command (Context c)
    {
        AMBRO_ASSERT(m_backlog < m_command_buffer.readerGetAvail(c))
        AMBRO_ASSERT(m_axis_stepper.bufferGetAvail(c).value() > 0)
        
        Command *cmd = m_command_buffer.readerGetPtr(c, m_backlog);
        
        StepFixedType new_x;
        TimeFixedType new_t;
        StepperStepType rel_x;
        StepperTimeType rel_t;
        StepperAccelType rel_a;
        
        // if command fits, be fast
        if (cmd->t == cmd->all_t && cmd->x <= StepperStepType::maxValue() && cmd->t <= StepperTimeType::maxValue()) {
            new_x = StepFixedType::importBits(0);
            new_t = TimeFixedType::importBits(0);
            rel_x = StepperStepType::importBits(cmd->x.bitsValue());
            rel_t = StepperTimeType::importBits(cmd->t.bitsValue());
            rel_a = StepperAccelType::importBits(-cmd->a.bitsValue());
        } else {
            // limit distance
            bool second_try = false;
            if (cmd->x <= StepperStepType::maxValue()) {
                new_x = StepFixedType::importBits(0);
                new_t = TimeFixedType::importBits(0);
            } else {
                // distance is too long, use largest possible distance, and compute time using quadratic formula 
                new_x = StepFixedType::importBits(cmd->x.bitsValue() - StepperStepType::maxValue().bitsValue());
                
            compute_time:
                AMBRO_ASSERT(new_x < cmd->x)
                
                // compute discriminant
                auto discriminant = cmd->v02 + (cmd->a * new_x).template shift<2>();
                AMBRO_ASSERT(discriminant.bitsValue() >= 0) // proof based on -x<=a<=x and 0<new_x<x
                
                // compute the thing with the square root
                auto q = (cmd->v0 + FixedSquareRoot(discriminant)).template shift<-1>();
                
                // compute solution as fraction of total time
                auto t_frac = FixedFracDivide(new_x, q);
                
                // multiply by the time of this command, and drop fraction bits at the same time
                new_t = FixedResMultiply(cmd->all_t, t_frac);
                
                // make sure the computed time isn't more than the previous time, just in case
                if (new_t > cmd->t) {
                    new_t = cmd->t;
                }
            }
            
            // limit time
            if (cmd->t.bitsValue() - new_t.bitsValue() > StepperTimeType::maxValue().bitsValue()) {
                // time is too large, use largest possible time
                new_t = TimeFixedType::importBits(cmd->t.bitsValue() - StepperTimeType::maxValue().bitsValue());
                
                // if we're here the second time because the recomputed time turned out larger than we wanted,
                // stop to avoid getting caught in an infinite loop
                if (second_try) {
                    goto out;
                }
                
                // compute time as a fraction of total time
                auto t_frac = FixedFracDivide(new_t, cmd->all_t);
                
                // compute distance for this time
                auto res = FixedResMultiply(t_frac, cmd->v0 + FixedResMultiply(cmd->a, t_frac));
                StepFixedType calc_x = res.template dropBitsSaturated<StepFixedType::num_bits, StepFixedType::is_signed>();
                
                // increment this by one, this should avoid the time exceeding the limit after recomputation
                if (calc_x < StepFixedType::maxValue()) {
                    calc_x.m_bits.m_int++;
                }
                
                // update distance, making sure it's in range
                if (calc_x < new_x) {
                    calc_x = new_x;
                } else if (calc_x > cmd->x) {
                    calc_x = cmd->x;
                }
                new_x = calc_x;
                
                // recompute time from distance unless distance is at a limit value
                if (new_x < cmd->x) {
                    second_try = true;
                    goto compute_time;
                }
            }
            
        out:;
            // compute rel_x and rel_t
            rel_x = StepperStepType::importBits(cmd->x.bitsValue() - new_x.bitsValue());
            rel_t = StepperTimeType::importBits(cmd->t.bitsValue() - new_t.bitsValue());
        
            // compute acceleration for the stepper command
            auto gt_frac = FixedFracDivide(rel_t, cmd->all_t);
            auto gt_frac2 = (gt_frac * gt_frac).template bitsDown<gt_frac_square_bits>();
            AccelFixedType a = FixedResMultiply(-cmd->a, gt_frac2);
            if (a < -rel_x) {
                rel_a = -rel_x;
            } else if (a > rel_x) {
                rel_a = rel_x.toSigned();
            } else {
                rel_a = StepperAccelType::importBits(a.bitsValue());
            }
        }
        
        AMBRO_ASSERT(new_x <= cmd->x)
        AMBRO_ASSERT(new_t <= cmd->t)
        AMBRO_ASSERT(cmd->x - new_x <= StepperStepType::maxValue())
        AMBRO_ASSERT(cmd->t - new_t <= StepperTimeType::maxValue())
        AMBRO_ASSERT(rel_x == cmd->x - new_x)
        AMBRO_ASSERT(rel_t == cmd->t - new_t)
        AMBRO_ASSERT(rel_a >= -rel_x)
        AMBRO_ASSERT(rel_a <= rel_x)
        
        // This statement has also been proven. It implies that  a command is complete
        // after finitely many stepper commands have been generated from it.
        // In words: if 'x' stayed the same, then either 't' increased, or the command is complete.
        AMBRO_ASSERT(!(new_x == cmd->x) || (new_t < cmd->t || (new_x.bitsValue() == 0 && new_t.bitsValue() == 0)))
        
        // send a stepper command
        m_axis_stepper.bufferProvide(c, cmd->dir, rel_x, rel_t, rel_a);
        m_stepper_avail = BoundedModuloDec(m_stepper_avail);
        
        // update our command
        cmd->x = new_x;
        cmd->t = new_t;
        cmd->num_cmds = BoundedModuloInc(cmd->num_cmds);
        
        // possibly complete our command 
        if (cmd->x.bitsValue() == 0 && cmd->t.bitsValue() == 0) {
            m_backlog = BoundedModuloInc(m_backlog);
        }
    }
    
    void axis_stepper_avail_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_axis_stepper.isRunning(c))
        AMBRO_ASSERT(m_axis_stepper.bufferGetAvail(c).value() > 0)
        
        // clean up backlog
        StepperBufferSizeType stepbuf_avail = m_axis_stepper.bufferGetAvail(c);
        while (m_backlog.value() > 0) {
            Command *backlock_cmd = m_command_buffer.readerGetPtr(c);
            if (stepbuf_avail < BoundedModuloAdd(m_stepper_avail, backlock_cmd->num_cmds)) {
                break;
            }
            m_backlog = BoundedModuloDec(m_backlog);
            m_stepper_avail = BoundedModuloAdd(m_stepper_avail, backlock_cmd->num_cmds);
            m_command_buffer.readerConsume(c);
        }
        
        // possibly send a stepper command
        if (m_backlog < m_command_buffer.readerGetAvail(c)) {
            send_stepper_command(c);
        }
        
        if (m_backlog < m_command_buffer.readerGetAvail(c)) {
            m_axis_stepper.bufferRequestEvent(c);
        } else if (m_backlog.value() > 0) {
            Command *backlock_cmd = m_command_buffer.readerGetPtr(c);
            StepperBufferSizeType event_cmds = BoundedModuloAdd(m_stepper_avail, backlock_cmd->num_cmds);
            m_axis_stepper.bufferRequestEvent(c, event_cmds);
        }
        
        // possibly send avail event to user
        if (m_command_buffer.writerGetAvail(c) > m_event_amount) {
            m_event_amount = BufferSizeType::maxValue();
            m_avail_event.prependNow(c);
        }
    }
    
    void avail_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_command_buffer.writerGetAvail(c).value() > 0)
        AMBRO_ASSERT(m_event_amount == BufferSizeType::maxValue())
        
        return AvailHandler::call(this, c);
    }
    
    MyStepper * my_get_stepper_handler ()
    {
        return GetStepper::call(this);
    }
    
    MyAxisStepper m_axis_stepper;
    CommandBuffer m_command_buffer;
    typename Loop::QueuedEvent m_avail_event;
    BufferSizeType m_event_amount;
    BufferSizeType m_backlog;
    StepperBufferSizeType m_stepper_avail;
    
    struct StepperAvailHandler : public AMBRO_WCALLBACK_TD(&AxisSplitter::axis_stepper_avail_handler, &AxisSplitter::m_axis_stepper) {};
    struct MyGetStepper : public AMBRO_WCALLBACK_TD(&AxisSplitter::my_get_stepper_handler, &AxisSplitter::m_axis_stepper) {};
};

#include <aprinter/EndNamespace.h>

#endif
