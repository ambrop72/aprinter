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

#ifndef AMBROLIB_AVR_SPI_H
#define AMBROLIB_AVR_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <aprinter/meta/Position.h>
#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/AvrPins.h>

#include <aprinter/BeginNamespace.h>

template <typename Position, typename Context, typename Handler, int CommandBufferBits>
class AvrSpi : public DebugObject<Context, void> {
    using FastEvent = typename Context::EventLoop::template FastEventSpec<AvrSpi>;
    
    static AvrSpi * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    enum {
        COMMAND_READ_BUFFER,
        COMMAND_READ_UNTIL_DIFFERENT,
        COMMAND_WRITE_BUFFER,
        COMMAND_WRITE_BYTE
    };
    
    struct Command {
        uint8_t type;
        uint8_t byte;
        union {
            struct {
                uint8_t *cur;
                uint8_t *end;
            } read_buffer;
            struct {
                uint8_t *data;
                uint8_t target_byte;
                uint8_t remain;
            } read_until_different;
            struct {
                uint8_t const *cur;
                uint8_t const *end;
            } write_buffer;
            struct {
                size_t count;
            } write_byte;
        } u;
    };
    
    using CommandSizeType = BoundedInt<CommandBufferBits, false>;
    
    static void init (Context c)
    {
        AvrSpi *o = self(c);
        
        c.eventLoop()->template initFastEvent<FastEvent>(c, AvrSpi::event_handler);
        o->m_start = CommandSizeType::import(0);
        o->m_end = CommandSizeType::import(0);
        
        c.pins()->template set<SckPin>(c, false);
        c.pins()->template set<MosiPin>(c, false);
        c.pins()->template set<MisoPin>(c, false);
        c.pins()->template setOutput<SckPin>(c);
        c.pins()->template setOutput<MosiPin>(c);
        c.pins()->template setInput<MisoPin>(c);
        
        SPCR = (1 << SPIE) | (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
        SPSR = 0;
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        AvrSpi *o = self(c);
        o->debugDeinit(c);
        
        SPCR = 0;
        SPSR = 0;
        
        c.eventLoop()->template resetFastEvent<FastEvent>(c);
    }
    
    static void cmdReadBuffer (Context c, uint8_t *data, size_t length, uint8_t send_byte)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!is_full(c))
        AMBRO_ASSERT(length > 0)
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_READ_BUFFER;
        cmd->byte = send_byte;
        cmd->u.read_buffer.cur = data;
        cmd->u.read_buffer.end = data + length;
        write_command(c);
    }
    
    static void cmdReadUntilDifferent (Context c, uint8_t target_byte, uint8_t max_length, uint8_t send_byte, uint8_t *data)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!is_full(c))
        AMBRO_ASSERT(max_length > 0)
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_READ_UNTIL_DIFFERENT;
        cmd->byte = send_byte;
        cmd->u.read_until_different.data = data;
        cmd->u.read_until_different.target_byte = target_byte;
        cmd->u.read_until_different.remain = max_length - 1;
        write_command(c);
    }
    
    static void cmdWriteBuffer (Context c, uint8_t first_byte, uint8_t const *data, size_t length)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!is_full(c))
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_WRITE_BUFFER;
        cmd->byte = first_byte;
        cmd->u.write_buffer.cur = data;
        cmd->u.write_buffer.end = data + length;
        write_command(c);
    }
    
    static void cmdWriteByte (Context c, uint8_t byte, size_t count)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!is_full(c))
        AMBRO_ASSERT(count > 0)
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_WRITE_BYTE;
        cmd->byte = byte;
        cmd->u.write_byte.count = count - 1;
        write_command(c);
    }
    
    static CommandSizeType getEndIndex (Context c)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        
        return o->m_end;
    }
    
    static bool indexReached (Context c, CommandSizeType index)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        
        CommandSizeType start;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            start = o->m_start;
        }
        return (BoundedModuloSubtract(o->m_end, start) <= BoundedModuloSubtract(o->m_end, index));
    }
    
    static bool endReached (Context c)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        
        CommandSizeType start;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            start = o->m_start;
        }
        return (start == o->m_end);
    }
    
    static void unsetEvent (Context c)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        
        c.eventLoop()->template resetFastEvent<FastEvent>(c);
    }
    
    static void spi_stc_isr (InterruptContext<Context> c)
    {
        AvrSpi *o = self(c);
        AMBRO_ASSERT(o->m_start != o->m_end)
        
        Command *cmd = o->m_current;
        switch (cmd->type) {
            case COMMAND_READ_BUFFER: {
                uint8_t * __restrict__ cur = cmd->u.read_buffer.cur;
                *cur = SPDR;
                cur++;
                if (AMBRO_UNLIKELY(cur != cmd->u.read_buffer.end)) {
                    cmd->u.read_buffer.cur = cur;
                    SPDR = cmd->byte;
                    return;
                }
            } break;
            case COMMAND_READ_UNTIL_DIFFERENT: {
                uint8_t byte = SPDR;
                *cmd->u.read_until_different.data = byte;
                if (AMBRO_UNLIKELY(byte == cmd->u.read_until_different.target_byte && cmd->u.read_until_different.remain != 0)) {
                    cmd->u.read_until_different.remain--;
                    SPDR = cmd->byte;
                    return;
                }
            } break;
            case COMMAND_WRITE_BUFFER: {
                if (AMBRO_UNLIKELY(cmd->u.write_buffer.cur != cmd->u.write_buffer.end)) {
                    uint8_t out = *cmd->u.write_buffer.cur;
                    cmd->u.write_buffer.cur++;
                    SPDR = out;
                    return;
                }
            } break;
            default:
            case COMMAND_WRITE_BYTE: {
                if (AMBRO_UNLIKELY(cmd->u.write_byte.count != 0)) {
                    cmd->u.write_byte.count--;
                    SPDR = cmd->byte;
                    return;
                }
            } break;
        }
        c.eventLoop()->template triggerFastEvent<FastEvent>(c);
        o->m_start = BoundedModuloInc(o->m_start);
        if (AMBRO_LIKELY(o->m_start != o->m_end)) {
            o->m_current = &o->m_buffer[o->m_start.value()];
            SPDR = o->m_current->byte;
        }
    }
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
#if defined(__AVR_ATmega164A__) || defined(__AVR_ATmega164PA__) || defined(__AVR_ATmega324A__) || \
    defined(__AVR_ATmega324PA__) || defined(__AVR_ATmega644A__) || defined(__AVR_ATmega644PA__) || \
    defined(__AVR_ATmega128__) || defined(__AVR_ATmega1284P__)
    
    using SckPin = AvrPin<AvrPortB, 7>;
    using MosiPin = AvrPin<AvrPortB, 5>;
    using MisoPin = AvrPin<AvrPortB, 6>;
    
#elif defined(__AVR_ATmega640__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega1281__) || \
    defined(__AVR_ATmega2560__) || defined(__AVR_ATmega2561__)
    
    using SckPin = AvrPin<AvrPortB, 1>;
    using MosiPin = AvrPin<AvrPortB, 2>;
    using MisoPin = AvrPin<AvrPortB, 3>;
    
#else
#error Your device is not supported by AvrSpi
#endif
    
    static void event_handler (Context c)
    {
        AvrSpi *o = self(c);
        o->debugAccess(c);
        
        return Handler::call(c);
    }
    
    static bool is_full (Context c)
    {
        AvrSpi *o = self(c);
        CommandSizeType start;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            start = o->m_start;
        }
        return (BoundedModuloSubtract(o->m_end, start) == CommandSizeType::maxValue());
    }
    
    static void write_command (Context c)
    {
        AvrSpi *o = self(c);
        AMBRO_ASSERT(!is_full(c))
        
        bool was_idle;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            was_idle = (o->m_start == o->m_end);
            o->m_end = BoundedModuloInc(o->m_end);
        }
        if (was_idle) {
            o->m_current = &o->m_buffer[o->m_start.value()];
            SPDR = o->m_current->byte;
        }
    }
    
    CommandSizeType m_start;
    CommandSizeType m_end;
    Command *m_current;
    Command m_buffer[(size_t)CommandSizeType::maxIntValue() + 1];
};

#define AMBRO_AVR_SPI_ISRS(avrspi, context) \
ISR(SPI_STC_vect) \
{ \
    (avrspi).spi_stc_isr(MakeInterruptContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
