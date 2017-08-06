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

#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>

namespace APrinter {

template <typename Arg>
class TemperatureObserver {
    using Context          = typename Arg::Context;
    using ParentObject     = typename Arg::ParentObject;
    using Config           = typename Arg::Config;
    using FpType           = typename Arg::FpType;
    using GetValueCallback = typename Arg::GetValueCallback;
    using Handler          = typename Arg::Handler;
    using Params           = typename Arg::Params;
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_event.init(c, APRINTER_CB_STATFUNC_T(&TemperatureObserver::event_handler));
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->m_event.deinit(c);
    }
    
    static void startObserving (Context c, FpType target)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_event.isSet(c))
        
        o->m_target = target;
        o->m_intervals = 0;
        o->m_event.appendNowNotAlready(c);
    }
    
    static void stopObserving (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_event.isSet(c))
        
        o->m_event.unset(c);
    }
    
    static bool isObserving (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return o->m_event.isSet(c);
    }
    
private:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using SampleInterval = decltype(Config::e(Params::SampleInterval::i()));
    using TimeFreq = APRINTER_FP_CONST_EXPR(Clock::time_freq);
    using Two = APRINTER_FP_CONST_EXPR(2.0);
    using IntervalsMax = APRINTER_FP_CONST_EXPR(65535.0);
    
    using CIntervalTicks = decltype(ExprCast<TimeType>(SampleInterval() * TimeFreq()));
    using CMinIntervals = decltype(ExprCast<uint16_t>(ExprFmin(IntervalsMax(), (Config::e(Params::MinTime::i()) / SampleInterval()) + Two())));
    using CValueTolerance = decltype(ExprCast<FpType>(Config::e(Params::ValueTolerance::i())));
    
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_event.appendAfterPrevious(c, APRINTER_CFG(Config, CIntervalTicks, c));
        
        FpType value = GetValueCallback::call(c);
        bool in_range = FloatAbs(value - o->m_target) < APRINTER_CFG(Config, CValueTolerance, c);
        
        if (!in_range) {
            o->m_intervals = 0;
        } else if (o->m_intervals < APRINTER_CFG(Config, CMinIntervals, c)) {
            o->m_intervals++;
        }
        
        return Handler::call(c, o->m_intervals >= APRINTER_CFG(Config, CMinIntervals, c));
    }
    
public:
    struct Object : public ObjBase<TemperatureObserver, ParentObject, MakeTypeList<TheDebugObject>> {
        typename Context::EventLoop::TimedEvent m_event;
        FpType m_target;
        uint16_t m_intervals;
    };
    
    using ConfigExprs = MakeTypeList<CIntervalTicks, CMinIntervals, CValueTolerance>;
};

APRINTER_ALIAS_STRUCT_EXT(TemperatureObserverService, (
    APRINTER_AS_TYPE(SampleInterval),
    APRINTER_AS_TYPE(ValueTolerance),
    APRINTER_AS_TYPE(MinTime)
), (
    APRINTER_ALIAS_STRUCT_EXT(Observer, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(FpType),
        APRINTER_AS_TYPE(GetValueCallback),
        APRINTER_AS_TYPE(Handler)
    ), (
        using Params = TemperatureObserverService;
        APRINTER_DEF_INSTANCE(Observer, TemperatureObserver)
    ))
))

}

#endif
