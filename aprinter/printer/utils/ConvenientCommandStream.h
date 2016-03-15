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

#ifndef APRINTER_CONVENIENT_COMMAND_STREAM_H
#define APRINTER_CONVENIENT_COMMAND_STREAM_H

#include <stddef.h>

#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ThePrinterMain>
class ConvenientCommandStream
: private ThePrinterMain::CommandStream,
  private ThePrinterMain::SendBufEventCallback
{
private:
    using TimeType = typename Context::Clock::TimeType;
    using CommandStream = typename ThePrinterMain::CommandStream;
    
public:
    enum class Error {SENDBUF_TIMEOUT, SENDBUF_OVERRUN};
    
    class UserCallback : public ThePrinterMain::CommandStreamCallback {
    public:
        virtual void commandStreamError (Context c, Error error) = 0;
        virtual bool mayWaitForSendBuf (Context c, size_t length) = 0;
    };
    
    void init (Context c, TimeType send_buf_timeout, UserCallback *user_callback)
    {
        m_overrun_event.init(c, APRINTER_CB_OBJFUNC_T(&ConvenientCommandStream::overrun_event_handler, this));
        m_send_buf_check_event.init(c, APRINTER_CB_OBJFUNC_T(&ConvenientCommandStream::send_buf_check_event_handler, this));
        m_send_buf_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&ConvenientCommandStream::send_buf_timeout_event_handler, this));
        
        m_send_buf_timeout = send_buf_timeout;
        m_send_buf_request = 0;
        
        CommandStream::init(c, user_callback, this);
    }
    
    void deinit (Context c)
    {
        CommandStream::deinit(c);
        
        m_send_buf_timeout_event.deinit(c);
        m_send_buf_check_event.deinit(c);
        m_overrun_event.deinit(c);
    }
    
    void updateSendBufEvent (Context c)
    {
        if (m_send_buf_request > 0) {
            m_send_buf_check_event.prependNow(c);
        }
    }
    
    void raiseSendOverrun (Context c)
    {
        m_overrun_event.prependNow(c);
    }
    
    bool isSendOverrunBeingRaised (Context c)
    {
        return m_overrun_event.isSet(c);
    }
    
    using CommandStream::tryCancelCommand;
    using CommandStream::setAcceptMsg;
    using CommandStream::hasCommand;
    using CommandStream::startCommand;
    using CommandStream::setPokeOverhead;
    
    CommandStream * getCommandStream (Context c)
    {
        return this;
    }
    
private:
    UserCallback * user_callback (Context c)
    {
        return (UserCallback *)CommandStream::getCallback(c);
    }
    
    bool request_send_buf_event_impl (Context c, size_t length)
    {
        AMBRO_ASSERT(m_send_buf_request == 0)
        AMBRO_ASSERT(length > 0)
        
        if (!user_callback(c)->mayWaitForSendBuf(c, length)) {
            return false;
        }
        m_send_buf_request = length;
        m_send_buf_check_event.prependNowNotAlready(c);
        m_send_buf_timeout_event.appendAfter(c, m_send_buf_timeout);
        return true;
    }
    
    void cancel_send_buf_event_impl (Context c)
    {
        AMBRO_ASSERT(m_send_buf_request > 0)
        
        m_send_buf_request = 0;
        m_send_buf_check_event.unset(c);
        m_send_buf_timeout_event.unset(c);
    }
    
    void overrun_event_handler (Context c)
    {
        return user_callback(c)->commandStreamError(c, Error::SENDBUF_OVERRUN);
    }
    
    void send_buf_check_event_handler (Context c)
    {
        AMBRO_ASSERT(m_send_buf_request > 0)
        
        if (user_callback(c)->get_send_buf_avail_impl(c) >= m_send_buf_request) {
            m_send_buf_request = 0;
            m_send_buf_timeout_event.unset(c);
            return CommandStream::reportSendBufEventDirectly(c);
        }
    }
    
    void send_buf_timeout_event_handler (Context c)
    {
        AMBRO_ASSERT(m_send_buf_request > 0)
        
        return user_callback(c)->commandStreamError(c, Error::SENDBUF_TIMEOUT);
    }
    
private:
    typename Context::EventLoop::QueuedEvent m_overrun_event;
    typename Context::EventLoop::QueuedEvent m_send_buf_check_event;
    typename Context::EventLoop::TimedEvent m_send_buf_timeout_event;
    TimeType m_send_buf_timeout;
    size_t m_send_buf_request;
};

#include <aprinter/EndNamespace.h>

#endif
