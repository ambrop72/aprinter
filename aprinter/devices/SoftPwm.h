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

#include <aprinter/meta/WrapCallback.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename Pin, typename PulseInterval, typename TimerCallback, template<typename, typename> class TimerTemplate>
class SoftPwm
: private DebugObject<Context, void>
{
private:
    struct TimerHandler;
    
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TimerInstance = TimerTemplate<Context, TimerHandler>;
    using FixedFracType = FixedPoint<16, false, -16>;
    
    void init (Context c, TimeType start_time)
    {
        m_lock.init(c);
        m_timer.init(c);
        m_state = false;
        m_start_time = start_time;
        c.pins()->template set<Pin>(c, false);
        c.pins()->template setOutput<Pin>(c);
        m_timer.set(c, start_time);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        m_timer.deinit(c);
        c.pins()->template set<Pin>(c, false);
        m_lock.deinit(c);
    }
    
    TimerInstance * getTimer ()
    {
        return &m_timer;
    }
    
private:
    static const TimeType interval = PulseInterval::value() / Clock::time_unit;
    
    bool timer_handler (typename TimerInstance::HandlerContext c)
    {
        this->debugAccess(c);
        
        TimeType next_time;
        if (AMBRO_LIKELY(!m_state)) {
            FixedFracType frac = TimerCallback::call(this, c);
            c.pins()->template set<Pin>(c, (frac.bitsValue() > 0));
            if (AMBRO_LIKELY(frac.bitsValue() > 0 && frac < FixedFracType::maxValue())) {
                auto res = FixedResMultiply(FixedPoint<32, false, 0>::importBits(interval), frac);
                next_time = m_start_time + (TimeType)res.bitsValue();
                m_state = true;
            } else {
                m_start_time += interval;
                next_time = m_start_time;
            }
        } else {
            c.pins()->template set<Pin>(c, false);
            m_start_time += interval;
            next_time = m_start_time;
            m_state = false;
        }
        m_timer.set(c, next_time);
        return true;
    }
    
    typename Context::Lock m_lock;
    TimerInstance m_timer;
    bool m_state;
    TimeType m_start_time;
    
    struct TimerHandler : public AMBRO_WCALLBACK_TD(&SoftPwm::timer_handler, &SoftPwm::m_timer) {};
};

#include <aprinter/EndNamespace.h>

#endif
