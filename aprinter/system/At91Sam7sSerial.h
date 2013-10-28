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

#include <aprinter/meta/IntTypeInfo.h>
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
    
public:
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
    void init (Context c, uint32_t baud)
    {
        m_lock.init(c);
        
        c.eventLoop()->template initFastEvent<RecvFastEvent>(c, At91Sam7sSerial::recv_event_handler);
        m_recv_start = RecvSizeType::import(0);
        m_recv_end = RecvSizeType::import(0);
        m_recv_overrun = false;
        
        c.eventLoop()->template initFastEvent<SendFastEvent>(c, At91Sam7sSerial::send_event_handler);
        m_send_start = SendSizeType::import(0);
        m_send_end = SendSizeType::import(0);;
        m_send_event = BoundedModuloInc(m_send_end);
        
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
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        at91sam7s_aic_disable_irq(AT91C_ID_US0);
        AT91C_BASE_US0->US_CR = AT91C_US_RSTTX | AT91C_US_RSTRX;
        at91sam7s_pmc_disable_periph(AT91C_ID_US0);
        
        c.eventLoop()->template resetFastEvent<SendFastEvent>(c);
        c.eventLoop()->template resetFastEvent<RecvFastEvent>(c);
        m_lock.deinit(c);
    }
    
    RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(out_overrun)
        
        RecvSizeType end;
        bool overrun;
        AMBRO_LOCK_T(m_lock, c, lock_c) {
            end = m_recv_end;
            overrun = m_recv_overrun;
        }
        
        *out_overrun = overrun;
        return recv_avail(m_recv_start, end);
    }
    
    char * recvGetChunkPtr (Context c)
    {
        this->debugAccess(c);
        
        return (m_recv_buffer + m_recv_start.value());
    }
    
    void recvConsume (Context c, RecvSizeType amount)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c) {
            AMBRO_ASSERT(amount <= recv_avail(m_recv_start, m_recv_end))
            m_recv_start = BoundedModuloAdd(m_recv_start, amount);
        }
    }
    
    void recvClearOverrun (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_recv_overrun)
        
        m_recv_overrun = false;
        
        while ((AT91C_BASE_US0->US_CSR & AT91C_US_RXRDY)) {
            (void)AT91C_BASE_US0->US_RHR;
        }
        
        AT91C_BASE_US0->US_IER = AT91C_US_RXRDY;
    }
    
    void recvForceEvent (Context c)
    {
        this->debugAccess(c);
        
        c.eventLoop()->template triggerFastEvent<RecvFastEvent>(c);
    }
    
    SendSizeType sendQuery (Context c)
    {
        this->debugAccess(c);
        
        SendSizeType start;
        AMBRO_LOCK_T(m_lock, c, lock_c) {
            start = m_send_start;
        }
        
        return send_avail(start, m_send_end);
    }
    
    SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        this->debugAccess(c);
        
        if (m_send_end.value() > 0 && rem_length > BoundedModuloNegative(m_send_end)) {
            rem_length = BoundedModuloNegative(m_send_end);
        }
        
        return rem_length;
    }
    
    char * sendGetChunkPtr (Context c)
    {
        this->debugAccess(c);
        
        return (m_send_buffer + m_send_end.value());
    }
    
    void sendProvide (Context c, SendSizeType amount)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c) {
            AMBRO_ASSERT(amount <= send_avail(m_send_start, m_send_end))
            m_send_end = BoundedModuloAdd(m_send_end, amount);
            m_send_event = BoundedModuloAdd(m_send_event, amount);
            AT91C_BASE_US0->US_IER = AT91C_US_TXRDY;
        }
    }
    
    void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount > SendSizeType::import(0))
        
        AMBRO_LOCK_T(m_lock, c, lock_c) {
            if (send_avail(m_send_start, m_send_end) >= min_amount) {
                m_send_event = BoundedModuloInc(m_send_end);
                c.eventLoop()->template triggerFastEvent<SendFastEvent>(lock_c);
            } else {
                m_send_event = BoundedModuloAdd(BoundedModuloInc(m_send_end), min_amount);
                c.eventLoop()->template resetFastEvent<SendFastEvent>(c);
            }
        }
    }
    
    void sendCancelEvent (Context c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c) {
            m_send_event = BoundedModuloInc(m_send_end);
            c.eventLoop()->template resetFastEvent<SendFastEvent>(c);
        }
    }
    
    void sendWaitFinished ()
    {
        bool not_finished;
        do {
            bool ie = interrupts_enabled();
            if (ie) {
                cli();
            }
            not_finished = (m_send_start != m_send_end);
            if (ie) {
                sei();
            }
        } while (not_finished);
    }
    
    void usart_irq (InterruptContext<Context> c)
    {
        uint32_t status = AT91C_BASE_US0->US_CSR;
        
        if (!m_recv_overrun && (status & AT91C_US_RXRDY)) {
            RecvSizeType new_end = BoundedModuloInc(m_recv_end);
            if (new_end != m_recv_start) {
                uint8_t ch = AT91C_BASE_US0->US_RHR;
                m_recv_buffer[m_recv_end.value()] = *(char *)&ch;
                m_recv_buffer[m_recv_end.value() + (sizeof(m_recv_buffer) / 2)] = *(char *)&ch;
                m_recv_end = new_end;
            } else {
                m_recv_overrun = true;
                AT91C_BASE_US0->US_IDR = AT91C_US_RXRDY;
            }
            
            c.eventLoop()->template triggerFastEvent<RecvFastEvent>(c);
        }
        
        if (m_send_start != m_send_end && (status & AT91C_US_TXRDY)) {
            AT91C_BASE_US0->US_THR = *(uint8_t *)&m_send_buffer[m_send_start.value()];
            m_send_start = BoundedModuloInc(m_send_start);
            
            if (m_send_start == m_send_end) {
                AT91C_BASE_US0->US_IDR = AT91C_US_TXRDY;
            }
            
            if (m_send_start == m_send_event) {
                m_send_event = BoundedModuloInc(m_send_end);
                c.eventLoop()->template triggerFastEvent<SendFastEvent>(c);
            }
        }
    }
    
    using EventLoopFastEvents = MakeTypeList<RecvFastEvent, SendFastEvent>;
    
private:
    RecvSizeType recv_avail (RecvSizeType start, RecvSizeType end)
    {
        return BoundedModuloSubtract(end, start);
    }
    
    SendSizeType send_avail (SendSizeType start, SendSizeType end)
    {
        return BoundedModuloDec(BoundedModuloSubtract(start, end));
    }
    
    static void recv_event_handler (Context c)
    {
        At91Sam7sSerial *o = PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
        o->debugAccess(c);
        
        RecvHandler::call(o, c);
    }
    
    static void send_event_handler (Context c)
    {
        At91Sam7sSerial *o = PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
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
