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

#ifndef AMBROLIB_FILE_CONFIG_STORE_H
#define AMBROLIB_FILE_CONFIG_STORE_H

#include <stdint.h>
#include <string.h>

#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ConfigManager, typename ThePrinterMain, typename Handler, typename Params>
class FileConfigStore {
public:
    struct Object;
    
private:
    using OptionSpecList = typename ConfigManager::RuntimeConfigOptionsList;
    using TheDebugObject = DebugObject<Context, Object>;
    using TheFsAccess = typename ThePrinterMain::template GetInput<>::template GetFsAccess<>;
    using TheFs = typename TheFsAccess::TheFileSystem;
    
    static constexpr char const *ConfigFileName = "aprinter.cfg";
    static size_t const MaxLineSize = 128;
    
    enum {
        STATE_IDLE,
        STATE_WRITE_ACCESS, STATE_WRITE_OPEN, STATE_WRITE_OPENWR, STATE_WRITE_WRITE, STATE_WRITE_TRUNCATE,
        STATE_READ_ACCESS, STATE_READ_OPEN, STATE_READ_READ
    };
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->event.init(c, APRINTER_CB_STATFUNC_T(&FileConfigStore::event_handler));
        o->access_client.init(c, APRINTER_CB_STATFUNC_T(&FileConfigStore::access_client_handler));
        o->state = STATE_IDLE;
        o->have_opener = false;
        o->have_file = false;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        reset_internal(c);
        o->access_client.deinit(c);
        o->event.deinit(c);
    }
    
    static void startWriting (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        
        o->access_client.requestAccess(c, true);
        o->state = STATE_WRITE_ACCESS;
    }
    
    static void startReading (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        
        o->access_client.requestAccess(c, false);
        o->state = STATE_READ_ACCESS;
    }
    
private:
    static void reset_internal (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->have_file) {
            o->fs_file.deinit(c);
            o->have_file = false;
        }
        if (o->have_opener) {
            o->fs_opener.deinit(c);
            o->have_opener = false;
        }
        o->access_client.reset(c);
        o->event.unset(c);
        o->state = STATE_IDLE;
    }
    
    static void complete_command (Context c, bool error)
    {
        reset_internal(c);
        return Handler::call(c, !error);
    }
    
    static void event_handler (Context c)
    {
        TheDebugObject::access(c);
        
        return Handler::call(c, false);
    }
    
    static void access_client_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_WRITE_ACCESS || o->state == STATE_READ_ACCESS)
        AMBRO_ASSERT(!o->have_opener)
        
        if (error) {
            return complete_command(c, true);
        }
        
        o->state = (o->state == STATE_WRITE_ACCESS) ? STATE_WRITE_OPEN : STATE_READ_OPEN;
        o->fs_opener.init(c, TheFs::getRootEntry(c), TheFs::EntryType::FILE_TYPE, ConfigFileName, false, APRINTER_CB_STATFUNC_T(&FileConfigStore::fs_opener_handler));
        o->have_opener =  true;
    }
    
    static void fs_opener_handler (Context c, typename TheFs::Opener::OpenerStatus status, typename TheFs::FsEntry entry)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_WRITE_OPEN || o->state == STATE_READ_OPEN)
        AMBRO_ASSERT(o->have_opener)
        AMBRO_ASSERT(!o->have_file)
        
        if (status != TheFs::Opener::OpenerStatus::SUCCESS) {
            return complete_command(c, true);
        }
        
        o->fs_opener.deinit(c);
        o->have_opener = false;
        
        o->fs_file.init(c, entry, APRINTER_CB_STATFUNC_T(&FileConfigStore::fs_file_handler));
        o->have_file = true;
        
        if (o->state == STATE_WRITE_OPEN) {
            o->state = STATE_WRITE_OPENWR;
            o->fs_file.startOpenWritable(c);
        } else {
            o->data_start = 0;
            o->data_length = 0;
            return start_read(c);
        }
    }
    
    static void start_write (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->data_length > 0)
        o->fs_file.startWrite(c, WrapBuffer::Make(o->buffer), MinValue(o->data_length, TheFs::TheBlockSize));
        o->state = STATE_WRITE_WRITE;
    }
    
    static void start_read (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->data_start == 0)
        o->fs_file.startRead(c, WrapBuffer::Make(o->buffer + o->data_length));
        o->state = STATE_READ_READ;
    }
    
    static void fs_file_handler (Context c, bool io_error, size_t read_length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_WRITE_OPENWR || o->state == STATE_WRITE_WRITE || o->state == STATE_WRITE_TRUNCATE || o->state == STATE_READ_READ)
        
        if (io_error) {
            return complete_command(c, true);
        }
        
        if (o->state == STATE_WRITE_OPENWR) {
            o->data_length = 0;
            o->write_option_index = 0;
            return work_write(c);
        } else if (o->state == STATE_WRITE_WRITE) {
            size_t write_length = MinValue(o->data_length, TheFs::TheBlockSize);
            memmove(o->buffer, o->buffer + write_length, o->data_length - write_length);
            o->data_length -= write_length;
            return work_write(c);
        } else if (o->state == STATE_WRITE_TRUNCATE) {
            return complete_command(c, io_error);
        } else {
            o->data_length += read_length;
            return work_read(c, (read_length == 0));
        }
    }
    
    static void work_write (Context c)
    {
        auto *o = Object::self(c);
        
        while (o->write_option_index < ConfigManager::NumRuntimeOptions && o->data_length < TheFs::TheBlockSize) {
            ConfigManager::getOptionString(c, o->write_option_index, o->buffer + o->data_length, MaxLineSize);
            size_t line_length = strlen(o->buffer + o->data_length);
            AMBRO_ASSERT(line_length < MaxLineSize)
            o->buffer[o->data_length + line_length] = '\n';
            o->data_length += line_length + 1;
            AMBRO_ASSERT(o->data_length <= sizeof(o->buffer))
            o->write_option_index++;
        }
        
        if (o->data_length == 0) {
            o->fs_file.startTruncate(c);
            o->state = STATE_WRITE_TRUNCATE;
            return;
        }
        
        start_write(c);
    }
    
    static void work_read (Context c, bool eof_reached)
    {
        auto *o = Object::self(c);
        
        while (o->data_length > 0) {
            char *data_ptr = o->buffer + o->data_start;
            char *newline_ptr = (char *)memchr(data_ptr, '\n', MinValue(o->data_length, MaxLineSize));
            if (!newline_ptr) {
                if (o->data_length >= MaxLineSize) {
                    return complete_command(c, true);
                }
                break;
            }
            
            size_t line_length = newline_ptr - data_ptr;
            char *equals_ptr = (char *)memchr(data_ptr, '=', line_length);
            if (equals_ptr) {
                *equals_ptr = '\0';
                *newline_ptr = '\0';
                ConfigManager::setOptionByStrings(c, data_ptr, equals_ptr + 1);
            }
            
            o->data_start += line_length + 1;
            o->data_length -= line_length + 1;
        }
        
        memmove(o->buffer, o->buffer + o->data_start, o->data_length);
        o->data_start = 0;
        
        if (eof_reached) {
            return complete_command(c, (o->data_length > 0));
        }
        
        start_read(c);
    }
    
public:
    struct Object : public ObjBase<FileConfigStore, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        typename Context::EventLoop::QueuedEvent event;
        typename TheFsAccess::Client access_client;
        typename TheFs::Opener fs_opener;
        typename TheFs::template File<true> fs_file;
        uint8_t state;
        bool have_opener : 1;
        bool have_file : 1;
        size_t data_start;
        size_t data_length;
        int write_option_index;
        char buffer[TheFs::TheBlockSize + (MaxLineSize - 1)];
    };
};

struct FileConfigStoreService {
    template <typename Context, typename ParentObject, typename ConfigManager, typename ThePrinterMain, typename Handler>
    using Store = FileConfigStore<Context, ParentObject, ConfigManager, ThePrinterMain, Handler, FileConfigStoreService>;
};

#include <aprinter/EndNamespace.h>

#endif
