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

#ifndef AMBROLIB_AT91SAM_USART_SPI_H
#define AMBROLIB_AT91SAM_USART_SPI_H

#include <stdint.h>

#include <sam/drivers/pmc/pmc.h>

#include <aprinter/base/Assert.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/hal/generic/SimpleSpi.h>
#include <aprinter/hal/at91/At91SamPins.h>

namespace APrinter {

#define APRINTER_AT91SAM_USART_SPI_DEFINE_DEVICE(Index, TheSckPin, TheSckPeriph, TheMosiPin, TheMosiPeriph, TheMisoPin, TheMisoPeriph) \
struct At91SamUsartSpiDevice##Index { \
    static Usart * usart () { return USART##Index; } \
    static int const DeviceId = ID_USART##Index; \
    static enum IRQn const IrqNum = USART##Index##_IRQn; \
    using SckPin = TheSckPin; \
    using SckPeriph = TheSckPeriph; \
    using MosiPin = TheMosiPin; \
    using MosiPeriph = TheMosiPeriph; \
    using MisoPin = TheMisoPin; \
    using MisoPeriph = TheMisoPeriph; \
};

#if defined(__SAM3X8E__)
APRINTER_AT91SAM_USART_SPI_DEFINE_DEVICE(0,
    decltype(At91SamPin<At91SamPioA,17>()), At91SamPeriphB,
    decltype(At91SamPin<At91SamPioA,11>()), At91SamPeriphA,
    decltype(At91SamPin<At91SamPioA,10>()), At91SamPeriphA
)
#else
#error "Unsupported device"
#endif

template <typename Context, typename ParentObject, typename TransferCompleteHandler, typename Params>
class At91SamUsartSimpleSpi {
    using Device = typename Params::Device;
    
public:
    static void init (Context c)
    {
        Context::Pins::template setPeripheral<typename Device::SckPin>(c, typename Device::SckPeriph());
        Context::Pins::template setPeripheral<typename Device::MosiPin>(c, typename Device::MosiPeriph());
        Context::Pins::template setPeripheral<typename Device::MisoPin>(c, typename Device::MisoPeriph());
        
        memory_barrier();
        
        pmc_enable_periph_clk(Device::DeviceId);
        Device::usart()->US_CR = US_CR_RSTRX | US_CR_RSTTX;
        Device::usart()->US_MR = US_MR_USART_MODE_SPI_MASTER | US_MR_USCLKS_MCK | US_MR_CHRL_8_BIT | US_MR_PAR_NO | US_MR_MSBF | US_MR_CLKO | US_MR_INACK;
        Device::usart()->US_BRGR = US_BRGR_CD((uint32_t)Params::ClockDivider);
        Device::usart()->US_IDR = UINT32_MAX;
        NVIC_ClearPendingIRQ(Device::IrqNum);
        NVIC_SetPriority(Device::IrqNum, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(Device::IrqNum);
        Device::usart()->US_CR = US_CR_RXEN | US_CR_TXEN;
    }
    
    static void deinit (Context c)
    {
        NVIC_DisableIRQ(Device::IrqNum);
        Device::usart()->US_CR = US_CR_RXDIS | US_CR_TXDIS;
        Device::usart()->US_IDR = UINT32_MAX;
        (void)Device::usart()->US_RHR;
        NVIC_ClearPendingIRQ(Device::IrqNum);
        pmc_disable_periph_clk(Device::DeviceId);
        
        memory_barrier();
        
        Context::Pins::template setInput<typename Device::SckPin>(c);
        Context::Pins::template setInput<typename Device::MosiPin>(c);
        Context::Pins::template setInput<typename Device::MisoPin>(c);
    }
    
    static void startTransfer (Context c, uint8_t byte)
    {
        memory_barrier();
        
        Device::usart()->US_THR = byte;
        Device::usart()->US_IER = US_IER_RXRDY;
    }
    
    static void nextByte (InterruptContext<Context> c, uint8_t byte)
    {
        Device::usart()->US_THR = byte;
    }
    
    static void noNextByte (InterruptContext<Context> c)
    {
        Device::usart()->US_IDR = US_IDR_RXRDY;
    }
    
    static void usart_irq (InterruptContext<Context> c)
    {
        AMBRO_ASSERT(Device::usart()->US_CSR & US_CSR_RXRDY)
        
        uint8_t byte = Device::usart()->US_RHR;
        TransferCompleteHandler::call(c, byte);
    }
    
public:
    struct Object {};
};

template <
    typename TDevice,
    uint16_t TClockDivider
>
struct At91SamUsartSimpleSpiService {
    using Device = TDevice;
    static uint16_t const ClockDivider = TClockDivider;
    
    template <typename Context, typename ParentObject, typename TransferCompleteHandler>
    using SimpleSpiDriver = At91SamUsartSimpleSpi<Context, ParentObject, TransferCompleteHandler, At91SamUsartSimpleSpiService>;
};


template <typename Device, uint16_t ClockDivider>
using At91SamUsartSpiService = SimpleSpiService<At91SamUsartSimpleSpiService<Device, ClockDivider>>;

#define APRINTER_AT91SAM_USART_SPI_GLOBAL(UsartIndex, thespi, context) \
extern "C" \
__attribute__((used)) \
void USART##UsartIndex##_Handler (void) \
{ \
    thespi::GetDriver::usart_irq(MakeInterruptContext(context)); \
}

}

#endif
