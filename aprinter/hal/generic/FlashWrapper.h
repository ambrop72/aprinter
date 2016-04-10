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

#ifndef AMBROLIB_FLASH_WRAPPER_H
#define AMBROLIB_FLASH_WRAPPER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Handler, typename Params>
class FlashWrapper {
public:
    struct Object;
    
private:
    using FastEvent = typename Context::EventLoop::template FastEventSpec<FlashWrapper>;
    using TheDebugObject = DebugObject<Context, Object>;
    struct FlashHandler;
    using TheFlash = typename Params::FlashService::template Flash<Context, Object, FlashHandler>;
    enum {STATE_IDLE, STATE_READ, STATE_WRITE, STATE_WRITE_BLOCK};
    
public:
    using SizeType = size_t;
    static SizeType const BlockSize = TheFlash::BlockSize;
    static SizeType const NumBlocks = TheFlash::NumBlocks;
    static SizeType const Size = BlockSize * NumBlocks;
    
private:
    static SizeType const WordsPerBlock = BlockSize / 4;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheFlash::init(c);
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, FlashWrapper::event_handler);
        
        o->state = STATE_IDLE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
        
        TheFlash::deinit(c);
    }
    
    static void startRead (Context c, SizeType offset, uint8_t *data, size_t length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        AMBRO_ASSERT(offset <= Size)
        AMBRO_ASSERT(length <= Size - offset)
        AMBRO_ASSERT(length > 0)
        
        o->state = STATE_READ;
        o->success = true;
        o->offset = offset;
        o->length = length;
        o->read.data = data;
        
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    
    static void startWrite (Context c, SizeType offset, uint8_t const *data, size_t length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        AMBRO_ASSERT(offset <= Size)
        AMBRO_ASSERT(length <= Size - offset)
        AMBRO_ASSERT(length > 0)
        
        o->state = STATE_WRITE;
        o->success = true;
        o->offset = offset;
        o->length = length;
        o->write.data = data;
        
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    
    using GetFlash = TheFlash;
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READ || o->state == STATE_WRITE)
        
        if (o->success) {
            if (o->state == STATE_READ) {
                SizeType offset = o->offset;
                SizeType length = o->length;
                uint8_t *data = o->read.data;
                uint8_t const volatile *read_ptr = TheFlash::getReadPointer();
                for (SizeType i = 0; i < length; i++) {
                    data[i] = read_ptr[offset + i];
                }
            } else {
                if (o->length > 0) {
                    SizeType block_idx = o->offset / BlockSize;
                    SizeType block_offset = o->offset % BlockSize;
                    SizeType word_offset = o->offset % 4;
                    SizeType block_word_idx = block_offset / 4;
                    
                    uint32_t const *block_read = (uint32_t const *)(TheFlash::getReadPointer() + (block_idx * BlockSize));
                    uint32_t volatile *block_write = TheFlash::getBlockWritePointer(c, block_idx);
                    
                    for (SizeType i = 0; i < WordsPerBlock; i++) {
                        block_write[i] = block_read[i];
                    }
                    
                    do {
                        SizeType take_bytes = MinValue((SizeType)(4 - word_offset), o->length);
                        uint32_t word = block_read[block_word_idx];
                        memcpy((uint8_t *)&word + word_offset, o->write.data, take_bytes);
                        block_write[block_word_idx] = word;
                        
                        o->offset += take_bytes;
                        o->length -= take_bytes;
                        o->write.data += take_bytes;
                        
                        word_offset = 0;
                        block_word_idx++;
                    } while (block_word_idx < WordsPerBlock && o->length > 0);
                    
                    o->state = STATE_WRITE_BLOCK;
                    TheFlash::startBlockWrite(c, block_idx);
                    return;
                }
            }
        }
        
        o->state = STATE_IDLE;
        return Handler::call(c, o->success);
    }
    
    static void flash_handler (Context c, bool success)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_WRITE_BLOCK)
        
        o->state = STATE_WRITE;
        o->success = success;
        
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    struct FlashHandler : public AMBRO_WFUNC_TD(&FlashWrapper::flash_handler) {};
    
public:
    struct Object : public ObjBase<FlashWrapper, ParentObject, MakeTypeList<
        TheDebugObject, TheFlash
    >> {
        uint8_t state;
        bool success;
        SizeType offset;
        SizeType length;
        union {
            struct {
                uint8_t *data;
            } read;
            struct {
                uint8_t const *data;
            } write;
        };
    };
};

APRINTER_ALIAS_STRUCT_EXT(FlashWrapperService, (
    APRINTER_AS_TYPE(FlashService)
), (
    template <typename Context, typename ParentObject, typename Handler>
    using Eeprom = FlashWrapper<Context, ParentObject, Handler, FlashWrapperService>;
))

#include <aprinter/EndNamespace.h>

#endif
