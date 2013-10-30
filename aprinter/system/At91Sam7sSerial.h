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

#ifndef AMBROLIB_AT91SAM7S_SERIAL_H
#define AMBROLIB_AT91SAM7S_SERIAL_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

void At91Sam7sSerial_Irq (void);

#include <aprinter/BeginNamespace.h>

struct At91Sam7sSerialParams {};

template <typename Position, typename Context, int RecvBufferBits, int SendBufferBits, typename Params, typename RecvHandler, typename SendHandler>
class At91Sam7sSerial
: private DebugObject<Context, void>
{
private:
    using RecvFastEvent = typename Context::EventLoop::template FastEventSpec<At91Sam7sSerial>;
    using SendFastEvent = typename Context::EventLoop::template FastEventSpec<RecvFastEvent>;
    
    static At91Sam7sSerial * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
    static void init (Context c, uint32_t baud)
    {
        At91Sam7sSerial *o = self(c);
        
        o->m_lock.init(c);
        
        c.eventLoop()->template initFastEvent<RecvFastEvent>(c, At91Sam7sSerial::recv_event_handler);
        o->m_recv_start = RecvSizeType::import(0);
        o->m_recv_end = RecvSizeType::import(0);
        o->m_recv_overrun = false;
        
        c.eventLoop()->template initFastEvent<SendFastEvent>(c, At91Sam7sSerial::send_event_handler);
        o->m_send_start = SendSizeType::import(0);
        o->m_send_end = SendSizeType::import(0);;
        o->m_send_event = BoundedModuloInc(o->m_send_end);
        
        at91sam7s_pmc_enable_periph(AT91C_ID_US0);
        AT91C_BASE_US0->US_CR = AT91C_US_RSTTX | AT91C_US_RSTRX;
        AT91C_BASE_PIOA->PIO_ODR = (UINT32_C(1) << 5);
        AT91C_BASE_PIOA->PIO_PER = (UINT32_C(1) << 5);
        AT91C_BASE_PIOA->PIO_ASR = (UINT32_C(1) << 6);
        AT91C_BASE_PIOA->PIO_PDR = (UINT32_C(1) << 6);
        AT91C_BASE_US0->US_BRGR = (uint16_t)((F_MCK / (16.0 * baud)) + 0.5);
        AT91C_BASE_US0->US_MR = AT91C_US_INACK | AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_NONE | AT91C_US_CHRL_8_BITS | AT91C_US_CLKS_CLOCK | AT91C_US_USMODE_NORMAL;
        AT91C_BASE_US0->US_TTGR = 0;
        AT91C_BASE_US0->US_IDR = UINT32_MAX;
        at91sam7s_aic_register_irq(AT91C_ID_US0, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, 4, At91Sam7sSerial_Irq);
        at91sam7s_aic_enable_irq(AT91C_ID_US0);
        AT91C_BASE_US0->US_IER = AT91C_US_RXRDY;
        AT91C_BASE_US0->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugDeinit(c);
        
        at91sam7s_aic_disable_irq(AT91C_ID_US0);
        AT91C_BASE_US0->US_CR = AT91C_US_RSTTX | AT91C_US_RSTRX;
        at91sam7s_pmc_disable_periph(AT91C_ID_US0);
        
        c.eventLoop()->template resetFastEvent<SendFastEvent>(c);
        c.eventLoop()->template resetFastEvent<RecvFastEvent>(c);
        o->m_lock.deinit(c);
    }
    
    static RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(out_overrun)
        
        RecvSizeType end;
        bool overrun;
        AMBRO_LOCK_T(o->m_lock, c, lock_c) {
            end = o->m_recv_end;
            overrun = o->m_recv_overrun;
        }
        
        *out_overrun = overrun;
        return recv_avail(o->m_recv_start, end);
    }
    
    static char * recvGetChunkPtr (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        return (o->m_recv_buffer + o->m_recv_start.value());
    }
    
    static void recvConsume (Context c, RecvSizeType amount)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(o->m_lock, c, lock_c) {
            AMBRO_ASSERT(amount <= recv_avail(o->m_recv_start, o->m_recv_end))
            o->m_recv_start = BoundedModuloAdd(o->m_recv_start, amount);
        }
    }
    
    static void recvClearOverrun (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_recv_overrun)
        
        o->m_recv_overrun = false;
        
        while ((AT91C_BASE_US0->US_CSR & AT91C_US_RXRDY)) {
            (void)AT91C_BASE_US0->US_RHR;
        }
        
        AT91C_BASE_US0->US_IER = AT91C_US_RXRDY;
    }
    
    static void recvForceEvent (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        c.eventLoop()->template triggerFastEvent<RecvFastEvent>(c);
    }
    
    static SendSizeType sendQuery (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        SendSizeType start;
        AMBRO_LOCK_T(o->m_lock, c, lock_c) {
            start = o->m_send_start;
        }
        
        return send_avail(start, o->m_send_end);
    }
    
    static SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        if (o->m_send_end.value() > 0 && rem_length > BoundedModuloNegative(o->m_send_end)) {
            rem_length = BoundedModuloNegative(o->m_send_end);
        }
        
        return rem_length;
    }
    
    static char * sendGetChunkPtr (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        return (o->m_send_buffer + o->m_send_end.value());
    }
    
    static void sendProvide (Context c, SendSizeType amount)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(o->m_lock, c, lock_c) {
            AMBRO_ASSERT(amount <= send_avail(o->m_send_start, o->m_send_end))
            o->m_send_end = BoundedModuloAdd(o->m_send_end, amount);
            o->m_send_event = BoundedModuloAdd(o->m_send_event, amount);
            AT91C_BASE_US0->US_IER = AT91C_US_TXRDY;
        }
    }
    
    static void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(min_amount > SendSizeType::import(0))
        
        AMBRO_LOCK_T(o->m_lock, c, lock_c) {
            if (send_avail(o->m_send_start, o->m_send_end) >= min_amount) {
                o->m_send_event = BoundedModuloInc(o->m_send_end);
                c.eventLoop()->template triggerFastEvent<SendFastEvent>(lock_c);
            } else {
                o->m_send_event = BoundedModuloAdd(BoundedModuloInc(o->m_send_end), min_amount);
                c.eventLoop()->template resetFastEvent<SendFastEvent>(c);
            }
        }
    }
    
    static void sendCancelEvent (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(o->m_lock, c, lock_c) {
            o->m_send_event = BoundedModuloInc(o->m_send_end);
            c.eventLoop()->template resetFastEvent<SendFastEvent>(c);
        }
    }
    
    static void sendWaitFinished (Context c)
    {
        At91Sam7sSerial *o = self(c);
        bool not_finished;
        do {
            bool ie = interrupts_enabled();
            if (ie) {
                cli();
            }
            not_finished = (o->m_send_start != o->m_send_end);
            if (ie) {
                sei();
            }
        } while (not_finished);
    }
    
    static void usart_irq (InterruptContext<Context> c)
    {
        At91Sam7sSerial *o = self(c);
        uint32_t status = AT91C_BASE_US0->US_CSR;
        
        if (!o->m_recv_overrun && (status & AT91C_US_RXRDY)) {
            RecvSizeType new_end = BoundedModuloInc(o->m_recv_end);
            if (new_end != o->m_recv_start) {
                uint8_t ch = AT91C_BASE_US0->US_RHR;
                o->m_recv_buffer[o->m_recv_end.value()] = *(char *)&ch;
                o->m_recv_buffer[o->m_recv_end.value() + (sizeof(o->m_recv_buffer) / 2)] = *(char *)&ch;
                o->m_recv_end = new_end;
            } else {
                o->m_recv_overrun = true;
                AT91C_BASE_US0->US_IDR = AT91C_US_RXRDY;
            }
            
            c.eventLoop()->template triggerFastEvent<RecvFastEvent>(c);
        }
        
        if (o->m_send_start != o->m_send_end && (status & AT91C_US_TXRDY)) {
            AT91C_BASE_US0->US_THR = *(uint8_t *)&o->m_send_buffer[o->m_send_start.value()];
            o->m_send_start = BoundedModuloInc(o->m_send_start);
            
            if (o->m_send_start == o->m_send_end) {
                AT91C_BASE_US0->US_IDR = AT91C_US_TXRDY;
            }
            
            if (o->m_send_start == o->m_send_event) {
                o->m_send_event = BoundedModuloInc(o->m_send_end);
                c.eventLoop()->template triggerFastEvent<SendFastEvent>(c);
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
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        RecvHandler::call(o, c);
    }
    
    static void send_event_handler (Context c)
    {
        At91Sam7sSerial *o = self(c);
        o->debugAccess(c);
        
        SendHandler::call(o, c);
    }
    
    InterruptLock<Context> m_lock;
    
    RecvSizeType m_recv_start;
    RecvSizeType m_recv_end;
    bool m_recv_overrun;
    char m_recv_buffer[2 * ((size_t)RecvSizeType::maxIntValue() + 1)];
    
    SendSizeType m_send_start;
    SendSizeType m_send_end;
    SendSizeType m_send_event;
    char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
};

#define AMBRO_AT91SAM7S_SERIAL_GLOBAL(the_serial, context) \
void At91Sam7sSerial_Irq (void) \
{ \
    (the_serial).usart_irq(MakeInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
