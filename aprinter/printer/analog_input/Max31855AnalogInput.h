/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_MAX_31855_ANALOG_INPUT_H
#define APRINTER_MAX_31855_ANALOG_INPUT_H

#include <stdint.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Params>
class Max31855AnalogInput {
public:
    struct Object;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    using TheClockUtils = ClockUtils<Context>;
    
    static int const SpiMaxCommands = 2;
    static int const SpiCommandBits = BitsInInt<SpiMaxCommands>::Value;
    struct SpiHandler;
    struct SpiArg : public Params::SpiService::template Spi<Context, Object, SpiHandler, SpiCommandBits> {};
    using TheSpi = typename SpiArg::template Instance<SpiArg>;
    
    static TimeType const ReadDelayTicks = 0.05 * Context::Clock::time_freq;
    static TimeType const SafetyDeadlineTicks = 0.5 * Context::Clock::time_freq;
    
public:
    static bool const IsRounded = true;
    
    using FixedType = FixedPoint<14, false, -14>;
    
    static bool isValueInvalid (FixedType value)
    {
        return value.bitsValue() == 0;
    }
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        Context::Pins::template set<typename Params::SsPin>(c, true);
        Context::Pins::template setOutput<typename Params::SsPin>(c);
        
        TheSpi::init(c);
        o->m_timer.init(c, APRINTER_CB_STATFUNC_T(&Max31855AnalogInput::timer_handler));
        o->m_value = FixedType::importBits(0);
        o->m_timer.appendAt(c, Context::Clock::getTime(c));
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_timer.deinit(c);
        TheSpi::deinit();
        
        Context::Pins::template set<typename Params::SsPin>(c, true);
    }
    
    static FixedType getValue (Context c)
    {
        auto *o = Object::self(c);
        return o->m_value;
    }
    
    static void check_safety (Context c)
    {
        auto *o = Object::self(c);
        
        if (have_timing_error(c)) {
            o->m_value = FixedType::importBits(0);
        }
    }
    
    using GetSpi = TheSpi;
    
private:
    static bool have_timing_error (Context c)
    {
        auto *o = Object::self(c);
        
        // Check for stall of SPI transactions.
        // If the current time is not in the interval [low_limit, high_limit) defined below around
        // the schuled time for a transaction, assume an invalid value.
        
        TimeType low_limit = o->m_timer.getSetTime(c) - ReadDelayTicks;
        TimeType high_limit = o->m_timer.getSetTime(c) + SafetyDeadlineTicks;
        TimeType now = Context::Clock::getTime(c);
        
        return (!TheClockUtils::timeGreaterOrEqual(now, low_limit) || TheClockUtils::timeGreaterOrEqual(now, high_limit));
    }
    
    static void timer_handler (Context c)
    {
        auto *o = Object::self(c);
        
        Context::Pins::template set<typename Params::SsPin>(c, false);
        TheSpi::cmdReadBuffer(c, o->m_buffer, 4, 0);
    }
    
    static void spi_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(TheSpi::endReached(c))
        
        Context::Pins::template set<typename Params::SsPin>(c, true);
        
        TimeType set_time = Context::Clock::getTime(c) + ReadDelayTicks;
        o->m_timer.appendAt(c, set_time);
        
        uint16_t value;
        uint32_t reading = ReadBinaryInt<uint32_t, BinaryBigEndian>((char *)o->m_buffer);
        bool have_fault = (reading & UINT32_C(0x3000F)) != 0;
        if (have_fault || have_timing_error(c)) {
            value = 0;
        } else {
            uint16_t temp_value = reading >> 18;
            if (temp_value < UINT16_C(8192)) {
                value = temp_value + UINT16_C(8192);
            } else {
                value = temp_value - UINT16_C(8192);
            }
        }
        o->m_value = FixedType::importBits(value);
    }
    struct SpiHandler : public AMBRO_WFUNC_TD(&Max31855AnalogInput::spi_handler) {};
    
public:
    struct Object : public ObjBase<Max31855AnalogInput, ParentObject, MakeTypeList<
        TheSpi
    >> {
        typename Context::EventLoop::TimedEvent m_timer;
        FixedType m_value;
        uint8_t m_buffer[4];
    };
};

APRINTER_ALIAS_STRUCT_EXT(Max31855AnalogInputService, (
    APRINTER_AS_TYPE(SsPin),
    APRINTER_AS_TYPE(SpiService)
), (
    template <typename Context, typename ParentObject>
    using AnalogInput = Max31855AnalogInput<Context, ParentObject, Max31855AnalogInputService>;
))

#include <aprinter/EndNamespace.h>

#endif
