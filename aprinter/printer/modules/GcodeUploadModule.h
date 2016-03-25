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

#ifndef APRINTER_GCODE_UPLOAD_MODULE_H
#define APRINTER_GCODE_UPLOAD_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/math/PrintInt.h>
#include <aprinter/fs/BufferedFile.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class GcodeUploadModule {
public:
    struct Object;
    
private:
    static uint16_t const MCodeStartGcodeUpload = 28;
    static uint16_t const MCodeStopGcodeUpload  = 29;
    
    static size_t const MaxCommandSize = Params::MaxCommandSize;
    static_assert(MaxCommandSize >= 32, "");
    
    using TheCommand = typename ThePrinterMain::TheCommand;
    using TheFsAccess = typename ThePrinterMain::template GetFsAccess<>;
    using TheBufferedFile = BufferedFile<Context, TheFsAccess>;
    
    enum class State {IDLE, OPENING, READY, WRITING, CLOSING};
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->file.init(c, APRINTER_CB_STATFUNC_T(&GcodeUploadModule::file_handler));
        o->state = State::IDLE;
        o->captured_stream = nullptr;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->file.deinit(c);
    }
    
    static bool check_command (Context c, TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == MCodeStartGcodeUpload) {
            handle_start_command(c, cmd);
            return false;
        }
        if (cmd->getCmdNumber(c) == MCodeStopGcodeUpload) {
            handle_stop_command(c, cmd);
            return false;
        }
        return true;
    }
    
private:
    static void handle_start_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        
        if (o->state != State::IDLE || o->captured_stream) {
            cmd->reportError(c, AMBRO_PSTR("GcodeUploadBusy"));
            return cmd->finishCommand(c);
        }
        
        char const *filename = cmd->get_command_param_str(c, 'F', nullptr);
        if (!filename) {
            cmd->reportError(c, AMBRO_PSTR("NoFileSpecified"));
            return cmd->finishCommand(c);
        }
        
        o->state = State::OPENING;
        o->file.startOpen(c, filename, true, TheBufferedFile::OpenMode::OPEN_WRITE);
    }
    
    static void handle_stop_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        
        if (o->state != State::READY) {
            cmd->reportError(c, AMBRO_PSTR("GcodeUploadNotRunning"));
            return cmd->finishCommand(c);
        }
        
        start_closing(c, true);
    }
    
    static void file_handler (Context c, typename TheBufferedFile::Error error, size_t read_length)
    {
        auto *o = Object::self(c);
        
        switch (o->state) {
            case State::OPENING: {
                AMBRO_ASSERT(!o->captured_stream)
                
                auto *cmd = ThePrinterMain::get_locked(c);
                if (error != TheBufferedFile::Error::NO_ERROR) {
                    AMBRO_PGM_P errstr = (error == TheBufferedFile::Error::NOT_FOUND) ? AMBRO_PSTR("NotFound") : AMBRO_PSTR("OpenFailed");
                    cmd->reportError(c, errstr);
                    o->file.reset(c);
                    o->state = State::IDLE;
                } else {
                    o->state = State::READY;
                    cmd->startCapture(c, &GcodeUploadModule::captured_command_handler);
                    o->captured_stream = cmd;
                }
                cmd->finishCommand(c);
            } break;
            
            case State::WRITING: {
                AMBRO_ASSERT(o->captured_stream)
                
                auto *cmd = ThePrinterMain::get_locked(c);
                if (error != TheBufferedFile::Error::NO_ERROR) {
                    cmd->reportError(c, AMBRO_PSTR("WriteFailed"));
                }
                o->state = State::READY;
                cmd->finishCommand(c);
            } break;
            
            case State::CLOSING: {
                AMBRO_PGM_P errstr = (error != TheBufferedFile::Error::NO_ERROR) ? AMBRO_PSTR("CloseFailed") : nullptr;
                complete_close(c, errstr);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
    static void captured_command_handler (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->captured_stream)
        AMBRO_ASSERT(!cmd || cmd == o->captured_stream)
        
        if (!cmd) {
            o->captured_stream = nullptr;
            if (o->state == State::READY) {
                start_closing(c, false);
            }
            return;
        }
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        
        if (cmd->getCmdCode(c) == 'M' && cmd->getCmdNumber(c) == MCodeStopGcodeUpload) {
            if (o->state == State::READY) {
                start_closing(c, true);
            } else {
                cmd->stopCapture(c);
                o->captured_stream = nullptr;
                cmd->finishCommand(c);
            }
            return;
        }
        
        if (o->state != State::READY) {
            cmd->reportError(c, AMBRO_PSTR("GcodeUploadClosed"));
            return cmd->finishCommand(c);
        }
        
        if (!o->file.isReady(c)) {
            cmd->reportError(c, AMBRO_PSTR("FileNotReady"));
            return cmd->finishCommand(c);
        }
        
        size_t command_length;
        if (!generate_command(c, cmd, &command_length)) {
            cmd->reportError(c, AMBRO_PSTR("CannotEncode"));
            return cmd->finishCommand(c);
        }
        
        o->state = State::WRITING;
        o->file.startWriteData(c, o->command_buf, command_length);
    }
    
    static void start_closing (Context c, bool for_command)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == State::READY)
        
        o->state = State::CLOSING;
        o->closing_for_command = for_command;
        
        if (!o->file.isReady(c)) {
            return complete_close(c, AMBRO_PSTR("FileNotReady"));
        }
        
        o->file.startWriteEof(c);
    }
    
    static void complete_close (Context c, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == State::CLOSING)
        
        o->file.reset(c);
        o->state = State::IDLE;
        
        if (o->closing_for_command) {
            auto *cmd = ThePrinterMain::get_locked(c);
            if (errstr) {
                cmd->reportError(c, errstr);
            }
            if (cmd == o->captured_stream) {
                cmd->stopCapture(c);
                o->captured_stream = nullptr;
            }
            cmd->finishCommand(c);
        }
    }
    
    static bool generate_command (Context c, TheCommand *cmd, size_t *out_length)
    {
        auto *o = Object::self(c);
        
        size_t length = 0;
        
        o->command_buf[length++] = cmd->getCmdCode(c);
        
        length += PrintNonnegativeIntDecimal(cmd->getCmdNumber(c), o->command_buf + length);
        
        auto num_parts = cmd->getNumParts(c);
        for (decltype(num_parts) i = 0; i < num_parts; i++) {
            auto part = cmd->getPart(c, i);
            char code = cmd->getPartCode(c, part);
            char const *arg = cmd->getPartStringValue(c, part);
            
            if (AMBRO_UNLIKELY(!arg)) {
                return false;
            }
            
            if (AMBRO_UNLIKELY(MaxCommandSize - length < 2)) {
                return false;
            }
            o->command_buf[length++] = ' ';
            o->command_buf[length++] = code;
            
            while (true) {
                char ch = *arg++;
                if (ch == '\0') {
                    break;
                }
                
                if (AMBRO_UNLIKELY(char_needs_escape(ch))) {
                    if (AMBRO_UNLIKELY(MaxCommandSize - length < 3)) {
                        return false;
                    }
                    uint8_t uch = ch;
                    o->command_buf[length++] = '\\';
                    o->command_buf[length++] = make_hex_digit(uch >> 4);
                    o->command_buf[length++] = make_hex_digit(uch & 0xF);
                } else {
                    if (AMBRO_UNLIKELY(MaxCommandSize - length < 1)) {
                        return false;
                    }
                    o->command_buf[length++] = ch;
                }
            }
        }
        
        if (AMBRO_UNLIKELY(MaxCommandSize - length < 1)) {
            return false;
        }
        o->command_buf[length++] = '\n';
        
        *out_length = length;
        return true;
    }
    
    static bool char_needs_escape (char ch)
    {
        return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\0');
    }
    
    static char make_hex_digit (uint8_t d)
    {
        return (d < 10) ? ('0' + d) : ('A' + (d - 10));
    }
    
public:
    struct Object : public ObjBase<GcodeUploadModule, ParentObject, EmptyTypeList> {
        TheBufferedFile file;
        State state;
        TheCommand *captured_stream;
        bool closing_for_command;
        char command_buf[MaxCommandSize];
    };
};

APRINTER_ALIAS_STRUCT_EXT(GcodeUploadModuleService, (
    APRINTER_AS_VALUE(size_t, MaxCommandSize)
), (
    APRINTER_MODULE_TEMPLATE(GcodeUploadModuleService, GcodeUploadModule)
))

#include <aprinter/EndNamespace.h>

#endif
