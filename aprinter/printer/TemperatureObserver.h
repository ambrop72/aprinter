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

#ifndef AMBROLIB_TEMPERATURE_OBSERVER_H
#define AMBROLIB_TEMPERATURE_OBSERVER_H

#include <math.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/OffsetCallback.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TSampleInterval,
    typename TValueTolerance,
    typename TMinTime
>
struct TemperatureObserverParams {
    using SampleInterval = TSampleInterval;
    using ValueTolerance = TValueTolerance;
    using MinTime = TMinTime;
};

template <typename Context, typename Params, typename GetValueCallback, typename Handler>
class TemperatureObserver
: private DebugObject<Context, void>
{
public:
    void init (Context c, double target)
    {
        m_event.init(c, AMBRO_OFFSET_CALLBACK_T(&TemperatureObserver::m_event, &TemperatureObserver::event_handler));
        m_target = target;
        m_intervals = 0;
        m_event.appendNowNotAlready(c);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        m_event.deinit(c);
    }
    
private:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    static const TimeType IntervalTicks = Params::SampleInterval::value() * Clock::time_freq;
    static const int MinIntervals = (Params::MinTime::value() / Params::SampleInterval::value()) + 2.0;
    using IntervalsType = typename ChooseInt<BitsInInt<MinIntervals>::value, false>::Type;
    
    void event_handler (Context c)
    {
        this->debugAccess(c);
        
        m_event.mainOnlyAppendAfterPrevious(c, IntervalTicks);
        
        double value = GetValueCallback::call(this, c);
        bool in_range = fabs(value - m_target) < Params::ValueTolerance::value();
        
        if (!in_range) {
            m_intervals = 0;
        } else if (m_intervals != MinIntervals) {
            m_intervals++;
        }
        
        return Handler::call(this, c, m_intervals == MinIntervals);
    }
    
    typename Context::EventLoop::QueuedEvent m_event;
    double m_target;
    IntervalsType m_intervals;
};

#include <aprinter/EndNamespace.h>

#endif
