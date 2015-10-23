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

#ifndef APRINTER_SERIAL_MODULE_H
#define APRINTER_SERIAL_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/printer/InputCommon.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/GcodeParser.h>
#include <aprinter/printer/GcodeCommand.h>

#include <aprinter/BeginNamespace.h>

#define SERIALMODULE_OK_STR "ok\n"

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class SerialModule {
public:
    struct Object;
    
private:
    static size_t const MaxFinishLen = sizeof(SERIALMODULE_OK_STR) - 1;
    
    struct SerialRecvHandler;
    struct SerialSendHandler;
    using TheSerial = typename Params::SerialService::template Serial<Context, Object, Params::RecvBufferSizeExp, Params::SendBufferSizeExp, SerialRecvHandler, SerialSendHandler>;
    using RecvSizeType = typename TheSerial::RecvSizeType;
    using SendSizeType = typename TheSerial::SendSizeType;
    using TheGcodeParser = GcodeParser<Context, typename Params::TheGcodeParserParams, typename RecvSizeType::IntType, typename ThePrinterMain::FpType, GcodeParserTypeSerial>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        TheSerial::init(c, Params::Baud);
        o->gcode_parser.init(c);
        o->command_stream.init(c, &o->callback);
        o->m_recv_next_error = 0;
        o->m_line_number = 1;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->command_stream.deinit(c);
        o->gcode_parser.deinit(c);
        TheSerial::deinit(c);
    }
    
    static typename ThePrinterMain::CommandStream * get_serial_stream (Context c)
    {
        auto *o = Object::self(c);
        return &o->command_stream;
    }
    
    using GetSerial = TheSerial;
    
private:
    struct StreamCallback : public ThePrinterMain::CommandStreamCallback {
        bool start_command_impl (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->command_stream.hasCommand(c))
            
            bool is_m110 = (o->gcode_parser.getCmdCode(c) == 'M' && o->gcode_parser.getCmdNumber(c) == 110);
            if (is_m110) {
                o->m_line_number = o->command_stream.get_command_param_uint32(c, 'L', (o->gcode_parser.getCmd(c)->have_line_number ? o->gcode_parser.getCmd(c)->line_number : -1));
            }
            if (o->gcode_parser.getCmd(c)->have_line_number) {
                if (o->gcode_parser.getCmd(c)->line_number != o->m_line_number) {
                    o->command_stream.reply_append_pstr(c, AMBRO_PSTR("Error:Line Number is not Last Line Number+1, Last Line:"));
                    o->command_stream.reply_append_uint32(c, (uint32_t)(o->m_line_number - 1));
                    o->command_stream.reply_append_ch(c, '\n');
                    return false;
                }
            }
            if (o->gcode_parser.getCmd(c)->have_line_number || is_m110) {
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
            TheSerial::recvConsume(c, RecvSizeType::import(o->gcode_parser.getLength(c)));
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
        if (!o->gcode_parser.haveCommand(c)) {
            o->gcode_parser.startCommand(c, TheSerial::recvGetChunkPtr(c), o->m_recv_next_error);
            o->m_recv_next_error = 0;
        }
        bool overrun;
        RecvSizeType avail = TheSerial::recvQuery(c, &overrun);
        if (o->gcode_parser.extendCommand(c, avail.value())) {
            return o->command_stream.startCommand(c, &o->gcode_parser);
        }
        if (overrun) {
            TheSerial::recvConsume(c, avail);
            TheSerial::recvClearOverrun(c);
            o->gcode_parser.resetCommand(c);
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
    
public:
    struct Object : public ObjBase<SerialModule, ParentObject, MakeTypeList<
        TheSerial
    >> {
        TheGcodeParser gcode_parser;
        typename ThePrinterMain::CommandStream command_stream;
        StreamCallback callback;
        int8_t m_recv_next_error;
        uint32_t m_line_number;
    };
};

template <
    uint32_t TBaud,
    int TRecvBufferSizeExp,
    int TSendBufferSizeExp,
    typename TTheGcodeParserParams,
    typename TSerialService
>
struct SerialModuleService {
    static uint32_t const Baud = TBaud;
    static int const RecvBufferSizeExp = TRecvBufferSizeExp;
    static int const SendBufferSizeExp = TSendBufferSizeExp;
    using TheGcodeParserParams = TTheGcodeParserParams;
    using SerialService = TSerialService;
    
    using ProvidedServices = MakeTypeList<ServiceList::SerialService>;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = SerialModule<Context, ParentObject, ThePrinterMain, SerialModuleService>;
};

#include <aprinter/EndNamespace.h>

#endif
