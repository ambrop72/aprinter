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
#include <stddef.h>
#include <limits.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/hal/stm32/Stm32f4Pins.h>
#include <aprinter/hal/generic/SdioInterface.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename CommandHandler, typename BusyTimeout, typename Params>
class Stm32f4Sdio {
public:
    struct Object;
    
private:
    static_assert(Params::SdClockPrescaler >= 0 && Params::SdClockPrescaler <= 255, "");
    
    using TheClockUtils = ClockUtils<Context>;
    using TheDebugObject = DebugObject<Context, Object>;
    using FastEvent = typename Context::EventLoop::template FastEventSpec<Stm32f4Sdio>;
    
    enum {INIT_STATE_OFF, INIT_STATE_POWERON, INIT_STATE_ON};
    enum {CMD_STATE_READY, CMD_STATE_BUSY, CMD_STATE_WAIT_BUSY};
    enum {DATA_STATE_READY, DATA_STATE_WAIT_COMPL, DATA_STATE_WAIT_RXTX, DATA_STATE_WAIT_DMA};
    
    static int const SdPinsAF = 12;
    using SdPinsMode = Stm32f4PinOutputMode<Stm32f4PinOutputTypeNormal, Stm32f4PinOutputSpeedHigh, Stm32f4PinPullModePullUp>;
    
    using SdPinCK = Stm32f4Pin<Stm32f4PortC, 12>;
    using SdPinCmd = Stm32f4Pin<Stm32f4PortD, 2>;
    using SdPinD0 = Stm32f4Pin<Stm32f4PortC, 8>;
    using SdPinD1 = Stm32f4Pin<Stm32f4PortC, 9>;
    using SdPinD2 = Stm32f4Pin<Stm32f4PortC, 10>;
    using SdPinD3 = Stm32f4Pin<Stm32f4PortC, 11>;
    
    static SDIO_TypeDef * sdio () { return SDIO; }
    
    static typename TheClockUtils::TimeType const DmaTimeoutTicks = 0.5 * TheClockUtils::time_freq;
    static typename TheClockUtils::TimeType const BusyTimeoutTicks = BusyTimeout::value() * TheClockUtils::time_freq;
    
public:
    static bool const IsWideMode = Params::IsWideMode;
    static size_t const BlockSize = 512;
    static size_t const MaxIoBlocks = -1;
    static int const MaxIoDescriptors = INT_MAX;
    
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
        
        NVIC_DisableIRQ(SDIO_IRQn);
        NVIC_SetPriority(SDIO_IRQn, INTERRUPT_PRIORITY-1);
        
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
        TheDebugObject::access(c);
        
        reset_internal(c);
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
            AMBRO_ASSERT(cmd_params.num_blocks <= MaxIoBlocks)
            AMBRO_ASSERT(CheckTransferVector(cmd_params.data_vector, cmd_params.num_blocks * (size_t)(BlockSize/4)))
            
            o->data_num_blocks = cmd_params.num_blocks;
            o->data_vector = cmd_params.data_vector;
            
            if (cmd_params.direction == SdioIface::DATA_DIR_READ) {
                start_data(c);
            }
        }
        
        sdio()->ICR = SDIO_FLAG_CMDSENT|SDIO_FLAG_CCRCFAIL|SDIO_FLAG_CTIMEOUT|SDIO_FLAG_CMDREND;
        
        uint32_t response_sdio;
        switch (cmd_params.response_type) {
            case SdioIface::RESPONSE_NONE:
                response_sdio = SDIO_RESPONSE_NO; break;
            case SdioIface::RESPONSE_SHORT:
            case SdioIface::RESPONSE_SHORT_BUSY:
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
    
    // This interrupt is called when we can transfer at least 8 words
    // from the FIFO (read) or into the FIFO (write). It must be fast!
    AMBRO_ALWAYS_INLINE
    static void sdio_irq (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        //AMBRO_ASSERT(o->data_dir == SdioIface::DATA_DIR_READ || o->data_dir == SdioIface::DATA_DIR_WRITE)
        //AMBRO_ASSERT(o->descriptor_index < o->data_vector.num_descriptors)
        //AMBRO_ASSERT(o->buffer_ptr[0] != o->buffer_ptr[1])
        
        // Fetch the current buffer pointers.
        uint32_t *buffer_ptr0 = o->buffer_ptr[0];
        uint32_t *buffer_ptr1 = o->buffer_ptr[1];
        
        // Compute the difference between the addresses.
        // The sign of this determined whether we are reading or writing.
        uint32_t buffer_diff = (char *)buffer_ptr1 - (char *)buffer_ptr0;
        
        // Note that while we will be doing the transfers, we could get a positive
        // edges of the RXFIFOHF/TXFIFOHE flag, but anyway end up up with the
        // flag cleared. Due to the edge the NVIC would immediately reschedule
        // the interrupt, and we could then run without sufficient data/space in
        // the FIFO. So we will use NVIC_ClearPendingIRQ before allowing another
        // interrupt to fire.
        
        if (buffer_diff < UINT32_C(0x80000000)) {
            // We are reading - buffer_diff is the number of bytes left.
            
            // Read words.
            if (AMBRO_LIKELY(buffer_diff >= 32)) {
                uint32_t fifo_out;
                asm(
                    "ldmia %[fifo]!, {r4, r5, r6, r7}\n"
                    "stmia %[buffer_ptr0]!, {r4, r5, r6, r7}\n"
                    "ldmia %[fifo]!, {r4, r5, r6, r7}\n"
                    "stmia %[buffer_ptr0]!, {r4, r5, r6, r7}\n"
                    : [buffer_ptr0] "=&r" (buffer_ptr0),
                      [fifo] "=&r" (fifo_out)
                    : "[buffer_ptr0]" (buffer_ptr0),
                      "[fifo]" (&sdio()->FIFO)
                    : "r4", "r5", "r6", "r7", "memory"
                );
            } else {
                int rem_words = buffer_diff / 4;
                int i = 0;
                do {
                    buffer_ptr0[i++] = sdio()->FIFO;
                } while (i < rem_words);
                buffer_ptr0 = buffer_ptr1;
            }
            
            // More bytes in buffer?
            if (AMBRO_LIKELY(buffer_ptr0 < buffer_ptr1)) {
                o->buffer_ptr[0] = buffer_ptr0;
                NVIC_ClearPendingIRQ(SDIO_IRQn);
                return;
            }
        } else {
            // We are writing - negate buffer_diff to get the number of bytes left.
            buffer_diff = -buffer_diff;
            
            // Write words.
            if (AMBRO_LIKELY(buffer_diff >= 32)) {
                uint32_t fifo_out;
                asm(
                    "ldmia %[buffer_ptr1]!, {r4, r5, r6, r7}\n"
                    "stmia %[fifo]!, {r4, r5, r6, r7}\n"
                    "ldmia %[buffer_ptr1]!, {r4, r5, r6, r7}\n"
                    "stmia %[fifo]!, {r4, r5, r6, r7}\n"
                    : [buffer_ptr1] "=&r" (buffer_ptr1),
                    [fifo] "=&r" (fifo_out)
                    : "[buffer_ptr1]" (buffer_ptr1),
                    "[fifo]" (&sdio()->FIFO)
                    : "r4", "r5", "r6", "r7", "memory"
                );
            } else {
                int rem_words = buffer_diff / 4;
                int i = 0;
                do {
                    sdio()->FIFO = buffer_ptr1[i++];
                } while (i < rem_words);
                buffer_ptr1 = buffer_ptr0;
            }
            
            // More bytes in buffer?
            if (AMBRO_LIKELY(buffer_ptr1 < buffer_ptr0)) {
                o->buffer_ptr[1] = buffer_ptr1;
                NVIC_ClearPendingIRQ(SDIO_IRQn);
                return;
            }
        }
        
        // Moving to the next buffer.
        int descriptor_index = o->descriptor_index + 1;
        o->descriptor_index = descriptor_index;
        
        // If this was the last buffer, disable the IRQ, and stop.
        if (AMBRO_UNLIKELY(descriptor_index >= o->data_vector.num_descriptors)) {
            NVIC_DisableIRQ(SDIO_IRQn);
            return;
        }
        
        // Load the new buffer pointers.
        auto desc = o->data_vector.descriptors[descriptor_index];
        bool buffer_order = (o->data_dir == SdioIface::DATA_DIR_READ);
        o->buffer_ptr[!buffer_order] = desc.buffer_ptr;
        o->buffer_ptr[ buffer_order] = desc.buffer_ptr + desc.num_words;
        
        NVIC_ClearPendingIRQ(SDIO_IRQn);
    }
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
    static void msp_init (Context c)
    {
        __HAL_RCC_SDIO_CLK_ENABLE();
    }
    
    static void msp_deinit (Context c)
    {
        __HAL_RCC_SDIO_CLK_DISABLE();
    }
    
    static void configure_interface (SdioIface::InterfaceParams if_params)
    {
        SD_InitTypeDef tmpinit = SD_InitTypeDef();
        tmpinit.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
        tmpinit.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
        tmpinit.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
        tmpinit.BusWide             = if_params.wide_data_bus ? SDIO_BUS_WIDE_4B : SDIO_BUS_WIDE_1B;
        tmpinit.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
        tmpinit.ClockDiv            = if_params.clock_full_speed ? Params::SdClockPrescaler : SDIO_INIT_CLK_DIV;
        SDIO_Init(sdio(), tmpinit);
    }
    
    static void start_data (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->data_state == DATA_STATE_READY)
        AMBRO_ASSERT(o->data_dir == SdioIface::DATA_DIR_READ || o->data_dir == SdioIface::DATA_DIR_WRITE)
        AMBRO_ASSERT(o->data_vector.num_descriptors > 0)
        
        sdio()->ICR = SDIO_FLAG_DCRCFAIL|SDIO_FLAG_DTIMEOUT|SDIO_FLAG_RXOVERR|SDIO_FLAG_TXUNDERR|SDIO_FLAG_STBITERR|SDIO_FLAG_DATAEND;
        
        o->data_state = DATA_STATE_WAIT_COMPL;
        o->descriptor_index = 0;
        
        auto desc = o->data_vector.descriptors[0];
        bool buffer_order = (o->data_dir == SdioIface::DATA_DIR_READ);
        o->buffer_ptr[!buffer_order] = desc.buffer_ptr;
        o->buffer_ptr[ buffer_order] = desc.buffer_ptr + desc.num_words;
        
        memory_barrier();
        
        if (o->data_dir == SdioIface::DATA_DIR_READ) {
            sdio()->MASK = SDIO_MASK_RXFIFOHFIE;
        } else {
            sdio()->MASK = SDIO_MASK_TXFIFOHEIE;
        }
        
        NVIC_ClearPendingIRQ(SDIO_IRQn);
        NVIC_EnableIRQ(SDIO_IRQn);
        
        SDIO_DataInitTypeDef data_init = SDIO_DataInitTypeDef();
        data_init.DataTimeOut   = Params::DataTimeoutBusClocks;
        data_init.DataLength    = (uint32_t)o->data_num_blocks * BlockSize;
        data_init.DataBlockSize = SDIO_DATABLOCK_SIZE_512B;
        data_init.TransferDir   = o->data_dir == SdioIface::DATA_DIR_READ ? SDIO_TRANSFER_DIR_TO_SDIO : SDIO_TRANSFER_DIR_TO_CARD;
        data_init.TransferMode  = SDIO_TRANSFER_MODE_BLOCK;
        data_init.DPSM          = SDIO_DPSM_ENABLE;
        SDIO_DataConfig(sdio(), &data_init);
    }
    
    static void reset_internal (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->init_state == INIT_STATE_ON) {
            if (o->cmd_state != CMD_STATE_READY) {
                sdio()->CMD = 0;
            }
            
            if (o->data_state != DATA_STATE_READY) {
                NVIC_DisableIRQ(SDIO_IRQn);
                sdio()->DCTRL = 0;
            }
        }
        
        if (o->init_state >= INIT_STATE_POWERON) {
            SDIO_PowerState_OFF(sdio());
            msp_deinit(c);
        }
        
        o->init_state = INIT_STATE_OFF;
        
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
        AMBRO_ASSERT(o->cmd_state == CMD_STATE_READY || o->cmd_state == CMD_STATE_BUSY || o->cmd_state == CMD_STATE_WAIT_BUSY)
        
        if (o->cmd_state == CMD_STATE_READY) {
            return;
        }
        
        uint32_t status = sdio()->STA;
        
        if (o->cmd_state == CMD_STATE_BUSY) {
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
                
                if (o->cmd_response_type == SdioIface::RESPONSE_SHORT_BUSY) {
                    o->cmd_state = CMD_STATE_WAIT_BUSY;
                    o->busy_poll_timer.setAfter(c, BusyTimeoutTicks);
                }
            }
        }
        
        if (o->cmd_state == CMD_STATE_WAIT_BUSY) {
            // The SDIO peripheral does not indicate the busy status but
            // looking at the D0 pin level seems to work.
            if (!(Context::Pins::template get<SdPinD0>(c))) {
                if (!(o->busy_poll_timer.isExpired(c))) {
                    return;
                }
                o->results.error_code = SdioIface::CMD_ERROR_BUSY_TIMEOUT;
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
                    
                    // When reading, we need to make sure the last words in the FIFO get flushed to memory,
                    // in case less than 8 words remain which would not trigger the RXFIFOHF flag.
                    // There is no such issue for writing, because as SDIO transmits, there will eventually
                    // be at least 8 words free in the FIFO, triggering the interrupt.
                    if (o->data_dir == SdioIface::DATA_DIR_READ) {
                        sdio()->MASK = SDIO_MASK_RXDAVLIE;
                    }
                    
                    o->data_state = DATA_STATE_WAIT_DMA;
                    o->poll_timer.setAfter(c, DmaTimeoutTicks);
                } break;
                
                case DATA_STATE_WAIT_DMA: {
                    if (o->data_error == SdioIface::DATA_ERROR_NONE) {
                        // Need to wait for the transfer to complete.
                        int descriptor_index;
                        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                            descriptor_index = o->descriptor_index;
                        }
                        if (descriptor_index < o->data_vector.num_descriptors) {
                            if (!o->poll_timer.isExpired(c)) {
                                return;
                            }
                            o->data_error = SdioIface::DATA_ERROR_DMA;
                        }
                    }
                    
                    NVIC_DisableIRQ(SDIO_IRQn);
                    sdio()->DCTRL = 0;
                    
                    memory_barrier();
                    
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
        uint8_t init_state;
        bool pending;
        uint8_t cmd_state;
        uint8_t data_state;
        uint8_t cmd_index;
        uint8_t cmd_response_type;
        uint8_t cmd_flags;
        uint8_t data_dir;
        size_t data_num_blocks;
        TransferVector<uint32_t> data_vector;
        int descriptor_index;
        uint32_t *buffer_ptr[2];
        SdioIface::CommandResults results;
        SdioIface::DataErrorCode data_error;
        typename TheClockUtils::PollTimer poll_timer;
        typename TheClockUtils::PollTimer busy_poll_timer;
    };
};

#define APRINTER_STM32F4_SDIO_GLOBAL(sdio, context) \
extern "C" \
__attribute__((used)) \
void SDIO_IRQHandler (void) \
{ \
    sdio::sdio_irq(MakeInterruptContext((context))); \
}

template <
    bool TIsWideMode,
    uint32_t TDataTimeoutBusClocks,
    int TSdClockPrescaler
>
struct Stm32f4SdioService {
    static bool const IsWideMode = TIsWideMode;
    static uint32_t const DataTimeoutBusClocks = TDataTimeoutBusClocks;
    static int const SdClockPrescaler = TSdClockPrescaler;
    
    template <typename Context, typename ParentObject, typename CommandHandler, typename BusyTimeout>
    using Sdio = Stm32f4Sdio<Context, ParentObject, CommandHandler, BusyTimeout, Stm32f4SdioService>;
};

#include <aprinter/EndNamespace.h>

#endif
