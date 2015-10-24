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
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/printer/GcodeParser.h>
#include <aprinter/printer/GcodeCommand.h>
#include <aprinter/printer/Console.h>

#include <aprinter/BeginNamespace.h>

#define TCPCONSOLE_OK_STR "ok\n"

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class TcpConsoleModule {
public:
    struct Object;
    
private:
    using TheTcpConnection = typename Context::Network::TcpConnection;
    
    static int const MaxClients = Params::MaxClients;
    static size_t const MaxFinishLen = sizeof(TCPCONSOLE_OK_STR) - 1;
    static size_t const BufferBaseSize = TheTcpConnection::RequiredRxBufSize;
    static_assert(Params::MaxCommandSize > 0, "");
    static_assert(BufferBaseSize >= Params::MaxCommandSize, "");
    static size_t const WrapExtraSize = Params::MaxCommandSize - 1;
    static_assert(TheTcpConnection::ProvidedTxBufSize >= MaxFinishLen, "");
    
    using TheGcodeParser = GcodeParser<Context, typename Params::TheGcodeParserParams, size_t, typename ThePrinterMain::FpType, GcodeParserTypeSerial>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->listener.init(c, APRINTER_CB_STATFUNC_T(&TcpConsoleModule::listener_accept_handler));
        o->listener.startListening(c, Params::Port);
        
        for (int i = 0; i < MaxClients; i++) {
            o->clients[i].init(c);
        }
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < MaxClients; i++) {
            o->clients[i].deinit(c);
        }
        
        o->listener.deinit(c);
    }
    
private:
    static bool listener_accept_handler (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < MaxClients; i++) {
            Client *cl = &o->clients[i];
            if (cl->m_state == Client::State::NOT_CONNECTED) {
                cl->accept_connection(c);
                return true;
            }
        }
        
        APRINTER_CONSOLE_MSG("//TcpConsoleAcceptNoSpace");
        return false;
    }
    
    struct Client : public ThePrinterMain::CommandStreamCallback
    {
        enum class State : uint8_t {NOT_CONNECTED, CONNECTED, DISCONNECTED_WAIT_CMD};
        
        void init (Context c)
        {
            m_next_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::next_event_handler, this));
            m_send_buf_check_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::send_buf_check_event_handler, this));
            m_connection.init(c, APRINTER_CB_OBJFUNC_T(&Client::connection_error_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_recv_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_send_handler, this));
            m_state = State::NOT_CONNECTED;
        }
        
        void deinit (Context c)
        {
            if (m_state != State::NOT_CONNECTED) {
                m_command_stream.deinit(c);
                m_gcode_parser.deinit(c);
            }
            m_connection.deinit(c);
            m_send_buf_check_event.deinit(c);
            m_next_event.deinit(c);
        }
        
        void accept_connection (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::NOT_CONNECTED)
            
            APRINTER_CONSOLE_MSG("//TcpConsoleAccept");
            
            m_connection.acceptConnection(c, &o->listener);
            m_state = State::CONNECTED;
            m_rx_buf_start = 0;
            m_rx_buf_length = 0;
            m_send_buf_request = 0;
            m_gcode_parser.init(c);
            m_command_stream.init(c, this);
        }
        
        void disconnect (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            
            m_command_stream.deinit(c);
            m_gcode_parser.deinit(c);
            m_next_event.unset(c);
            m_send_buf_check_event.unset(c);
            m_connection.reset(c);
            m_state = State::NOT_CONNECTED;
        }
        
        void start_disconnect (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            
            if (m_command_stream.hasCommand(c)) {
                m_connection.reset(c);
                m_state = State::DISCONNECTED_WAIT_CMD;
                update_send_buf_event(c);
            } else {
                disconnect(c);
            }
        }
        
        void update_send_buf_event (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            
            if (m_send_buf_request > 0 && !m_send_buf_check_event.isSet(c)) {
                m_send_buf_check_event.prependNowNotAlready(c);
            }
        }
        
        void connection_error_handler (Context c, bool remote_closed)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            
            if (remote_closed) {
                APRINTER_CONSOLE_MSG("//TcpConsoleClosed");
            } else {
                APRINTER_CONSOLE_MSG("//TcpConsoleError");
            }
            
            start_disconnect(c);
        }
        
        void connection_recv_handler (Context c, size_t bytes_read)
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
            
            if (!m_command_stream.hasCommand(c) && !m_next_event.isSet(c)) {
                m_next_event.prependNowNotAlready(c);
            }
        }
        
        void connection_send_handler (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED)
            
            update_send_buf_event(c);
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
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            AMBRO_ASSERT(!m_command_stream.hasCommand(c))
            
            if (m_state == State::DISCONNECTED_WAIT_CMD) {
                return disconnect(c);
            }
            
            size_t avail = MinValue(Params::MaxCommandSize, m_rx_buf_length);
            bool line_buffer_exhausted = (avail == Params::MaxCommandSize);
            
            if (!m_gcode_parser.haveCommand(c)) {
                m_gcode_parser.startCommand(c, m_rx_buf + m_rx_buf_start, 0);
            }
            
            if (m_gcode_parser.extendCommand(c, avail, line_buffer_exhausted)) {
                return m_command_stream.startCommand(c, &m_gcode_parser);
            }
            
            if (line_buffer_exhausted) {
                return disconnect(c);
            }
        }
        
        void send_buf_check_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            AMBRO_ASSERT(m_send_buf_request > 0)
            
            if (m_state != State::CONNECTED || m_connection.getSendBufferSpace(c) >= m_send_buf_request + MaxFinishLen) {
                m_send_buf_request = 0;
                return m_command_stream.reportSendBufEventDirectly(c);
            }
        }
        
        bool start_command_impl (Context c)
        {
            return true;
        }
        
        void finish_command_impl (Context c, bool no_ok)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            
            if (m_state == State::CONNECTED) {
                if (!no_ok) {
                    m_command_stream.reply_append_pstr(c, AMBRO_PSTR(TCPCONSOLE_OK_STR));
                }
                m_connection.pokeSending(c);
                
                size_t cmd_len = m_gcode_parser.getLength(c);
                AMBRO_ASSERT(cmd_len <= m_rx_buf_length)
                
                m_rx_buf_start = buf_add(m_rx_buf_start, cmd_len);
                m_rx_buf_length -= cmd_len;
                
                m_connection.acceptReceivedData(c, cmd_len);
            }
            
            m_next_event.prependNowNotAlready(c);
        }
        
        void reply_poke_impl (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            
            if (m_state == State::CONNECTED) {
                m_connection.pokeSending(c);
            }
        }
        
        void reply_append_buffer_impl (Context c, char const *str, size_t length)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            
            if (m_state == State::CONNECTED) {
                size_t avail = m_connection.getSendBufferSpace(c);
                if (avail < length) {
                    m_connection.raiseError(c);
                    return;
                }
                m_connection.copySendData(c, str, length);
            }
        }
        
        bool request_send_buf_event_impl (Context c, size_t length)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            AMBRO_ASSERT(m_send_buf_request == 0)
            AMBRO_ASSERT(length > 0)
            
            if (length > TheTcpConnection::ProvidedTxBufSize - MaxFinishLen) {
                return false;
            }
            m_send_buf_request = length;
            m_send_buf_check_event.prependNowNotAlready(c);
            return true;
        }
        
        void cancel_send_buf_event_impl (Context c)
        {
            AMBRO_ASSERT(m_state == State::CONNECTED || m_state == State::DISCONNECTED_WAIT_CMD)
            AMBRO_ASSERT(m_send_buf_request > 0)
            
            m_send_buf_request = 0;
            m_send_buf_check_event.unset(c);
        }
        
        typename Context::EventLoop::QueuedEvent m_next_event;
        typename Context::EventLoop::QueuedEvent m_send_buf_check_event;
        TheTcpConnection m_connection;
        TheGcodeParser m_gcode_parser;
        typename ThePrinterMain::CommandStream m_command_stream;
        size_t m_rx_buf_start;
        size_t m_rx_buf_length;
        size_t m_send_buf_request;
        State m_state;
        char m_rx_buf[BufferBaseSize + WrapExtraSize];
    };
    
public:
    struct Object : public ObjBase<TcpConsoleModule, ParentObject, EmptyTypeList> {
        typename Context::Network::TcpListener listener;
        Client clients[MaxClients];
    };
};

template <
    typename TTheGcodeParserParams,
    uint16_t TPort,
    int TMaxClients,
    size_t TMaxCommandSize
>
struct TcpConsoleModuleService {
    using TheGcodeParserParams = TTheGcodeParserParams;
    static uint16_t const Port = TPort;
    static int const MaxClients = TMaxClients;
    static size_t const MaxCommandSize = TMaxCommandSize;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = TcpConsoleModule<Context, ParentObject, ThePrinterMain, TcpConsoleModuleService>;
};

#include <aprinter/EndNamespace.h>

#endif
