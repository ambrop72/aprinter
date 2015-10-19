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

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/printer/InputCommon.h>
#include <aprinter/printer/ServiceList.h>
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
    static size_t const MaxFinishLen = sizeof(TCPCONSOLE_OK_STR) - 1;
    
    using TheGcodeParser = GcodeParser<Context, Object, typename Params::TheGcodeParserParams, size_t, GcodeParserTypeSerial>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->listener.init(c, APRINTER_CB_STATFUNC_T(&TcpConsoleModule::listener_accept_handler));
        o->listener_working = o->listener.startListening(c, Params::Port);
        
        o->connection.init(c, APRINTER_CB_STATFUNC_T(&TcpConsoleModule::connection_error_handler),
                              APRINTER_CB_STATFUNC_T(&TcpConsoleModule::connection_recv_handler),
                              APRINTER_CB_STATFUNC_T(&TcpConsoleModule::connection_send_handler));
        o->have_connection = false;
        
        /*
        TheGcodeParser::init(c);
        o->command_stream.init(c, &o->callback);
        o->m_recv_next_error = 0;
        o->m_line_number = 1;
        */
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        /*
        o->command_stream.deinit(c);
        TheGcodeParser::deinit(c);
        */
        
        o->connection.deinit(c);
        o->listener.deinit(c);
    }
    
    static bool check_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (cmd->getCmdNumber(c) == 988) {
            if (!o->listener_working) {
                o->listener_working = o->listener.startListening(c, Params::Port);
            }
            cmd->finishCommand(c);
            return false;
        }
        if (cmd->getCmdNumber(c) == 987) {
            cmd->reply_append_uint32(c, o->listener_working);
            cmd->reply_append_ch(c, ' ');
            cmd->reply_append_uint32(c, o->have_connection);
            cmd->reply_append_ch(c, '\n');
            cmd->finishCommand(c);
            return false;
        }
        return true;
    }
    
private:
    static void close_connection (Context c)
    {
        auto *o = Object::self(c);
        
        o->connection.reset(c);
        o->have_connection = false;
    }
    
    static bool listener_accept_handler (Context c)
    {
        auto *o = Object::self(c);
        
        APRINTER_CONSOLE_MSG("//TcpAccept");
        
        if (o->have_connection) {
            close_connection(c);
        }
        
        o->connection.acceptConnection(c, &o->listener);
        o->have_connection = true;
        
        return true;
    }
    
    static void connection_error_handler (Context c, bool remote_closed)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->have_connection)
        
        if (remote_closed) {
            APRINTER_CONSOLE_MSG("//TcpClosed");
        } else {
            APRINTER_CONSOLE_MSG("//TcpError");
        }
        
        close_connection(c);
    }
    
    static void connection_recv_handler (Context c, size_t length)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->have_connection)
        
        auto *out = ThePrinterMain::get_msg_output(c);
        out->reply_append_pstr(c, AMBRO_PSTR("//TcpRx len="));
        out->reply_append_uint32(c, length);
        out->reply_append_ch(c, '\n');
        out->reply_poke(c);
        
        o->connection.acceptReceivedData(c, length);
    }
    
    static void connection_send_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->have_connection)
        
        //
    }
    
    /*
    struct StreamCallback : public ThePrinterMain::CommandStreamCallback {
        bool start_command_impl (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->command_stream.hasCommand(c))
            
            bool is_m110 = (TheGcodeParser::getCmdCode(c) == 'M' && TheGcodeParser::getCmdNumber(c) == 110);
            if (is_m110) {
                o->m_line_number = o->command_stream.get_command_param_uint32(c, 'L', (TheGcodeParser::getCmd(c)->have_line_number ? TheGcodeParser::getCmd(c)->line_number : -1));
            }
            if (TheGcodeParser::getCmd(c)->have_line_number) {
                if (TheGcodeParser::getCmd(c)->line_number != o->m_line_number) {
                    o->command_stream.reply_append_pstr(c, AMBRO_PSTR("Error:Line Number is not Last Line Number+1, Last Line:"));
                    o->command_stream.reply_append_uint32(c, (uint32_t)(o->m_line_number - 1));
                    o->command_stream.reply_append_ch(c, '\n');
                    return false;
                }
            }
            if (TheGcodeParser::getCmd(c)->have_line_number || is_m110) {
                o->m_line_number++;
            }
            return true;
        }
        
        void finish_command_impl (Context c, bool no_ok)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->command_stream.hasCommand(c))
            
            if (!no_ok) {
                o->command_stream.reply_append_pstr(c, AMBRO_PSTR(SERIALMODULE_OK_STR));
            }
            TheSerial::sendPoke(c);
            TheSerial::recvConsume(c, RecvSizeType::import(TheGcodeParser::getLength(c)));
            TheSerial::recvForceEvent(c);
        }
        
        void reply_poke_impl (Context c)
        {
            TheSerial::sendPoke(c);
        }
        
        void reply_append_buffer_impl (Context c, char const *str, size_t length)
        {
            SendSizeType avail = TheSerial::sendQuery(c);
            if (length > avail.value()) {
                length = avail.value();
            }
            while (length > 0) {
                char *chunk_data = TheSerial::sendGetChunkPtr(c);
                uint8_t chunk_length = TheSerial::sendGetChunkLen(c, SendSizeType::import(length)).value();
                memcpy(chunk_data, str, chunk_length);
                str += chunk_length;
                TheSerial::sendProvide(c, SendSizeType::import(chunk_length));
                length -= chunk_length;
            }
        }
        
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
        void reply_append_pbuffer_impl (Context c, AMBRO_PGM_P pstr, size_t length)
        {
            SendSizeType avail = TheSerial::sendQuery(c);
            if (length > avail.value()) {
                length = avail.value();
            }
            while (length > 0) {
                char *chunk_data = TheSerial::sendGetChunkPtr(c);
                uint8_t chunk_length = TheSerial::sendGetChunkLen(c, SendSizeType::import(length)).value();
                AMBRO_PGM_MEMCPY(chunk_data, pstr, chunk_length);
                pstr += chunk_length;
                TheSerial::sendProvide(c, SendSizeType::import(chunk_length));
                length -= chunk_length;
            }
        }
#endif
        
        bool request_send_buf_event_impl (Context c, size_t length)
        {
            if (length > SendSizeType::maxIntValue() - MaxFinishLen) {
                return false;
            }
            TheSerial::sendRequestEvent(c, SendSizeType::import(length + MaxFinishLen));
            return true;
        }
        
        void cancel_send_buf_event_impl (Context c)
        {
            TheSerial::sendRequestEvent(c, SendSizeType::import(0));
        }
    };
    
    static void serial_recv_handler (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->command_stream.hasCommand(c)) {
            return;
        }
        if (!TheGcodeParser::haveCommand(c)) {
            TheGcodeParser::startCommand(c, TheSerial::recvGetChunkPtr(c), o->m_recv_next_error);
            o->m_recv_next_error = 0;
        }
        bool overrun;
        RecvSizeType avail = TheSerial::recvQuery(c, &overrun);
        if (TheGcodeParser::extendCommand(c, avail.value())) {
            return o->command_stream.startCommand(c, &o->gcode_command);
        }
        if (overrun) {
            TheSerial::recvConsume(c, avail);
            TheSerial::recvClearOverrun(c);
            TheGcodeParser::resetCommand(c);
            o->m_recv_next_error = GCODE_ERROR_RECV_OVERRUN;
        }
    }
    struct SerialRecvHandler : public AMBRO_WFUNC_TD(&SerialModule::serial_recv_handler) {};
    
    static void serial_send_handler (Context c)
    {
        auto *o = Object::self(c);
        o->command_stream.reportSendBufEventDirectly(c);
    }
    struct SerialSendHandler : public AMBRO_WFUNC_TD(&SerialModule::serial_send_handler) {};
    */
    
public:
    struct Object : public ObjBase<TcpConsoleModule, ParentObject, MakeTypeList<
        TheGcodeParser
    >> {
        typename Context::Network::TcpListener listener;
        bool listener_working;
        typename Context::Network::TcpConnection connection;
        bool have_connection;
        /*
        typename ThePrinterMain::CommandStream command_stream;
        StreamCallback callback;
        GcodeCommandWrapper<Context, typename ThePrinterMain::FpType, TheGcodeParser> gcode_command;
        int8_t m_recv_next_error;
        uint32_t m_line_number;
        */
    };
};

template <
    typename TTheGcodeParserParams,
    uint16_t TPort
>
struct TcpConsoleModuleService {
    using TheGcodeParserParams = TTheGcodeParserParams;
    static uint16_t const Port = TPort;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = TcpConsoleModule<Context, ParentObject, ThePrinterMain, TcpConsoleModuleService>;
};

#include <aprinter/EndNamespace.h>

#endif
