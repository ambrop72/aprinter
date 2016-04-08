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

#ifndef APRINTER_MILLISECOND_CLOCK_INFO_MODULE_H
#define APRINTER_MILLISECOND_CLOCK_INFO_MODULE_H

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename ModuleArg>
class MillisecondClockInfoModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    static bool check_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == 938) {
            cmd->reply_append_uint32(c, Context::MillisecondClock::getTime(c));
            cmd->reply_append_ch(c, '\n');
            cmd->finishCommand(c);
            return false;
        }
        return true;
    }
    
    struct Object {};
};

struct MillisecondClockInfoModuleService {
    APRINTER_MODULE_TEMPLATE(MillisecondClockInfoModuleService, MillisecondClockInfoModule)
};

#include <aprinter/EndNamespace.h>

#endif
