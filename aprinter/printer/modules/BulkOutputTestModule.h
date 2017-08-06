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

#ifndef APRINTER_BULK_OUTPUT_TEST_MODULE_H
#define APRINTER_BULK_OUTPUT_TEST_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/printer/utils/ModuleUtils.h>

namespace APrinter {

#define APRINTER_BULKOUTPUT_TEST_DATA "0123456789ABCDEF"

template <typename ModuleArg>
class BulkOutputTestModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
    static size_t const TestDataLength = sizeof(APRINTER_BULKOUTPUT_TEST_DATA) - 1;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->chunk_count = 0;
    }
    
    static bool check_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == 942) {
            handle_test_bulk_output_command(c, cmd);
            return false;
        }
        if (cmd->getCmdNumber(c) == 943) {
            handle_message_test_command(c, cmd);
            return false;
        }
        return true;
    }
    
private:
    static void handle_test_bulk_output_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->chunk_count == 0)
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        
        uint32_t chunk_length = cmd->get_command_param_uint32(c, 'L', 32);
        if (!(chunk_length > 0 && chunk_length < SIZE_MAX)) {
            cmd->reportError(c, AMBRO_PSTR("BadChunkLength"));
            return cmd->finishCommand(c);
        }
        o->chunk_length = chunk_length;
        
        o->chunk_count = cmd->get_command_param_uint32(c, 'C', 64);
        
        return next_chunk(c);
    }
    
    static void handle_message_test_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        uint32_t msg_length = cmd->get_command_param_uint32(c, 'L', 32);
        uint32_t msg_count = cmd->get_command_param_uint32(c, 'C', 1);
        
        for (auto i : LoopRange<uint32_t>(msg_count)) {
            auto *out = ThePrinterMain::get_msg_output(c);
            
            uint32_t remain = msg_length;
            while (remain > 0) {
                size_t chunk_len = MinValue(remain, (uint32_t)TestDataLength);
                out->reply_append_pbuffer(c, AMBRO_PSTR(APRINTER_BULKOUTPUT_TEST_DATA), chunk_len);
                remain -= chunk_len;
            }
            
            out->reply_append_ch(c, '\n');
            out->reply_poke(c);
        }
        
        cmd->finishCommand(c);
    }
    
    static void next_chunk (Context c)
    {
        auto *o = Object::self(c);
        
        auto *cmd = ThePrinterMain::get_locked(c);
        
        if (o->chunk_count == 0) {
            return cmd->finishCommand(c);
        }
        
        if (!cmd->requestSendBufEvent(c, o->chunk_length + 1, &BulkOutputTestModule::send_buf_event_handler)) {
            cmd->reportError(c, AMBRO_PSTR("SendBufRequestFailed"));
            o->chunk_count = 0;
            return cmd->finishCommand(c);
        }
    }
    
    static void send_buf_event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->chunk_count > 0)
        
        auto *cmd = ThePrinterMain::get_locked(c);
        
        size_t remain = o->chunk_length;
        while (remain > 0) {
            size_t write_length = MinValue(remain, TestDataLength);
            cmd->reply_append_pbuffer(c, AMBRO_PSTR(APRINTER_BULKOUTPUT_TEST_DATA), write_length);
            remain -= write_length;
        }
        
        cmd->reply_append_ch(c, '\n');
        cmd->reply_poke(c);
        
        o->chunk_count--;
        
        return next_chunk(c);
    }
    
public:
    struct Object : public ObjBase<BulkOutputTestModule, ParentObject, EmptyTypeList> {
        size_t chunk_length;
        uint32_t chunk_count;
    };
};

struct BulkOutputTestModuleService {
    APRINTER_MODULE_TEMPLATE(BulkOutputTestModuleService, BulkOutputTestModule)
};

}

#endif
