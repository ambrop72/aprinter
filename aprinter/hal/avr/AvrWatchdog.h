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

#ifndef AMBROLIB_AVR_WATCHDOG_H
#define AMBROLIB_AVR_WATCHDOG_H

#include <avr/io.h>
#include <avr/wdt.h>

#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Hints.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class AvrWatchdog {
    APRINTER_USE_TYPES1(Arg, (Context, ParentObject, Params))
    
    static_assert(Params::WatchdogPrescaler >= 0, "");
    static_assert(Params::WatchdogPrescaler < 10, "");
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static constexpr double WatchdogTime = PowerOfTwoFunc<double>(11 + Params::WatchdogPrescaler) / 131072.0;
    
    static void init (Context c)
    {
        wdt_enable(Params::WatchdogPrescaler);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        wdt_disable();
    }
    
    template <typename ThisContext>
    static void reset (ThisContext c)
    {
        TheDebugObject::access(c);
        
        wdt_reset();
    }
    
    APRINTER_NO_RETURN
    static void emergency_abort ()
    {
        while (true);
    }
    
public:
    struct Object : public ObjBase<AvrWatchdog, ParentObject, MakeTypeList<TheDebugObject>> {};
};

APRINTER_ALIAS_STRUCT_EXT(AvrWatchdogService, (
    APRINTER_AS_VALUE(int, WatchdogPrescaler)
), (
    APRINTER_ALIAS_STRUCT_EXT(Watchdog, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_VALUE(bool, DebugMode)
    ), (
        using Params = AvrWatchdogService;
        APRINTER_DEF_INSTANCE(Watchdog, AvrWatchdog)
    ))
))

#define AMBRO_AVR_WATCHDOG_GLOBAL \
void clear_mcusr () __attribute__((naked)) __attribute__((section("init3"))); \
void clear_mcusr () \
{ \
    MCUSR = 0; \
    wdt_disable(); \
}

#include <aprinter/EndNamespace.h>

#endif
