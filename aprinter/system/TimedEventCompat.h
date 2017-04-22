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

#ifndef APRINTER_TIMED_EVENT_COMPAT_H
#define APRINTER_TIMED_EVENT_COMPAT_H

#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>

#include <aprinter/BeginNamespace.h>

template <typename TimedEvent>
class TimedEventCompat :
    private TimedEvent
{
public:
    using typename TimedEvent::Context;
    using typename TimedEvent::TimeType;
    
    using HandlerType = Callback<void(Context)>;
    
    void init (Context c, HandlerType handler)
    {
        AMBRO_ASSERT(handler)
        
        TimedEvent::init(c);
        m_handler = handler;
    }
    
    using TimedEvent::deinit;
    using TimedEvent::unset;
    using TimedEvent::isSet;
    using TimedEvent::getSetTime;
    using TimedEvent::appendAtNotAlready;
    using TimedEvent::appendAt;
    using TimedEvent::appendNowNotAlready;
    using TimedEvent::appendAfter;
    using TimedEvent::appendAfterNotAlready;
    using TimedEvent::appendAfterPrevious;
    
private:
    void handleTimerExpired (Context c) override final
    {
        m_handler(c);
    }
    
private:
    HandlerType m_handler;
};

#include <aprinter/EndNamespace.h>

#endif
