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

#ifndef AIPSTACK_TIMER_WRAPPER_H
#define AIPSTACK_TIMER_WRAPPER_H

#include <tuple>

#include <aipstack/misc/Preprocessor.h>

#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

template <typename PlatformImpl, typename Wrapper, typename Id>
class TimerWrapperOne :
    private PlatformFacade<PlatformImpl>::Timer
{
    friend Wrapper;
    
    inline TimerWrapperOne (PlatformFacade<PlatformImpl> platform) :
        PlatformFacade<PlatformImpl>::Timer(platform)
    {
    }
    
    void handleTimerExpired () override final
    {
        static_cast<Wrapper &>(*this).template timerWrapperTimerExpired<Id>();
    }
};

template <typename PlatformImpl, typename Derived, typename... Ids>
class TimerWrapper :
    private TimerWrapperOne<PlatformImpl, TimerWrapper<PlatformImpl, Derived, Ids...>, Ids> ...
{
    template <typename, typename, typename>
    friend class TimerWrapperOne;
    
    template <typename Id>
    inline void timerWrapperTimerExpired ()
    {
        return static_cast<Derived &>(*this).timerExpired(Id());
    }
    
public:
    inline TimerWrapper (PlatformFacade<PlatformImpl> platform) :
        TimerWrapperOne<PlatformImpl, TimerWrapper, Ids>(platform)...
    {
    }
    
    template <typename Id>
    inline typename PlatformFacade<PlatformImpl>::Timer & tim (Id)
    {
        return static_cast<TimerWrapperOne<PlatformImpl, TimerWrapper, Id> &>(*this);
    }
    
    inline PlatformFacade<PlatformImpl> platform () const
    {
        using FirstId = std::tuple_element_t<0, std::tuple<Ids...>>;
        return TimerWrapperOne<PlatformImpl, TimerWrapper, FirstId>::platform();
    }
};

#define AIPSTACK_DECL_TIMERS_DECL_TIMER(Dummy, TimerName) struct TimerName {};
#define AIPSTACK_DECL_TIMERS_GIVE_TIMER(Dummy, TimerName) TimerName

#define AIPSTACK_DECL_TIMERS(TimerWrapperName, PlatformImpl, Derived, TheTimers) \
AIPSTACK_AS_MAP(AIPSTACK_DECL_TIMERS_DECL_TIMER, AIPSTACK_AS_MAP_DELIMITER_NONE, 0, TheTimers) \
using TimerWrapperName = AIpStack::TimerWrapper< \
    PlatformImpl, \
    Derived, \
    AIPSTACK_AS_MAP(AIPSTACK_DECL_TIMERS_GIVE_TIMER, AIPSTACK_AS_MAP_DELIMITER_COMMA, 0, TheTimers) \
>;

#define AIPSTACK_USE_TIMERS(TimerWrapperName) \
friend TimerWrapperName; \
using TimerWrapperName::tim;

#define AIPSTACK_DECL_TIMERS_CLASS(ClassName, PlatformImpl, Derived, TheTimers) \
class ClassName { \
    friend Derived; \
    AIPSTACK_DECL_TIMERS(Timers, PlatformImpl, Derived, TheTimers) \
};

#define AIPSTACK_USE_TIMERS_CLASS(TimersDeclClass, TheTimers) \
using AipstackTimers = typename TimersDeclClass::Timers; \
friend AipstackTimers; \
using AipstackTimers::tim; \
AIPSTACK_USE_TYPES1(TimersDeclClass, TheTimers)

}

#endif
