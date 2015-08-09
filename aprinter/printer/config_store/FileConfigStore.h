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
    using TheDebugObject = DebugObject<Context, Object>;
    using TheFsAccess = typename ThePrinterMain::template GetInput<>::template GetFsAccess<>;
    using TheFs = typename TheFsAccess::TheFileSystem;
    
    static constexpr char const *ConfigFileName = "aprinter.cfg";
    static size_t const MaxLineSize = 128;
    static bool const OpenCaseInsens = true;
    
    enum {
        STATE_IDLE,
        STATE_WRITE_ACCESS, STATE_WRITE_BUFFER, STATE_WRITE_OPEN, STATE_WRITE_OPENWR, STATE_WRITE_WRITE, STATE_WRITE_TRUNCATE, STATE_WRITE_FLUSH,
        STATE_READ_ACCESS, STATE_READ_BUFFER, STATE_READ_OPEN, STATE_READ_READ
    };
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->access_client.init(c, APRINTER_CB_STATFUNC_T(&FileConfigStore::access_client_handler));
        o->user_buffer.init(c, APRINTER_CB_STATFUNC_T(&FileConfigStore::user_buffer_handler));
        o->state = STATE_IDLE;
        o->have_opener = false;
        o->have_file = false;
        o->have_flush = false;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        reset_internal(c);
        o->user_buffer.deinit(c);
        o->access_client.deinit(c);
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
        
        if (o->have_flush) {
            o->have_flush = false;
            o->fs_flush.deinit(c);
        }
        if (o->have_file) {
            o->have_file = false;
            o->fs_file.deinit(c);
        }
        if (o->have_opener) {
            o->have_opener = false;
            o->fs_opener.deinit(c);
        }
        o->user_buffer.reset(c);
        o->access_client.reset(c);
        o->state = STATE_IDLE;
    }
    
    static void complete_command (Context c, bool error)
    {
        reset_internal(c);
        return Handler::call(c, !error);
    }
    
    static void access_client_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_WRITE_ACCESS || o->state == STATE_READ_ACCESS)
        
        if (error) {
            return complete_command(c, true);
        }
        
        o->state = (o->state == STATE_WRITE_ACCESS) ? STATE_WRITE_BUFFER : STATE_READ_BUFFER;
        o->user_buffer.requestUserBuffer(c);
    }
    
    static void user_buffer_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_WRITE_BUFFER || o->state == STATE_READ_BUFFER)
        AMBRO_ASSERT(!o->have_opener)
        
        if (error) {
            return complete_command(c, true);
        }
        
        o->state = (o->state == STATE_WRITE_BUFFER) ? STATE_WRITE_OPEN : STATE_READ_OPEN;
        o->fs_opener.init(c, TheFs::getRootEntry(c), TheFs::EntryType::FILE_TYPE, ConfigFileName, OpenCaseInsens, APRINTER_CB_STATFUNC_T(&FileConfigStore::fs_opener_handler));
        o->have_opener = true;
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
            o->line_buffer_length = 0;
            start_read(c);
        }
    }
    
    static void start_write (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->data_length > 0)
        o->fs_file.startWrite(c, WrapBuffer::Make(o->user_buffer.getUserBuffer(c)), o->data_length);
        o->state = STATE_WRITE_WRITE;
    }
    
    static void start_read (Context c)
    {
        auto *o = Object::self(c);
        o->fs_file.startRead(c, WrapBuffer::Make(o->user_buffer.getUserBuffer(c)));
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
            o->write_option_index = 0;
            o->data_length = 0;
            o->line_buffer_length = 0;
            
            return work_write(c);
        }
        else if (o->state == STATE_WRITE_WRITE) {
            o->data_length = 0;
            
            return work_write(c);
        }
        else if (o->state == STATE_WRITE_TRUNCATE) {
            AMBRO_ASSERT(o->have_file)
            AMBRO_ASSERT(!o->have_flush)
            
            o->fs_file.deinit(c);
            o->have_file = false;
            
            o->fs_flush.init(c, APRINTER_CB_STATFUNC_T(&FileConfigStore::fs_flush_handler));
            o->have_flush = true;
            
            o->fs_flush.requestFlush(c);
            o->state = STATE_WRITE_FLUSH;
        }
        else { // STATE_READ_READ
            AMBRO_ASSERT(read_length <= TheFs::TheBlockSize)
            o->data_length = read_length;
            
            return work_read(c);
        }
    }
    
    static void work_write (Context c)
    {
        auto *o = Object::self(c);
        
        while (true) {
            size_t amount = MinValue(o->line_buffer_length, TheFs::TheBlockSize - o->data_length);
            if (amount != 0) {
                memcpy(o->user_buffer.getUserBuffer(c) + o->data_length, o->line_buffer, amount);
                o->line_buffer_length -= amount;
                memmove(o->line_buffer, o->line_buffer + amount, o->line_buffer_length);
                o->data_length += amount;
            }
            
            if (o->line_buffer_length != 0 || o->write_option_index == ConfigManager::NumRuntimeOptions) {
                break;
            }
            
            ConfigManager::getOptionString(c, o->write_option_index, o->line_buffer, MaxLineSize);
            size_t base_length = strlen(o->line_buffer);
            AMBRO_ASSERT(base_length < MaxLineSize)
            o->line_buffer[base_length] = '\n';
            o->line_buffer_length = base_length + 1;
            o->write_option_index++;
        }
        
        if (o->data_length == 0) {
            o->fs_file.startTruncate(c);
            o->state = STATE_WRITE_TRUNCATE;
            return;
        }
        
        start_write(c);
    }
    
    static void work_read (Context c)
    {
        auto *o = Object::self(c);
        
        size_t parse_pos = 0;
        
        while (true) {
            char *data_ptr = o->user_buffer.getUserBuffer(c) + parse_pos;
            size_t data_left = o->data_length - parse_pos;
            
            size_t allowed_rem_line_length = MaxLineSize - o->line_buffer_length;
            size_t look_forward_length = MinValue(data_left, allowed_rem_line_length);
            char *newline_ptr = (char *)memchr(data_ptr, '\n', look_forward_length);
            
            size_t line_data_length = newline_ptr ? ((newline_ptr + 1) - data_ptr) : look_forward_length;
            memcpy(o->line_buffer + o->line_buffer_length, data_ptr, line_data_length);
            o->line_buffer_length += line_data_length;
            parse_pos += line_data_length;
            
            if (!newline_ptr) {
                if (data_left >= allowed_rem_line_length) {
                    return complete_command(c, true);
                }
                AMBRO_ASSERT(parse_pos == o->data_length)
                AMBRO_ASSERT(o->line_buffer_length < MaxLineSize)
                break;
            }
            
            char *equals_ptr = (char *)memchr(o->line_buffer, '=', o->line_buffer_length - 1);
            if (equals_ptr) {
                *equals_ptr = '\0';
                o->line_buffer[o->line_buffer_length - 1] = '\0';
                ConfigManager::setOptionByStrings(c, o->line_buffer, equals_ptr + 1);
            }
            o->line_buffer_length = 0;
        }
        
        if (o->data_length == 0) {
            return complete_command(c, (o->line_buffer_length > 0));
        }
        
        start_read(c);
    }
    
    static void fs_flush_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_WRITE_FLUSH)
        
        return complete_command(c, error);
    }
    
public:
    struct Object : public ObjBase<FileConfigStore, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        typename TheFsAccess::Client access_client;
        typename TheFsAccess::UserBuffer user_buffer;
        union {
            typename TheFs::Opener fs_opener;
            typename TheFs::template File<true> fs_file;
            typename TheFs::template FlushRequest<> fs_flush;
        };
        uint8_t state : 4;
        bool have_opener : 1;
        bool have_file : 1;
        bool have_flush : 1;
        int write_option_index;
        size_t data_length;
        size_t line_buffer_length;
        char line_buffer[MaxLineSize];
    };
};

struct FileConfigStoreService {
    template <typename Context, typename ParentObject, typename ConfigManager, typename ThePrinterMain, typename Handler>
    using Store = FileConfigStore<Context, ParentObject, ConfigManager, ThePrinterMain, Handler, FileConfigStoreService>;
};

#include <aprinter/EndNamespace.h>

#endif
