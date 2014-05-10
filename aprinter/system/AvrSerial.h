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
#include <util/atomic.h>

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <bool TDoubleSpeed>
struct AvrSerialParams {
    static const bool DoubleSpeed = TDoubleSpeed;
};

template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename Params, typename RecvHandler, typename SendHandler>
class AvrSerial {
private:
    using RecvFastEvent = typename Context::EventLoop::template FastEventSpec<AvrSerial>;
    using SendFastEvent = typename Context::EventLoop::template FastEventSpec<RecvFastEvent>;
    
public:
    struct Object;
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
    static void init (Context c, uint32_t baud)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<RecvFastEvent>(c, AvrSerial::recv_event_handler);
        o->m_recv_start = RecvSizeType::import(0);
        o->m_recv_end = RecvSizeType::import(0);
        o->m_recv_overrun = false;
        
        Context::EventLoop::template initFastEvent<SendFastEvent>(c, AvrSerial::send_event_handler);
        o->m_send_start = SendSizeType::import(0);
        o->m_send_end = SendSizeType::import(0);;
        o->m_send_event = BoundedModuloInc(o->m_send_end);
        
        uint32_t d = (Params::DoubleSpeed ? 8 : 16) * baud;
        uint32_t ubrr = (((2 * (uint32_t)F_CPU) + d) / (2 * d)) - 1;
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            UBRR0H = (ubrr >> 8);
            UBRR0L = ubrr;
            UCSR0A = ((int)Params::DoubleSpeed << U2X0);
            UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
            UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
        }
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            UCSR0C = 0;
            UCSR0B = 0;
            UCSR0A = 0;
            UBRR0L = 0;
            UBRR0H = 0;
        }
        
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
        
        while ((UCSR0A & (1 << RXC0))) {
            (void)UDR0;
        }
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            UCSR0B |= (1 << RXCIE0);
        }
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
            UCSR0B |= (1 << UDRIE0);
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
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                not_finished = (o->m_send_start != o->m_send_end);
            }
        } while (not_finished);
    }
    
    static void rx_isr (AtomicContext<Context> c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->m_recv_overrun)
        
        RecvSizeType new_end = BoundedModuloInc(o->m_recv_end);
        if (new_end != o->m_recv_start) {
            char ch = UDR0;
            o->m_recv_buffer[o->m_recv_end.value()] = ch;
            o->m_recv_buffer[o->m_recv_end.value() + (sizeof(o->m_recv_buffer) / 2)] = ch;
            o->m_recv_end = new_end;
        } else {
            o->m_recv_overrun = true;
            UCSR0B &= ~(1 << RXCIE0);
        }
        
        Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
    }
    
    static void udre_isr (AtomicContext<Context> c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_send_start != o->m_send_end)
        
        UDR0 = o->m_send_buffer[o->m_send_start.value()];
        o->m_send_start = BoundedModuloInc(o->m_send_start);
        
        if (o->m_send_start == o->m_send_end) {
            UCSR0B &= ~(1 << UDRIE0);
        }
        
        if (o->m_send_start == o->m_send_event) {
            o->m_send_event = BoundedModuloInc(o->m_send_end);
            Context::EventLoop::template triggerFastEvent<SendFastEvent>(c);
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
    struct Object : public ObjBase<AvrSerial, ParentObject, EmptyTypeList>,
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

#define AMBRO_AVR_SERIAL_ISRS(avrserial, context) \
ISR(USART0_RX_vect) \
{ \
    avrserial::rx_isr(MakeAtomicContext(context)); \
} \
ISR(USART0_UDRE_vect) \
{ \
    avrserial::udre_isr(MakeAtomicContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
