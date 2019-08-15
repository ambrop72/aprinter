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

#ifndef APRINTER_AT91SAM_SPI_SPI_LL_H
#define APRINTER_AT91SAM_SPI_SPI_LL_H

#include <stdint.h>

#include <sam/drivers/pmc/pmc.h>

#include <aprinter/base/Assert.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/hal/at91/At91SamPins.h>

namespace APrinter {

template <
    uint32_t Address,
    int SpiId_,
    enum IRQn SpiIrq_,
    typename SckPin_,
    typename SckPeriph_,
    typename MosiPin_,
    typename MosiPeriph_,
    typename MisoPin_,
    typename MisoPeriph_
>
struct At91SamSpiSpiLLDevice {
    static Spi * spi () { return (Spi *)Address; }
    static int const SpiId = SpiId_;
    static enum IRQn const SpiIrq = SpiIrq_;
    using SckPin = SckPin_;
    using SckPeriph = SckPeriph_;
    using MosiPin = MosiPin_;
    using MosiPeriph = MosiPeriph_;
    using MisoPin = MisoPin_;
    using MisoPeriph = MisoPeriph_;
};

template <typename Context, typename ParentObject, typename InterruptHandler, typename Device>
class At91SamSpiSpiLLImpl {
public:
    static void init (Context c)
    {
        Context::Pins::template setPeripheral<typename Device::SckPin>(c, typename Device::SckPeriph());
        Context::Pins::template setPeripheral<typename Device::MosiPin>(c, typename Device::MosiPeriph());
        Context::Pins::template setPeripheral<typename Device::MisoPin>(c, typename Device::MisoPeriph());
        
        pmc_enable_periph_clk(Device::SpiId);

        Device::spi()->SPI_CR = SPI_CR_SWRST;
        Device::spi()->SPI_MR = SPI_MR_MSTR | SPI_MR_MODFDIS | SPI_MR_WDRBT | SPI_MR_PCS(0);
        Device::spi()->SPI_CSR[0] = SPI_CSR_NCPHA | SPI_CSR_BITS_8_BIT | SPI_CSR_SCBR(255);
        Device::spi()->SPI_IDR = UINT32_MAX;
        Device::spi()->SPI_CR = SPI_CR_SPIEN;

        NVIC_ClearPendingIRQ(Device::SpiIrq);
        NVIC_SetPriority(Device::SpiIrq, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(Device::SpiIrq);
    }
    
    static void deinit (Context c)
    {
        NVIC_DisableIRQ(Device::SpiIrq);

        Device::spi()->SPI_CR = SPI_CR_SPIDIS;
        Device::spi()->SPI_IDR = UINT32_MAX;
        (void)Device::spi()->SPI_RDR;

        NVIC_ClearPendingIRQ(Device::SpiIrq);
        pmc_disable_periph_clk(Device::SpiId);
        
        Context::Pins::template setInput<typename Device::SckPin>(c);
        Context::Pins::template setInput<typename Device::MosiPin>(c);
        Context::Pins::template setInput<typename Device::MisoPin>(c);
    }

    static bool canSendByte ()
    {
        return (Device::spi()->SPI_SR & SPI_SR_TDRE);
    }

    static bool canRecvByte ()
    {
        return (Device::spi()->SPI_SR & SPI_SR_RDRF);
    }

    static void sendByte (uint8_t byte)
    {
        Device::spi()->SPI_TDR = byte;
    }

    static uint8_t recvByte ()
    {
        return Device::spi()->SPI_RDR;
    }

    static void enableCanSendByteInterrupt (bool enable)
    {
        if (enable) {
            Device::spi()->SPI_IER = SPI_IER_TDRE;
        } else {
            Device::spi()->SPI_IDR = SPI_IDR_TDRE;
        }
    }

    static void enableCanRecvByteInterrupt (bool enable)
    {
        if (enable) {
            Device::spi()->SPI_IER = SPI_IER_RDRF;
        } else {
            Device::spi()->SPI_IDR = SPI_IDR_RDRF;
        }
    }
    
    static void spi_irq (InterruptContext<Context> c)
    {
        InterruptHandler::call(c);
    }
    
public:
    struct Object {};
};

template <typename Device>
struct At91SamSpiSpiLL {
    template <typename Context, typename ParentObject, typename InterruptHandler>
    using SpiLL = At91SamSpiSpiLLImpl<Context, ParentObject, InterruptHandler, Device>;
};

#define APRINTER_AT91SAM_SPI_SPI_LL_GLOBAL(spi_name, TheSpiLL, context) \
extern "C" \
__attribute__((used)) \
void spi_name##_Handler (void) \
{ \
    TheSpiLL::spi_irq(MakeInterruptContext(context)); \
}

#define APRINTER_DEFINE_AT91SAM_SPI_SPI_LL_DEVICE(spi_name, SckPin, SckPeriph, MosiPin, MosiPeriph, MisoPin, MisoPeriph) \
struct At91SamSpiSpiLLDevice##spi_name : public At91SamSpiSpiLLDevice< \
    GET_PERIPHERAL_ADDR(spi_name), ID_##spi_name, spi_name##_IRQn, \
    SckPin, SckPeriph, MosiPin, MosiPeriph, MisoPin, MisoPeriph> {};

#if defined(__SAM3X8E__)

APRINTER_DEFINE_AT91SAM_SPI_SPI_LL_DEVICE(SPI0, \
    decltype(At91SamPin<At91SamPioA, 27>()), At91SamPeriphA, \
    decltype(At91SamPin<At91SamPioA, 26>()), At91SamPeriphA, \
    decltype(At91SamPin<At91SamPioA, 25>()), At91SamPeriphA)

#elif defined(__SAM3U4E__)

APRINTER_DEFINE_AT91SAM_SPI_SPI_LL_DEVICE(SPI, \
    decltype(At91SamPin<At91SamPioA, 15>()), At91SamPeriphA, \
    decltype(At91SamPin<At91SamPioA, 14>()), At91SamPeriphA, \
    decltype(At91SamPin<At91SamPioA, 13>()), At91SamPeriphA)

#else
#error "Unsupported device"
#endif

}

#endif
