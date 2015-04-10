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

#ifndef AMBROLIB_SDCARD_INPUT_H
#define AMBROLIB_SDCARD_INPUT_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class SdCardInput {
public:
    struct Object;
    
private:
    using ThePrinterMain = typename ClientParams::ThePrinterMain;
    using TheDebugObject = DebugObject<Context, Object>;
    struct SdCardInitHandler;
    struct SdCardCommandHandler;
    using TheSdCard = typename Params::SdCardService::template SdCard<Context, Object, 1, SdCardInitHandler, SdCardCommandHandler>;
    static size_t const BlockSize = 512;
    enum {STATE_INACTIVE, STATE_ACTIVATING, STATE_READY, STATE_READING};
    
public:
    static size_t const NeedBufAvail = BlockSize;
    
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
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_INACTIVE)
        
        TheSdCard::activate(c);
        o->state = STATE_ACTIVATING;
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state != STATE_INACTIVE)
        
        TheSdCard::deactivate(c);
        o->state = STATE_INACTIVE;
    }
    
    static bool eofReached (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_READING)
        
        return (o->block >= TheSdCard::getCapacityBlocks(c));
    }
    
    static bool canRead (Context c, size_t buf_avail)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY)
        
        return (o->block < TheSdCard::getCapacityBlocks(c) && buf_avail >= BlockSize);
    }
    
    static void startRead (Context c, size_t buf_avail, size_t buf_wrap, uint8_t *buf1, uint8_t *buf2)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY)
        AMBRO_ASSERT(o->block < TheSdCard::getCapacityBlocks(c))
        AMBRO_ASSERT(buf_avail >= BlockSize)
        AMBRO_ASSERT(buf_wrap > 0)
        
        size_t effective_wrap = MinValue(BlockSize, buf_wrap);
        TheSdCard::queueReadBlock(c, o->block, buf1, effective_wrap, buf2, &o->read_state);
        o->state = STATE_READING;
    }
    
    static bool checkCommand (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        return true;
    }
    
    static bool startingIo (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        return true;
    }
    
    static void pausingIo (Context c)
    {
    }
    
    using GetSdCard = TheSdCard;
    
private:
    static void sd_card_init_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_ACTIVATING)
        
        if (error_code) {
            o->state = STATE_INACTIVE;
        } else {
            o->state = STATE_READY;
            o->block = 0;
        }
        return ClientParams::ActivateHandler::call(c, error_code);
    }
    struct SdCardInitHandler : public AMBRO_WFUNC_TD(&SdCardInput::sd_card_init_handler) {};
    
    static void sd_card_command_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READING)
        
        bool error;
        if (!TheSdCard::checkReadBlock(c, &o->read_state, &error)) {
            return;
        }
        TheSdCard::unsetEvent(c);
        o->state = STATE_READY;
        size_t bytes = 0;
        if (!error) {
            bytes = BlockSize;
            o->block++;
        }
        return ClientParams::ReadHandler::call(c, error, bytes);
    }
    struct SdCardCommandHandler : public AMBRO_WFUNC_TD(&SdCardInput::sd_card_command_handler) {};
    
public:
    struct Object : public ObjBase<SdCardInput, ParentObject, MakeTypeList<
        TheDebugObject, TheSdCard
    >> {
        uint8_t state;
        uint32_t block;
        typename TheSdCard::ReadState read_state;
    };
};

template <typename TSdCardService>
struct SdCardInputService {
    using SdCardService = TSdCardService;
    
    template <typename Context, typename ParentObject, typename ClientParams>
    using Input = SdCardInput<Context, ParentObject, ClientParams, SdCardInputService>;
};

#include <aprinter/EndNamespace.h>

#endif
