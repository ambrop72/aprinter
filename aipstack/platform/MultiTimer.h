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

#ifndef AIPSTACK_MULTI_TIMER_H
#define AIPSTACK_MULTI_TIMER_H

#include <aipstack/meta/ChooseInt.h>
#include <aipstack/meta/TypeListUtils.h>
#include <aipstack/meta/ListForEach.h>
#include <aipstack/misc/Preprocessor.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

template <typename PlatformImpl, typename MT, typename TimerId>
class MultiTimerOne
{
    using Platform = PlatformFacade<PlatformImpl>;
    AIPSTACK_USE_TYPES1(Platform, (TimeType))
    
public:
    // WARNING: After calling any function which adjusts the timer state and
    // after timer expiration, MultiTimer::doDelayedUpdate must be called
    // before control returns to the event loop, or unsetAll or deinit. This
    // is an optimization which allows preventing redundant updates of the
    // underlying timer.
    
    inline bool isSet ()
    {
        return (mt().m_state & MT::TimerBit(TimerId())) != 0;
    }
    
    inline TimeType getSetTime ()
    {
        return mt().m_times[MT::TimerIndex(TimerId())];
    }
    
    inline void setAt (TimeType abs_time)
    {
        mt().m_times[MT::TimerIndex(TimerId())] = abs_time;
        mt().m_state |= MT::TimerBit(TimerId()) | MT::DirtyBit;
    }
    
    inline void setAfter (TimeType rel_time)
    {
        TimeType abs_time = mt().platform().getTime() + rel_time;
        setAt(abs_time);
    }
    
    inline void unset ()
    {
        mt().m_state = (mt().m_state & ~MT::TimerBit(TimerId())) | MT::DirtyBit;
    }
    
private:
    inline MT & mt ()
    {
        return static_cast<MT &>(*this);
    }
};

template <typename PlatformImpl, typename Derived, typename UserData, typename... TimerIds>
class MultiTimer :
    private MultiTimerOne<PlatformImpl, MultiTimer<PlatformImpl, Derived, UserData, TimerIds...>, TimerIds>...,
    private PlatformFacade<PlatformImpl>::Timer,
    public UserData
{
    template <typename, typename, typename>
    friend class MultiTimerOne;
    
    using Platform = PlatformFacade<PlatformImpl>;
    AIPSTACK_USE_TYPES1(Platform, (TimeType, Timer))
    
    static int const NumTimers = sizeof...(TimerIds);
    using TimerIdsList = MakeTypeList<TimerIds...>;
    
    using StateType = ChooseInt<NumTimers + 1, false>;
    
    template <typename TimerId>
    static constexpr int TimerIndex (TimerId)
    {
        return TypeListIndex<TimerIdsList, TimerId>::Value;
    }
    
    template <typename TimerId>
    static constexpr StateType TimerBit (TimerId)
    {
        return (StateType)1 << TimerIndex(TimerId());
    }
    
    static constexpr StateType DirtyBit = (StateType)1 << NumTimers;
    
private:
    // UserData would be placed in front of m_state using up
    // what might otherwise be holes in the memory layout.
    StateType m_state;
    TimeType m_times[NumTimers];
    
public:
    using Timer::platform;
    
    inline MultiTimer (Platform platform) :
        Timer(platform),
        m_state(0)
    {
    }
    
    inline void unsetAll ()
    {
        Timer::unset();
        m_state = 0;
    }
    
    template <typename TimerId>
    inline MultiTimerOne<PlatformImpl, MultiTimer, TimerId> & tim (TimerId)
    {
        return static_cast<MultiTimerOne<PlatformImpl, MultiTimer, TimerId> &>(*this);
    }
    
    inline void doDelayedUpdate ()
    {
        // Do an update if the dirty bit is set.
        StateType state = m_state;
        if ((state & DirtyBit) != 0) {
            updateTimer(state);
        }
    }
    
private:
    void updateTimer (StateType state)
    {
        // Clear the dirty bit and write back state.
        state &= ~DirtyBit;
        m_state = state;
        
        if (AIPSTACK_UNLIKELY(state == 0)) {
            // No user timer is set, unset the underlying timer.
            Timer::unset();
            return;
        }
        
        // This is the value with the most significant bit one and others zero.
        constexpr TimeType msb = ((TimeType)-1 / 2) + 1;
        
        // We use this as the base time to compare timers to. We will also
        // be computing the minium time relative to this time for efficiency.
        TimeType ref_time = platform().getEventTime() - msb;
        
        // State for the minimum calculation.
        TimeType min_time_rel = (TimeType)-1;
        
        // Go through all timers to find the minimum time.
        ListFor<TimerIdsList>([&] AIPSTACK_TL(TimerId, {
            if ((state & MultiTimer::TimerBit(TimerId())) != 0) {
                TimeType time_rel = m_times[MultiTimer::TimerIndex(TimerId())] - ref_time;
                if (time_rel < min_time_rel) {
                    min_time_rel = time_rel;
                }
            }
        }));
        
        // Set the underlying timer to the minimum time.
        TimeType min_time = min_time_rel + ref_time;
        Timer::setAt(min_time);
    }
    
    void handleTimerExpired () override final
    {
        // Any delayed update must have been applied before returning to event loop.
        AIPSTACK_ASSERT((m_state & DirtyBit) == 0)
        
        TimeType set_time = Timer::getSetTime();
        
        bool not_handled = ListForBreak<TimerIdsList>([&] AIPSTACK_TL(TimerId,
        {
            if ((m_state & MultiTimer::TimerBit(TimerId())) != 0 &&
                m_times[MultiTimer::TimerIndex(TimerId())] == set_time)
            {
                // Clear the timer bit and set the dirty bit. The timer callback is
                // responsible for calling doDelayedUpdate or one of the functions that
                // reset things.
                m_state = (m_state & ~MultiTimer::TimerBit(TimerId())) | DirtyBit;
                static_cast<Derived &>(*this).timerExpired(TimerId());
                return false;
            }
            return true;
        }));
        
        AIPSTACK_ASSERT(!not_handled)
    }
};

}

#endif
