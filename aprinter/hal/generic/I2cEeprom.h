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

#ifndef AMBROLIB_I2C_EEPROM_H
#define AMBROLIB_I2C_EEPROM_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/misc/ClockUtils.h>

namespace APrinter {

template <typename Context, typename ParentObject, typename Handler, typename Params>
class I2cEeprom {
public:
    struct Object;
    
private:
    static_assert(Params::Size % Params::BlockSize == 0, "");
    static_assert(Params::Size <= UINT32_C(65536), "");
    struct I2cHandler;
    using TheClockUtils = ClockUtils<Context>;
    using TimeType = typename TheClockUtils::TimeType;
    static TimeType const WriteTimeoutTicks = Params::WriteTimeout::value() * TheClockUtils::time_freq;
    using TheDebugObject = DebugObject<Context, Object>;
    using TheI2c = typename Params::I2cService::template I2c<Context, Object, I2cHandler>;
    enum {STATE_IDLE, STATE_READ_SEEK, STATE_READ_READ, STATE_WRITE_TRANSFER, STATE_WRITE_POLL};
    
public:
    using SizeType = uint32_t;
    static SizeType const Size = Params::Size;
    static SizeType const BlockSize = Params::BlockSize;
    static SizeType const NumBlocks = Size / BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheI2c::init(c);
        o->state = STATE_IDLE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        TheI2c::deinit(c);
    }
    
    static void startRead (Context c, SizeType offset, uint8_t *data, size_t length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        AMBRO_ASSERT(offset <= Size)
        AMBRO_ASSERT(length <= Size - offset)
        AMBRO_ASSERT(length > 0)
        
        o->state = STATE_READ_SEEK;
        o->success = true;
        o->read.data = data;
        o->read.length = length;
        
        o->addr_buf[0] = (offset >> 8);
        o->addr_buf[1] = offset;
        TheI2c::startWrite(c, Params::I2cAddr, o->addr_buf, 2, NULL, 0);
    }
    
    static void startWrite (Context c, SizeType offset, uint8_t const *data, size_t length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        AMBRO_ASSERT(offset <= Size)
        AMBRO_ASSERT(length <= Size - offset)
        AMBRO_ASSERT(length > 0)
        
        o->state = STATE_WRITE_TRANSFER;
        o->success = true;
        o->write.offset = offset;
        o->write.data = data;
        o->write.length = length;
        
        SizeType rem_in_page = BlockSize - (o->write.offset % BlockSize);
        SizeType write_amount = MinValue(o->write.length, rem_in_page);
        start_write(c, write_amount);
    }
    
    using GetI2c = TheI2c;
    
private:
    static void i2c_handler (Context c, bool success)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READ_SEEK || o->state == STATE_READ_READ || o->state == STATE_WRITE_TRANSFER || o->state == STATE_WRITE_POLL)
        
        if (!success && o->state != STATE_WRITE_POLL) {
            o->success = false;
            goto end;
        }
        
        if (o->state == STATE_READ_SEEK) {
            TheI2c::startRead(c, Params::I2cAddr, o->read.data, o->read.length);
            o->state = STATE_READ_READ;
            return;
        } else if (o->state == STATE_WRITE_TRANSFER) {
            TheI2c::startWrite(c, Params::I2cAddr, o->addr_buf, 2, NULL, 0);
            o->state = STATE_WRITE_POLL;
            o->write.poll_timer.setAfter(c, WriteTimeoutTicks);
            return;
        } else if (o->state == STATE_WRITE_POLL) {
            if (!success) {
                if (o->write.poll_timer.isExpired(c)) {
                    o->success = false;
                    goto end;
                }
                TheI2c::startWrite(c, Params::I2cAddr, o->addr_buf, 2, NULL, 0);
                return;
            }
            if (o->write.length != 0) {
                SizeType write_amount = MinValue(o->write.length, BlockSize);
                start_write(c, write_amount);
                o->state = STATE_WRITE_TRANSFER;
                return;
            }
        }
        
    end:
        o->state = STATE_IDLE;
        return Handler::call(c, o->success);
    }
    struct I2cHandler : public AMBRO_WFUNC_TD(&I2cEeprom::i2c_handler) {};
    
    static void start_write (Context c, SizeType write_amount)
    {
        auto *o = Object::self(c);
        
        o->addr_buf[0] = (o->write.offset >> 8);
        o->addr_buf[1] = o->write.offset;
        TheI2c::startWrite(c, Params::I2cAddr, o->addr_buf, 2, o->write.data, write_amount);
        o->write.offset += write_amount;
        o->write.data += write_amount;
        o->write.length -= write_amount;
    }

public:
    struct Object : public ObjBase<I2cEeprom, ParentObject, MakeTypeList<
        TheDebugObject,
        TheI2c
    >> {
        uint8_t state;
        bool success;
        uint8_t addr_buf[2];
        union {
            struct {
                uint8_t *data;
                SizeType length;
            } read;
            struct {
                SizeType offset;
                uint8_t const *data;
                SizeType length;
                typename TheClockUtils::PollTimer poll_timer;
            } write;
        };
    };
};

APRINTER_ALIAS_STRUCT_EXT(I2cEepromService, (
    APRINTER_AS_TYPE(I2cService),
    APRINTER_AS_VALUE(uint8_t, I2cAddr),
    APRINTER_AS_VALUE(uint32_t, Size),
    APRINTER_AS_VALUE(uint32_t, BlockSize),
    APRINTER_AS_TYPE(WriteTimeout)
), (
    template <typename Context, typename ParentObject, typename Handler>
    using Eeprom = I2cEeprom<Context, ParentObject, Handler, I2cEepromService>;
))

}

#endif
