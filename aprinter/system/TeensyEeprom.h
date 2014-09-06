/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef AMBROLIB_TEENSY_EEPROM_H
#define AMBROLIB_TEENSY_EEPROM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

extern "C" {
    void eeprom_initialize (void);
    void eeprom_read_block (void *buf, const void *addr, uint32_t len);
    int eeprom_is_ready (void);
    void eeprom_write_byte_nonblock (uint32_t offset, uint8_t value);
    void eeprom_write_word_nonblock (uint32_t offset, uint16_t value);
    void eeprom_write_dword_nonblock (uint32_t offset, uint32_t value);
}

template <typename Context, typename ParentObject, typename Handler, typename Params>
class TeensyEeprom {
public:
    struct Object;
    
private:
    using FastEvent = typename Context::EventLoop::template FastEventSpec<TeensyEeprom>;
    using TheDebugObject = DebugObject<Context, Object>;
    enum {STATE_IDLE, STATE_READ, STATE_WRITE};
    
public:
    using SizeType = uint32_t;
    static SizeType const Size = Params::Size;
    static SizeType const BlockSize = Params::FakeBlockSize;
    static SizeType const NumBlocks = Size / BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, TeensyEeprom::event_handler);
        o->state = STATE_IDLE;
        
        eeprom_initialize();
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
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
        o->offset = offset;
        o->length = length;
        o->write.data = data;
        
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READ || o->state == STATE_WRITE)
        
        if (o->state == STATE_READ) {
            eeprom_read_block(o->read.data, (void const *)o->offset, o->length);
        } else {
            if (!eeprom_is_ready()) {
                Context::EventLoop::template triggerFastEvent<FastEvent>(c);
                return;
            }
            if (o->length > 0) {
                SizeType bytes;
                if (o->offset % 4 == 0 && o->length >= 4) {
                    bytes = 4;
                    uint32_t dword;
                    memcpy(&dword, o->write.data, bytes);
                    eeprom_write_dword_nonblock(o->offset, dword);
                } else if (o->offset % 2 == 0 && o->length >= 2) {
                    bytes = 2;
                    uint16_t word;
                    memcpy(&word, o->write.data, bytes);
                    eeprom_write_word_nonblock(o->offset, word);
                } else {
                    bytes = 1;
                    eeprom_write_byte_nonblock(o->offset, *o->write.data);
                }
                o->offset += bytes;
                o->length -= bytes;
                o->write.data += bytes;
                Context::EventLoop::template triggerFastEvent<FastEvent>(c);
                return;
            }
        }
        
        o->state = STATE_IDLE;
        return Handler::call(c, true);
    }
    
public:
    struct Object : public ObjBase<TeensyEeprom, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        uint8_t state;
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

template <
    uint32_t TSize,
    uint32_t TFakeBlockSize
>
struct TeensyEepromService {
    static uint32_t const Size = TSize;
    static uint32_t const FakeBlockSize = TFakeBlockSize;
    
    template <typename Context, typename ParentObject, typename Handler>
    using Eeprom = TeensyEeprom<Context, ParentObject, Handler, TeensyEepromService>;
};

#include <aprinter/EndNamespace.h>

#endif
