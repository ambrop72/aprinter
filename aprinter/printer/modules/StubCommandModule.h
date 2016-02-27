/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_STUB_COMMAND_MODULE_H
#define APRINTER_STUB_COMMAND_MODULE_H

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/ProgramMemory.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class StubCommandModule {
public:
    static bool check_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        switch (cmd->getCmdNumber(c)) {
            case 80:   // ATX power on
            case 81: { // ATX power off
                cmd->finishCommand(c);
            } break;
            
            case 115: {
                cmd->reply_append_pstr(c, AMBRO_PSTR("ok FIRMWARE_NAME:APrinter\n"));
                cmd->finishCommand(c, true);
            } break;
            
            default:
                return true;
        }
        
        return false;
    }
    
    static bool check_g_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        switch (cmd->getCmdNumber(c)) {
            case 21: { // set units to millimeters
                cmd->finishCommand(c);
            } break;
            
            default:
                return true;
        }
        
        return false;
    }
    
public:
    struct Object {};
};

struct StubCommandModuleService {
    APRINTER_MODULE_TEMPLATE(StubCommandModuleService, StubCommandModule)
};

#include <aprinter/EndNamespace.h>

#endif
