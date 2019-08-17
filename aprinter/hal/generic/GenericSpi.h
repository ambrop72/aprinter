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

#ifndef APRINTER_GENERIC_SPI_H
#define APRINTER_GENERIC_SPI_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Hints.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/misc/ClockUtils.h>

namespace APrinter {

template <typename Arg>
class GenericSpiImpl {
    using Context                      = typename Arg::Context;
    using ParentObject                 = typename Arg::ParentObject;
    using Handler                      = typename Arg::Handler;
    static int const CommandBufferBits = Arg::CommandBufferBits;
    using Params                       = typename Arg::Params;
    
public:
    struct Object;
    
private:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TheClockUtils = ClockUtils<Context>;

    using TheDebugObject = DebugObject<Context, Object>;

    struct InterruptHandler;
    using TheLLDriver = typename Params::LLDriver::template SpiLL<Context, Object, InterruptHandler>;
    
    using FastEvent = typename Context::EventLoop::template FastEventSpec<GenericSpiImpl>;

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
                TimeType timeout;
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
    
public:
    using CommandSizeType = BoundedInt<CommandBufferBits, false>;
    
    static void init (Context c, uint32_t speed_Hz)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, GenericSpiImpl::event_handler);
        o->m_start = CommandSizeType::import(0);
        o->m_end = CommandSizeType::import(0);
        
        TheLLDriver::init(c, speed_Hz);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        TheLLDriver::deinit(c);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
    }
    
    static void cmdReadBuffer (Context c, uint8_t *data, size_t length, uint8_t send_byte)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!is_full(c))
        AMBRO_ASSERT(length > 0)
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_READ_BUFFER;
        cmd->byte = send_byte;
        cmd->u.read_buffer.cur = data;
        cmd->u.read_buffer.end = data + length;
        write_command(c);
    }
    
    static void cmdReadUntilDifferent (Context c, uint8_t target_byte, uint8_t send_byte, TimeType rel_timeout, uint8_t *data)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!is_full(c))
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_READ_UNTIL_DIFFERENT;
        cmd->byte = send_byte;
        cmd->u.read_until_different.data = data;
        cmd->u.read_until_different.target_byte = target_byte;
        cmd->u.read_until_different.timeout = rel_timeout;
        write_command(c);
    }
    
    static void cmdWriteBuffer (Context c, uint8_t const *data, size_t length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!is_full(c))
        AMBRO_ASSERT(length > 0)
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_WRITE_BUFFER;
        cmd->byte = data[0];
        cmd->u.write_buffer.cur = data + 1;
        cmd->u.write_buffer.end = data + length;
        write_command(c);
    }
    
    static void cmdWriteByte (Context c, uint8_t byte, size_t extra_count)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!is_full(c))
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_WRITE_BYTE;
        cmd->byte = byte;
        cmd->u.write_byte.count = extra_count;
        write_command(c);
    }
    
    static CommandSizeType getEndIndex (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return o->m_end;
    }
    
    static bool indexReached (Context c, CommandSizeType index)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        CommandSizeType start = get_start(c);
        return (BoundedModuloSubtract(o->m_end, start) <= BoundedModuloSubtract(o->m_end, index));
    }
    
    static bool endReached (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        CommandSizeType start = get_start(c);
        return (start == o->m_end);
    }
    
    static void unsetEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
    }

    using GetLLDriver = TheLLDriver;
    
private:
    static void interrupt_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_start != o->m_end)

        if (!TheLLDriver::canRecvByte()) {
            return;
        }

        uint8_t byte = TheLLDriver::recvByte();
        
        Command *cmd = o->m_current;

        switch (cmd->type) {
            case COMMAND_READ_BUFFER: {
                uint8_t * __restrict__ cur = cmd->u.read_buffer.cur;
                *cur = byte;
                cur++;
                if (AMBRO_UNLIKELY(cur != cmd->u.read_buffer.end)) {
                    cmd->u.read_buffer.cur = cur;
                    TheLLDriver::sendByte(cmd->byte);
                    return;
                }
            } break;
            case COMMAND_READ_UNTIL_DIFFERENT: {
                *cmd->u.read_until_different.data = byte;
                if (AMBRO_UNLIKELY(byte == cmd->u.read_until_different.target_byte &&
                    !TheClockUtils::timeGreaterOrEqual(Clock::getTime(c), cmd->u.read_until_different.timeout)))
                {
                    TheLLDriver::sendByte(cmd->byte);
                    return;
                }
            } break;
            case COMMAND_WRITE_BUFFER: {
                if (AMBRO_UNLIKELY(cmd->u.write_buffer.cur != cmd->u.write_buffer.end)) {
                    uint8_t out = *cmd->u.write_buffer.cur;
                    cmd->u.write_buffer.cur++;
                    TheLLDriver::sendByte(out);
                    return;
                }
            } break;
            default:
            case COMMAND_WRITE_BYTE: {
                if (AMBRO_UNLIKELY(cmd->u.write_byte.count != 0)) {
                    cmd->u.write_byte.count--;
                    TheLLDriver::sendByte(cmd->byte);
                    return;
                }
            } break;
        }

        o->m_start = BoundedModuloInc(o->m_start);

        if (AMBRO_LIKELY(o->m_start != o->m_end)) {
            o->m_current = &o->m_buffer[o->m_start.value()];
            start_command_common(c);

            TheLLDriver::sendByte(o->m_current->byte);
        } else {
            TheLLDriver::enableCanRecvByteInterrupt(false);
        }

        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    struct InterruptHandler : public AMBRO_WFUNC_TD(&GenericSpiImpl::interrupt_handler) {};
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return Handler::call(c);
    }
    
    static CommandSizeType get_start (Context c)
    {
        auto *o = Object::self(c);
        CommandSizeType start;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            start = o->m_start;
        }
        return start;
    }
    
    static bool is_full (Context c)
    {
        auto *o = Object::self(c);
        CommandSizeType start = get_start(c);
        return (BoundedModuloSubtract(o->m_end, start) == CommandSizeType::maxValue());
    }
    
    static void write_command (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!is_full(c))
        
        bool was_idle;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            was_idle = (o->m_start == o->m_end);
            o->m_end = BoundedModuloInc(o->m_end);
        }

        if (was_idle) {
            o->m_current = &o->m_buffer[o->m_start.value()];
            start_command_common(c);

            memory_barrier();

            TheLLDriver::enableCanRecvByteInterrupt(true);
            TheLLDriver::sendByte(o->m_current->byte);
        }
    }

    template <typename ThisContext>
    static void start_command_common (ThisContext c)
    {
        auto *o = Object::self(c);
        Command *cmd = o->m_current;

        if (cmd->type == COMMAND_READ_UNTIL_DIFFERENT) {
            cmd->u.read_until_different.timeout = Clock::getTime(c) + cmd->u.read_until_different.timeout;
        }
    }
    
public:
    struct Object : public ObjBase<GenericSpiImpl, ParentObject, MakeTypeList<
        TheDebugObject,
        TheLLDriver
    >> {
        CommandSizeType m_start;
        CommandSizeType m_end;
        Command *m_current;
        Command m_buffer[(size_t)CommandSizeType::maxIntValue() + 1];
    };
};

template <typename LLDriver_>
struct GenericSpi {
    using LLDriver = LLDriver_;
    
    APRINTER_ALIAS_STRUCT_EXT(Spi, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Handler),
        APRINTER_AS_VALUE(int, CommandBufferBits)
    ), (
        using Params = GenericSpi;
        APRINTER_DEF_INSTANCE(Spi, GenericSpiImpl)
    ))
};

}

#endif
