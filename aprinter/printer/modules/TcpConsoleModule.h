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

#ifndef APRINTER_TCP_CONSOLE_MODULE_H
#define APRINTER_TCP_CONSOLE_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/printer/utils/ConvenientCommandStream.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename ModuleArg>
class TcpConsoleModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    using TheNetwork = typename Context::Network;
    using TheTcpListener = typename TheNetwork::TcpListener;
    using TheTcpConnection = typename TheNetwork::TcpConnection;
    
    using TheConvenientStream = ConvenientCommandStream<Context, ThePrinterMain>;
    
    using TheGcodeParser = typename Params::TheGcodeParserService::template Parser<Context, size_t, typename ThePrinterMain::FpType>;
    
    static int const MaxClients = Params::MaxClients;
    static_assert(MaxClients > 0, "");
    static_assert(Params::MaxPcbs > 0, "");
    
    static size_t const MaxCommandSize = Params::MaxCommandSize;
    static_assert(MaxCommandSize > 0, "");
    
    static size_t const SendBufferSize = Params::SendBufferSize;
    static_assert(SendBufferSize >= TheTcpConnection::MinSendBufSize, "");
    static size_t const GuaranteedSendBuf = SendBufferSize - TheTcpConnection::MaxSndBufOverhead;
    static_assert(GuaranteedSendBuf >= ThePrinterMain::CommandSendBufClearance, "");
    
    static size_t const RecvBufferSize = Params::RecvBufferSize;
    static_assert(RecvBufferSize >= TheTcpConnection::MinRecvBufSize, "");
    static_assert(RecvBufferSize >= MaxCommandSize, "");
    
    static size_t const RecvMirrorSize = MaxCommandSize - 1;
    
    static TimeType const SendBufTimeoutTicks = Params::SendBufTimeout::value() * Context::Clock::time_freq;
    static TimeType const SendEndTimeoutTicks = Params::SendEndTimeout::value() * Context::Clock::time_freq;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->listener.init(c, APRINTER_CB_STATFUNC_T(&TcpConsoleModule::listener_accept_handler));
        
        typename TheNetwork::TcpListenParams listen_params = {};
        listen_params.port = Params::Port;
        listen_params.max_pcbs = Params::MaxPcbs;
        listen_params.min_rcv_buf_size = RecvBufferSize;
        
        if (!o->listener.startListening(c, listen_params)) {
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleListenError\n"));
        }
        
        for (Client &client : o->clients) {
            client.init(c);
        }
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        for (Client &client : o->clients) {
            client.deinit(c);
        }
        
        o->listener.deinit(c);
    }
    
private:
    static void listener_accept_handler (Context c)
    {
        auto *o = Object::self(c);
        
        for (Client &client : o->clients) {
            if (client.m_state == Client::State::NOT_CONNECTED) {
                return client.accept_connection(c);
            }
        }
        
        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleAcceptNoSlot\n"));
    }
    
    struct Client : private TheConvenientStream::UserCallback, TheNetwork::TcpConnectionCallback
    {
        enum class State : uint8_t {NOT_CONNECTED, CONNECTED, SENDING_END, WAITING_CMD};
        
        static bool state_not_disconnected (State state)
        {
            return state == OneOf(State::CONNECTED, State::SENDING_END, State::WAITING_CMD);
        }
        
        void init (Context c)
        {
            m_connection.init(c, this);
            m_state = State::NOT_CONNECTED;
        }
        
        void deinit (Context c)
        {
            if (m_state != State::NOT_CONNECTED) {
                m_send_timeout_event.deinit(c);
                m_command_stream.deinit(c);
                m_gcode_parser.deinit(c);
            }
            m_connection.deinit(c);
        }
        
        void accept_connection (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::NOT_CONNECTED)
            
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleConnected\n"));
            
            typename TheNetwork::TcpConnectionBufferInfo bufinfo = {};
            bufinfo.send_buf = m_send_buf;
            bufinfo.send_buf_size = SendBufferSize;
            bufinfo.recv_buf = m_recv_buf;
            bufinfo.recv_buf_size = RecvBufferSize;
            bufinfo.recv_buf_mirror = RecvMirrorSize;
            
            m_connection.acceptConnection(c, &o->listener, bufinfo);
            
            m_gcode_parser.init(c);
            m_command_stream.init(c, SendBufTimeoutTicks, this, APRINTER_CB_OBJFUNC_T(&Client::next_event_handler, this));
            m_send_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::send_timeout_event_handler, this));
            
            m_state = State::CONNECTED;
        }
        
        void disconnect (Context c)
        {
            AMBRO_ASSERT(state_not_disconnected(m_state))
            
            m_send_timeout_event.deinit(c);
            m_command_stream.deinit(c);
            m_gcode_parser.deinit(c);
            
            m_connection.reset(c);
            
            m_state = State::NOT_CONNECTED;
        }
        
        void start_disconnect (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::SENDING_END))
            
            if (m_command_stream.tryCancelCommand(c)) {
                disconnect(c);
            } else {
                m_connection.reset(c);
                m_state = State::WAITING_CMD;
                m_command_stream.updateSendBufEvent(c);
                m_send_timeout_event.unset(c);
            }
        }
        
        void start_send_end (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            
            m_connection.closeSending(c);
            m_state = State::SENDING_END;
            m_command_stream.updateSendBufEvent(c);
            m_command_stream.unsetNextEvent(c);
            m_send_timeout_event.appendAfter(c, SendEndTimeoutTicks);
        }
        
        void connectionAborted (Context c) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::SENDING_END))
            
            m_command_stream.setAcceptMsg(c, false);
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleError\n"));
            
            start_disconnect(c);
        }
        
        void dataReceived (Context c, size_t amount) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::SENDING_END))
            
            if (m_state == State::CONNECTED) {
                m_command_stream.setNextEventIfNoCommand(c);
            }
        }
        
        void dataSent (Context c, size_t amount) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::SENDING_END))
            
            if (m_state == State::CONNECTED) {
                m_command_stream.updateSendBufEvent(c);
            } else {
                if (amount == 0) {
                    ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleEndSent\n"));
                    start_disconnect(c);
                }
            }
        }
        
        void send_timeout_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == State::SENDING_END)
            
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleSendEndTimeout\n"));
            start_disconnect(c);
        }
        
        void next_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::WAITING_CMD))
            AMBRO_ASSERT(!m_command_stream.hasCommand(c))
            
            if (m_state == State::WAITING_CMD) {
                return disconnect(c);
            }
            
            size_t avail = MinValue(MaxCommandSize, m_connection.getRecvBufferUsed(c));
            bool line_buffer_exhausted = (avail == MaxCommandSize);
            
            if (!m_gcode_parser.haveCommand(c)) {
                m_gcode_parser.startCommand(c, m_connection.getRecvBufferPtr(c).ptr1, 0);
            }
            
            if (m_gcode_parser.extendCommand(c, avail, line_buffer_exhausted)) {
                return m_command_stream.startCommand(c, &m_gcode_parser);
            }
            
            if (line_buffer_exhausted || m_connection.wasEndReceived(c)) {
                m_command_stream.setAcceptMsg(c, false);
                auto err = line_buffer_exhausted ? AMBRO_PSTR("//TcpConsoleLineTooLong\n") : AMBRO_PSTR("//TcpConsoleClosed\n");
                ThePrinterMain::print_pgm_string(c, err);
                start_send_end(c);
            }
        }
        
        void finish_command_impl (Context c) override
        {
            AMBRO_ASSERT(state_not_disconnected(m_state))
            
            if (m_state == OneOf(State::CONNECTED, State::SENDING_END)) {
                m_connection.consumeRecvData(c, m_gcode_parser.getLength(c));
            }
            
            if (m_state != State::SENDING_END) {
                m_command_stream.setNextEventAfterCommandFinished(c);
            }
        }
        
        void reply_poke_impl (Context c, bool push) override
        {
            AMBRO_ASSERT(state_not_disconnected(m_state))
            
            if (push && m_state == State::CONNECTED) {
                m_connection.pokeSending(c);
            }
        }
        
        void reply_append_buffer_impl (Context c, char const *str, size_t length) override
        {
            AMBRO_ASSERT(state_not_disconnected(m_state))
            
            if (m_state == State::CONNECTED && !m_command_stream.isSendOverrunBeingRaised(c)) {
                size_t avail = m_connection.getSendBufferSpace(c);
                if (avail < length) {
                    m_command_stream.raiseSendOverrun(c);
                    return;
                }
                m_connection.copySendData(c, MemRef(str, length));
            }
        }
        
        size_t get_send_buf_avail_impl (Context c) override
        {
            AMBRO_ASSERT(state_not_disconnected(m_state))
            
            return (m_state != State::CONNECTED) ? (size_t)-1 : m_connection.getSendBufferSpace(c);
        }
        
        void commandStreamError (Context c, typename TheConvenientStream::Error error) override
        {
            AMBRO_ASSERT(state_not_disconnected(m_state))
            
            // Ignore errors if we're no longer fully connected.
            if (m_state != State::CONNECTED) {
                return;
            }
            
            m_command_stream.setAcceptMsg(c, false);
            
            if (error == TheConvenientStream::Error::SENDBUF_TIMEOUT) {
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleSendTimeout\n"));
            }
            else if (error == TheConvenientStream::Error::SENDBUF_OVERRUN) {
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleSendOverrun\n"));
            }
            
            start_send_end(c);
        }
        
        bool mayWaitForSendBuf (Context c, size_t length) override
        {
            AMBRO_ASSERT(state_not_disconnected(m_state))
            
            return (m_state != State::CONNECTED || length <= GuaranteedSendBuf);
        }
        
        TheTcpConnection m_connection;
        TheGcodeParser m_gcode_parser;
        TheConvenientStream m_command_stream;
        typename Context::EventLoop::TimedEvent m_send_timeout_event;
        State m_state;
        char m_send_buf[SendBufferSize];
        char m_recv_buf[RecvBufferSize+RecvMirrorSize];
    };
    
public:
    struct Object : public ObjBase<TcpConsoleModule, ParentObject, EmptyTypeList> {
        TheTcpListener listener;
        Client clients[MaxClients];
    };
};

APRINTER_ALIAS_STRUCT_EXT(TcpConsoleModuleService, (
    APRINTER_AS_TYPE(TheGcodeParserService),
    APRINTER_AS_VALUE(uint16_t, Port),
    APRINTER_AS_VALUE(int, MaxClients),
    APRINTER_AS_VALUE(int, MaxPcbs),
    APRINTER_AS_VALUE(size_t, MaxCommandSize),
    APRINTER_AS_VALUE(size_t, SendBufferSize),
    APRINTER_AS_VALUE(size_t, RecvBufferSize),
    APRINTER_AS_TYPE(SendBufTimeout),
    APRINTER_AS_TYPE(SendEndTimeout)
), (
    APRINTER_MODULE_TEMPLATE(TcpConsoleModuleService, TcpConsoleModule)
))

#include <aprinter/EndNamespace.h>

#endif
