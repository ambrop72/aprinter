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

#include <aprinter/meta/Object.h>
#include <aprinter/meta/RemoveReference.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <uint32_t TToval, uint8_t TPrescval>
struct Mk20WatchdogParams {
    static const uint32_t Toval = TToval;
    static const uint8_t Prescval = TPrescval;
};

template <typename Context, typename ParentObject, typename TParams>
class Mk20Watchdog {
public:
    using Params = TParams;
    
private:
    static_assert(Params::Toval >= 4, "");
    static_assert(Params::Prescval < 8, "");
    
public:
    struct Object;
    static constexpr double WatchdogTime = Params::Toval / (1000.0 / (Params::Prescval + 1));
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
    }
    
    template <typename ThisContext>
    static void reset (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(AtomicTempLock(), c, lock_c) {
            WDOG_REFRESH = UINT16_C(0xA602);
            WDOG_REFRESH = UINT16_C(0xB480);
        }
    }
    
public:
    struct Object : public ObjBase<Mk20Watchdog, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {};
};

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
