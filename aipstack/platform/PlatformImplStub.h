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

#ifndef AIPSTACK_PLATFORM_IMPL_STUB_H
#define AIPSTACK_PLATFORM_IMPL_STUB_H

#include <stdint.h>

#include <aprinter/base/NonCopyable.h>

#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

/**
 * Stub platform implementation documenting the required interface.
 * 
 * This documents the platform implementation required by the stack. A type
 * implementing this interface is used as the PlatformImpl template parameter
 * to @ref PlatformFacade, which is only a thin wrapper around the PlatformImpl.
 */
class PlatformImplStub
{
public:
    /**
     * Type alias for the @ref PlatformRef type using this PlatformImpl.
     * 
     * The @ref PlatformRef type represents a reference to the platform; see
     * the explanation in @ref ImplIsStatic.
     * 
     * This type alias is not required but is used in declarations.
     */
    using ThePlatformRef = AIpStack::PlatformRef<PlatformImplStub>;
    
    /**
     * Defines whether the platform implementation is static.
     * 
     * If this is true, then the required functions in this class must be static,
     * and @ref PlatformRef will be an empty class.
     * 
     * If this is false, then the required functions may be static or non-static,
     * and @ref PlatformRef will contain a pointer to the platform implementation
     * class (e.g. to this class).
     * 
     * Using a static implementation is generally more efficient, as pointers to
     * the platform implementation class are not stored in various places in the
     * stack and not dereferenced when accessing the platform implementation.
     * However, a static implementation generally implies that global variables
     * need to be used in the implementation and that multiple instances of the
     * stack cannot be exist at the same time (except perhaps with the use of
     * thread-local-variables). A non-static implementation is less efficient but
     * more flexibile in this respect.
     */
    static bool const ImplIsStatic = false;
    
    /**
     * Defines the unsigned integer type representing time in platform-defined units.
     * 
     * This must be an alias for an unsigned integer type at least 32 bits wide
     * which is used to represent time in units (i.e. "ticks") the inverse of
     * @ref TimeFreq.
     * 
     * In this interface this type usually represents absolute time (a point in time),
     * but generally it can also represent relative time especially in computations
     * involving time.
     * 
     * The functions @ref getTime and @ref getEventTime provide a clock based on this
     * type. It is assumed that the clock wraps around to zero after reaching the maximum
     * value representable in TimeType.
     * 
     * The interpretation of absolute times is relative. Let "Now" be the current time
     * and "ClockPeriodTicks" be the maximum value representable in TimeType plus 1.
     * Then, the values [Now, Now+ClockPeriodTicks/2) are understood to be in the
     * present and the future and the values [Now-ClockPeriodTicks/2, Now) are understood
     * to be in the past (addition and subtraction are modulo ClockPeriodTicks). This
     * is important for the implementation of @ref Timer.
     * 
     * In practice the stack will not set a @ref Timer to expire more than
     * 7/16*ClockPeriodTicks in the future or in the past. This tolerance of
     * 1/16*ClockPeriodTicks is to account for event processing latencies.
     */
    using TimeType = uint64_t;
    
    /**
     * Defines the frequency of the clock in Hz.
     * 
     * This indirectly defines the duration of one tick as used in @ref TimeType,
     * that is the inverse of TimeFreq. The frequency must be at least 100 Hz, that is
     * the resolution of @ref TimeType must be at least 10 ms.
     * 
     * Additionally, the stacks requires that it is able to set a @ref Timer to expire
     * at least 600 s in the future or past. Together with the tolerance as described in
     * @ref TimeType, this means that (7/16*ClockPeriodTicks)/TimeFreq >= 600 must hold,
     * where ClockPeriodTicks is the maximum value representable in @ref TimeType.
     */
    static constexpr double TimeFreq = 1000.0;
    
    /**
     * Get the current time in ticks.
     * 
     * This function must implement a clock as described in @ref TimeType and
     * @ref TimeFreq.
     * 
     * This function must never throw an exception.
     * 
     * @return The current time in ticks.
     */
    TimeType getTime ()
    {
        return 1234;
    }
    
    /**
     * Get a time in ticks that is not significantly earlier than the current time.
     * 
     * This function is like @ref getTime, except that it may return a cached value
     * taken at the start of an event loop iteration or other time which is not
     * significantly earlier than the actual time of this call. 
     * 
     * This function must never throw an exception.
     * 
     * @return The current time or cached time in ticks.
     */
    TimeType getEventTime ()
    {
        return 1234;
    }
    
    /**
     * Provides notification when the clock reaches a specific time.
     * 
     * This allows the user to be notified when the clock (as defined by @ref TimeType
     * and @ref getTime) reaches a specific time.
     * 
     * A timer conceptually has two main states: not-set and set. The not-set state
     * is the default when the timer is constructed and is an inactive state where
     * the timer does not expire. In the set state, the timer has an associated time
     * which it is set to expire at. Expiration is reported via the virtual function
     * @ref handleTimerExpired; the timer automatically changes its state to not-set
     * just before calling this function.
     * 
     * The implementation must not place any limit on the number of timers used and
     * functions in this class must never throw exceptions. This can clearly be
     * achieved with an implementation that collects timers in an intrusive data
     * structure.
     * 
     * The stack will never copy or move timers, so it is advised to disable such
     * operations, perhaps by inheriting @ref APrinter::NonCopyable.
     */
    class Timer :
        private ThePlatformRef,
        private APrinter::NonCopyable<Timer>
    {
    private:
        bool m_is_set;
        TimeType m_set_time;
        
    public:
        /**
         * Construct the timer.
         * 
         * Upon construction the timer must be in the not-set state.
         * 
         * @param ref Platform reference; see @ref ThePlatformRef and @ref ImplIsStatic.
         *        The timer is assumed to store the platform reference and must expose
         *        it using the @ref ref function.
         */
        Timer (ThePlatformRef ref) :
            ThePlatformRef(ref),
            m_is_set(false)
        {
        }
        
        /**
         * Destruct the timer.
         * 
         * A timer may be destructed in any state. There must be no restrictions on
         * the context the timer is destructed from. For example, destructing the timer
         * from its own @ref handleTimerExpired callback or from the callback of another
         * timer must be supported by the implementation.
         */
        ~Timer ()
        {
        }
        
        /**
         * Return the platform reference.
         * 
         * The simplest way to implement this function is to publicly inherit
         * @ref ThePlatformRef and initialize it in the constructor from the 'ref'
         * argument. If this is done, this function does not even need to be declared
         * as the inherited @ref PlatformRef::ref implementation is suitable.
         * 
         * @return The platform reference.
         */
        inline ThePlatformRef ref () const
        {
            return ThePlatformRef::ref();
        }
        
        /**
         * Return whether the timer is set.
         * 
         * @return True if the timer is in set state, false if it is in not-set state.
         */
        inline bool isSet () const
        {
            return m_is_set;
        }
        
        /**
         * Return the last time that the timer was set to.
         * 
         * This must return the exact value of the 'abs_time' argument of the most
         * recent @ref setAt call. This function must not and will not be called
         * if @ref setAt has never been called for this timer.
         * 
         * @return The last time the timer was set to.
         */
        inline TimeType getSetTime () const
        {
            return m_set_time;
        }
        
        /**
         * Unset the timer if it is set.
         * 
         * This must bring the timer to the not-set state (if not already).
         */
        void unset ()
        {
            m_is_set = false;
        }
        
        /**
         * Set the timer to expire at the given time.
         * 
         * This must bring the timer to the set state (if not already) and set
         * its expiration time to the given absolute time (regardless of whether
         * the timer was already set).
         * 
         * Note that setting the timer to expire in the past or at the current time
         * is normal and must be supported; in this case @ref handleTimerExpired
         * should be called as soon as possible (but not from this function).
         * 
         * @param abs_time Absolute expiration time. See @ref TimeType for an
         *        explanation of how this is to be interpreted.
         */
        void setAt (TimeType abs_time)
        {
            m_is_set = true;
            m_set_time = abs_time;
        }
        
    protected:
        /**
         * Callback used to report the expiration of the timer.
         * 
         * This must be called in set state after the clock reaches the time that
         * the timer has been set to expire at. The timer must be transitioned to
         * the not-set state just before this function is called.
         * 
         * This must not be called when the timer is already in not-set state.
         */
        virtual void handleTimerExpired () = 0;
    };
};

}

#endif
