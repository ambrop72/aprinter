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

#ifndef APRINTER_STM32F4SDIO_H
#define APRINTER_STM32F4SDIO_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/hal/stm32/Stm32f4Pins.h>
#include <aprinter/hal/generic/SdioInterface.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename CommandHandler, typename Params>
class Stm32f4Sdio {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using FastEvent = typename Context::EventLoop::template FastEventSpec<Stm32f4Sdio>;
    
    enum {INIT_STATE_OFF, INIT_STATE_POWERON, INIT_STATE_ON};
    enum {CMD_STATE_READY, CMD_STATE_BUSY};
    enum {DATA_STATE_READY, DATA_STATE_WAIT_COMPL, DATA_STATE_WAIT_RXTX, DATA_STATE_WAIT_DMA};
    
    static int const SdPinsAF = 12;
    using SdPinsMode = Stm32f4PinOutputMode<Stm32f4PinOutputTypeNormal, Stm32f4PinOutputSpeedHigh, Stm32f4PinPullModePullUp>;
    
    using SdPinCK = Stm32f4Pin<Stm32f4PortC, 12>;
    using SdPinCmd = Stm32f4Pin<Stm32f4PortD, 2>;
    using SdPinD0 = Stm32f4Pin<Stm32f4PortC, 8>;
    using SdPinD1 = Stm32f4Pin<Stm32f4PortC, 9>;
    using SdPinD2 = Stm32f4Pin<Stm32f4PortC, 10>;
    using SdPinD3 = Stm32f4Pin<Stm32f4PortC, 11>;
    
    static void dma_clk_enable () { __HAL_RCC_DMA2_CLK_ENABLE(); }
    static uint32_t const DmaChannel = DMA_CHANNEL_4;
    static DMA_Stream_TypeDef * dma_rx_stream () { return DMA2_Stream3; }
    static DMA_Stream_TypeDef * dma_tx_stream () { return DMA2_Stream6; }
    static SDIO_TypeDef * sdio () { return SDIO; }
    
public:
    static bool const IsWideMode = Params::IsWideMode;
    static size_t const BlockSize = 512;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        Context::Pins::template setAlternateFunction<SdPinCK,  SdPinsAF, SdPinsMode>(c);
        Context::Pins::template setAlternateFunction<SdPinCmd, SdPinsAF, SdPinsMode>(c);
        Context::Pins::template setAlternateFunction<SdPinD0,  SdPinsAF, SdPinsMode>(c);
        if (IsWideMode) {
            Context::Pins::template setAlternateFunction<SdPinD1,  SdPinsAF, SdPinsMode>(c);
            Context::Pins::template setAlternateFunction<SdPinD2,  SdPinsAF, SdPinsMode>(c);
            Context::Pins::template setAlternateFunction<SdPinD3,  SdPinsAF, SdPinsMode>(c);
        }
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, Stm32f4Sdio::event_handler);
        
        o->init_state = INIT_STATE_OFF;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        reset_internal(c);
    }
    
    static void reset (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        reset_internal(c);
        
        o->init_state = INIT_STATE_OFF;
    }
    
    static void startPowerOn (Context c, SdioIface::InterfaceParams if_params)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_OFF)
        
        msp_init(c);
        
        configure_interface(if_params);
        
        __SDIO_DISABLE();
        
        SDIO_PowerState_ON(sdio());
        
        o->init_state = INIT_STATE_POWERON;
    }
    
    static void completePowerOn (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_POWERON)
        
        __SDIO_ENABLE();
        
        o->init_state = INIT_STATE_ON;
        o->pending = false;
        o->cmd_state = CMD_STATE_READY;
        o->data_state = DATA_STATE_READY;
    }
    
    static void reconfigureInterface (Context c, SdioIface::InterfaceParams if_params)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_ON)
        AMBRO_ASSERT(!o->pending)
        AMBRO_ASSERT(o->cmd_state == CMD_STATE_READY)
        AMBRO_ASSERT(o->data_state == DATA_STATE_READY)
        
        configure_interface(if_params);
    }
    
    static void startCommand (Context c, SdioIface::CommandParams cmd_params)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_ON)
        AMBRO_ASSERT(!o->pending)
        AMBRO_ASSERT(o->cmd_state == CMD_STATE_READY)
        AMBRO_ASSERT(o->data_state == DATA_STATE_READY)
        
        o->data_dir = cmd_params.direction;
        
        if (cmd_params.direction != SdioIface::DATA_DIR_NONE) {
            AMBRO_ASSERT(cmd_params.direction == SdioIface::DATA_DIR_READ || cmd_params.direction == SdioIface::DATA_DIR_WRITE)
            AMBRO_ASSERT(cmd_params.num_blocks >= 1)
            
            o->data_num_blocks = cmd_params.num_blocks;
            o->data_ptr = cmd_params.data_ptr;
            
            if (cmd_params.direction == SdioIface::DATA_DIR_READ) {
                start_data(c);
            }
        }
        
        clear_static_flags(SDIO_FLAG_CMDSENT|SDIO_FLAG_CCRCFAIL|SDIO_FLAG_CTIMEOUT|SDIO_FLAG_CMDREND);
        
        uint32_t response_sdio;
        switch (cmd_params.response_type) {
            case SdioIface::RESPONSE_NONE:
                response_sdio = SDIO_RESPONSE_NO; break;
            case SdioIface::RESPONSE_SHORT:
                response_sdio = SDIO_RESPONSE_SHORT; break;
            case SdioIface::RESPONSE_LONG:
                response_sdio = SDIO_RESPONSE_LONG; break;
            default:
                AMBRO_ASSERT(0);
        }
        
        SDIO_CmdInitTypeDef cmd = SDIO_CmdInitTypeDef();
        cmd.Argument = cmd_params.argument;
        cmd.CmdIndex = cmd_params.cmd_index;
        cmd.Response = response_sdio;
        cmd.WaitForInterrupt = SDIO_WAIT_NO;
        cmd.CPSM = SDIO_CPSM_ENABLE;
        SDIO_SendCommand(sdio(), &cmd);
        
        o->cmd_state = CMD_STATE_BUSY;
        o->cmd_index = cmd_params.cmd_index;
        o->cmd_response_type = cmd_params.response_type;
        o->cmd_flags = cmd_params.flags;
        o->pending = true;
        o->results = SdioIface::CommandResults{SdioIface::CMD_ERROR_NONE};
        o->data_error = SdioIface::DATA_ERROR_NONE;
        
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
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
        HAL_DMA_DeInit(&o->dma_rx);
        HAL_DMA_Init(&o->dma_rx);
        
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
        HAL_DMA_DeInit(&o->dma_tx);
        HAL_DMA_Init(&o->dma_tx);
        
        // SDIO clock
        __HAL_RCC_SDIO_CLK_ENABLE();
    }
    
    static void msp_deinit (Context c)
    {
        auto *o = Object::self(c);
        
        __HAL_RCC_SDIO_CLK_DISABLE();
        HAL_DMA_DeInit(&o->dma_tx);
        HAL_DMA_DeInit(&o->dma_rx);
    }
    
    static void configure_interface (SdioIface::InterfaceParams if_params)
    {
        SD_InitTypeDef tmpinit = SD_InitTypeDef();
        tmpinit.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
        tmpinit.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
        tmpinit.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
        tmpinit.BusWide             = if_params.wide_data_bus ? SDIO_BUS_WIDE_4B : SDIO_BUS_WIDE_1B;
        tmpinit.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
        tmpinit.ClockDiv            = if_params.clock_full_speed ? SDIO_TRANSFER_CLK_DIV : SDIO_INIT_CLK_DIV;
        SDIO_Init(sdio(), tmpinit);
    }
    
    static void clear_static_flags (uint32_t flags)
    {
        sdio()->ICR = flags;
    }
    
    static void start_data (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->data_state == DATA_STATE_READY)
        AMBRO_ASSERT(o->data_dir == SdioIface::DATA_DIR_READ || o->data_dir == SdioIface::DATA_DIR_WRITE)
        AMBRO_ASSERT(o->data_num_blocks >= 1)
        
        size_t data_len = o->data_num_blocks * BlockSize;
        
        clear_static_flags(SDIO_FLAG_DCRCFAIL|SDIO_FLAG_DTIMEOUT|SDIO_FLAG_RXOVERR|SDIO_FLAG_TXUNDERR|SDIO_FLAG_STBITERR|SDIO_FLAG_DATAEND);
        
        __SDIO_DMA_ENABLE();
        
        memory_barrier_dma();
        
        if (o->data_dir == SdioIface::DATA_DIR_READ) {
            HAL_DMA_Start(&o->dma_rx, (uint32_t)&sdio()->FIFO, (uint32_t)o->data_ptr, data_len / 4);
        } else {
            HAL_DMA_Start(&o->dma_tx, (uint32_t)o->data_ptr, (uint32_t)&sdio()->FIFO, data_len / 4);
        }
        
        SDIO_DataInitTypeDef data_init = SDIO_DataInitTypeDef();
        data_init.DataTimeOut   = Params::DataTimeoutBusClocks;
        data_init.DataLength    = data_len;
        data_init.DataBlockSize = SDIO_DATABLOCK_SIZE_512B;
        data_init.TransferDir   = o->data_dir == SdioIface::DATA_DIR_READ ? SDIO_TRANSFER_DIR_TO_SDIO : SDIO_TRANSFER_DIR_TO_CARD;
        data_init.TransferMode  = SDIO_TRANSFER_MODE_BLOCK;
        data_init.DPSM          = SDIO_DPSM_ENABLE;
        SDIO_DataConfig(sdio(), &data_init);
        
        o->data_state = DATA_STATE_WAIT_COMPL;
    }
    
    static DMA_HandleTypeDef * current_dma (Context c)
    {
        auto *o = Object::self(c);
        return o->data_dir == SdioIface::DATA_DIR_READ ? &o->dma_rx : &o->dma_tx;
    }
    
    static void reset_internal (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->init_state == INIT_STATE_ON) {
            if (o->cmd_state != CMD_STATE_READY) {
                sdio()->CMD = 0;
            }
            
            if (o->data_state != DATA_STATE_READY) {
                sdio()->DCTRL = 0;
                HAL_DMA_Abort(current_dma(c));
            }
        }
        
        if (o->init_state >= INIT_STATE_POWERON) {
            SDIO_PowerState_OFF(sdio());
            msp_deinit(c);
        }
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
    }
    
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_ON)
        AMBRO_ASSERT(o->pending)
        AMBRO_ASSERT(o->cmd_state != CMD_STATE_READY || o->data_state != DATA_STATE_READY)
        
        work_cmd(c);
        work_data(c);
        
        if (o->cmd_state != CMD_STATE_READY || o->data_state != DATA_STATE_READY) {
            Context::EventLoop::template triggerFastEvent<FastEvent>(c);
            return;
        }
        
        o->pending = false;
        
        return CommandHandler::call(c, o->results, o->data_error);
    }
    
    static void work_cmd (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->cmd_state == CMD_STATE_READY || o->cmd_state == CMD_STATE_BUSY)
        
        if (o->cmd_state == CMD_STATE_READY) {
            return;
        }
        
        uint32_t status = sdio()->STA;
        
        if (o->cmd_response_type == SdioIface::RESPONSE_NONE) {
            if (!(status & SDIO_FLAG_CMDSENT)) {
                return;
            }
        } else {
            if (!(o->cmd_flags & SdioIface::CMD_FLAG_NO_CRC_CHECK)) {
                if ((status & SDIO_FLAG_CCRCFAIL)) {
                    o->results.error_code = SdioIface::CMD_ERROR_RESPONSE_CHECKSUM;
                    goto cmd_done;
                }
            }
            
            if ((status & SDIO_FLAG_CTIMEOUT)) {
                o->results.error_code = SdioIface::CMD_ERROR_RESPONSE_TIMEOUT;
                goto cmd_done;
            }
            
            if (!(status & SDIO_FLAG_CMDREND) && !(status & SDIO_FLAG_CCRCFAIL)) {
                return;
            }
            
            if (!(o->cmd_flags & SdioIface::CMD_FLAG_NO_CMDNUM_CHECK)) {
                if (SDIO_GetCommandResponse(sdio()) != o->cmd_index) {
                    o->results.error_code = SdioIface::CMD_ERROR_BAD_RESPONSE_CMD;
                    goto cmd_done;
                }
            }
            
            o->results.response[0] = sdio()->RESP1;
            
            if (o->cmd_response_type == SdioIface::RESPONSE_LONG) {
                o->results.response[1] = sdio()->RESP2;
                o->results.response[2] = sdio()->RESP3;
                o->results.response[3] = sdio()->RESP4;
            }
        }
        
    cmd_done:
        o->cmd_state = CMD_STATE_READY;
        
        if (o->data_dir == SdioIface::DATA_DIR_WRITE) {
            start_data(c);
        }
    }
    
    static void work_data (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->data_state == DATA_STATE_READY) {
            return;
        }
        
        uint32_t status = sdio()->STA;
        
        while (true) {
            switch (o->data_state) {
                case DATA_STATE_WAIT_COMPL: {
                    if ((status & SDIO_FLAG_DCRCFAIL)) {
                        o->data_error = SdioIface::DATA_ERROR_CHECKSUM;
                    }
                    else if ((status & SDIO_FLAG_DTIMEOUT)) {
                        o->data_error = SdioIface::DATA_ERROR_TIMEOUT;
                    }
                    else if ((status & SDIO_FLAG_RXOVERR)) {
                        o->data_error = SdioIface::DATA_ERROR_RX_OVERRUN;
                    }
                    else if ((status & SDIO_FLAG_TXUNDERR)) {
                        o->data_error = SdioIface::DATA_ERROR_TX_OVERRUN;
                    }
                    else if ((status & SDIO_FLAG_STBITERR)) {
                        o->data_error = SdioIface::DATA_ERROR_STBITER;
                    }
                    else if (!(status & SDIO_FLAG_DATAEND)) {
                        return;
                    }
                    
                    o->data_state = DATA_STATE_WAIT_RXTX;
                } break;
                
                case DATA_STATE_WAIT_RXTX: {
                    if ((status & SDIO_FLAG_RXACT) || (status & SDIO_FLAG_TXACT)) {
                        return;
                    }
                    
                    o->data_state = DATA_STATE_WAIT_DMA;
                } break;
                
                case DATA_STATE_WAIT_DMA: {
                    if (o->data_error == SdioIface::DATA_ERROR_NONE) {
                        HAL_StatusTypeDef dma_status = HAL_DMA_PollForTransfer(current_dma(c), HAL_DMA_FULL_TRANSFER, 0);
                        if (dma_status == HAL_TIMEOUT) {
                            return;
                        }
                        
                        if (dma_status != HAL_OK) {
                            o->data_error = SdioIface::DATA_ERROR_DMA;
                        } else if (o->data_dir == SdioIface::DATA_DIR_READ) {
                            memory_barrier_dma();
                        }
                    }
                    
                    sdio()->DCTRL = 0;
                    HAL_DMA_Abort(current_dma(c));
                    
                    o->data_state = DATA_STATE_READY;
                    return;
                } break;
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
    }
    
public:
    struct Object : public ObjBase<Stm32f4Sdio, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        DMA_HandleTypeDef dma_rx;
        DMA_HandleTypeDef dma_tx;
        uint8_t init_state;
        bool pending;
        uint8_t cmd_state;
        uint8_t data_state;
        uint8_t cmd_index;
        uint8_t cmd_response_type;
        uint8_t cmd_flags;
        uint8_t data_dir;
        size_t data_num_blocks;
        uint32_t *data_ptr;
        SdioIface::CommandResults results;
        SdioIface::DataErrorCode data_error;
    };
};

template <
    bool TIsWideMode,
    uint32_t TDataTimeoutBusClocks
>
struct Stm32f4SdioService {
    static bool const IsWideMode = TIsWideMode;
    static uint32_t const DataTimeoutBusClocks = TDataTimeoutBusClocks;
    
    template <typename Context, typename ParentObject, typename CommandHandler>
    using Sdio = Stm32f4Sdio<Context, ParentObject, CommandHandler, Stm32f4SdioService>;
};

#include <aprinter/EndNamespace.h>

#endif
