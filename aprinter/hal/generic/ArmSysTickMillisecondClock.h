/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_ARM_SYSTICK_MILLISECOND_CLOCK_H
#define APRINTER_ARM_SYSTICK_MILLISECOND_CLOCK_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject>
class ArmSysTickMillisecondClock {
private:
    static uint32_t const SysTickInterruptClocks = F_MCK / 1000;
    
public:
    using TimeType = uint32_t;
    
    static constexpr double time_freq = F_MCK / double(SysTickInterruptClocks);
    static constexpr double time_unit = 1.0 / time_freq;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->ticks = 0;
        
        memory_barrier();
        
        NVIC_ClearPendingIRQ(SysTick_IRQn);
        NVIC_SetPriority(SysTick_IRQn, 1);
        SysTick_Config(SysTickInterruptClocks);
        NVIC_EnableIRQ(SysTick_IRQn);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        auto *o = Object::self(c);
        return o->ticks;
    }
    
    static void systick_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        o->ticks++;
    }
    
public:
    struct Object : public ObjBase<ArmSysTickMillisecondClock, ParentObject, EmptyTypeList> {
        TimeType volatile ticks;
    };
};

#define APRINTER_ARM_SYSTICK_MILLISECOND_CLOCK_GLOBAL(TheClock, context) \
extern "C" \
__attribute__((used)) \
void SysTick_Handler (void) \
{ \
    TheClock::systick_handler(MakeInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
