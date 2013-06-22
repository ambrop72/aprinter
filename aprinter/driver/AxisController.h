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

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/meta/IsPowerOfTwo.h>
#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename AxisStepperType, typename GetAxisStepper, typename CommandSizeType, CommandSizeType CommandBufferSize>
class AxisController : private DebugObject<Context, AxisController<Context, AxisStepperType, GetAxisStepper, CommandSizeType, CommandBufferSize>> {
private:
    static_assert(!IntTypeInfo<CommandSizeType>::is_signed, "CommandSizeType must be unsigned");
    static_assert(IsPowerOfTwo<uintmax_t, (uintmax_t)CommandBufferSize + 1>::value, "CommandBufferSize+1 must be a power of two");
    
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    
    static const int step_bits = 23;
    static const bool step_singed = true;
    
public:
    using StepBoundedType = BoundedInt<step_bits, step_singed>;
    using TimeType = typename Clock::TimeType;
    using RelStepBoundedType = typename AxisStepperType::StepBoundedType;
    using RelAccelBoundedType = typename AxisStepperType::AccelBoundedType;
    using RelTimeBoundedType = typename AxisStepperType::TimeBoundedType;
    
    void init (Context c)
    {
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
    }
    
private:
    static AxisStepperType * axisStepper (AxisController *o)
    {
        return GetAxisStepper::call(o);
    }
    
    
};

#include <aprinter/EndNamespace.h>

#endif
