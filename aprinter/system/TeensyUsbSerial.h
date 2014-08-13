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

#ifndef AMBROLIB_TEENSY_USB_SERIAL_H
#define AMBROLIB_TEENSY_USB_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define OLD_CPLUSPLUS __cplusplus
#undef __cplusplus
extern "C" {
#include <usb_serial.h>
}
#define __cplusplus OLD_CPLUSPLUS
#undef OLD_CPLUSPLUS

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler, typename Params>
class TeensyUsbSerial {
private:
    using RecvFastEvent = typename Context::EventLoop::template FastEventSpec<TeensyUsbSerial>;
    using SendFastEvent = typename Context::EventLoop::template FastEventSpec<RecvFastEvent>;
    
public:
    struct Object;
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c, uint32_t baud)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<RecvFastEvent>(c, TeensyUsbSerial::recv_event_handler);
        o->m_recv_start = RecvSizeType::import(0);
        o->m_recv_end = RecvSizeType::import(0);
        o->m_recv_force = false;
        
        Context::EventLoop::template initFastEvent<SendFastEvent>(c, TeensyUsbSerial::send_event_handler);
        o->m_send_avail_event.init(c, TeensyUsbSerial::send_avail_event_handler);
        o->m_send_start = SendSizeType::import(0);
        o->m_send_end = SendSizeType::import(0);
        o->m_send_event = SendSizeType::import(0);
        
        Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->m_send_avail_event.deinit(c);
        Context::EventLoop::template resetFastEvent<SendFastEvent>(c);
        Context::EventLoop::template resetFastEvent<RecvFastEvent>(c);
    }
    
    static RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(out_overrun)
        
        *out_overrun = (o->m_recv_end == BoundedModuloDec(o->m_recv_start));
        return recv_avail(o->m_recv_start, o->m_recv_end);
    }
    
    static char * recvGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return (o->m_recv_buffer + o->m_recv_start.value());
    }
    
    static void recvConsume (Context c, RecvSizeType amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        AMBRO_ASSERT(amount <= recv_avail(o->m_recv_start, o->m_recv_end))
        o->m_recv_start = BoundedModuloAdd(o->m_recv_start, amount);
    }
    
    static void recvClearOverrun (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_recv_end == BoundedModuloDec(o->m_recv_start))
    }
    
    static void recvForceEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_recv_force = true;
    }
    
    static SendSizeType sendQuery (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return send_avail(o->m_send_start, o->m_send_end);
    }
    
    static SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        if (o->m_send_end.value() > 0 && rem_length > BoundedModuloNegative(o->m_send_end)) {
            rem_length = BoundedModuloNegative(o->m_send_end);
        }
        return rem_length;
    }
    
    static char * sendGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return (o->m_send_buffer + o->m_send_end.value());
    }
    
    static void sendProvide (Context c, SendSizeType amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(amount <= send_avail(o->m_send_start, o->m_send_end))
        
        o->m_send_end = BoundedModuloAdd(o->m_send_end, amount);
    }
    
    static void sendPoke (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        if (o->m_send_start != o->m_send_end) {
            Context::EventLoop::template resetFastEvent<SendFastEvent>(c);
            do_send(c);
        }
    }
    
    static void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_send_event = min_amount;
        o->m_send_avail_event.unset(c);
        if (o->m_send_event > SendSizeType::import(0)) {
            o->m_send_avail_event.prependNowNotAlready(c);
        }
    }
    
    using EventLoopFastEvents = MakeTypeList<RecvFastEvent>;
    
private:
    using Loop = typename Context::EventLoop;
    
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
        TheDebugObject::access(c);
        
        Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
        RecvSizeType virtual_start = BoundedModuloDec(o->m_recv_start);
        while (o->m_recv_end != virtual_start) {
            RecvSizeType amount = (o->m_recv_end > virtual_start) ? BoundedModuloNegative(o->m_recv_end) : BoundedUnsafeSubtract(virtual_start, o->m_recv_end);
            int bytes = usb_serial_read(o->m_recv_buffer + o->m_recv_end.value(), amount.value());
            if (bytes <= 0) {
                break;
            }
            memcpy(o->m_recv_buffer + ((size_t)RecvSizeType::maxIntValue() + 1) + o->m_recv_end.value(), o->m_recv_buffer + o->m_recv_end.value(), bytes);
            o->m_recv_end = BoundedModuloAdd(o->m_recv_end, RecvSizeType::import(bytes));
            o->m_recv_force = true;
        }
        if (o->m_recv_force) {
            o->m_recv_force = false;
            RecvHandler::call(c);
        }
    }
    
    static void send_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_send_start != o->m_send_end)
        
        do_send(c);
    }
    
    static void do_send (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_send_start != o->m_send_end)
        
        do {
            SendSizeType amount = (o->m_send_end < o->m_send_start) ? BoundedModuloNegative(o->m_send_start) : BoundedUnsafeSubtract(o->m_send_end, o->m_send_start);
            uint32_t bytes = usb_serial_write_buffer_free();
            if (bytes == 0) {
                Context::EventLoop::template triggerFastEvent<SendFastEvent>(c);
                return;
            }
            if (bytes > amount.value()) {
                bytes = amount.value();
            }
            usb_serial_write(o->m_send_buffer + o->m_send_start.value(), bytes);
            o->m_send_start = BoundedModuloAdd(o->m_send_start, SendSizeType::import(bytes));
            if (o->m_send_event > SendSizeType::import(0)) {
                o->m_send_avail_event.unset(c);
                o->m_send_avail_event.prependNowNotAlready(c);
            }
        } while (o->m_send_start != o->m_send_end);
    }
    
    static void send_avail_event_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_send_event > SendSizeType::import(0))
        
        if (send_avail(o->m_send_start, o->m_send_end) >= o->m_send_event) {
            o->m_send_event = SendSizeType::import(0);
            SendHandler::call(c);
        }
    }
    
public:
    struct Object : public ObjBase<TeensyUsbSerial, ParentObject, MakeTypeList<TheDebugObject>> {
        RecvSizeType m_recv_start;
        RecvSizeType m_recv_end;
        bool m_recv_force;
        char m_recv_buffer[2 * ((size_t)RecvSizeType::maxIntValue() + 1)];
        typename Loop::QueuedEvent m_send_avail_event;
        SendSizeType m_send_start;
        SendSizeType m_send_end;
        SendSizeType m_send_event;
        char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
    };
};

struct TeensyUsbSerialService {
    template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler>
    using Serial = TeensyUsbSerial<Context, ParentObject, RecvBufferBits, SendBufferBits, RecvHandler, SendHandler, TeensyUsbSerialService>;
};

#include <aprinter/EndNamespace.h>

#endif
