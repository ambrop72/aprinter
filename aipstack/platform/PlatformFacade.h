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

#include <aipstack/meta/BasicMetaUtils.h>

#include <aipstack/misc/NonCopyable.h>

namespace AIpStack {

template <typename Impl>
class PlatformFacade;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

template <typename Impl, bool ImplIsStatic>
struct PlatformRefBase {};

template <typename Impl>
struct PlatformRefBase<Impl, false>
{
    Impl *m_platform_impl;
    
    inline PlatformRefBase (Impl *impl) :
        m_platform_impl(impl)
    {}
};

#endif

/**
 * A reference to the platform implementation.
 * 
 * This class is either an empty class if the platform implementation (Impl) is
 * static or contains a pointer to Impl if not. Whether the platform implementation
 * is static is determined by the constant ImplIsStatic in Impl; see
 * @ref PlatformImplStub::ImplIsStatic.
 * 
 * This class is designed to be inherited by classes which need to access the
 * platform facilities. The empty-base optimization will ensure that no memory is
 * wasted if the platform implementation is static. However, note that if some
 * class already uses another class which includes and exposes a platform reference
 * (such as @ref PlatformFacade::Timer), retrieving the platform reference from that
 * class should be preferred.
 * 
 * Usually the platform is accessed through the @ref PlatformFacade wrapper, an
 * instance of which can be obtained by calling the @ref platform function in this
 * class.
 * 
 * @tparam Impl The platform implementation class, providing the API described
 *         in @ref PlatformImplStub.
 */
template <typename Impl>
class PlatformRef :
    private PlatformRefBase<Impl, Impl::ImplIsStatic>
{
    using Base = PlatformRefBase<Impl, Impl::ImplIsStatic>;
    
public:
    /**
     * Construct the platform reference.
     * 
     * If the platform implementation is static, no arguments must be passed.
     * If the platform implementation is not static, a pointer to the platform
     * implementation (Impl) must be passed as the single argument.
     * 
     * @param args No arguments (static platform implementation) or a pointer to
     *        Impl (non-static platform implementation).
     */
    template <typename... Args>
    inline PlatformRef (Args && ... args) :
        Base(std::forward<Args>(args)...)
    {
    }
    
    /**
     * Construct the platform reference from a @ref PlatformFacade.
     * 
     * The platform reference is initialized to the one stored within
     * the given @ref PlatformFacade.
     * 
     * @param platform The platform facade to get the platform reference from.
     */
    inline PlatformRef (PlatformFacade<Impl> platform) :
        PlatformRef(platform.ref())
    {
    }
    
    /**
     * Get the platform reference as a value.
     * 
     * This is a convenience function which simply returns *this.
     * 
     * @return The platform reference.
     */
    inline PlatformRef ref () const
    {
        return *this;
    }
    
    /**
     * Construct and return a @ref PlatformFacade for this platform reference.
     * 
     * This is a convenience function which returns a @ref PlatformFacade
     * constructed from this platform reference.
     * 
     * @return The platform facade.
     */
    inline PlatformFacade<Impl> platform () const
    {
        return PlatformFacade<Impl>(*this);
    }
    
    /**
     * Return the stored pointer to the platform implementation.
     * 
     * This function can only be called when Impl is a non-static platform
     * implementation (see @ref PlatformImplStub::ImplIsStatic).
     * 
     * No arguments must be passed, the argument in the declaration just performs
     * enable_if magic to disable this function for static platform implementations.
     * 
     * @return The pointer to the platform implementation.
     */
    template <typename Dummy = std::true_type>
    inline Impl * platformImpl (
        std::enable_if_t<!Impl::ImplIsStatic, Dummy> = std::true_type()) const
    {
        return this->m_platform_impl;
    }
};

/**
 * A wrapper to the platform implementation as described in @ref PlatformImplStub.
 * 
 * This class wraps the platform implementation. It performs various compile-time
 * checks and provides various convenience functions and definitions based on the
 * base functionality.
 * 
 * This class internally stores a @ref PlatformRef. It needs to be constructed
 * with one and allows retrieving it using @ref ref.
 * 
 * Note that the definitions here which directly map to those in the platform
 * implementation are not fully documented here but in @ref PlatformImplStub.
 * 
 * @tparam Impl The platform implementation class, providing the API described
 *         in @ref PlatformImplStub.
 */
template <typename Impl>
class PlatformFacade :
    private PlatformRef<Impl>
{
public:
    /**
     * A type alias for the @ref PlatformRef with the Impl class used.
     */
    using Ref = PlatformRef<Impl>;
    
    /**
     * Construct the platform facade.
     * 
     * @param ref The platform reference. It will be stored in this object
     *        and can be retrieved using @ref ref if needed.
     */
    inline PlatformFacade (Ref ref = Ref()) :
        Ref(ref)
    {
    }
    
    /**
     * Return the platform reference stored in this object.
     * 
     * @return The platform reference that the object was constructed with.
     */
    inline Ref ref () const
    {
        return static_cast<Ref const &>(*this);
    }
    
    /**
     * An unsigned integer type representing time in platform-defined units.
     * 
     * See @ref PlatformImplStub::TimeType for details.
     */
    using TimeType = typename Impl::TimeType;
    
    static_assert(std::is_integral<TimeType>::value, "");
    static_assert(std::is_unsigned<TimeType>::value, "");
    static_assert(std::numeric_limits<TimeType>::radix == 2, "");
    
    /**
     * The number of bits in @ref TimeType.
     */
    static int const TimeBits = std::numeric_limits<TimeType>::digits;
    
    static_assert(TimeBits >= 32, "");
    
    /**
     * The frequency of the clock in Hz.
     * 
     * See @ref PlatformImplStub::TimeFreq for details.
     */
    static constexpr double TimeFreq = Impl::TimeFreq;
    
    static_assert(TimeFreq >= 100.0, "");
    
    /**
     * The maximum relative time in the future or past that a \ref Timer may be
     * set to exire at.
     * 
     * See @ref PlatformImplStub::TimeType for the reasoning behind this definition.
     */
    static TimeType const WorkingTimeSpanTicks = 7 * ((TimeType)1 << (TimeBits - 4));
    
    /**
     * @ref WorkingTimeSpanTicks converted from ticks to seconds.
     * 
     * This is simply @ref WorkingTimeSpanTicks divided by @ref TimeFreq.
     * 
     * Note that when converting a seconds value not exceeding this value to ticks,
     * there is no need to worry about roundoff errors causing the result to be
     * greater than @ref WorkingTimeSpanTicks, because the latter is a pessimistic
     * estimate which will still accomodate such relatively small errors.
     */
    static constexpr double WorkingTimeSpanSec = WorkingTimeSpanTicks / TimeFreq;
    
    static_assert(WorkingTimeSpanSec >= 600.0, "");
    
    /**
     * The value of the most significant bit in @ref TimeType.
     * 
     * This may be useful for certain time calculations.
     */
    static TimeType const TimeMSB = (TimeType)1 << (TimeBits - 1);
    
    /**
     * Get the current time in ticks.
     * 
     * See @ref PlatformImplStub::getTime for details.
     * 
     * @return The current time in ticks.
     */
    inline TimeType getTime () const
    {
        return callImpl<TimeType()>(&Impl::getTime);
    }
    
    /**
     * Get a time in ticks that is not significantly earlier than the current time.
     * 
     * See @ref PlatformImplStub::getEventTime for details.
     * 
     * @return The current time or cached time in ticks.
     */
    inline TimeType getEventTime () const
    {
        return callImpl<TimeType()>(&Impl::getEventTime);
    }
    
    /**
     * Determine if the first time value is greater than or equal to the second.
     * 
     * This is eqivalent to (@ref TimeType)(t1 - t2) \< @ref TimeMSB (casting the
     * difference ensures modulo reduction).
     * 
     * @param t1 The first time to be compared.
     * @param t2 The second time to be compaed.
     * @return Whether t1 is considered greater than or equal to t2.
     */
    inline static bool timeGreaterOrEqual (TimeType t1, TimeType t2)
    {
        return (TimeType)(t1 - t2) < TimeMSB;
    }
    
    /**
     * A trivial wrapper around @ref PlatformRef.
     * 
     * This is designed to be inherited when a reference to the platform needs
     * to be stored. Note that inheriting it as opposed to declaring it as a member
     * saves memory when the platform implementation is static, due to the empty-base
     * optimization.
     * 
     * This class inherits @ref PlatformRef, so the @ref PlatformFacade can be
     * obtained using @ref PlatformRef::platform.
     * 
     * @tparam Derived The type of the class inheriting this class or anything
     *         which will prevent base class ambiguity problems. This type is not
     *         used by this class in any way.
     */
    template <typename Derived>
    class RefWrapper :
        public Ref
    {
    public:
        /**
         * Constructor.
         * 
         * @param platform The @ref PlatformFacade object from which the
         *        @ref PlatformRef object to be stored will be obtained.
         */
        inline RefWrapper (PlatformFacade platform) :
            Ref(platform)
        {
        }
    };
    
    /**
     * Provides notification when the clock reaches a specific time.
     * 
     * See @ref PlatformImplStub::Timer for details. This is a trivial wrapper
     * around that class.
     * 
     * Note that directly using this class can often be problematic due to base
     * class ambiguity and the need to override virtual functions differently for
     * different timers in the same class. The class TimerWrapper and its
     * associated macros can be used to work around such problems.
     */
    class Timer :
        private NonCopyable<Timer>,
        private Impl::Timer
    {
        using ImplTimer = typename Impl::Timer;
        
    public:
        /**
         * Construct the timer.
         * 
         * See @ref PlatformImplStub::Timer::Timer for details.
         * 
         * The only difference is that this constructor accepts a @ref PlatformFacade
         * instead of a @ref PlatformRef. This function obtains the @ref PlatformRef
         * by calling @ref PlatformRef::ref on the given 'platform' and passes that
         * to the @ref PlatformImplStub::Timer::Timer constructor.
         * 
         * @param platform The platform facade.
         */
        inline Timer (PlatformFacade platform) :
            ImplTimer(platform.ref())
        {
        }
        
        /**
         * Return the platform facade.
         * 
         * This function calls @ref PlatformImplStub::Timer::ref to obtain the
         * @ref PlatformRef stored in the timer, calls @ref PlatformRef::platform on
         * that and returns the result.
         * 
         * @return The platform facade.
         */
        inline PlatformFacade platform () const
        {
            Ref ref = ImplTimer::ref();
            return ref.platform();
        }
        
        /**
         * Return a reference to the wrapped timer implementation class,
         * corresponding to @ref PlatformImplStub::Timer.
         * 
         * This is never used by the stack but may be useful for application code
         * which uses the platform interface.
         * 
         * @return A reference to the wrapped timer implementation class.
         */
        inline ImplTimer & impl ()
        {
            return static_cast<ImplTimer &>(*this);
        }
        
        /**
         * Return whether the timer is set.
         * 
         * See @ref PlatformImplStub::Timer::isSet for details.
         * 
         * @return Whether the timer is set.
         */
        inline bool isSet () const
        {
            return callObj<ImplTimer, bool()const>(&ImplTimer::isSet, *this);
        }
        
        /**
         * Return the last time that the timer was set to.
         * 
         * See @ref PlatformImplStub::Timer::getSetTime for details.
         * 
         * @return The last time the timer was set to.
         */
        inline TimeType getSetTime () const
        {
            return callObj<ImplTimer, TimeType()const>(&ImplTimer::getSetTime, *this);
        }
        
        /**
         * Unset the timer if it is set.
         * 
         * See @ref PlatformImplStub::Timer::unset for details.
         */
        inline void unset ()
        {
            return callObj<ImplTimer, void()>(&ImplTimer::unset, *this);
        }
        
        /**
         * Set the timer to expire at the given time.
         * 
         * See @ref PlatformImplStub::Timer::setAt for details.
         * 
         * @param abs_time Absolute expiration time.
         */
        inline void setAt (TimeType abs_time)
        {
            return callObj<ImplTimer, void(TimeType)>(&ImplTimer::setAt, *this, abs_time);
        }
        
        /**
         * Set the timer to expire after the given relative time.
         * 
         * This is equivalent to @ref setAt((@ref TimeType)(@ref getTime() + rel_time)).
         * Note that casting the sum ensures modulo reduction.
         * 
         * @param rel_time Relative expiration time.
         */
        void setAfter (TimeType rel_time)
        {
            TimeType abs_time = platform().getTime() + rel_time;
            return setAt(abs_time);
        }
        
        /**
         * Set the timer to expire now.
         * 
         * This is equivalent to @ref setAfter ""(0).
         */
        inline void setNow ()
        {
            return setAfter(0);
        }
        
    protected:
        /**
         * Callback used to report the expiration of the timer.
         * 
         * See @ref PlatformImplStub::Timer::handleTimerExpired for details.
         */
        virtual void handleTimerExpired () override = 0;
    };
    
private:
    template <typename Func>
    using RetType = GetReturnType<Func>;
    
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
    
    template <typename Obj, typename Func, typename... Args>
    inline static RetType<Func> callObj (
        Func Obj::*func_ptr, Obj const &obj, Args && ... args)
    {
        return (obj.*func_ptr)(std::forward<Args>(args)...);
    }
};

}

#endif
