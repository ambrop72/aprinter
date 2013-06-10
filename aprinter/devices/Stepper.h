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

#ifndef AMBROLIB_STEPPER_H
#define AMBROLIB_STEPPER_H

#include <avr/cpufunc.h>

#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename DirPin, typename StepPin, typename EnablePin>
class Stepper
: private DebugObject<Context, Stepper<Context, DirPin, StepPin, EnablePin>>
{
public:
    void init (Context c)
    {
        c.pins()->template set<DirPin>(c, false);
        c.pins()->template set<StepPin>(c, false);
        c.pins()->template set<EnablePin>(c, true);
        c.pins()->template setOutput<DirPin>(c);
        c.pins()->template setOutput<StepPin>(c);
        c.pins()->template setOutput<EnablePin>(c);
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
    }
    
    void enable (Context c, bool e)
    {
        this->debugAccess(c);
        
        c.pins()->template set<EnablePin>(c, !e);
    }
    
    void setDir (Context c, bool dir)
    {
        c.pins()->template set<DirPin>(c, dir);
    }
    
    void step (Context c)
    {
        this->debugAccess(c);
        
        c.pins()->template set<StepPin>(c, true);
        c.pins()->template set<StepPin>(c, false);
    }
};

#include <aprinter/EndNamespace.h>

#endif
