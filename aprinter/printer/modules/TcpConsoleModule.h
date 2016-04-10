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
    static size_t const MaxCommandSize = Params::MaxCommandSize;
    static_assert(MaxCommandSize > 0, "");
    static size_t const WrapExtraSize = MaxCommandSize - 1;
    static size_t const BufferBaseSize = TheTcpConnection::RequiredRxBufSize;
    static_assert(BufferBaseSize >= MaxCommandSize, "");
    
    static TimeType const SendBufTimeoutTicks = Params::SendBufTimeout::value() * Context::Clock::time_freq;
    
    static_assert(TheTcpConnection::ProvidedTxBufSize >= ThePrinterMain::CommandSendBufClearance, "TCP send buffer is too small");
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->listener.init(c, APRINTER_CB_STATFUNC_T(&TcpConsoleModule::listener_accept_handler));
        
        if (!o->listener.startListening(c, Params::Port, Params::MaxClients)) {
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
        enum class State : uint8_t {NOT_CONNECTED, CONNECTED, DISCONNECTED_WAIT_CMD};
        
        void init (Context c)
        {
            m_connection.init(c, this);
            m_state = State::NOT_CONNECTED;
        }
        
        void deinit (Context c)
        {
            if (m_state != State::NOT_CONNECTED) {
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
            
            m_connection.acceptConnection(c, &o->listener);
            
            m_gcode_parser.init(c);
            m_command_stream.init(c, SendBufTimeoutTicks, this, APRINTER_CB_OBJFUNC_T(&Client::next_event_handler, this));
            
            m_state = State::CONNECTED;
            m_rx_buf_start = 0;
            m_rx_buf_length = 0;
        }
        
        void disconnect (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            
            m_command_stream.deinit(c);
            m_gcode_parser.deinit(c);
            
            m_connection.reset(c);
            
            m_state = State::NOT_CONNECTED;
        }
        
        void start_disconnect (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            
            if (m_command_stream.tryCancelCommand(c)) {
                disconnect(c);
            } else {
                m_connection.reset(c);
                m_state = State::DISCONNECTED_WAIT_CMD;
                m_command_stream.updateSendBufEvent(c);
            }
        }
        
        void connectionErrorHandler (Context c, bool remote_closed) override
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            
            m_command_stream.setAcceptMsg(c, false);
            auto err = remote_closed ? AMBRO_PSTR("//TcpConsoleDisconnected\n") : AMBRO_PSTR("//TcpConsoleError\n");
            ThePrinterMain::print_pgm_string(c, err);
            
            start_disconnect(c);
        }
        
        void connectionRecvHandler (Context c, size_t bytes_read) override
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            AMBRO_ASSERT(bytes_read <= BufferBaseSize - m_rx_buf_length)
            
            size_t write_offset = buf_add(m_rx_buf_start, m_rx_buf_length);
            size_t first_chunk_len = MinValue(bytes_read, (size_t)(BufferBaseSize - write_offset));
            
            m_connection.copyReceivedData(c, m_rx_buf + write_offset, first_chunk_len);
            if (first_chunk_len < bytes_read) {
                m_connection.copyReceivedData(c, m_rx_buf, bytes_read - first_chunk_len);
            }
            
            if (write_offset < WrapExtraSize) {
                memcpy(m_rx_buf + BufferBaseSize + write_offset, m_rx_buf + write_offset, MinValue(bytes_read, WrapExtraSize - write_offset));
            }
            if (bytes_read > BufferBaseSize - write_offset) {
                memcpy(m_rx_buf + BufferBaseSize, m_rx_buf, MinValue(bytes_read - (BufferBaseSize - write_offset), WrapExtraSize));
            }
            
            m_rx_buf_length += bytes_read;
            
            m_command_stream.setNextEventIfNoCommand(c);
        }
        
        void connectionSendHandler (Context c) override
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            
            m_command_stream.updateSendBufEvent(c);
        }
        
        static size_t buf_add (size_t start, size_t count)
        {
            size_t x = start + count;
            if (x >= BufferBaseSize) {
                x -= BufferBaseSize;
            }
            return x;
        }
        
        void next_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            AMBRO_ASSERT(!m_command_stream.hasCommand(c))
            
            if (m_state == State::DISCONNECTED_WAIT_CMD) {
                return disconnect(c);
            }
            
            size_t avail = MinValue(MaxCommandSize, m_rx_buf_length);
            bool line_buffer_exhausted = (avail == MaxCommandSize);
            
            if (!m_gcode_parser.haveCommand(c)) {
                m_gcode_parser.startCommand(c, m_rx_buf + m_rx_buf_start, 0);
            }
            
            if (m_gcode_parser.extendCommand(c, avail, line_buffer_exhausted)) {
                return m_command_stream.startCommand(c, &m_gcode_parser);
            }
            
            if (line_buffer_exhausted) {
                m_command_stream.setAcceptMsg(c, false);
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//TcpConsoleLineTooLong\n"));
                return disconnect(c);
            }
        }
        
        void finish_command_impl (Context c) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            
            if (m_state == State::CONNECTED) {
                size_t cmd_len = m_gcode_parser.getLength(c);
                AMBRO_ASSERT(cmd_len <= m_rx_buf_length)
                m_rx_buf_start = buf_add(m_rx_buf_start, cmd_len);
                m_rx_buf_length -= cmd_len;
                m_connection.acceptReceivedData(c, cmd_len);
            }
            
            m_command_stream.setNextEventAfterCommandFinished(c);
        }
        
        void reply_poke_impl (Context c) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            
            if (m_state == State::CONNECTED) {
                m_connection.pokeSending(c);
            }
        }
        
        void reply_append_buffer_impl (Context c, char const *str, size_t length) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            
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
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            
            return (m_state != State::CONNECTED) ? (size_t)-1 : m_connection.getSendBufferSpace(c);
        }
        
        void commandStreamError (Context c, typename TheConvenientStream::Error error) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            
            // Ignore errors if we're disconnected already and waiting for command to finish.
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
            
            start_disconnect(c);
        }
        
        bool mayWaitForSendBuf (Context c, size_t length) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::CONNECTED, State::DISCONNECTED_WAIT_CMD))
            
            return (m_state != State::CONNECTED || length <= TheTcpConnection::ProvidedTxBufSize);
        }
        
        TheTcpConnection m_connection;
        TheGcodeParser m_gcode_parser;
        TheConvenientStream m_command_stream;
        size_t m_rx_buf_start;
        size_t m_rx_buf_length;
        State m_state;
        char m_rx_buf[BufferBaseSize + WrapExtraSize];
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
    APRINTER_AS_VALUE(size_t, MaxCommandSize),
    APRINTER_AS_TYPE(SendBufTimeout)
), (
    APRINTER_MODULE_TEMPLATE(TcpConsoleModuleService, TcpConsoleModule)
))

#include <aprinter/EndNamespace.h>

#endif
