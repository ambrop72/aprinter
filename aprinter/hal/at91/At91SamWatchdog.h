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

#ifndef AMBROLIB_AT91SAM_WATCHDOG_H
#define AMBROLIB_AT91SAM_WATCHDOG_H

#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Hints.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class At91SamWatchdog {
    APRINTER_USE_TYPES1(Arg, (Context, ParentObject, Params))
    APRINTER_USE_VALS(Arg, (DebugMode))
    
    static_assert(Params::Wdv <= 0xFFF, "");
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static constexpr double WatchdogTime = Params::Wdv / (F_SCLK / 128.0);
    
    static void init (Context c)
    {
        TheDebugObject::init(c);
        
        WDT->WDT_MR = WDT_MR_WDV(Params::Wdv) | WDT_MR_WDD(Params::Wdv) | WDT_MR_WDRSTEN |
            (DebugMode ? WDT_MR_WDDBGHLT : 0);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    template <typename ThisContext>
    static void reset (ThisContext c)
    {
        TheDebugObject::access(c);
        
        WDT->WDT_CR = WDT_CR_KEY(0xA5) | WDT_CR_WDRSTT;
    }
    
    template <typename TheCommand>
    static bool check_command (Context c, TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == 947) {
            uint8_t rst_type = (RSTC->RSTC_SR >> RSTC_SR_RSTTYP_Pos) & RSTC_SR_RSTTYP_Msk;
            
            char const *rst_type_str = "";
            switch (rst_type) {
                case 0: rst_type_str = "General";  break;
                case 1: rst_type_str = "Backup";   break;
                case 2: rst_type_str = "Watchdog"; break;
                case 3: rst_type_str = "Software"; break;
                case 4: rst_type_str = "User";     break;
            }
            
            cmd->reply_append_str(c, "ResetType=");
            cmd->reply_append_str(c, rst_type_str);
            cmd->reply_append_ch(c, '\n');
            cmd->finishCommand(c);
            
            return false;
        }
        
        return true;
    }
    
    APRINTER_NO_RETURN
    static void emergency_abort ()
    {
        // This is called after a fatal error has been detected. We spin here
        // doing either nothing or resetting the watchdog if debug mode is enabled.
        while (true) {
            if (DebugMode) {
                WDT->WDT_CR = WDT_CR_KEY(0xA5) | WDT_CR_WDRSTT;
            }
        }
    }
    
public:
    struct Object : public ObjBase<At91SamWatchdog, ParentObject, MakeTypeList<TheDebugObject>> {};
};

APRINTER_ALIAS_STRUCT_EXT(At91SamWatchdogService, (
    APRINTER_AS_VALUE(uint32_t, Wdv)
), (
    APRINTER_ALIAS_STRUCT_EXT(Watchdog, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_VALUE(bool, DebugMode)
    ), (
        using Params = At91SamWatchdogService;
        APRINTER_DEF_INSTANCE(Watchdog, At91SamWatchdog)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
