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

#ifndef APRINTER_SD_CARD_MODULE_H
#define APRINTER_SD_CARD_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/input/InputCommon.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/utils/GcodeCommand.h>
#include <aprinter/printer/utils/ModuleUtils.h>
#include <aprinter/printer/utils/JsonBuilder.h>

namespace APrinter {

template <typename ModuleArg>
class SdCardModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    using TheCommand = typename ThePrinterMain::TheCommand;
    
    struct InputReadHandler;
    struct InputClearBufferHandler;
    struct InputStartHandler;
    APRINTER_MAKE_INSTANCE(TheInput, (Params::InputService::template Input<Context, Object, InputClientParams<ThePrinterMain, InputReadHandler, InputClearBufferHandler, InputStartHandler>>))
    
    using DataWordType = typename TheInput::DataWordType;
    
    static const size_t BufferBaseSize = Params::BufferBaseSize;
    static_assert(BufferBaseSize % sizeof(DataWordType) == 0, "Buffer size must be a multiple of data word size");
    static const size_t BufferBaseSizeWords = BufferBaseSize / sizeof(DataWordType);
    
    static const size_t BlockSize = TheInput::ReadBlockSize;
    static_assert(BlockSize % sizeof(DataWordType) == 0, "");
    static_assert(BufferBaseSize % BlockSize == 0, "Buffer size must be a multiple of block size");
    
    static const size_t MaxCommandSize = Params::MaxCommandSize;
    static_assert(MaxCommandSize > 0, "");
    static_assert(BufferBaseSize >= BlockSize + (MaxCommandSize - 1), "");
    
    static const size_t WrapExtraSize = MaxCommandSize - 1;
    static const size_t WrapExtraSizeWords = (WrapExtraSize + (sizeof(DataWordType) - 1)) / sizeof(DataWordType);
    
    using ParserSizeType = ChooseIntForMax<MaxCommandSize, false>;
    using TheGcodeParser = typename Params::TheGcodeParserService::template Parser<Context, ParserSizeType, typename ThePrinterMain::FpType>;
    
    static TimeType const BaseRetryTimeTicks = 0.5 * Context::Clock::time_freq;
    static int const ReadRetryCount = 5;
    
    enum {SDCARD_PAUSED, SDCARD_RUNNING, SDCARD_PAUSING};
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        TheInput::init(c);
        o->command_stream.init(c, &o->callback, &o->callback);
        o->command_stream.setAcceptMsg(c, false);
        o->command_stream.setAutoOkAndPoke(c, false);
        o->m_next_event.init(c, APRINTER_CB_STATFUNC_T(&SdCardModule::next_event_handler));
        o->m_retry_timer.init(c, APRINTER_CB_STATFUNC_T(&SdCardModule::retry_timer_handler));
        o->m_state = SDCARD_PAUSED;
        o->m_echo_pending = true;
        o->m_poke_pending = false;
        init_buffering(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        deinit_buffering(c);
        o->m_retry_timer.deinit(c);
        o->m_next_event.deinit(c);
        o->command_stream.deinit(c);
        TheInput::deinit(c);
    }
    
    static bool check_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        // Cannot issue SD-card commands from the SD-card.
        if (cmd == &o->command_stream) {
            return true;
        }
        
        switch (cmd->getCmdNumber(c)) {
            case 24: handle_start_command(c, cmd);  return false;
            case 25: handle_pause_command(c, cmd);  return false;
            case 26: handle_rewind_command(c, cmd); return false;
        }
        
        // Let the Input module implement its own commands.
        return TheInput::checkCommand(c, cmd);
    }
    
    template <typename TheJsonBuilder>
    static void get_json_status (Context c, TheJsonBuilder *json)
    {
        json->addKeyObject(JsonSafeString{"sdcard"});
        TheInput::get_json_status(c, json);
        json->endObject();
    }
    
    template <typename This=SdCardModule>
    using GetFsAccess = typename This::TheInput::template GetFsAccess<>;
    
    using GetInput = TheInput;
    
private:
    struct StreamCallback: public ThePrinterMain::CommandStreamCallback, ThePrinterMain::SendBufEventCallback {
        void finish_command_impl (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
            buf_sanity(c);
            AMBRO_ASSERT(!o->m_eof)
            
            if (o->m_poke_pending) {
                reply_poke_impl(c, false);
            }
            
            if (o->command_stream.getGcodeCommand(c) == &o->gcode_m400_command) {
                o->m_eof = true;
                if (o->m_reading) {
                    o->m_state = SDCARD_PAUSING;
                    o->m_pausing_on_command = false;
                } else {
                    complete_pause(c);
                }
                return;
            }
            
            AMBRO_ASSERT(!o->gcode_parser.haveCommand(c))
            AMBRO_ASSERT(o->gcode_parser.getLength(c) <= o->m_length)
            
            size_t cmd_len = o->gcode_parser.getLength(c);
            o->m_start = buf_add(o->m_start, cmd_len);
            o->m_length -= cmd_len;
            
            o->m_next_event.prependNowNotAlready(c);
            
            if (!o->m_reading && can_read(c) && o->m_retry_counter == 0) {
                start_read(c);
            }
        }
        
        void reply_poke_impl (Context c, bool push)
        {
            auto *o = Object::self(c);
            
            o->m_poke_pending = false;
            ThePrinterMain::get_msg_output(c)->reply_poke(c);
        }
        
        void reply_append_buffer_impl (Context c, char const *str, size_t length)
        {
            auto *o = Object::self(c);
            
            while (length > 0) {
                o->m_poke_pending = true;
                
                if (o->m_echo_pending) {
                    o->m_echo_pending = false;
                    ThePrinterMain::get_msg_output(c)->reply_append_pstr(c, AMBRO_PSTR("//SdEcho "));
                }
                
                size_t line_length = 0;
                while (line_length < length) {
                    if (str[line_length] == '\n') {
                        line_length++;
                        o->m_echo_pending = true;
                        break;
                    }
                    line_length++;
                }
                
                ThePrinterMain::get_msg_output(c)->reply_append_buffer(c, str, line_length);
                
                str += line_length;
                length -= line_length;
            }
        }
        
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
        void reply_append_pbuffer_impl (Context c, AMBRO_PGM_P pstr, size_t length)
        {
            auto *o = Object::self(c);
            
            while (length > 0) {
                o->m_poke_pending = true;
                
                if (o->m_echo_pending) {
                    o->m_echo_pending = false;
                    ThePrinterMain::get_msg_output(c)->reply_append_pstr(c, AMBRO_PSTR("//SdEcho "));
                }
                
                size_t line_length = 0;
                while (line_length < length) {
                    if (AMBRO_PGM_READBYTE(pstr + line_length) == '\n') {
                        line_length++;
                        o->m_echo_pending = true;
                        break;
                    }
                    line_length++;
                }
                
                ThePrinterMain::get_msg_output(c)->reply_append_pbuffer(c, pstr, line_length);
                
                pstr += line_length;
                length -= line_length;
            }
        }
#endif
        size_t get_send_buf_avail_impl (Context c)
        {
            return (size_t)-1;
        }
        
        bool request_send_buf_event_impl (Context c, size_t length)
        {
            return false;
        }
        
        void cancel_send_buf_event_impl (Context c)
        {
        }
    };
    
private:
    static bool do_start (Context c, TheCommand *err_output)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_PAUSED)
        
        if (!TheInput::startingIo(c, err_output)) {
            return false;
        }
        
        o->m_state = SDCARD_RUNNING;
        o->m_eof = false;
        o->m_reading = false;
        o->m_retry_counter = 0;
        o->command_stream.clearError(c);
        
        if (can_read(c)) {
            start_read(c);
        }
        
        if (!o->command_stream.maybeResumeCommand(c)) {
            o->m_next_event.prependNowNotAlready(c);
        }
        
        return true;
    }
    
    static void handle_start_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        do {
            if (o->m_state != SDCARD_PAUSED) {
                cmd->reportError(c, AMBRO_PSTR("SdPrintAlreadyActive"));
                break;
            }
            if (!do_start(c, cmd)) {
                cmd->reportError(c, nullptr);
            }
        } while (false);
        cmd->finishCommand(c);
    }
    
    static void handle_pause_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        do {
            if (o->m_state == SDCARD_PAUSED) {
                cmd->reportError(c, AMBRO_PSTR("SdPrintNotRunning"));
                break;
            }
            AMBRO_ASSERT(o->m_state != SDCARD_PAUSING || o->m_reading)
            o->m_next_event.unset(c);
            if (o->command_stream.getGcodeCommand(c) == &o->gcode_m400_command) {
                o->command_stream.maybeCancelCommand(c);
            } else {
                o->command_stream.maybePauseCommand(c);
            }
            if (o->m_reading) {
                o->m_state = SDCARD_PAUSING;
                o->m_pausing_on_command = true;
                return;
            }
            complete_pause(c);
        } while (false);
        cmd->finishCommand(c);
    }
    
    static void handle_rewind_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        do {
            if (o->m_state != SDCARD_PAUSED) {
                cmd->reportError(c, AMBRO_PSTR("SdPrintRunning"));
                break;
            }
            uint32_t seek_pos = cmd->get_command_param_uint32(c, 'S', 0);
            if (seek_pos != 0) {
                cmd->reportError(c, AMBRO_PSTR("CanOnlySeekToZero"));
                break;
            }
            if (!TheInput::rewind(c, cmd)) {
                cmd->reportError(c, nullptr);
            }
        } while (false);
        cmd->finishCommand(c);
    }
    
    static void input_read_handler (Context c, bool error, size_t bytes_read)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_RUNNING || o->m_state == SDCARD_PAUSING)
        buf_sanity(c);
        AMBRO_ASSERT(o->m_reading)
        AMBRO_ASSERT(bytes_read <= BufferBaseSize - o->m_length)
        AMBRO_ASSERT(!o->m_retry_timer.isSet(c))
        AMBRO_ASSERT(o->m_retry_counter <= ReadRetryCount)
        
        o->m_reading = false;
        
        if (!error) {
            size_t write_offset = buf_add(o->m_start, o->m_length);
            if (write_offset < WrapExtraSize) {
                memcpy((char *)o->m_buffer + BufferBaseSize + write_offset, (char *)o->m_buffer + write_offset, MinValue(bytes_read, WrapExtraSize - write_offset));
            }
            if (bytes_read > BufferBaseSize - write_offset) {
                memcpy((char *)o->m_buffer + BufferBaseSize, (char *)o->m_buffer, MinValue(bytes_read - (BufferBaseSize - write_offset), WrapExtraSize));
            }
            o->m_length += bytes_read;
        }
        
        if (o->m_state == SDCARD_PAUSING) {
            if (o->m_pausing_on_command) {
                auto *cmd = ThePrinterMain::get_locked(c);
                cmd->finishCommand(c);
            }
            return complete_pause(c);
        }
        
        if (error) {
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//Error:SdRead\n"));
            o->m_retry_counter++;
            if (o->m_retry_counter <= ReadRetryCount) {
                TimeType retry_time = Context::Clock::getTime(c) + (BaseRetryTimeTicks << (o->m_retry_counter - 1));
                o->m_retry_timer.appendAt(c, retry_time);
            }
        } else {
            o->m_retry_counter = 0;
            
            if (can_read(c)) {
                start_read(c);
            }
        }
        
        if (!o->command_stream.hasCommand(c) && !o->m_eof && !o->m_next_event.isSet(c)) {
            o->m_next_event.prependNowNotAlready(c);
        }
    }
    struct InputReadHandler : public AMBRO_WFUNC_TD(&SdCardModule::input_read_handler) {};
    
    static void clear_input_buffer (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_PAUSED)
        
        o->command_stream.maybeCancelCommand(c);
        deinit_buffering(c);
        init_buffering(c);
    }
    struct InputClearBufferHandler : public AMBRO_WFUNC_TD(&SdCardModule::clear_input_buffer) {};
    
    static bool input_start_handler (Context c, TheCommand *err_output)
    {
        return do_start(c, err_output);
    }
    struct InputStartHandler : public AMBRO_WFUNC_TD(&SdCardModule::input_start_handler) {};
    
    static void next_event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
        buf_sanity(c);
        AMBRO_ASSERT(!o->command_stream.hasCommand(c))
        AMBRO_ASSERT(!o->m_eof)
        
        AMBRO_PGM_P eof_str;
        ParserSizeType avail;
        bool line_buffer_exhausted;
        
        if (o->command_stream.haveError(c)) {
            eof_str = AMBRO_PSTR("//SdCmdError\n");
            goto eof;
        }
        
        if (!o->gcode_parser.haveCommand(c)) {
            o->gcode_parser.startCommand(c, (char *)o->m_buffer + o->m_start, 0);
        }
        
        avail = MinValue(MaxCommandSize, o->m_length);
        line_buffer_exhausted = (avail == MaxCommandSize);
        
        if (o->gcode_parser.extendCommand(c, avail, line_buffer_exhausted)) {
            if (o->gcode_parser.getNumParts(c) == GCODE_ERROR_EOF) {
                eof_str = AMBRO_PSTR("//SdEof\n");
                goto eof;
            }
            return o->command_stream.startCommand(c, &o->gcode_parser);
        }
        
        if (line_buffer_exhausted) {
            eof_str = AMBRO_PSTR("//SdLnEr\n");
            goto eof;
        }
        
        if (TheInput::eofReached(c)) {
            eof_str = AMBRO_PSTR("//SdEnd\n");
            goto eof;
        }
        
        if (o->m_retry_counter > ReadRetryCount) {
            eof_str = AMBRO_PSTR("//SdAbort\n");
            goto eof;
        }
        
        return;
        
    eof:
        ThePrinterMain::print_pgm_string(c, eof_str);
        return o->command_stream.startCommand(c, &o->gcode_m400_command);
    }
    
    static void retry_timer_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
        AMBRO_ASSERT(!o->m_reading)
        AMBRO_ASSERT(o->m_retry_counter > 0)
        AMBRO_ASSERT(o->m_retry_counter <= ReadRetryCount)
        
        start_read(c);
    }
    
    static void init_buffering (Context c)
    {
        auto *o = Object::self(c);
        
        o->gcode_parser.init(c);
        o->m_start = 0;
        o->m_length = 0;
    }
    
    static void deinit_buffering (Context c)
    {
        auto *o = Object::self(c);
        o->gcode_parser.deinit(c);
    }
    
    static bool can_read (Context c)
    {
        auto *o = Object::self(c);
        return (BufferBaseSize - o->m_length >= BlockSize && TheInput::canRead(c));
    }
    
    static size_t buf_add (size_t start, size_t count)
    {
        static_assert(BufferBaseSize <= SIZE_MAX / 2, "");
        size_t x = start + count;
        if (x >= BufferBaseSize) {
            x -= BufferBaseSize;
        }
        return x;
    }
    
    static void start_read (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->m_reading)
        AMBRO_ASSERT(can_read(c))
        
        o->m_reading = true;
        size_t write_offset = buf_add(o->m_start, o->m_length);
        AMBRO_ASSERT(write_offset % BlockSize == 0)
        TheInput::startRead(c, o->m_buffer + write_offset / sizeof(DataWordType));
    }
    
    static void buf_sanity (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_start < BufferBaseSize)
        AMBRO_ASSERT(o->m_length <= BufferBaseSize)
    }
    
    static void complete_pause (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_RUNNING || o->m_state == SDCARD_PAUSING)
        AMBRO_ASSERT(!o->m_reading)
        
        TheInput::pausingIo(c);
        o->m_retry_timer.unset(c);
        o->m_state = SDCARD_PAUSED;
    }
    
public:
    struct Object : public ObjBase<SdCardModule, ParentObject, MakeTypeList<
        TheInput
    >> {
        TheGcodeParser gcode_parser;
        typename ThePrinterMain::CommandStream command_stream;
        StreamCallback callback;
        GcodeM400Command<Context, typename ThePrinterMain::FpType> gcode_m400_command;
        typename Context::EventLoop::QueuedEvent m_next_event;
        typename Context::EventLoop::TimedEvent m_retry_timer;
        uint8_t m_state : 3;
        uint8_t m_eof : 1;
        uint8_t m_reading : 1;
        uint8_t m_pausing_on_command : 1;
        uint8_t m_echo_pending : 1;
        uint8_t m_poke_pending : 1;
        uint8_t m_retry_counter;
        size_t m_start;
        size_t m_length;
        DataWordType m_buffer[BufferBaseSizeWords + WrapExtraSizeWords];
    };
};

APRINTER_ALIAS_STRUCT_EXT(SdCardModuleService, (
    APRINTER_AS_TYPE(InputService),
    APRINTER_AS_TYPE(TheGcodeParserService),
    APRINTER_AS_VALUE(size_t, BufferBaseSize),
    APRINTER_AS_VALUE(size_t, MaxCommandSize)
), (
    APRINTER_MODULE_TEMPLATE(SdCardModuleService, SdCardModule)
    
    using ProvidedServices = If<InputService::ProvidesFsAccess, MakeTypeList<ServiceDefinition<ServiceList::FsAccessService>>, EmptyTypeList>;
))

}

#endif
