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

#ifndef AMBROLIB_AT91SAM3X_SERIAL_H
#define AMBROLIB_AT91SAM3X_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <sam/drivers/pmc/pmc.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler, typename Params>
class At91Sam3xSerial {
private:
    using RecvFastEvent = typename Context::EventLoop::template FastEventSpec<At91Sam3xSerial>;
    using SendFastEvent = typename Context::EventLoop::template FastEventSpec<RecvFastEvent>;
    
public:
    struct Object;
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
    static void init (Context c, uint32_t baud)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<RecvFastEvent>(c, At91Sam3xSerial::recv_event_handler);
        o->m_recv_start = RecvSizeType::import(0);
        o->m_recv_end = RecvSizeType::import(0);
        o->m_recv_overrun = false;
        
        Context::EventLoop::template initFastEvent<SendFastEvent>(c, At91Sam3xSerial::send_event_handler);
        o->m_send_start = SendSizeType::import(0);
        o->m_send_end = SendSizeType::import(0);;
        o->m_send_event = BoundedModuloInc(o->m_send_end);
        
        pmc_enable_periph_clk(ID_UART);
        UART->UART_CR = UART_CR_RSTTX | UART_CR_RSTRX;
        PIOA->PIO_ODR = (UINT32_C(1) << 8);
        PIOA->PIO_PER = (UINT32_C(1) << 8);
        PIOA->PIO_ABSR &= ~(UINT32_C(1) << 9);
        PIOA->PIO_PDR = (UINT32_C(1) << 9);
        UART->UART_BRGR = (uint16_t)(((float)F_MCK / (16.0f * baud)) + 0.5f);
        UART->UART_MR = UART_MR_PAR_NO | UART_MR_CHMODE_NORMAL;
        UART->UART_IDR = UINT32_MAX;
        NVIC_ClearPendingIRQ(UART_IRQn);
        NVIC_SetPriority(UART_IRQn, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(UART_IRQn);
        UART->UART_IER = UART_IER_RXRDY;
        UART->UART_CR = UART_CR_TXEN | UART_CR_RXEN;
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        NVIC_DisableIRQ(UART_IRQn);
        UART->UART_CR = UART_CR_RSTTX | UART_CR_RSTRX;
        NVIC_ClearPendingIRQ(UART_IRQn);
        pmc_disable_periph_clk(ID_UART);
        
        Context::EventLoop::template resetFastEvent<SendFastEvent>(c);
        Context::EventLoop::template resetFastEvent<RecvFastEvent>(c);
    }
    
    static RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(out_overrun)
        
        RecvSizeType end;
        bool overrun;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            end = o->m_recv_end;
            overrun = o->m_recv_overrun;
        }
        
        *out_overrun = overrun;
        return recv_avail(o->m_recv_start, end);
    }
    
    static char * recvGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        return (o->m_recv_buffer + o->m_recv_start.value());
    }
    
    static void recvConsume (Context c, RecvSizeType amount)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            AMBRO_ASSERT(amount <= recv_avail(o->m_recv_start, o->m_recv_end))
            o->m_recv_start = BoundedModuloAdd(o->m_recv_start, amount);
        }
    }
    
    static void recvClearOverrun (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_recv_overrun)
        
        o->m_recv_overrun = false;
        
        while ((UART->UART_SR & UART_SR_RXRDY)) {
            (void)UART->UART_RHR;
        }
        
        UART->UART_IER = UART_IER_RXRDY;
    }
    
    static void recvForceEvent (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
    }
    
    static SendSizeType sendQuery (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        SendSizeType start;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            start = o->m_send_start;
        }
        
        return send_avail(start, o->m_send_end);
    }
    
    static SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        if (o->m_send_end.value() > 0 && rem_length > BoundedModuloNegative(o->m_send_end)) {
            rem_length = BoundedModuloNegative(o->m_send_end);
        }
        
        return rem_length;
    }
    
    static char * sendGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        return (o->m_send_buffer + o->m_send_end.value());
    }
    
    static void sendProvide (Context c, SendSizeType amount)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            AMBRO_ASSERT(amount <= send_avail(o->m_send_start, o->m_send_end))
            o->m_send_end = BoundedModuloAdd(o->m_send_end, amount);
            o->m_send_event = BoundedModuloAdd(o->m_send_event, amount);
            UART->UART_IER = UART_IER_TXRDY;
        }
    }
    
    static void sendPoke (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
    }
    
    static void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(min_amount > SendSizeType::import(0))
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            if (send_avail(o->m_send_start, o->m_send_end) >= min_amount) {
                o->m_send_event = BoundedModuloInc(o->m_send_end);
                Context::EventLoop::template triggerFastEvent<SendFastEvent>(lock_c);
            } else {
                o->m_send_event = BoundedModuloAdd(BoundedModuloInc(o->m_send_end), min_amount);
                Context::EventLoop::template resetFastEvent<SendFastEvent>(c);
            }
        }
    }
    
    static void sendCancelEvent (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            o->m_send_event = BoundedModuloInc(o->m_send_end);
            Context::EventLoop::template resetFastEvent<SendFastEvent>(c);
        }
    }
    
    static void sendWaitFinished (Context c)
    {
        auto *o = Object::self(c);
        bool not_finished;
        do {
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                not_finished = (o->m_send_start != o->m_send_end);
            }
        } while (not_finished);
    }
    
    static void uart_irq (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        uint32_t status = UART->UART_SR;
        
        if (!o->m_recv_overrun && (status & UART_SR_RXRDY)) {
            RecvSizeType new_end = BoundedModuloInc(o->m_recv_end);
            if (new_end != o->m_recv_start) {
                uint8_t ch = UART->UART_RHR;
                o->m_recv_buffer[o->m_recv_end.value()] = *(char *)&ch;
                o->m_recv_buffer[o->m_recv_end.value() + (sizeof(o->m_recv_buffer) / 2)] = *(char *)&ch;
                o->m_recv_end = new_end;
            } else {
                o->m_recv_overrun = true;
                UART->UART_IDR = UART_IDR_RXRDY;
            }
            
            Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
        }
        
        if (o->m_send_start != o->m_send_end && (status & UART_SR_TXRDY)) {
            UART->UART_THR = *(uint8_t *)&o->m_send_buffer[o->m_send_start.value()];
            o->m_send_start = BoundedModuloInc(o->m_send_start);
            
            if (o->m_send_start == o->m_send_end) {
                UART->UART_IDR = UART_IDR_TXRDY;
            }
            
            if (o->m_send_start == o->m_send_event) {
                o->m_send_event = BoundedModuloInc(o->m_send_end);
                Context::EventLoop::template triggerFastEvent<SendFastEvent>(c);
            }
        }
    }
    
    using EventLoopFastEvents = MakeTypeList<RecvFastEvent, SendFastEvent>;
    
private:
    static RecvSizeType recv_avail (RecvSizeType start, RecvSizeType end)
    {
        return BoundedModuloSubtract(end, start);
    }
    
    static SendSizeType send_avail (SendSizeType start, SendSizeType end)
    {
        return BoundedModuloDec(BoundedModuloSubtract(start, end));
    }
    
    static void recv_event_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        RecvHandler::call(c);
    }
    
    static void send_event_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        SendHandler::call(c);
    }
    
public:
    struct Object : public ObjBase<At91Sam3xSerial, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        RecvSizeType m_recv_start;
        RecvSizeType m_recv_end;
        bool m_recv_overrun;
        char m_recv_buffer[2 * ((size_t)RecvSizeType::maxIntValue() + 1)];
        SendSizeType m_send_start;
        SendSizeType m_send_end;
        SendSizeType m_send_event;
        char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
    };
};

struct At91Sam3xSerialService {
    template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler>
    using Serial = At91Sam3xSerial<Context, ParentObject, RecvBufferBits, SendBufferBits, RecvHandler, SendHandler, At91Sam3xSerialService>;
};

#define AMBRO_AT91SAM3X_SERIAL_GLOBAL(the_serial, context) \
extern "C" \
__attribute__((used)) \
void UART_Handler (void) \
{ \
    the_serial::uart_irq(MakeInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
