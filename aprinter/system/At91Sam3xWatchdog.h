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

#ifndef AMBROLIB_AT91SAM3X_WATCHDOG_H
#define AMBROLIB_AT91SAM3X_WATCHDOG_H

#include <math.h>

#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

template <uint32_t TWdv>
struct At91Sam3xWatchdogParams {
    static const uint32_t Wdv = TWdv;
};

template <typename Position, typename Context, typename Params>
class At91Sam3xWatchdog : private DebugObject<Context, void>
{
    static_assert(Params::Wdv <= 0xFFF, "");
    
    static At91Sam3xWatchdog * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    static constexpr double WatchdogTime = Params::Wdv / (F_SCLK / 128.0);
    
    static void init (Context c)
    {
        At91Sam3xWatchdog *o = self(c);
        o->debugInit(c);
        
        WDT->WDT_MR = WDT_MR_WDV(Params::Wdv) | WDT_MR_WDRSTEN | WDT_MR_WDD(Params::Wdv);
    }
    
    static void deinit (Context c)
    {
        At91Sam3xWatchdog *o = self(c);
        o->debugDeinit(c);
    }
    
    template <typename ThisContext>
    static void reset (ThisContext c)
    {
        At91Sam3xWatchdog *o = self(c);
        o->debugAccess(c);
        
        WDT->WDT_CR = WDT_CR_KEY(0xA5) | WDT_CR_WDRSTT;
    }
};

#include <aprinter/EndNamespace.h>

#endif
