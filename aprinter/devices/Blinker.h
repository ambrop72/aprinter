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

#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Position, typename Context, typename Pin, typename Handler>
class Blinker
: private DebugObject<Context, void>
{
    AMBRO_MAKE_SELF(Context, Blinker, Position)
    
public:
    using TimeType = typename Context::Clock::TimeType;
    
    static void init (Context c, TimeType interval)
    {
        Blinker *o = self(c);
        o->m_interval = interval;
        o->m_next_time = c.clock()->getTime(c);
        o->m_state = false;
        o->m_timer.init(c, Blinker::timer_handler);
        o->m_timer.appendAt(c, o->m_next_time);
        
        c.pins()->template set<Pin>(c, o->m_state);
        c.pins()->template setOutput<Pin>(c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        Blinker *o = self(c);
        o->debugDeinit(c);
        
        o->m_timer.deinit(c);
    }
    
    static void setInterval (Context c, TimeType interval)
    {
        Blinker *o = self(c);
        o->debugAccess(c);
        
        o->m_interval = interval;
    }
    
private:
    using Loop = typename Context::EventLoop;
    
    static void timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        Blinker *o = self(c);
        o->debugAccess(c);
        
        o->m_state = !o->m_state;
        c.pins()->template set<Pin>(c, o->m_state);
        o->m_next_time += o->m_interval;
        o->m_timer.appendAt(c, o->m_next_time);
        
        return Handler::call(c);
    }
    
    TimeType m_interval;
    TimeType m_next_time;
    bool m_state;
    typename Loop::QueuedEvent m_timer;
};

#include <aprinter/EndNamespace.h>

#endif
