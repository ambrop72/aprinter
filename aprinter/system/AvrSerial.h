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

#include <aprinter/meta/IsPowerOfTwo.h>
#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/AvrLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context,
          typename RecvSizeType, RecvSizeType RecvBufferSize, typename RecvHandler,
          typename SendSizeType, SendSizeType SendBufferSize, typename SendHandler>
class AvrSerial
: private DebugObject<Context, AvrSerial<Context, RecvSizeType, RecvBufferSize, RecvHandler, SendSizeType, SendBufferSize, SendHandler>>
{
    static_assert(!IntTypeInfo<RecvSizeType>::is_signed, "RecvSizeType must be unsigned");
    static_assert(!IntTypeInfo<SendSizeType>::is_signed, "SendSizeType must be unsigned");
    static_assert(IsPowerOfTwo<uintmax_t, (uintmax_t)RecvBufferSize + 1>::value, "RecvBufferSize+1 must be a power of two");
    static_assert(IsPowerOfTwo<uintmax_t, (uintmax_t)SendBufferSize + 1>::value, "SendBufferSize+1 must be a power of two");
    
private:
    typedef typename Context::EventLoop Loop;
    
public:
    void init (Context c, uint32_t baud)
    {
        m_recv_queued_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AvrSerial::m_recv_queued_event, &AvrSerial::recv_queued_event_handler));
        m_recv_start = 0;
        m_recv_end = 0;
        m_recv_overrun = false;
        m_recv_lock.init(c);
        
        m_send_queued_event.init(c, AMBRO_OFFSET_CALLBACK_T(&AvrSerial::m_send_queued_event, &AvrSerial::send_queued_event_handler));
        m_send_start = 0;
        m_send_end = 0;
        m_send_event = send_buffer_mod - 1;
        m_send_lock.init(c);
        
        uint16_t ubrr = (((2 * (uint32_t)F_CPU) + (16 * baud)) / (2 * 16 * baud)) - 1;
        
        UBRR0H = (ubrr >> 8);
        UBRR0L = ubrr;
        UCSR0A = 0;
        UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
        UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        UCSR0C = 0;
        UCSR0B = 0;
        UCSR0A = 0;
        UBRR0L = 0;
        UBRR0H = 0;
        
        m_send_lock.deinit(c);
        m_send_queued_event.deinit(c);
        m_recv_lock.deinit(c);
        m_recv_queued_event.deinit(c);
    }
    
    RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(out_overrun)
        
        bool overrun;
        RecvSizeType start;
        RecvSizeType end;
        
        AMBRO_LOCK_T(m_recv_lock, c, lock_c, {
            start = m_recv_start;
            end = m_recv_end;
            overrun = m_recv_overrun;
        });
        
        *out_overrun = overrun;
        return recv_avail(start, end);
    }
    
    RecvSizeType recvGetChunkLen (Context c, RecvSizeType rem_length)
    {
        this->debugAccess(c);
        
        if (rem_length > recv_buffer_mod - m_recv_start) {
            rem_length = recv_buffer_mod - m_recv_start;
        }
        
        return rem_length;
    }
    
    char * recvGetChunkPtr (Context c)
    {
        this->debugAccess(c);
        
        return (m_recv_buffer + m_recv_start);
    }
    
    void recvConsume (Context c, RecvSizeType amount)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_recv_lock, c, lock_c, {
            AMBRO_ASSERT(amount <= recv_avail(m_recv_start, m_recv_end))
            m_recv_start = (RecvSizeType)(m_recv_start + amount) % recv_buffer_mod;
        });
    }
    
    void recvClearOverrun (Context c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_recv_lock, c, lock_c, {
            m_recv_overrun = false;
        });
    }
    
    SendSizeType sendQuery (Context c)
    {
        this->debugAccess(c);
        
        SendSizeType start;
        AMBRO_LOCK_T(m_send_lock, c, lock_c, {
            start = m_send_start;
        });
        
        return send_avail(start, m_send_end);
    }
    
    SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        this->debugAccess(c);
        
        if (rem_length > send_buffer_mod - m_send_end) {
            rem_length = send_buffer_mod - m_send_end;
        }
        
        return rem_length;
    }
    
    char * sendGetChunkPtr (Context c)
    {
        this->debugAccess(c);
        
        return (m_send_buffer + m_send_end);
    }
    
    void sendProvide (Context c, SendSizeType amount)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_send_lock, c, lock_c, {
            AMBRO_ASSERT(amount <= send_avail(m_send_start, m_send_end))
            m_send_end = (SendSizeType)(m_send_end + amount) % send_buffer_mod;
            UCSR0B |= (1 << UDRIE0);
        });
    }
    
    void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount > 0)
        AMBRO_ASSERT(min_amount <= SendBufferSize)
        
        AMBRO_LOCK_T(m_send_lock, c, lock_c, {
            if (send_avail(m_send_start, m_send_end) >= min_amount) {
                m_send_event = send_buffer_mod - 1;
                m_send_queued_event.appendNow(lock_c);
            } else {
                m_send_event = min_amount - 1;
                m_send_queued_event.unset(lock_c);
            }
        });
    }
    
    void sendCancelEvent (Context c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_send_lock, c, lock_c, {
            m_send_event = send_buffer_mod - 1;
            m_send_queued_event.unset(lock_c, true);
        });
    }
    
    void sendBlock (Context c, SendSizeType min_amount)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(min_amount > 0)
        AMBRO_ASSERT(min_amount <= SendBufferSize)
        
        bool ok;
        do {
            AMBRO_LOCK_T(m_send_lock, c, lock_c, {
                ok = (send_avail(m_send_start, m_send_end) >= min_amount);
            });
        } while (!ok);
    }
    
    void rx_isr (AvrInterruptContext<Context> c)
    {
        uint8_t byte = UDR0;
        if (!m_recv_overrun) {
            RecvSizeType new_end = (RecvSizeType)(m_recv_end + 1) % recv_buffer_mod;
            if (new_end != m_recv_start) {
                m_recv_buffer[m_recv_end] = byte;
                m_recv_end = new_end;
            } else {
                m_recv_overrun = true;
            }
        }
        
        m_recv_queued_event.appendNow(c);
    }
    
    void udre_isr (AvrInterruptContext<Context> c)
    {
        AMBRO_ASSERT(m_send_start != m_send_end)
        
        UDR0 = m_send_buffer[m_send_start];
        m_send_start = (SendSizeType)(m_send_start + 1) % send_buffer_mod;
        
        if (m_send_start == m_send_end) {
            UCSR0B &= ~(1 << UDRIE0);
        }
        
        if (send_avail(m_send_start, m_send_end) > m_send_event) {
            m_send_event = send_buffer_mod - 1;
            m_send_queued_event.appendNow(c);
        }
    }
    
private:
    static const size_t recv_buffer_mod = (size_t)RecvBufferSize + 1;
    static const size_t send_buffer_mod = (size_t)SendBufferSize + 1;
    
    RecvSizeType recv_avail (RecvSizeType start, RecvSizeType end)
    {
        return ((RecvSizeType)(end - start) % recv_buffer_mod);
    }
    
    SendSizeType send_avail (SendSizeType start, SendSizeType end)
    {
        return (SendSizeType)((SendSizeType)(start - 1) - end) % send_buffer_mod;
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
    
    typename Loop::QueuedEvent m_recv_queued_event;
    RecvSizeType m_recv_start;
    RecvSizeType m_recv_end;
    bool m_recv_overrun;
    char m_recv_buffer[(size_t)RecvBufferSize + 1];
    AvrLock<Context> m_recv_lock;
    
    typename Loop::QueuedEvent m_send_queued_event;
    SendSizeType m_send_start;
    SendSizeType m_send_end;
    SendSizeType m_send_event;
    char m_send_buffer[(size_t)SendBufferSize + 1];
    AvrLock<Context> m_send_lock;
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
