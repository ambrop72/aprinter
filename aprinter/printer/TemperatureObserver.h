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

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/Object.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/base/DebugObject.h>

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

template <typename Context, typename ParentObject, typename FpType, typename Params, typename GetValueCallback, typename Handler>
class TemperatureObserver {
public:
    struct Object;
    
    static void init (Context c, FpType target)
    {
        auto *o = Object::self(c);
        
        o->m_event.init(c, &TemperatureObserver::event_handler);
        o->m_target = target;
        o->m_intervals = 0;
        o->m_event.appendNowNotAlready(c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        o->m_event.deinit(c);
    }
    
private:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    static const TimeType IntervalTicks = Params::SampleInterval::value() * Clock::time_freq;
    static const int MinIntervals = (Params::MinTime::value() / Params::SampleInterval::value()) + 2.0;
    using IntervalsType = typename ChooseInt<BitsInInt<MinIntervals>::value, false>::Type;
    
    static void event_handler (typename Context::EventLoop::QueuedEvent *, Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        o->m_event.appendAfterPrevious(c, IntervalTicks);
        
        FpType value = GetValueCallback::call(c);
        bool in_range = FloatAbs(value - o->m_target) < (FpType)Params::ValueTolerance::value();
        
        if (!in_range) {
            o->m_intervals = 0;
        } else if (o->m_intervals != MinIntervals) {
            o->m_intervals++;
        }
        
        return Handler::call(c, o->m_intervals == MinIntervals);
    }
    
public:
    struct Object : public ObjBase<TemperatureObserver, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        typename Context::EventLoop::QueuedEvent m_event;
        FpType m_target;
        IntervalsType m_intervals;
    };
};

#include <aprinter/EndNamespace.h>

#endif
