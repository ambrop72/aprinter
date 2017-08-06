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

#ifndef AMBROLIB_AVR_EEPROM_H
#define AMBROLIB_AVR_EEPROM_H

#include <stdint.h>
#include <stddef.h>
#include <avr/io.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

namespace APrinter {

template <typename Context, typename ParentObject, typename Handler, typename Params>
class AvrEeprom {
public:
    struct Object;
    
private:
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1284P__)
    static uint16_t const EepromSize = UINT16_C(4096);
#else
#error Your device is not supported by AvrEeprom
#endif
    
    static_assert(EepromSize % Params::FakeBlockSize == 0, "");
    
    using FastEvent = typename Context::EventLoop::template FastEventSpec<AvrEeprom>;
    using TheDebugObject = DebugObject<Context, Object>;
    enum {STATE_IDLE, STATE_READ, STATE_WRITE};
    
public:
    using SizeType = uint16_t;
    static SizeType const Size = EepromSize;
    static SizeType const BlockSize = Params::FakeBlockSize;
    static SizeType const NumBlocks = Size / BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, AvrEeprom::event_handler);
        o->state = STATE_IDLE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        EECR = 0;
        
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
    
    static void eeprom_isr (AtomicContext<Context> c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_WRITE)
        
        EECR = 0;
        
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
            SizeType offset = o->offset;
            SizeType length = o->length;
            uint8_t *data = o->read.data;
            for (SizeType i = 0; i < length; i++) {
                EEAR = offset + i;
                EECR = (1 << EERE);
                data[i] = EEDR;
            }
        } else {
            if (o->length > 0) {
                EEAR = o->offset;
                EEDR = *o->write.data;
                o->offset++;
                o->length--;
                o->write.data++;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    EECR = (1 << EEMPE);
                    EECR = (1 << EEPE) | (1 << EERIE);
                }
                return;
            }
        }
        
        o->state = STATE_IDLE;
        return Handler::call(c, true);
    }
    
public:
    struct Object : public ObjBase<AvrEeprom, ParentObject, MakeTypeList<
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
    uint16_t TFakeBlockSize
>
struct AvrEepromService {
    static uint16_t const FakeBlockSize = TFakeBlockSize;
    
    template <typename Context, typename ParentObject, typename Handler>
    using Eeprom = AvrEeprom<Context, ParentObject, Handler, AvrEepromService>;
};

#define AMBRO_AVR_EEPROM_ISRS(TheEeprom, context) \
ISR(EE_READY_vect) \
{ \
    TheEeprom::eeprom_isr(MakeAtomicContext(context)); \
}

}

#endif
