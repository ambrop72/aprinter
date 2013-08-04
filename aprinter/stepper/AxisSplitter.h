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

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename TheAxisStepper>
class AxisSplitter {
private:
    static const int step_bits = TheAxisStepper::StepFixedType::num_bits + 6;
    static const int time_bits = TheAxisStepper::TimeFixedType::num_bits + 9;
    static const int gt_frac_square_bits = step_bits + 1;
    
public:
    using StepFixedType = FixedPoint<step_bits, false, 0>;
    using AccelFixedType = FixedPoint<step_bits, true, 0>;
    using TimeFixedType = FixedPoint<time_bits, false, 0>;
    
private:
    using StepperStepType = typename TheAxisStepper::StepFixedType;
    using StepperAccelType = typename TheAxisStepper::AccelFixedType;
    using StepperTimeType = typename TheAxisStepper::TimeFixedType;
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
    };
    
public:
    void setInput (bool dir, StepFixedType x, TimeFixedType t, AccelFixedType a)
    {
        AMBRO_ASSERT(a >= -x)
        AMBRO_ASSERT(a <= x)
        
        Command *cmd = &m_command;
        cmd->dir = dir;
        cmd->a = a;
        cmd->v0 = (x + a).toUnsignedUnsafe();
        cmd->v02 = cmd->v0 * cmd->v0;
        cmd->all_t = t;
        cmd->x = x;
        cmd->t = t;
    }
    
    void getOutput (typename TheAxisStepper::Command *out_cmd, bool *out_is_last)
    {
        Command *cmd = &m_command;
        
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
            rel_a = StepperAccelType::importBits(cmd->a.bitsValue());
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
                auto discriminant = cmd->v02 - (cmd->a * new_x).template shift<2>();
                AMBRO_ASSERT(discriminant.bitsValue() >= 0) // proof based on -x<=a<=x and 0<=new_x<=x
                
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
                    AMBRO_ASSERT(new_x < cmd->x)
                    new_x.m_bits.m_int++;
                    goto out;
                }
                
                if (new_x != cmd->x) {
                    // compute time as a fraction of total time
                    auto t_frac = FixedFracDivide(new_t, cmd->all_t);
                    
                    // compute distance for this time
                    auto res = FixedResMultiply(t_frac, cmd->v0 - FixedResMultiply(cmd->a, t_frac));
                    
                    // add one. This ensures our result of the above polynomial evaluation is always greater
                    // or equal to the precise value (error in t_frac not considered).
                    // To prove this, take into account that the error of FixedResMultiply is in (-1, 0].
                    res.m_bits.m_int++;
                    
                    // convert to StepFixedType
                    StepFixedType calc_x = res.template dropBitsSaturated<StepFixedType::num_bits, StepFixedType::is_signed>();
                    
                    // update distance
                    if (calc_x < new_x) {
                        calc_x = new_x;
                    } else if (calc_x >= cmd->x) {
                        calc_x = StepFixedType::importBits(cmd->x.bitsValue() - 1);
                    }
                    new_x = calc_x;
                    
                    // recompute time from distance
                    second_try = true;
                    goto compute_time;
                }
            }
            
        out:
            // compute rel_x and rel_t
            rel_x = StepperStepType::importBits(cmd->x.bitsValue() - new_x.bitsValue());
            rel_t = StepperTimeType::importBits(cmd->t.bitsValue() - new_t.bitsValue());
            
            // compute acceleration for the stepper command
            auto gt_frac = FixedFracDivide(rel_t, cmd->all_t);
            auto gt_frac2 = (gt_frac * gt_frac).template bitsDown<gt_frac_square_bits>();
            AccelFixedType a = FixedResMultiply(cmd->a, gt_frac2);
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
        
        // This statement has also been proven. It implies that a command is complete
        // after finitely many stepper commands have been generated from it.
        // In words: if 'x' stayed the same, then either 't' decreased, or the command is complete.
        AMBRO_ASSERT(!(new_x == cmd->x) || (new_t < cmd->t || (new_x.bitsValue() == 0 && new_t.bitsValue() == 0)))
        
        // send a stepper command
        TheAxisStepper::generate_command(cmd->dir, rel_x, rel_t, rel_a, out_cmd);
        
        // update our command
        cmd->x = new_x;
        cmd->t = new_t;
        
        // possibly complete our command
        *out_is_last = (cmd->x.bitsValue() == 0 && cmd->t.bitsValue() == 0);
    }
    
public:
    Command m_command;
};

#include <aprinter/EndNamespace.h>

#endif
