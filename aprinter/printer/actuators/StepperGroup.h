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
#include <aprinter/base/Inline.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename LazySteppersList>
class StepperGroup {
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_enable, enable)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_disable, disable)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_setDir, setDir)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_stepOn, stepOn)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_stepOff, stepOff)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_emergency, emergency)
    
    template <typename TheLazySteppersList=LazySteppersList>
    using SteppersList = typename TheLazySteppersList::List;
    
public:
    static void enable (Context c)
    {
        ListForEachForward<SteppersList<>>(Foreach_enable(), c);
    }
    
    static void disable (Context c)
    {
        ListForEachForward<SteppersList<>>(Foreach_disable(), c);
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void setDir (ThisContext c, bool dir)
    {
        ListForEachForward<SteppersList<>>(Foreach_setDir(), c, dir);
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void stepOn (ThisContext c)
    {
        ListForEachForward<SteppersList<>>(Foreach_stepOn(), c);
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void stepOff (ThisContext c)
    {
        ListForEachForward<SteppersList<>>(Foreach_stepOff(), c);
    }
    
    static void emergency ()
    {
        ListForEachForward<SteppersList<>>(Foreach_emergency());
    }
};

#include <aprinter/EndNamespace.h>

#endif
