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

#ifndef AMBROLIB_AVR_SERIAL_H
#define AMBROLIB_AVR_SERIAL_H

#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/meta/BoundedInt.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/AvrLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler>
class AvrSerial
: private DebugObject<Context, void>
{
private:
    typedef typename Context::EventLoop Loop;
    
public:
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
    void init (Context c, uint32_t baud)
    {
        m_lock.init(c);
        
        m_recv_queued_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AvrSerial::m_recv_queued_event, &AvrSerial::recv_queued_event_handler));
        m_recv_start = RecvSizeType::import(0);
        m_recv_end = RecvSizeType::import(0);
        m_recv_overrun = false;
        
        m_send_queued_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AvrSerial::m_send_queued_event, &AvrSerial::send_queued_event_handler));
        m_send_start = SendSizeType::import(0);
        m_send_end = SendSizeType::import(0);;
        m_send_event = BoundedModuloInc(m_send_end);
        
        uint16_t ubrr = (((2 * (uint32_t)F_CPU) + (16 * baud)) / (2 * 16 * baud)) - 1;
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            UBRR0H = (ubrr >> 8);
            UBRR0L = ubrr;
            UCSR0A = 0;
            UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
            UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
        });
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            UCSR0C = 0;
            UCSR0B = 0;
            UCSR0A = 0;
            UBRR0L = 0;
            UBRR0H = 0;
        });
        
        m_send_queued_event.deinit(c);
        m_recv_queued_event.deinit(c);
        m_lock.deinit(c);
    }
    
    RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(out_overrun)
        
        RecvSizeType end;
        bool overrun;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            end = m_recv_end;
            overrun = m_recv_overrun;
        });
        
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
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            AMBRO_ASSERT(amount <= recv_avail(m_recv_start, m_recv_end))
            m_recv_start = BoundedModuloAdd(m_recv_start, amount);
        });
    }
    
    void recvClearOverrun (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_recv_overrun)
        
        m_recv_overrun = false;
        
        while ((UCSR0A & (1 << RXC0))) {
            (void)UDR0;
        }
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            UCSR0B |= (1 << RXCIE0);
        });
    }
    
    void recvForceEvent (Context c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_recv_queued_event.appendNowIfNotAlready(lock_c);
        });
    }
    
    SendSizeType sendQuery (Context c)
    {
        this->debugAccess(c);
        
        SendSizeType start;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            start = m_send_start;
        });
        
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
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            AMBRO_ASSERT(amount <= send_avail(m_send_start, m_send_end))
            m_send_end = BoundedModuloAdd(m_send_end, amount);
            m_send_event = BoundedModuloAdd(m_send_event, amount);
            UCSR0B |= (1 << UDRIE0);
        });
    }
    
    void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount > SendSizeType::import(0))
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            if (send_avail(m_send_start, m_send_end) >= min_amount) {
                m_send_event = BoundedModuloInc(m_send_end);
                m_send_queued_event.appendNowIfNotAlready(lock_c);
            } else {
                m_send_event = BoundedModuloAdd(BoundedModuloInc(m_send_end), min_amount);;
                m_send_queued_event.unset(lock_c);
            }
        });
    }
    
    void sendCancelEvent (Context c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_send_event = BoundedModuloInc(m_send_end);
            m_send_queued_event.unset(lock_c, true);
        });
    }
    
    void sendBlock (Context c, SendSizeType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount > SendSizeType::import(0))
        
        bool ok;
        do {
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                ok = (send_avail(m_send_start, m_send_end) >= min_amount);
            });
        } while (!ok);
    }
    
    void rx_isr (AvrInterruptContext<Context> c)
    {
        AMBRO_ASSERT(!m_recv_overrun)
        
        RecvSizeType new_end = BoundedModuloInc(m_recv_end);
        if (new_end != m_recv_start) {
            char ch = UDR0;
            m_recv_buffer[m_recv_end.value()] = ch;
            m_recv_buffer[m_recv_end.value() + (sizeof(m_recv_buffer) / 2)] = ch;
            m_recv_end = new_end;
        } else {
            m_recv_overrun = true;
            UCSR0B &= ~(1 << RXCIE0);
        }
        
        m_recv_queued_event.appendNowIfNotAlready(c);
    }
    
    void udre_isr (AvrInterruptContext<Context> c)
    {
        AMBRO_ASSERT(m_send_start != m_send_end)
        
        UDR0 = m_send_buffer[m_send_start.value()];
        m_send_start = BoundedModuloInc(m_send_start);
        
        if (m_send_start == m_send_end) {
            UCSR0B &= ~(1 << UDRIE0);
        }
        
        if (m_send_start == m_send_event) {
            m_send_event = BoundedModuloInc(m_send_end);
            m_send_queued_event.appendNowIfNotAlready(c);
        }
    }
    
private:
    RecvSizeType recv_avail (RecvSizeType start, RecvSizeType end)
    {
        return BoundedModuloSubtract(end, start);
    }
    
    SendSizeType send_avail (SendSizeType start, SendSizeType end)
    {
        return BoundedModuloDec(BoundedModuloSubtract(start, end));
    }
    
    void recv_queued_event_handler (Context c)
    {
        this->debugAccess(c);
        
        RecvHandler::call(this, c);
    }
    
    void send_queued_event_handler (Context c)
    {
        this->debugAccess(c);
        
        SendHandler::call(this, c);
    }
    
    AvrLock<Context> m_lock;
    
    typename Loop::QueuedEvent m_recv_queued_event;
    RecvSizeType m_recv_start;
    RecvSizeType m_recv_end;
    bool m_recv_overrun;
    char m_recv_buffer[2 * ((size_t)RecvSizeType::maxIntValue() + 1)];
    
    typename Loop::QueuedEvent m_send_queued_event;
    SendSizeType m_send_start;
    SendSizeType m_send_end;
    SendSizeType m_send_event;
    char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
};

#define AMBRO_AVR_SERIAL_ISRS(avrserial, context) \
ISR(USART0_RX_vect) \
{ \
    (avrserial).rx_isr(MakeAvrInterruptContext(context)); \
} \
ISR(USART0_UDRE_vect) \
{ \
    (avrserial).udre_isr(MakeAvrInterruptContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
