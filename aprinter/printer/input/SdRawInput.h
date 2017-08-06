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

#ifndef AMBROLIB_SDRAW_INPUT_H
#define AMBROLIB_SDRAW_INPUT_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/TransferVector.h>

namespace APrinter {

template <typename Arg>
class SdRawInput {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using ClientParams = typename Arg::ClientParams;
    using Params       = typename Arg::Params;
    
public:
    struct Object;
    
private:
    using ThePrinterMain = typename ClientParams::ThePrinterMain;
    using TheDebugObject = DebugObject<Context, Object>;
    struct SdCardInitHandler;
    struct SdCardCommandHandler;
    APRINTER_MAKE_INSTANCE(TheSdCard, (Params::SdCardService::template SdCard<Context, Object, SdCardInitHandler, SdCardCommandHandler>))
    static size_t const BlockSize = 512;
    enum {STATE_INACTIVE, STATE_ACTIVATING, STATE_PAUSED, STATE_READY, STATE_READING};
    
public:
    static size_t const ReadBlockSize = BlockSize;
    using DataWordType = typename TheSdCard::DataWordType;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheSdCard::init(c);
        o->state = STATE_INACTIVE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        TheSdCard::deinit(c);
    }
    
    static bool startingIo (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state <= STATE_PAUSED)
        
        if (!check_file_paused(c, cmd)) {
            return false;
        }
        o->state = STATE_READY;
        return true;
    }
    
    static void pausingIo (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY)
        
        o->state = STATE_PAUSED;
    }
    
    static bool rewind (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state <= STATE_PAUSED)
        
        if (!check_file_paused(c, cmd)) {
            return false;
        }
        o->block = 0;
        ClientParams::ClearBufferHandler::call(c);
        return true;
    }
    
    static bool eofReached (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_READING)
        
        return (o->block >= TheSdCard::getCapacityBlocks(c));
    }
    
    static bool canRead (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY)
        
        return (o->block < TheSdCard::getCapacityBlocks(c));
    }
    
    static void startRead (Context c, DataWordType *buf)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY)
        AMBRO_ASSERT(o->block < TheSdCard::getCapacityBlocks(c))
        
        o->desc = TransferDescriptor<DataWordType>{buf, BlockSize/sizeof(DataWordType)};
        TheSdCard::startReadOrWrite(c, false, o->block, 1, TransferVector<DataWordType>{&o->desc, 1});
        o->state = STATE_READING;
    }
    
    static bool checkCommand (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        TheDebugObject::access(c);
        
        auto cmd_num = cmd->getCmdNumber(c);
        if (cmd_num == 21) {
            handle_mount_command(c, cmd);
            return false;
        }
        if (cmd_num == 22) {
            handle_unmount_command(c, cmd);
            return false;
        }
        return true;
    }
    
    template <typename TheJsonBuilder>
    static void get_json_status (Context c, TheJsonBuilder *json)
    {
    }
    
    using GetSdCard = TheSdCard;
    
private:
    static void handle_mount_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        if (o->state != STATE_INACTIVE) {
            cmd->reportError(c, AMBRO_PSTR("SdAlreadyInited"));
            cmd->finishCommand(c);
            return;
        }
        TheSdCard::activate(c);
        o->state = STATE_ACTIVATING;
    }
    
    static void sd_card_init_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_ACTIVATING)
        
        if (error_code) {
            o->state = STATE_INACTIVE;
        } else {
            o->state = STATE_PAUSED;
            o->block = 0;
        }
        
        typename ThePrinterMain::TheCommand *cmd = ThePrinterMain::get_locked(c);
        if (error_code) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("SD error "));
            cmd->reply_append_uint32(c, error_code);
        } else {
            cmd->reply_append_pstr(c, AMBRO_PSTR("SD mounted"));
        }
        cmd->reply_append_ch(c, '\n');
        cmd->finishCommand(c);
    }
    struct SdCardInitHandler : public AMBRO_WFUNC_TD(&SdRawInput::sd_card_init_handler) {};
    
    static void handle_unmount_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        do {
            if (o->state < STATE_PAUSED) {
                cmd->reportError(c, AMBRO_PSTR("SdNotInited"));
                break;
            }
            if (o->state > STATE_PAUSED) {
                cmd->reportError(c, AMBRO_PSTR("SdPrintRunning"));
                break;
            }
            TheSdCard::deactivate(c);
            o->state = STATE_INACTIVE;
            ClientParams::ClearBufferHandler::call(c);
            cmd->reply_append_pstr(c, AMBRO_PSTR("SD unmounted\n"));
        } while (false);
        cmd->finishCommand(c);
    }
    
    static void sd_card_command_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READING)
        
        o->state = STATE_READY;
        size_t bytes = 0;
        if (!error) {
            bytes = BlockSize;
            o->block++;
        }
        return ClientParams::ReadHandler::call(c, error, bytes);
    }
    struct SdCardCommandHandler : public AMBRO_WFUNC_TD(&SdRawInput::sd_card_command_handler) {};
    
    static bool check_file_paused (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (o->state != STATE_PAUSED) {
            cmd->reportError(c, AMBRO_PSTR("SdNotInited"));
            return false;
        }
        return true;
    }
    
public:
    struct Object : public ObjBase<SdRawInput, ParentObject, MakeTypeList<
        TheDebugObject, TheSdCard
    >> {
        uint8_t state;
        uint32_t block;
        TransferDescriptor<DataWordType> desc;
    };
};

APRINTER_ALIAS_STRUCT_EXT(SdRawInputService, (
    APRINTER_AS_TYPE(SdCardService)
), (
    static bool const ProvidesFsAccess = false;
    
    APRINTER_ALIAS_STRUCT_EXT(Input, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(ClientParams)
    ), (
        using Params = SdRawInputService;
        APRINTER_DEF_INSTANCE(Input, SdRawInput)
    ))
))

}

#endif
