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

#include <math.h>

#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

template <uint32_t TToval, uint8_t TPrescval>
struct Mk20WatchdogParams {
    static const uint32_t Toval = TToval;
    static const uint32_t Prescval = TPrescval;
};

template <typename Position, typename Context, typename Params>
class Mk20Watchdog : private DebugObject<Context, void>
{
    static_assert(Params::Toval >= 4, "");
    static_assert(Params::Prescval < 8, "");
    
    static Mk20Watchdog * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    static constexpr double WatchdogTime = Params::Toval / (1000.0 / (Params::Prescval + 1));
    
    static void init (Context c)
    {
        Mk20Watchdog *o = self(c);
        o->debugInit(c);
        
        WDOG_UNLOCK = WDOG_UNLOCK_SEQ1;
        WDOG_UNLOCK = WDOG_UNLOCK_SEQ2;
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        WDOG_TOVALH = Params::Toval >> 16;
        WDOG_TOVALL = Params::Toval;
        WDOG_PRESC = Params::Prescval << 8;
        WDOG_STCTRLH = WDOG_STCTRLH_WDOGEN | WDOG_STCTRLH_ALLOWUPDATE;
    }
    
    static void deinit (Context c)
    {
        Mk20Watchdog *o = self(c);
        o->debugDeinit(c);
    }
    
    template <typename ThisContext>
    static void reset (ThisContext c)
    {
        Mk20Watchdog *o = self(c);
        o->debugAccess(c);
        
        WDOG_REFRESH = UINT16_C(0xA602);
        WDOG_REFRESH = UINT16_C(0xB480);
    }
};

#include <aprinter/EndNamespace.h>

#endif
