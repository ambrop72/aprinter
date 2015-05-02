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

#ifndef APRINTER_STM32F4_SD_CARD_H
#define APRINTER_STM32F4_SD_CARD_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/system/Stm32f4Pins.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, int MaxCommands, typename InitHandler, typename CommandHandler, typename Params>
class Stm32f4SdCard {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    enum {STATE_INACTIVE, STATE_ACTIVATING, STATE_RUNNING};
    
    static int const MaxInitAttempts = 100;
    
    static int const SdPinsAF = 12;
    using SdPinsMode = Stm32f4PinOutputMode<Stm32f4PinOutputTypeNormal, Stm32f4PinOutputSpeedHigh, Stm32f4PinPullModePullUp>;
    using SdPinD0 = Stm32f4Pin<Stm32f4PortC, 8>;
    using SdPinD1 = Stm32f4Pin<Stm32f4PortC, 9>;
    using SdPinD2 = Stm32f4Pin<Stm32f4PortC, 10>;
    using SdPinD3 = Stm32f4Pin<Stm32f4PortC, 11>;
    using SdPinCK = Stm32f4Pin<Stm32f4PortC, 12>;
    using SdPinCmd = Stm32f4Pin<Stm32f4PortD, 2>;
    
    static void dma_clk_enable () { __HAL_RCC_DMA2_CLK_ENABLE(); }
    static uint32_t const DmaChannel = DMA_CHANNEL_4;
    static DMA_Stream_TypeDef * dma_rx_stream () { return DMA2_Stream3; }
    static DMA_Stream_TypeDef * dma_tx_stream () { return DMA2_Stream6; }
    static IRQn_Type const DmaRxIrqn = DMA2_Stream3_IRQn;
    static IRQn_Type const DmaTxIrqn = DMA2_Stream6_IRQn;
    
public:
    using BlockIndexType = uint32_t;
    static size_t const BlockSize = 512;
    
    class ReadState {
        friend Stm32f4SdCard;
        // TBD
    };
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->event.init(c, APRINTER_CB_STATFUNC_T(&Stm32f4SdCard::event_handler));
        o->state = STATE_INACTIVE;
        
        Context::Pins::template setAlternateFunction<SdPinD0,  SdPinsAF, SdPinsMode>(c);
        Context::Pins::template setAlternateFunction<SdPinD1,  SdPinsAF, SdPinsMode>(c);
        Context::Pins::template setAlternateFunction<SdPinD2,  SdPinsAF, SdPinsMode>(c);
        Context::Pins::template setAlternateFunction<SdPinD3,  SdPinsAF, SdPinsMode>(c);
        Context::Pins::template setAlternateFunction<SdPinCK,  SdPinsAF, SdPinsMode>(c);
        Context::Pins::template setAlternateFunction<SdPinCmd, SdPinsAF, SdPinsMode>(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        deactivate_common(c);
        o->event.deinit(c);
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_INACTIVE)
        
        o->state = STATE_ACTIVATING;
        o->event.prependNowNotAlready(c);
        o->init_attempts_left = MaxInitAttempts;
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state != STATE_INACTIVE)
        
        deactivate_common(c);
    }
    
    static BlockIndexType getCapacityBlocks (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(o->capacity_blocks > 0)
        
        return o->capacity_blocks;
    }
    
    static void queueReadBlock (Context c, BlockIndexType block, uint8_t *data1, size_t wrap, uint8_t *data2, ReadState *state)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(block < o->capacity_blocks)
        
        // TBD
    }
    
    static bool checkReadBlock (Context c, ReadState *state, bool *out_error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        
        // TBD
        return false;
    }
    
    static void unsetEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        
        // TBD
    }
    
    static void msp_init (Context c)
    {
        auto *o = Object::self(c);
        
        dma_clk_enable();
        
        // DMA Rx
        o->dma_rx = DMA_HandleTypeDef();
        o->dma_rx.Instance = dma_rx_stream();
        o->dma_rx.Init.Channel             = DmaChannel;
        o->dma_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        o->dma_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        o->dma_rx.Init.MemInc              = DMA_MINC_ENABLE;
        o->dma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        o->dma_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        o->dma_rx.Init.Mode                = DMA_PFCTRL;
        o->dma_rx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        o->dma_rx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        o->dma_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        o->dma_rx.Init.MemBurst            = DMA_MBURST_INC4;
        o->dma_rx.Init.PeriphBurst         = DMA_PBURST_INC4;
        __HAL_LINKDMA(&o->sd, hdmarx, o->dma_rx);
        HAL_DMA_DeInit(&o->dma_rx);
        HAL_DMA_Init(&o->dma_rx);
        HAL_NVIC_SetPriority(DmaRxIrqn, 6, 0);
        HAL_NVIC_ClearPendingIRQ(DmaRxIrqn);
        HAL_NVIC_EnableIRQ(DmaRxIrqn);
        
        // DMA Tx
        o->dma_tx = DMA_HandleTypeDef();
        o->dma_tx.Instance = dma_tx_stream();
        o->dma_tx.Init.Channel             = DmaChannel;
        o->dma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        o->dma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        o->dma_tx.Init.MemInc              = DMA_MINC_ENABLE;
        o->dma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        o->dma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        o->dma_tx.Init.Mode                = DMA_PFCTRL;
        o->dma_tx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        o->dma_tx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        o->dma_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        o->dma_tx.Init.MemBurst            = DMA_MBURST_INC4;
        o->dma_tx.Init.PeriphBurst         = DMA_PBURST_INC4;
        __HAL_LINKDMA(&o->sd, hdmatx, o->dma_tx);
        HAL_DMA_DeInit(&o->dma_tx);
        HAL_DMA_Init(&o->dma_tx);
        HAL_NVIC_SetPriority(DmaTxIrqn, 6, 0);
        HAL_NVIC_ClearPendingIRQ(DmaTxIrqn);
        HAL_NVIC_EnableIRQ(DmaTxIrqn);
        
        // SDIO
        __HAL_RCC_SDIO_CLK_ENABLE();
        HAL_NVIC_SetPriority(SDIO_IRQn, 5, 0);
        HAL_NVIC_ClearPendingIRQ(SDIO_IRQn);
        HAL_NVIC_EnableIRQ(SDIO_IRQn);
    }
    
    static void msp_deinit (Context c)
    {
        auto *o = Object::self(c);
        
        HAL_NVIC_DisableIRQ(SDIO_IRQn);
        HAL_NVIC_DisableIRQ(DmaTxIrqn);
        HAL_NVIC_DisableIRQ(DmaRxIrqn);
        
        __HAL_RCC_SDIO_CLK_DISABLE();
        HAL_DMA_DeInit(&o->dma_tx);
        HAL_DMA_DeInit(&o->dma_rx);
    }
    
    static void sdio_irq_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        HAL_SD_IRQHandler(&o->sd);
    }
    
    static void dma_rx_irq_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        HAL_DMA_IRQHandler(o->sd.hdmarx); 
    }
    
    static void dma_tx_irq_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        HAL_DMA_IRQHandler(o->sd.hdmatx); 
    }
    
private:
    static void deactivate_common (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->state == STATE_RUNNING) {
            HAL_SD_DeInit(&o->sd);
        }
        o->event.unset(c);
        o->state = STATE_INACTIVE;
    }
    
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_ACTIVATING)
        AMBRO_ASSERT(o->init_attempts_left > 0)
        
        o->sd = SD_HandleTypeDef();
        o->sd.Instance = SDIO;
        o->sd.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
        o->sd.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
        o->sd.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
        o->sd.Init.BusWide             = SDIO_BUS_WIDE_1B;
        o->sd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
        o->sd.Init.ClockDiv            = SDIO_TRANSFER_CLK_DIV;
        
        uint8_t error_code = 99;
        do {
            HAL_SD_CardInfoTypedef sd_info;
            if (HAL_SD_Init(&o->sd, &sd_info) != SD_OK) {
                if (o->init_attempts_left > 1) {
                    o->init_attempts_left--;
                    HAL_SD_DeInit(&o->sd);
                    o->event.prependNowNotAlready(c);
                    return;
                }
                error_code = 1;
                goto out;
            }
            
            if (HAL_SD_WideBusOperation_Config(&o->sd, SDIO_BUS_WIDE_4B) != SD_OK) {
                error_code = 2;
                goto out;
            }
            
            uint64_t capacity_blocks_calc = sd_info.CardCapacity / BlockSize;
            if (capacity_blocks_calc == 0) {
                error_code = 3;
                goto out;
            }
            if (capacity_blocks_calc > UINT32_MAX) {
                error_code = 4;
                goto out;
            }
            o->capacity_blocks = capacity_blocks_calc;
            
            error_code = 0;
        } while (0);
        
    out:
        if (error_code) {
            HAL_SD_DeInit(&o->sd);
            deactivate_common(c);
        } else {
            o->state = STATE_RUNNING;
        }
        return InitHandler::call(c, error_code);
    }
    
public:
    struct Object : public ObjBase<Stm32f4SdCard, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        typename Context::EventLoop::QueuedEvent event;
        uint8_t state;
        int init_attempts_left;
        uint32_t capacity_blocks;
        SD_HandleTypeDef sd;
        DMA_HandleTypeDef dma_rx;
        DMA_HandleTypeDef dma_tx;
    };
};

struct Stm32f4SdCardService {
    template <typename Context, typename ParentObject, int MaxCommands, typename InitHandler, typename CommandHandler>
    using SdCard = Stm32f4SdCard<Context, ParentObject, MaxCommands, InitHandler, CommandHandler, Stm32f4SdCardService>;
};

#define APRINTER_STM32F4_SD_CARD_GLOBAL(sdcard, context) \
extern "C" void HAL_SD_MspInit (SD_HandleTypeDef *hsd) \
{ \
    sdcard::msp_init((context)); \
} \
extern "C" void HAL_SD_MspDeInit (SD_HandleTypeDef *hsd) \
{ \
    sdcard::msp_deinit((context)); \
} \
extern "C" void SDIO_IRQHandler () \
{ \
    sdcard::sdio_irq_handler(MakeInterruptContext((context))); \
} \
extern "C" void DMA2_Stream3_IRQHandler () \
{ \
    sdcard::dma_rx_irq_handler(MakeInterruptContext((context))); \
} \
extern "C" void DMA2_Stream6_IRQHandler () \
{ \
    sdcard::dma_tx_irq_handler(MakeInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
