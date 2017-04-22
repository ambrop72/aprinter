/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef APRINTER_TIMED_EVENT_WRAPPER_H
#define APRINTER_TIMED_EVENT_WRAPPER_H

#include <aprinter/base/Preprocessor.h>

#include <aprinter/BeginNamespace.h>

template <typename TimedEvent, typename Wrapper, typename Id>
class TimedEventWrapperOne : private TimedEvent
{
    friend Wrapper;
    using Context = typename TimedEvent::Context;
    
    void handleTimerExpired (Context c) override final
    {
        static_cast<Wrapper *>(this)->template timedEventWrapperTimerExpired<Id>(c);
    }
};

template <typename TimedEvent, typename Impl, typename... Ids>
class TimedEventWrapper :
    private TimedEventWrapperOne<TimedEvent, TimedEventWrapper<TimedEvent, Impl, Ids...>, Ids> ...
{
    template <typename, typename, typename>
    friend class TimedEventWrapperOne;
    
    using Context = typename TimedEvent::Context;
    
    template <typename Id>
    inline void timedEventWrapperTimerExpired (Context c)
    {
        static_cast<Impl *>(this)->timerExpired(Id(), c);
    }
    
public:
    template <typename Id>
    inline TimedEvent & tim (Id)
    {
        return static_cast<TimedEventWrapperOne<TimedEvent, TimedEventWrapper, Id> &>(*this);
    }
};

#define APRINTER_DECL_TIMERS_DECL_TIMER(Dummy, TimerName) struct TimerName {};
#define APRINTER_DECL_TIMERS_GIVE_TIMER(Dummy, TimerName) TimerName

#define APRINTER_DECL_TIMERS(TimersWrapperName, Context, Impl, TheTimers) \
APRINTER_AS_MAP(APRINTER_DECL_TIMERS_DECL_TIMER, APRINTER_AS_MAP_DELIMITER_NONE, 0, TheTimers) \
using TimersWrapperName = APrinter::TimedEventWrapper< \
    Context::EventLoop::TimedEventNew, \
    Impl, \
    APRINTER_AS_MAP(APRINTER_DECL_TIMERS_GIVE_TIMER, APRINTER_AS_MAP_DELIMITER_COMMA, 0, TheTimers) \
>;

#define APRINTER_DECL_TIMERS_CLASS(ClassName, Context, Impl, TheTimers) \
class ClassName { \
    friend Impl; \
    APRINTER_DECL_TIMERS(Timers, Context, Impl, TheTimers) \
};

#define APRINTER_USE_TIMERS_CLASS(TimersDeclClass, TheTimers) \
using AprinterTimers = typename TimersDeclClass::Timers; \
friend AprinterTimers; \
using AprinterTimers::tim; \
APRINTER_USE_TYPES1(TimersDeclClass, TheTimers)

#include <aprinter/EndNamespace.h>

#endif
