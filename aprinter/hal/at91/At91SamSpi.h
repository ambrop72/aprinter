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

#include <sam/drivers/pmc/pmc.h>

#include <aprinter/base/Assert.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/hal/generic/SimpleSpi.h>
#include <aprinter/hal/at91/At91SamPins.h>

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

template <typename Context, typename ParentObject, typename TransferCompleteHandler, typename Device>
class At91SamSimpleSpi {
public:
    static void init (Context c)
    {
        Context::Pins::template setPeripheral<typename Device::SckPin>(c, At91SamPeriphA());
        Context::Pins::template setPeripheral<typename Device::MosiPin>(c, At91SamPeriphA());
        Context::Pins::template setInput<typename Device::MisoPin>(c);
        
        memory_barrier();
        
        pmc_enable_periph_clk(Device::SpiId);
        Device::spi()->SPI_MR = SPI_MR_MSTR | SPI_MR_MODFDIS | SPI_MR_PCS(0);
        Device::spi()->SPI_CSR[0] = SPI_CSR_NCPHA | SPI_CSR_BITS_8_BIT | SPI_CSR_SCBR(255);
        Device::spi()->SPI_IDR = UINT32_MAX;
        NVIC_ClearPendingIRQ(Device::SpiIrq);
        NVIC_SetPriority(Device::SpiIrq, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(Device::SpiIrq);
        Device::spi()->SPI_CR = SPI_CR_SPIEN;
    }
    
    static void deinit (Context c)
    {
        NVIC_DisableIRQ(Device::SpiIrq);
        Device::spi()->SPI_CR = SPI_CR_SPIDIS;
        (void)Device::spi()->SPI_RDR;
        NVIC_ClearPendingIRQ(Device::SpiIrq);
        pmc_disable_periph_clk(Device::SpiId);
        
        memory_barrier();
        
        Context::Pins::template setInput<typename Device::MosiPin>(c);
        Context::Pins::template setInput<typename Device::SckPin>(c);
    }
    
    static void startTransfer (Context c, uint8_t byte)
    {
        memory_barrier();
        Device::spi()->SPI_TDR = byte;
        Device::spi()->SPI_IER = SPI_IER_RDRF;
    }
    
    static void nextByte (InterruptContext<Context> c, uint8_t byte)
    {
        Device::spi()->SPI_TDR = byte;
    }
    
    static void noNextByte (InterruptContext<Context> c)
    {
        Device::spi()->SPI_IDR = SPI_IDR_RDRF;
    }
    
    static void spi_irq (InterruptContext<Context> c)
    {
        AMBRO_ASSERT(Device::spi()->SPI_SR & SPI_SR_RDRF)
        
        uint8_t byte = Device::spi()->SPI_RDR;
        TransferCompleteHandler::call(c, byte);
    }
    
public:
    struct Object {};
};

template <typename Device>
struct At91SamSimpleSpiService {
    template <typename Context, typename ParentObject, typename TransferCompleteHandler>
    using SimpleSpiDriver = At91SamSimpleSpi<Context, ParentObject, TransferCompleteHandler, Device>;
};


template <typename Device>
using At91SamSpiService = SimpleSpiService<At91SamSimpleSpiService<Device>>;

#if defined(__SAM3X8E__)

struct At91Sam3xSpiDevice : public At91SamSpiDevice<
    GET_PERIPHERAL_ADDR(SPI0),
    ID_SPI0,
    SPI0_IRQn, 
    At91SamPin<At91SamPioA, 27>,
    At91SamPin<At91SamPioA, 26>,
    At91SamPin<At91SamPioA, 25>
> {};

#define AMBRO_AT91SAM3X_SPI_GLOBAL(thespi, context) \
extern "C" \
__attribute__((used)) \
void SPI0_Handler (void) \
{ \
    thespi::GetDriver::spi_irq(MakeInterruptContext(context)); \
}

#elif defined(__SAM3U4E__)

struct At91Sam3uSpiDevice : public At91SamSpiDevice<
    GET_PERIPHERAL_ADDR(SPI),
    ID_SPI,
    SPI_IRQn, 
    At91SamPin<At91SamPioA, 15>,
    At91SamPin<At91SamPioA, 14>,
    At91SamPin<At91SamPioA, 13>
> {};

#define AMBRO_AT91SAM3U_SPI_GLOBAL(thespi, context) \
extern "C" \
__attribute__((used)) \
void SPI_Handler (void) \
{ \
    thespi::GetDriver::spi_irq(MakeInterruptContext(context)); \
}

#else

#error "Unsupported device"

#endif

#include <aprinter/EndNamespace.h>

#endif
