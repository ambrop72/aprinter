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

#ifndef APRINTER_STEPPER_GROUP_H
#define APRINTER_STEPPER_GROUP_H

#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Hints.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class StepperGroup {
    using Context          = typename Arg::Context;
    using LazySteppersList = typename Arg::LazySteppersList;
    
private:
    template <typename TheLazySteppersList=LazySteppersList>
    using SteppersList = typename TheLazySteppersList::List;
    
public:
    static void enable (Context c)
    {
        ListForEachForward<SteppersList<>>([&] APRINTER_TL(stepper, stepper::enable(c)));
    }
    
    static void disable (Context c)
    {
        ListForEachForward<SteppersList<>>([&] APRINTER_TL(stepper, stepper::disable(c)));
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void setDir (ThisContext c, bool dir)
    {
        ListForEachForward<SteppersList<>>([&] APRINTER_TL(stepper, stepper::setDir(c, dir)));
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void stepOn (ThisContext c)
    {
        ListForEachForward<SteppersList<>>([&] APRINTER_TL(stepper, stepper::stepOn(c)));
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void stepOff (ThisContext c)
    {
        ListForEachForward<SteppersList<>>([&] APRINTER_TL(stepper, stepper::stepOff(c)));
    }
    
    static void emergency ()
    {
        ListForEachForward<SteppersList<>>([&] APRINTER_TL(stepper, stepper::emergency()));
    }
};

APRINTER_ALIAS_STRUCT_EXT(StepperGroupArg, (
    APRINTER_AS_TYPE(Context),
    APRINTER_AS_TYPE(LazySteppersList)
), (
    APRINTER_DEF_INSTANCE(StepperGroupArg, StepperGroup)
))

#include <aprinter/EndNamespace.h>

#endif
