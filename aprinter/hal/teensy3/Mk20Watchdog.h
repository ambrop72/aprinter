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

#ifndef AMBROLIB_MK20_WATCHDOG_H
#define AMBROLIB_MK20_WATCHDOG_H

#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Hints.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class Mk20Watchdog {
    APRINTER_USE_TYPES1(Arg, (Context, ParentObject, Params))
    
public:
    struct Object;
    
private:
    static_assert(Params::Toval >= 4, "");
    static_assert(Params::Prescval < 8, "");
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static constexpr double WatchdogTime = Params::Toval / (1000.0 / (Params::Prescval + 1));
    
    static void init (Context c)
    {
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    template <typename ThisContext>
    static void reset (ThisContext c)
    {
        TheDebugObject::access(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            // Need to use assembly to do this without any delay in
            // between stores introduced by the compiler.
#if 0
            WDOG_REFRESH = UINT16_C(0xA602);
            WDOG_REFRESH = UINT16_C(0xB480);
#else
            asm volatile (
                "strh %[magic1], [%[wdog_refresh]]\n"
                "strh %[magic2], [%[wdog_refresh]]\n"
                :
                : [wdog_refresh] "r" (&WDOG_REFRESH),
                  [magic1] "r" (UINT16_C(0xA602)),
                  [magic2] "r" (UINT16_C(0xB480))
            );
#endif
        }
    }
    
    APRINTER_NO_RETURN
    static void emergency_abort ()
    {
        while (true);
    }
    
public:
    struct Object : public ObjBase<Mk20Watchdog, ParentObject, MakeTypeList<TheDebugObject>> {};
};

APRINTER_ALIAS_STRUCT_EXT(Mk20WatchdogService, (
    APRINTER_AS_VALUE(uint32_t, Toval),
    APRINTER_AS_VALUE(uint8_t, Prescval)
), (
    APRINTER_ALIAS_STRUCT_EXT(Watchdog, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_VALUE(bool, DebugMode)
    ), (
        using Params = Mk20WatchdogService;
        APRINTER_DEF_INSTANCE(Watchdog, Mk20Watchdog)
    ))
))

#define AMBRO_MK20_WATCHDOG_GLOBAL(watchdog) \
extern "C" \
__attribute__((used)) \
void startup_early_hook (void) \
{ \
    using TheWatchdog = watchdog; \
    asm volatile ("nop"); \
    asm volatile ("nop"); \
    asm volatile ("nop"); \
    asm volatile ("nop"); \
    WDOG_TOVALH = TheWatchdog::Params::Toval >> 16; \
    WDOG_TOVALL = TheWatchdog::Params::Toval; \
    WDOG_PRESC = (uint32_t)TheWatchdog::Params::Prescval << 8; \
    WDOG_STCTRLH = WDOG_STCTRLH_WDOGEN | WDOG_STCTRLH_ALLOWUPDATE; \
}

#include <aprinter/EndNamespace.h>

#endif
