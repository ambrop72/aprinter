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

#ifndef AMBROLIB_BLINKER_H
#define AMBROLIB_BLINKER_H

#include <stdint.h>

#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Pin, typename Handler>
class Blinker
{
public:
    struct Object;
    using TimeType = typename Context::Clock::TimeType;
    
    static void init (Context c, TimeType interval)
    {
        auto *o = Object::self(c);
        o->interval = interval;
        o->next_time = c.clock()->getTime(c);
        o->state = false;
        o->timer.init(c, Blinker::timer_handler);
        o->timer.appendAt(c, o->next_time);
        
        c.pins()->template set<Pin>(c, o->state);
        c.pins()->template setOutput<Pin>(c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        o->timer.deinit(c);
    }
    
    static void setInterval (Context c, TimeType interval)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        o->interval = interval;
    }
    
private:
    using Loop = typename Context::EventLoop;
    
    static void timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        o->state = !o->state;
        c.pins()->template set<Pin>(c, o->state);
        o->next_time += o->interval;
        o->timer.appendAt(c, o->next_time);
        
        return Handler::call(c);
    }
    
public:
    struct Object : public ObjBase<Blinker, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        TimeType interval;
        TimeType next_time;
        bool state;
        typename Loop::QueuedEvent timer;
    };
};

#include <aprinter/EndNamespace.h>

#endif
