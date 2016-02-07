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

#ifndef APRINTER_FILE_CONFIG_STORE_H
#define APRINTER_FILE_CONFIG_STORE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/fs/BufferedFile.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ConfigManager, typename ThePrinterMain, typename Handler, typename Params>
class FileConfigStore {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using TheFsAccess = typename ThePrinterMain::template GetFsAccess<>;
    using TheBufferedFile = BufferedFile<Context, TheFsAccess>;
    
    static constexpr char const *ConfigFileName = "aprinter.cfg";
    static size_t const MaxLineSize = 128;
    
    enum class State : uint8_t {
        IDLE,
        WRITE_OPEN, WRITE_OPTION, WRITE_EOF,
        READ_OPEN, READ_DATA
    };
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->buffered_file.init(c, APRINTER_CB_STATFUNC_T(&FileConfigStore::file_handler));
        o->state = State::IDLE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->buffered_file.deinit(c);
    }
    
    static void startWriting (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == State::IDLE)
        
        o->buffered_file.startOpen(c, ConfigFileName, false, TheBufferedFile::OpenMode::OPEN_WRITE);
        o->state = State::WRITE_OPEN;
    }
    
    static void startReading (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == State::IDLE)
        
        o->buffered_file.startOpen(c, ConfigFileName, false, TheBufferedFile::OpenMode::OPEN_READ);
        o->state = State::READ_OPEN;
    }
    
private:
    static void complete_command (Context c, bool error)
    {
        auto *o = Object::self(c);
        
        o->buffered_file.reset(c);
        o->state = State::IDLE;
        
        return Handler::call(c, !error);
    }
    
    static void file_handler (Context c, typename TheBufferedFile::Error error, size_t read_length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state != State::IDLE)
        
        if (error != TheBufferedFile::Error::NO_ERROR) {
            return complete_command(c, true);
        }
        
        switch (o->state) {
            case State::WRITE_OPEN: {
                o->write_option_index = 0;
                
                work_write(c);
            } break;
            
            case State::WRITE_OPTION: {
                work_write(c);
            } break;
            
            case State::WRITE_EOF: {
                return complete_command(c, false);
            } break;
            
            case State::READ_OPEN: {
                o->read_data_length = 0;
                o->read_line_overflow = false;
                o->read_error = false;
                
                work_read(c);
            } break;
            
            case State::READ_DATA: {
                AMBRO_ASSERT(read_length <= MaxLineSize - o->read_data_length)
                
                if (read_length == 0) {
                    if (o->read_data_length > 0) {
                        o->read_error = true;
                    }
                    return complete_command(c, o->read_error);
                }
                
                o->read_data_length += read_length;
                
                work_read(c);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
    static void work_write (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->write_option_index >= ConfigManager::NumRuntimeOptions) {
            o->buffered_file.startWriteEof(c);
            o->state = State::WRITE_EOF;
            return;
        }
        
        ConfigManager::getOptionString(c, o->write_option_index, o->line_buffer, MaxLineSize);
        size_t base_length = strlen(o->line_buffer);
        AMBRO_ASSERT(base_length < MaxLineSize)
        o->line_buffer[base_length] = '\n';
        
        o->write_option_index++;
        
        o->buffered_file.startWriteData(c, o->line_buffer, base_length + 1);
        o->state = State::WRITE_OPTION;
    }
    
    static void work_read (Context c)
    {
        auto *o = Object::self(c);
        
        while (o->read_data_length > 0) {
            char *newline_ptr = (char *)memchr(o->line_buffer, '\n', o->read_data_length);
            if (!newline_ptr) {
                if (o->read_data_length == MaxLineSize) {
                    o->read_data_length = 0;
                    o->read_line_overflow = true;
                    o->read_error = true;
                }
                break;
            }
            
            size_t line_length = (newline_ptr + 1) - o->line_buffer;
            
            if (o->read_line_overflow) {
                o->read_line_overflow = false;
            } else {
                char *equals_ptr = (char *)memchr(o->line_buffer, '=', line_length - 1);
                if (equals_ptr) {
                    *equals_ptr = '\0';
                    *newline_ptr = '\0';
                    ConfigManager::setOptionByStrings(c, o->line_buffer, equals_ptr + 1);
                } else {
                    o->read_error = true;
                }
            }
            
            o->read_data_length -= line_length;
            memmove(o->line_buffer, o->line_buffer + line_length, o->read_data_length);
        }
        
        o->buffered_file.startReadData(c, o->line_buffer + o->read_data_length, MaxLineSize - o->read_data_length);
        o->state = State::READ_DATA;
    }
    
public:
    struct Object : public ObjBase<FileConfigStore, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        TheBufferedFile buffered_file;
        State state;
        int write_option_index;
        size_t read_data_length;
        bool read_line_overflow : 1;
        bool read_error : 1;
        char line_buffer[MaxLineSize];
    };
};

struct FileConfigStoreService {
    template <typename Context, typename ParentObject, typename ConfigManager, typename ThePrinterMain, typename Handler>
    using Store = FileConfigStore<Context, ParentObject, ConfigManager, ThePrinterMain, Handler, FileConfigStoreService>;
};

#include <aprinter/EndNamespace.h>

#endif
