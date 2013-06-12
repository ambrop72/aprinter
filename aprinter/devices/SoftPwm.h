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

#ifndef AMBROLIB_SOFT_PWM_H
#define AMBROLIB_SOFT_PWM_H

#include <stdint.h>

#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename Pin, uint32_t PulseInterval>
class SoftPwm
: private DebugObject<Context, SoftPwm<Context, Pin, PulseInterval>>
{
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef typename Context::EventLoop Loop;
    
    void init (Context c)
    {
#ifdef AMBROLIB_ASSERTIONS
        m_enabled = false;
#endif
        m_on_time = 0;
        m_timer.init(c, AMBRO_OFFSET_CALLBACK_T(&SoftPwm::m_timer, &SoftPwm::timer_handler));
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        m_timer.deinit(c);
    }
    
    void enable (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_enabled)
        
#ifdef AMBROLIB_ASSERTIONS
        m_enabled = true;
#endif
        m_start_time = start_time;
        m_timer.appendAt(c, m_start_time, false);
        c.pins()->template setOutput<Pin>(c);
    }
    
    void disable (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_enabled)
        
#ifdef AMBROLIB_ASSERTIONS
        m_enabled = false;
#endif
        m_timer.unset(c);
        c.pins()->template set<Pin>(c, false);
    }
    
    void setOnTime (Context c, TimeType on_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(on_time != 0)
        
        m_on_time = on_time;
    }
    
private:
    void timer_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_on_time != 0)
        AMBRO_ASSERT(m_enabled)
        
        TimeType next_time;
        if (m_start_time == m_timer.getSetTime(c)) {
            next_time = m_start_time + m_on_time;
            c.pins()->template set<Pin>(c, true);
        } else {
            m_start_time += (TimeType)((double)PulseInterval * 0.000001 / Clock::time_unit);
            next_time = m_start_time;
            c.pins()->template set<Pin>(c, false);
        }
        
        m_timer.appendAt(c, next_time);
    }
    
#ifdef AMBROLIB_ASSERTIONS
    bool m_enabled;
#endif
    TimeType m_on_time;
    TimeType m_start_time;
    typename Loop::QueuedEvent m_timer;
};

#include <aprinter/EndNamespace.h>

#endif
