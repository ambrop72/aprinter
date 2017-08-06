/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_LINUX_STDINOUT_SERIAL_H
#define APRINTER_LINUX_STDINOUT_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Preprocessor.h>

namespace APrinter {

template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler, typename Params>
class LinuxStdInOutSerial {
public:
    struct Object;
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
private:
    APRINTER_USE_TYPE1(Context::EventLoop, FdEvFlags)
    using TheDebugObject = DebugObject<Context, Object>;
    
    static int const InputFd = 0;
    static int const OutputFd = 1;
    
public:
    static void init (Context c, uint32_t baud)
    {
        auto *o = Object::self(c);
        
        o->m_recv_force_event.init(c, APRINTER_CB_STATFUNC_T(&LinuxStdInOutSerial::recv_force_event_handler));
        o->m_send_avail_event.init(c, APRINTER_CB_STATFUNC_T(&LinuxStdInOutSerial::send_avail_event_handler));
        o->m_recv_fd_event.init(c, APRINTER_CB_STATFUNC_T(&LinuxStdInOutSerial::recv_fd_event_handler));
        o->m_send_fd_event.init(c, APRINTER_CB_STATFUNC_T(&LinuxStdInOutSerial::send_fd_event_handler));
        
        o->m_recv_start = RecvSizeType::import(0);
        o->m_recv_end = RecvSizeType::import(0);
        o->m_recv_dead = false;
        
        o->m_send_start = SendSizeType::import(0);
        o->m_send_end = SendSizeType::import(0);
        o->m_send_event = SendSizeType::import(0);
        o->m_send_dead = false;
        
        Context::EventLoop::setFdNonblocking(InputFd);
        Context::EventLoop::setFdNonblocking(OutputFd);
        
        o->m_recv_fd_event.start(c, InputFd, FdEvFlags::EV_READ);
        o->m_send_fd_event.start(c, OutputFd, 0);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->m_send_fd_event.deinit(c);
        o->m_recv_fd_event.deinit(c);
        o->m_send_avail_event.deinit(c);
        o->m_recv_force_event.deinit(c);
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
        
        bool was_full = (o->m_recv_end == BoundedModuloDec(o->m_recv_start));
        o->m_recv_start = BoundedModuloAdd(o->m_recv_start, amount);
        
        if (was_full && amount.value() > 0 && !o->m_recv_dead) {
            o->m_recv_fd_event.changeEvents(c, FdEvFlags::EV_READ);
        }
    }
    
    static void recvClearOverrun (Context c)
    {
        TheDebugObject::access(c);
    }
    
    static void recvForceEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_recv_force_event.prependNow(c);
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
        TheDebugObject::access(c);
        
        do_send(c);
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
    
private:
    static RecvSizeType recv_avail (RecvSizeType start, RecvSizeType end)
    {
        return BoundedModuloSubtract(end, start);
    }
    
    static void recv_fd_event_handler (Context c, int events)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_recv_dead)
        
        bool read_something = false;
        RecvSizeType virtual_start = BoundedModuloDec(o->m_recv_start);
        
        while (o->m_recv_end != virtual_start) {
            RecvSizeType to_read = (o->m_recv_end > virtual_start) ? BoundedModuloNegative(o->m_recv_end) : BoundedUnsafeSubtract(virtual_start, o->m_recv_end);
            
            ssize_t read_res = ::read(InputFd, o->m_recv_buffer + o->m_recv_end.value(), to_read.value());
            if (read_res <= 0) {
                bool again = false;
                if (read_res < 0) {
                    int err = errno;
                    again = (err == EWOULDBLOCK || err == EAGAIN);
                }
                if (!again) {
                    recv_enter_dead(c);
                }
                break;
            }
            
            AMBRO_ASSERT_FORCE(read_res <= to_read.value())
            memcpy(o->m_recv_buffer + ((size_t)RecvSizeType::maxIntValue() + 1) + o->m_recv_end.value(), o->m_recv_buffer + o->m_recv_end.value(), read_res);
            
            o->m_recv_end = BoundedModuloAdd(o->m_recv_end, RecvSizeType::import(read_res));
            read_something = true;
        }
        
        if (o->m_recv_end == virtual_start && !o->m_recv_dead) {
            o->m_recv_fd_event.changeEvents(c, 0);
        }
        
        if (read_something) {
            RecvHandler::call(c);
        }
    }
    
    static void recv_enter_dead (Context c)
    {
        auto *o = Object::self(c);
        o->m_recv_dead = true;
        o->m_recv_fd_event.reset(c);
    }
    
    static void recv_force_event_handler (Context c)
    {
        TheDebugObject::access(c);
        
        RecvHandler::call(c);
    }
    
    static SendSizeType send_avail (SendSizeType start, SendSizeType end)
    {
        return BoundedModuloDec(BoundedModuloSubtract(start, end));
    }
    
    static void do_send (Context c)
    {
        auto *o = Object::self(c);
        
        while (o->m_send_start != o->m_send_end) {
            SendSizeType to_write = (o->m_send_end < o->m_send_start) ? BoundedModuloNegative(o->m_send_start) : BoundedUnsafeSubtract(o->m_send_end, o->m_send_start);
            
            SendSizeType written;
            if (o->m_send_dead) {
                written = to_write;
            } else {
                ssize_t write_res = ::write(OutputFd, o->m_send_buffer + o->m_send_start.value(), to_write.value());
                if (write_res < 0) {
                    int err = errno;
                    if (err == EWOULDBLOCK || err == EAGAIN) {
                        o->m_send_fd_event.changeEvents(c, FdEvFlags::EV_WRITE);
                        return;
                    }
                    send_enter_dead(c);
                    written = to_write;
                } else {
                    AMBRO_ASSERT_FORCE(write_res > 0)
                    AMBRO_ASSERT_FORCE(write_res <= to_write.value())
                    written = SendSizeType::import(write_res);
                }
            }
            
            o->m_send_start = BoundedModuloAdd(o->m_send_start, written);
            
            if (o->m_send_event > SendSizeType::import(0)) {
                o->m_send_avail_event.prependNow(c);
            }
        }
    }
    
    static void send_enter_dead (Context c)
    {
        auto *o = Object::self(c);
        o->m_send_dead = true;
        o->m_send_fd_event.reset(c);
    }
    
    static void send_avail_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_send_event > SendSizeType::import(0))
        
        if (send_avail(o->m_send_start, o->m_send_end) >= o->m_send_event) {
            o->m_send_event = SendSizeType::import(0);
            SendHandler::call(c);
        }
    }
    
    static void send_fd_event_handler (Context c, int events)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_send_dead)
        
        o->m_send_fd_event.changeEvents(c, 0);
        do_send(c);
    }
    
public:
    struct Object : public ObjBase<LinuxStdInOutSerial, ParentObject, MakeTypeList<TheDebugObject>> {
        typename Context::EventLoop::QueuedEvent m_recv_force_event;
        typename Context::EventLoop::QueuedEvent m_send_avail_event;
        typename Context::EventLoop::FdEvent m_recv_fd_event;
        typename Context::EventLoop::FdEvent m_send_fd_event;
        RecvSizeType m_recv_start;
        RecvSizeType m_recv_end;
        SendSizeType m_send_start;
        SendSizeType m_send_end;
        SendSizeType m_send_event;
        bool m_recv_dead;
        bool m_send_dead;
        char m_recv_buffer[2 * ((size_t)RecvSizeType::maxIntValue() + 1)];
        char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
    };
};

struct LinuxStdInOutSerialService {
    template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler>
    using Serial = LinuxStdInOutSerial<Context, ParentObject, RecvBufferBits, SendBufferBits, RecvHandler, SendHandler, LinuxStdInOutSerialService>;
};

}

#endif
