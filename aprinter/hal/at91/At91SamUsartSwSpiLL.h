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

#ifndef AMBROLIB_AT91SAM_USART_SPI_SW_SPI_LL_H
#define AMBROLIB_AT91SAM_USART_SPI_SW_SPI_LL_H

#include <stdint.h>

#include <sam/drivers/pmc/pmc.h>

#include <aprinter/base/Assert.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/hal/at91/At91SamPins.h>

namespace APrinter {

template <
    uint32_t Address,
    int DeviceId_,
    enum IRQn IrqNum_,
    typename SckPin_,
    typename SckPeriph_,
    typename MosiPin_,
    typename MosiPeriph_,
    typename MisoPin_,
    typename MisoPeriph_
>
struct At91SamUsartSwSpiLLDevice {
    static Usart * usart () { return (Usart *)Address; }
    static int const DeviceId = DeviceId_;
    static enum IRQn const IrqNum = IrqNum_;
    using SckPin = SckPin_;
    using SckPeriph = SckPeriph_;
    using MosiPin = MosiPin_;
    using MosiPeriph = MosiPeriph_;
    using MisoPin = MisoPin_;
    using MisoPeriph = MisoPeriph_;
};

template <typename Context, typename ParentObject, typename InterruptHandler, typename Params>
class At91SamUsartSwSpiLLImpl {
    using Device = typename Params::Device;
    
public:
    static void init (Context c)
    {
        Context::Pins::template setPeripheral<typename Device::SckPin>(c, typename Device::SckPeriph());
        Context::Pins::template setPeripheral<typename Device::MosiPin>(c, typename Device::MosiPeriph());
        Context::Pins::template setPeripheral<typename Device::MisoPin>(c, typename Device::MisoPeriph());
        
        pmc_enable_periph_clk(Device::DeviceId);

        Device::usart()->US_CR = US_CR_RSTRX | US_CR_RSTTX;
        Device::usart()->US_MR = US_MR_USART_MODE_SPI_MASTER | US_MR_USCLKS_MCK | US_MR_CHRL_8_BIT | US_MR_PAR_NO | US_MR_MSBF | US_MR_CLKO | US_MR_INACK;
        Device::usart()->US_BRGR = US_BRGR_CD((uint32_t)Params::ClockDivider);
        Device::usart()->US_IDR = UINT32_MAX;
        (void)Device::usart()->US_RHR;
        Device::usart()->US_CR = US_CR_RXEN | US_CR_TXEN;

        NVIC_ClearPendingIRQ(Device::IrqNum);
        NVIC_SetPriority(Device::IrqNum, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(Device::IrqNum);
    }
    
    static void deinit (Context c)
    {
        NVIC_DisableIRQ(Device::IrqNum);

        Device::usart()->US_CR = US_CR_RXDIS | US_CR_TXDIS;
        Device::usart()->US_IDR = UINT32_MAX;
        (void)Device::usart()->US_RHR;

        NVIC_ClearPendingIRQ(Device::IrqNum);
        pmc_disable_periph_clk(Device::DeviceId);
        
        Context::Pins::template setInput<typename Device::SckPin>(c);
        Context::Pins::template setInput<typename Device::MosiPin>(c);
        Context::Pins::template setInput<typename Device::MisoPin>(c);
    }

    static bool canSendByte ()
    {
        return (Device::usart()->US_CSR & US_CSR_TXRDY);
    }

    static bool canRecvByte ()
    {
        return (Device::usart()->US_CSR & US_CSR_RXRDY);
    }

    static void sendByte (uint8_t byte)
    {
        Device::usart()->US_THR = byte;
    }

    static uint8_t recvByte ()
    {
        return Device::usart()->US_RHR;
    }

    static void enableCanSendByteInterrupt (bool enable)
    {
        if (enable) {
            Device::usart()->US_IER = US_IER_TXRDY;
        } else {
            Device::usart()->US_IDR = US_IDR_TXRDY;
        }
    }

    static void enableCanRecvByteInterrupt (bool enable)
    {
        if (enable) {
            Device::usart()->US_IER = US_IER_RXRDY;
        } else {
            Device::usart()->US_IDR = US_IDR_RXRDY;
        }
    }
    
    static void usart_irq (InterruptContext<Context> c)
    {
        InterruptHandler::call(c);
    }
    
public:
    struct Object {};
};

template <
    typename Device_,
    uint16_t ClockDivider_
>
struct At91SamUsartSwSpiLL {
    using Device = Device_;
    static uint16_t const ClockDivider = ClockDivider_;
    
    template <typename Context, typename ParentObject, typename InterruptHandler>
    using SwSpiLL = At91SamUsartSwSpiLLImpl<Context, ParentObject, InterruptHandler, At91SamUsartSwSpiLL>;
};

#define APRINTER_AT91SAM_USART_SW_SPI_LL_GLOBAL(usart_index, TheSwSpiLL, context) \
extern "C" \
__attribute__((used)) \
void USART##usart_index##_Handler (void) \
{ \
    TheSwSpiLL::usart_irq(MakeInterruptContext(context)); \
}

#define APRINTER_DEFINE_AT91SAM_USART_SW_SPI_LL_DEVICE(usart_index, SckPin, SckPeriph, MosiPin, MosiPeriph, MisoPin, MisoPeriph) \
struct At91SamUsartSwSpiLLDeviceUSART##usart_index : public At91SamUsartSwSpiLLDevice< \
    GET_PERIPHERAL_ADDR(USART##usart_index), ID_USART##usart_index, USART##usart_index##_IRQn, \
    SckPin, SckPeriph, MosiPin, MosiPeriph, MisoPin, MisoPeriph \
> {};

#if defined(__SAM3X8E__)

APRINTER_DEFINE_AT91SAM_USART_SW_SPI_LL_DEVICE(0,
    decltype(At91SamPin<At91SamPioA,17>()), At91SamPeriphB,
    decltype(At91SamPin<At91SamPioA,11>()), At91SamPeriphA,
    decltype(At91SamPin<At91SamPioA,10>()), At91SamPeriphA)

#else
#error "Unsupported device"
#endif

}

#endif
