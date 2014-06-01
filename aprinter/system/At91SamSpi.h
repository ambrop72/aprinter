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

#ifndef AMBROLIB_AT91SAM_SPI_H
#define AMBROLIB_AT91SAM_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <sam/drivers/pmc/pmc.h>

#include <aprinter/meta/Object.h>
#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/At91SamPins.h>

#include <aprinter/BeginNamespace.h>

template <
    uint32_t TSpiAddr,
    int TSpiId,
    enum IRQn TSpiIrq,
    typename TSckPin,
    typename TMosiPin,
    typename TMisoPin
>
struct At91SamSpiDevice {
    static Spi * spi () { return (Spi *)TSpiAddr; }
    static int const SpiId = TSpiId;
    static enum IRQn const SpiIrq = TSpiIrq;
    using SckPin = TSckPin;
    using MosiPin = TMosiPin;
    using MisoPin = TMisoPin;
};

template <typename Context, typename ParentObject, typename Handler, int CommandBufferBits, typename Device>
class At91SamSpiBase {
    using FastEvent = typename Context::EventLoop::template FastEventSpec<At91SamSpiBase>;
    
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
    
public:
    struct Object;
    using CommandSizeType = BoundedInt<CommandBufferBits, false>;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, At91SamSpiBase::event_handler);
        o->m_start = CommandSizeType::import(0);
        o->m_end = CommandSizeType::import(0);
        
        Context::Pins::template setPeripheralOutputA<typename Device::SckPin>(c);
        Context::Pins::template setPeripheralOutputA<typename Device::MosiPin>(c);
        Context::Pins::template setInput<typename Device::MisoPin>(c);
        
        pmc_enable_periph_clk(Device::SpiId);
        Device::spi()->SPI_MR = SPI_MR_MSTR | SPI_MR_MODFDIS | SPI_MR_PCS(0);
        Device::spi()->SPI_CSR[0] = SPI_CSR_NCPHA | SPI_CSR_BITS_8_BIT | SPI_CSR_SCBR(255);
        Device::spi()->SPI_IDR = UINT32_MAX;
        NVIC_ClearPendingIRQ(Device::SpiIrq);
        NVIC_SetPriority(Device::SpiIrq, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(Device::SpiIrq);
        Device::spi()->SPI_CR = SPI_CR_SPIEN;
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        NVIC_DisableIRQ(Device::SpiIrq);
        Device::spi()->SPI_CR = SPI_CR_SPIDIS;
        (void)Device::spi()->SPI_RDR;
        NVIC_ClearPendingIRQ(Device::SpiIrq);
        pmc_disable_periph_clk(Device::SpiId);
        
        Context::Pins::template setInput<typename Device::MosiPin>(c);
        Context::Pins::template setInput<typename Device::SckPin>(c);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
    }
    
    static void cmdReadBuffer (Context c, uint8_t *data, size_t length, uint8_t send_byte)
    {
        auto *o = Object::self(c);
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
    
    static void cmdReadUntilDifferent (Context c, uint8_t target_byte, uint8_t max_extra_length, uint8_t send_byte, uint8_t *data)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!is_full(c))
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_READ_UNTIL_DIFFERENT;
        cmd->byte = send_byte;
        cmd->u.read_until_different.data = data;
        cmd->u.read_until_different.target_byte = target_byte;
        cmd->u.read_until_different.remain = max_extra_length;
        write_command(c);
    }
    
    static void cmdWriteBuffer (Context c, uint8_t first_byte, uint8_t const *data, size_t length)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!is_full(c))
        
        Command *cmd = &o->m_buffer[o->m_end.value()];
        cmd->type = COMMAND_WRITE_BUFFER;
        cmd->byte = first_byte;
        cmd->u.write_buffer.cur = data;
        cmd->u.write_buffer.end = data + length;
        write_command(c);
    }
    
    static void cmdWriteByte (Context c, uint8_t byte, size_t extra_count)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
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
        o->debugAccess(c);
        
        return o->m_end;
    }
    
    static bool indexReached (Context c, CommandSizeType index)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        CommandSizeType start = get_start(c);
        return (BoundedModuloSubtract(o->m_end, start) <= BoundedModuloSubtract(o->m_end, index));
    }
    
    static bool endReached (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        CommandSizeType start = get_start(c);
        return (start == o->m_end);
    }
    
    static void unsetEvent (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
    }
    
    static void spi_irq (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_start != o->m_end)
        AMBRO_ASSERT(Device::spi()->SPI_SR & SPI_SR_RDRF)
        
        uint8_t byte = Device::spi()->SPI_RDR;
        Command *cmd = o->m_current;
        switch (cmd->type) {
            case COMMAND_READ_BUFFER: {
                uint8_t * __restrict__ cur = cmd->u.read_buffer.cur;
                *cur = byte;
                cur++;
                if (AMBRO_UNLIKELY(cur != cmd->u.read_buffer.end)) {
                    cmd->u.read_buffer.cur = cur;
                    Device::spi()->SPI_TDR = cmd->byte;
                    return;
                }
            } break;
            case COMMAND_READ_UNTIL_DIFFERENT: {
                *cmd->u.read_until_different.data = byte;
                if (AMBRO_UNLIKELY(byte == cmd->u.read_until_different.target_byte && cmd->u.read_until_different.remain != 0)) {
                    cmd->u.read_until_different.remain--;
                    Device::spi()->SPI_TDR = cmd->byte;
                    return;
                }
            } break;
            case COMMAND_WRITE_BUFFER: {
                if (AMBRO_UNLIKELY(cmd->u.write_buffer.cur != cmd->u.write_buffer.end)) {
                    uint8_t out = *cmd->u.write_buffer.cur;
                    cmd->u.write_buffer.cur++;
                    Device::spi()->SPI_TDR = out;
                    return;
                }
            } break;
            default:
            case COMMAND_WRITE_BYTE: {
                if (AMBRO_UNLIKELY(cmd->u.write_byte.count != 0)) {
                    cmd->u.write_byte.count--;
                    Device::spi()->SPI_TDR = cmd->byte;
                    return;
                }
            } break;
        }
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
        o->m_start = BoundedModuloInc(o->m_start);
        if (AMBRO_LIKELY(o->m_start != o->m_end)) {
            o->m_current = &o->m_buffer[o->m_start.value()];
            Device::spi()->SPI_TDR = o->m_current->byte;
        } else {
            Device::spi()->SPI_IDR = SPI_IDR_RDRF;
        }
    }
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
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
            Device::spi()->SPI_TDR = o->m_current->byte;
            Device::spi()->SPI_IER = SPI_IER_RDRF;
        }
    }
    
public:
    struct Object : public ObjBase<At91SamSpiBase, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        CommandSizeType m_start;
        CommandSizeType m_end;
        Command *m_current;
        Command m_buffer[(size_t)CommandSizeType::maxIntValue() + 1];
    };
};

template <typename Device>
struct At91SamSpiService {
    template <typename Context, typename ParentObject, typename Handler, int CommandBufferBits>
    using Spi = At91SamSpiBase<Context, ParentObject, Handler, CommandBufferBits, Device>;
};

#if defined(__SAM3X8E__)

using At91Sam3xSpiDevice = At91SamSpiDevice<
    GET_PERIPHERAL_ADDR(SPI0),
    ID_SPI0,
    SPI0_IRQn, 
    At91SamPin<At91SamPioA, 27>,
    At91SamPin<At91SamPioA, 26>,
    At91SamPin<At91SamPioA, 25>
>;

#define AMBRO_AT91SAM3X_SPI_GLOBAL(thespi, context) \
extern "C" \
__attribute__((used)) \
void SPI0_Handler (void) \
{ \
    thespi::spi_irq(MakeInterruptContext(context)); \
}

#elif defined(__SAM3U4E__)

using At91Sam3uSpiDevice = At91SamSpiDevice<
    GET_PERIPHERAL_ADDR(SPI),
    ID_SPI,
    SPI_IRQn, 
    At91SamPin<At91SamPioA, 15>,
    At91SamPin<At91SamPioA, 14>,
    At91SamPin<At91SamPioA, 13>
>;

#define AMBRO_AT91SAM3U_SPI_GLOBAL(thespi, context) \
extern "C" \
__attribute__((used)) \
void SPI_Handler (void) \
{ \
    thespi::spi_irq(MakeInterruptContext(context)); \
}

#else

#error "Unsupported device"

#endif

#include <aprinter/EndNamespace.h>

#endif
