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

#ifndef AIPSTACK_PLATFORM_FACADE_H
#define AIPSTACK_PLATFORM_FACADE_H

#include <type_traits>
#include <utility>
#include <limits>

#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/NonCopyable.h>

namespace AIpStack {

template <typename Impl>
class PlatformFacade;

template <typename Impl, bool ImplIsStatic>
class PlatformRefBase {};

template <typename Impl>
class PlatformRefBase<Impl, false>
{
private:
    Impl *m_platform_impl;
    
public:
    inline PlatformRefBase (Impl *impl) :
        m_platform_impl(impl)
    {
    }
    
    inline Impl * platformImpl () const
    {
        return m_platform_impl;
    }
};

template <typename Impl>
class PlatformRef :
    public PlatformRefBase<Impl, Impl::ImplIsStatic>
{
    using Base = PlatformRefBase<Impl, Impl::ImplIsStatic>;
    
public:
    template <typename... Args>
    inline PlatformRef (Args && ... args) :
        Base(std::forward<Args>(args)...)
    {
    }
    
    inline PlatformRef (PlatformFacade<Impl> platform) :
        PlatformRef(platform.ref())
    {
    }
    
    inline PlatformRef ref () const
    {
        return *this;
    }
    
    inline PlatformFacade<Impl> platform () const
    {
        return PlatformFacade<Impl>(ref());
    }
};

template <typename Impl>
class PlatformFacade :
    private PlatformRef<Impl>
{
public:
    using Ref = PlatformRef<Impl>;
    
    inline PlatformFacade (Ref ref = Ref()) :
        Ref(ref)
    {
    }
    
    inline Ref ref () const
    {
        return static_cast<Ref const &>(*this);
    }
    
    using TimeType = typename Impl::TimeType;
    
    static_assert(std::is_integral<TimeType>::value, "");
    static_assert(std::is_unsigned<TimeType>::value, "");
    static_assert(std::numeric_limits<TimeType>::radix == 2, "");
    
    static int const TimeBits = std::numeric_limits<TimeType>::digits;
    
    static_assert(TimeBits >= 32, "");
    
    static constexpr double TimeFreq = Impl::TimeFreq;
    
    static_assert(TimeFreq >= 100.0, "");
    
    static TimeType const WorkingTimeSpanTicks = 7 * ((TimeType)1 << (TimeBits - 4));
    
    static constexpr double WorkingTimeSpanSec = WorkingTimeSpanTicks / TimeFreq;
    
    static_assert(WorkingTimeSpanSec >= 600.0, "");
    
    inline TimeType getTime () const
    {
        return callImpl<TimeType()>(&Impl::getTime);
    }
    
    template <typename Derived>
    class RefWrapper :
        public Ref
    {
    public:
        inline RefWrapper (PlatformFacade platform) :
            Ref(platform)
        {
        }
    };
    
    class Timer :
        private APrinter::NonCopyable,
        private Impl::Timer
    {
        using ImplTimer = typename Impl::Timer;
        
    public:
        inline Timer (PlatformFacade platform) :
            ImplTimer(platform.ref())
        {
        }
        
        inline PlatformFacade platform () const
        {
            Ref ref = ImplTimer::ref();
            return ref.platform();
        }
        
        inline ImplTimer & impl ()
        {
            return static_cast<ImplTimer &>(*this);
        }
        
        inline bool isSet ()
        {
            return callObj<ImplTimer, bool()>(&ImplTimer::isSet, *this);
        }
        
        inline TimeType getSetTime ()
        {
            return callObj<ImplTimer, TimeType()>(&ImplTimer::getSetTime, *this);
        }
        
        inline void unset ()
        {
            return callObj<ImplTimer, void()>(&ImplTimer::unset, *this);
        }
        
        inline void setAt (TimeType abs_time)
        {
            return callObj<ImplTimer, void(TimeType)>(&ImplTimer::setAt, *this, abs_time);
        }
        
        void setAfter (TimeType rel_time)
        {
            TimeType abs_time = platform().getTime() + rel_time;
            return setAt(abs_time);
        }
        
    protected:
        virtual void handleTimerExpired () override = 0;
    };
    
    template <typename Derived>
    class TimerWrapper :
        public Timer
    {
    public:
        inline TimerWrapper (PlatformFacade platform) :
            Timer(platform)
        {
        }
    };
    
private:
    template <typename Func>
    using RetType = APrinter::GetReturnType<Func>;
    
    template <typename Func, typename... Args>
    inline RetType<Func> callImpl (Func Impl::*func_ptr, Args && ... args) const
    {
        static_assert(!Impl::ImplIsStatic, "");
        Impl *impl = ref().platformImpl();
        return (impl->*func_ptr)(std::forward<Args>(args)...);
    }
    
    template <typename Func, typename... Args>
    inline RetType<Func> callImpl (Func *func_ptr, Args && ... args) const
    {
        return (*func_ptr)(std::forward<Args>(args)...);
    }
    
    template <typename Obj, typename Func, typename... Args>
    inline static RetType<Func> callObj (Func Obj::*func_ptr, Obj &obj, Args && ... args)
    {
        return (obj.*func_ptr)(std::forward<Args>(args)...);
    }
};

}

#endif
