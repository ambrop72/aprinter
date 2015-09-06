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
#include <aprinter/meta/If.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/InputCommon.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/GcodeCommand.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class SdCardModule {
public:
    struct Object;
    
private:
    using TheCommand = typename ThePrinterMain::TheCommand;
    
    struct InputReadHandler;
    struct InputClearBufferHandler;
    using TheInput = typename Params::InputService::template Input<Context, Object, InputClientParams<ThePrinterMain, InputReadHandler, InputClearBufferHandler>>;
    static const size_t BufferBaseSize = Params::BufferBaseSize;
    static const size_t MaxCommandSize = Params::MaxCommandSize;
    static_assert(MaxCommandSize > 0, "");
    static_assert(BufferBaseSize >= TheInput::NeedBufAvail + (MaxCommandSize - 1), "");
    static const size_t WrapExtraSize = MaxCommandSize - 1;
    using ParserSizeType = ChooseIntForMax<MaxCommandSize, false>;
    using TheGcodeParser = typename Params::template GcodeParserTemplate<Context, Object, typename Params::TheGcodeParserParams, ParserSizeType>;
    
    enum {SDCARD_PAUSED, SDCARD_RUNNING, SDCARD_PAUSING};
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        TheInput::init(c);
        o->command_stream.init(c, &o->callback);
        o->m_next_event.init(c, APRINTER_CB_STATFUNC_T(&SdCardModule::next_event_handler));
        o->m_state = SDCARD_PAUSED;
        init_buffering(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        deinit_buffering(c);
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
        
        // Start/resume SD stream.
        if (cmd->getCmdNumber(c) == 24) {
            if (!cmd->tryLockedCommand(c)) {
                return false;
            }
            do {
                if (o->m_state != SDCARD_PAUSED) {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdPrintAlreadyActive\n"));
                    break;
                }
                if (!TheInput::startingIo(c, cmd)) {
                    break;
                }
                o->m_state = SDCARD_RUNNING;
                o->m_eof = false;
                o->m_reading = false;
                if (can_read(c)) {
                    start_read(c);
                }
                if (!o->command_stream.maybeResumeLockingCommand(c)) {
                    o->m_next_event.prependNowNotAlready(c);
                }
            } while (false);
            cmd->finishCommand(c);
            return false;
        }
        
        // Pause SD stream.
        if (cmd->getCmdNumber(c) == 25) {
            if (!cmd->tryLockedCommand(c)) {
                return false;
            }
            do {
                if (o->m_state == SDCARD_PAUSED) {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdPrintNotRunning\n"));
                    break;
                }
                AMBRO_ASSERT(o->m_state != SDCARD_PAUSING || o->m_reading)
                o->m_next_event.unset(c);
                o->command_stream.maybePauseLockingCommand(c);
                if (o->m_reading) {
                    o->m_state = SDCARD_PAUSING;
                    o->m_pausing_on_command = true;
                    return false;
                }
                complete_pause(c);
            } while (false);
            cmd->finishCommand(c);
            return false;
        }
        
        // Rewind SD stream.
        if (cmd->getCmdNumber(c) == 26) {
            if (!cmd->tryLockedCommand(c)) {
                return false;
            }
            do {
                if (o->m_state != SDCARD_PAUSED) {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdPrintRunning\n"));
                    break;
                }
                uint32_t seek_pos = cmd->get_command_param_uint32(c, 'S', 0);
                if (seek_pos != 0) {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("Error:CanOnlySeekToZero\n"));
                    break;
                }
                if (!TheInput::rewind(c, cmd)) {
                    break;
                }
            } while (false);
            cmd->finishCommand(c);
            return false;
        }
        
        // Let the Input module implement its own commands.
        return TheInput::checkCommand(c, cmd);
    }
    
    template <typename This=SdCardModule>
    using GetFsAccess = typename This::TheInput::template GetFsAccess<>;
    
    using GetInput = TheInput;
    
private:
    struct StreamCallback : public ThePrinterMain::CommandStreamCallback {
        bool start_command_impl (Context c)
        {
            return true;
        }
        
        void finish_command_impl (Context c, bool no_ok)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
            buf_sanity(c);
            AMBRO_ASSERT(!o->m_eof)
            AMBRO_ASSERT(!TheGcodeParser::haveCommand(c))
            AMBRO_ASSERT(TheGcodeParser::getLength(c) <= o->m_length)
            
            size_t cmd_len = TheGcodeParser::getLength(c);
            o->m_next_event.prependNowNotAlready(c);
            o->m_start = buf_add(o->m_start, cmd_len);
            o->m_length -= cmd_len;
            if (!o->m_reading && can_read(c)) {
                start_read(c);
            }
        }
        
        void reply_poke_impl (Context c)
        {
        }
        
        void reply_append_buffer_impl (Context c, char const *str, AMBRO_PGM_P pstr, size_t length)
        {
        }
        
        void reply_append_ch_impl (Context c, char ch)
        {
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
    static void input_read_handler (Context c, bool error, size_t bytes_read)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_RUNNING || o->m_state == SDCARD_PAUSING)
        buf_sanity(c);
        AMBRO_ASSERT(o->m_reading)
        AMBRO_ASSERT(bytes_read <= BufferBaseSize - o->m_length)
        
        o->m_reading = false;
        if (!error) {
            size_t write_offset = buf_add(o->m_start, o->m_length);
            if (write_offset < WrapExtraSize) {
                memcpy(o->m_buffer + BufferBaseSize + write_offset, o->m_buffer + write_offset, MinValue(bytes_read, WrapExtraSize - write_offset));
            }
            if (bytes_read > BufferBaseSize - write_offset) {
                memcpy(o->m_buffer + BufferBaseSize, o->m_buffer, MinValue(bytes_read - (BufferBaseSize - write_offset), WrapExtraSize));
            }
            o->m_length += bytes_read;
        }
        if (o->m_state == SDCARD_PAUSING) {
            if (o->m_pausing_on_command) {
                ThePrinterMain::finish_locked(c);
            }
            return complete_pause(c);
        }
        if (error) {
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//SdRdEr\n"));
            return start_read(c);
        }
        if (can_read(c)) {
            start_read(c);
        }
        if (!o->command_stream.hasCommand(c) && !o->m_eof) {
            if (!o->m_next_event.isSet(c)) {
                o->m_next_event.prependNowNotAlready(c);
            }
        }
    }
    struct InputReadHandler : public AMBRO_WFUNC_TD(&SdCardModule::input_read_handler) {};
    
    static void clear_input_buffer (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_PAUSED)
        
        o->command_stream.maybeCancelLockingCommand(c);
        deinit_buffering(c);
        init_buffering(c);
    }
    struct InputClearBufferHandler : public AMBRO_WFUNC_TD(&SdCardModule::clear_input_buffer) {};
    
    static void next_event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
        buf_sanity(c);
        AMBRO_ASSERT(!o->command_stream.hasCommand(c))
        AMBRO_ASSERT(!o->m_eof)
        
        AMBRO_PGM_P eof_str;
        if (!TheGcodeParser::haveCommand(c)) {
            TheGcodeParser::startCommand(c, o->m_buffer + o->m_start, 0);
        }
        ParserSizeType avail = MinValue(MaxCommandSize, o->m_length);
        if (TheGcodeParser::extendCommand(c, avail)) {
            if (TheGcodeParser::getNumParts(c) == GCODE_ERROR_EOF) {
                eof_str = AMBRO_PSTR("//SdEof\n");
                goto eof;
            }
            return o->command_stream.startCommand(c, &o->gcode_command);
        }
        if (avail == MaxCommandSize) {
            eof_str = AMBRO_PSTR("//SdLnEr\n");
            goto eof;
        }
        if (TheInput::eofReached(c)) {
            eof_str = AMBRO_PSTR("//SdEnd\n");
            goto eof;
        }
        return;
        
    eof:
        ThePrinterMain::print_pgm_string(c, eof_str);
        
        o->m_eof = true;
        if (o->m_reading) {
            o->m_state = SDCARD_PAUSING;
            o->m_pausing_on_command = false;
        } else {
            complete_pause(c);
        }
    }
    
    static void init_buffering (Context c)
    {
        auto *o = Object::self(c);
        TheGcodeParser::init(c);
        o->m_start = 0;
        o->m_length = 0;
    }
    
    static void deinit_buffering (Context c)
    {
        TheGcodeParser::deinit(c);
    }
    
    static bool can_read (Context c)
    {
        auto *o = Object::self(c);
        return TheInput::canRead(c, BufferBaseSize - o->m_length);
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
        TheInput::startRead(c, BufferBaseSize - o->m_length, WrapBuffer::Make(BufferBaseSize - write_offset, o->m_buffer + write_offset, o->m_buffer));
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
        o->m_state = SDCARD_PAUSED;
    }
    
public:
    struct Object : public ObjBase<SdCardModule, ParentObject, MakeTypeList<
        TheInput,
        TheGcodeParser
    >> {
        typename ThePrinterMain::CommandStream command_stream;
        StreamCallback callback;
        GcodeCommandWrapper<Context, typename ThePrinterMain::FpType, TheGcodeParser> gcode_command;
        typename Context::EventLoop::QueuedEvent m_next_event;
        uint8_t m_state : 3;
        uint8_t m_eof : 1;
        uint8_t m_reading : 1;
        uint8_t m_pausing_on_command : 1;
        size_t m_start;
        size_t m_length;
        char m_buffer[BufferBaseSize + WrapExtraSize];
    };
};

template <
    typename TInputService,
    template<typename, typename, typename, typename> class TGcodeParserTemplate,
    typename TTheGcodeParserParams,
    size_t TBufferBaseSize,
    size_t TMaxCommandSize
>
struct SdCardModuleService {
    using InputService = TInputService;
    template <typename X, typename Y, typename Z, typename W> using GcodeParserTemplate = TGcodeParserTemplate<X, Y, Z, W>;
    using TheGcodeParserParams = TTheGcodeParserParams;
    static size_t const BufferBaseSize = TBufferBaseSize;
    static size_t const MaxCommandSize = TMaxCommandSize;
    
    using ProvidedServices = If<InputService::ProvidesFsAccess, MakeTypeList<ServiceList::FsAccessService>, EmptyTypeList>;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = SdCardModule<Context, ParentObject, ThePrinterMain, SdCardModuleService>;
};

#include <aprinter/EndNamespace.h>

#endif
