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

#ifndef AMBROLIB_NULL_SERIAL_H
#define AMBROLIB_NULL_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>

namespace APrinter {

template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler, typename Params>
class NullSerial {
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
        
        o->m_recv_force_event.init(c, APRINTER_CB_STATFUNC_T(&NullSerial::recv_force_event_handler));
        o->m_send_avail_event.init(c, APRINTER_CB_STATFUNC_T(&NullSerial::send_avail_event_handler));
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->m_send_avail_event.deinit(c);
        o->m_recv_force_event.deinit(c);
    }
    
    static RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(out_overrun)
        
        *out_overrun = false;
        return RecvSizeType::import(0);
    }
    
    static char * recvGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return &o->m_recv_dummy_buf;
    }
    
    static void recvConsume (Context c, RecvSizeType amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(amount == RecvSizeType::import(0))
    }
    
    static void recvClearOverrun (Context c)
    {
        TheDebugObject::access(c);
        AMBRO_ASSERT(false)
    }
    
    static void recvForceEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_recv_force_event.unset(c);
        o->m_recv_force_event.prependNowNotAlready(c);
    }
    
    static SendSizeType sendQuery (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return SendSizeType::maxValue();
    }
    
    static SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return rem_length;
    }
    
    static char * sendGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return o->m_send_buffer;
    }
    
    static void sendProvide (Context c, SendSizeType amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(amount <= SendSizeType::maxValue())
    }
    
    static void sendPoke (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
    }
    
    static void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_send_avail_event.unset(c);
        if (min_amount > SendSizeType::import(0)) {
            o->m_send_avail_event.prependNowNotAlready(c);
        }
    }
    
    using EventLoopFastEvents = EmptyTypeList;
    
private:
    using Loop = typename Context::EventLoop;
    
    static void recv_force_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        RecvHandler::call(c);
    }
    
    static void send_avail_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        SendHandler::call(c);
    }
    
public:
    struct Object : public ObjBase<NullSerial, ParentObject, MakeTypeList<TheDebugObject>> {
        typename Loop::QueuedEvent m_recv_force_event;
        typename Loop::QueuedEvent m_send_avail_event;
        char m_recv_dummy_buf;
        char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
    };
};

struct NullSerialService {
    template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler>
    using Serial = NullSerial<Context, ParentObject, RecvBufferBits, SendBufferBits, RecvHandler, SendHandler, NullSerialService>;
};

}

#endif
