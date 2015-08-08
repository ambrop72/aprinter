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

#ifndef APRINTER_FS_TEST_MODULE_H
#define APRINTER_FS_TEST_MODULE_H

#include <stdint.h>
#include <string.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Callback.h>
#include <aprinter/meta/MinMax.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain>
class FsTestModule {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using TheFsAccess = typename ThePrinterMain::template GetInput<>::template GetFsAccess<>;
    using TheFs = typename TheFsAccess::TheFileSystem;
    
    enum {STATE_IDLE, STATE_ACCESS, STATE_BUFFER, STATE_OPEN, STATE_OPENWR, STATE_WRITE, STATE_TRUNCATE, STATE_FLUSH};
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->access_client.init(c, APRINTER_CB_STATFUNC_T(&FsTestModule::access_client_handler));
        o->user_buffer.init(c, APRINTER_CB_STATFUNC_T(&FsTestModule::user_buffer_handler));
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
    
    static bool check_command (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        TheDebugObject::access(c);
        
        if (cmd->getCmdNumber(c) == 935) {
            handle_write_command(c, cmd);
            return false;
        }
        return true;
    }
    
private:
    static void reset_internal (Context c)
    {
        auto *o = Object::self(c);
        if (o->have_opener) {
            o->have_opener = false;
            o->fs_opener.deinit(c);
        }
        if (o->have_file) {
            o->have_file = false;
            o->fs_file.deinit(c);
        }
        if (o->have_flush) {
            o->have_flush = false;
            o->fs_flush.deinit(c);
        }
        o->user_buffer.reset(c);
        o->access_client.reset(c);
        o->state = STATE_IDLE;
    }
    
    static void debug_msg (Context c, AMBRO_PGM_P msg)
    {
        auto *output = ThePrinterMain::get_msg_output(c);
        output->reply_append_pstr(c, msg);
        output->reply_poke(c);
    }
    
    static void complete_command (Context c, AMBRO_PGM_P errstr)
    {
        reset_internal(c);
        auto *cmd = ThePrinterMain::get_locked(c);
        if (errstr) {
            cmd->reply_append_pstr(c, errstr);
        }
        cmd->finishCommand(c);
    }
    
    static void handle_write_command (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        AMBRO_ASSERT(o->state == STATE_IDLE)
        if (!(o->open_file_name = cmd->get_command_param_str(c, 'F', nullptr))) {
            return complete_command(c, AMBRO_PSTR("Error:NoFileSpecified\n"));
        }
        o->write_data = cmd->get_command_param_str(c, 'D', "1234567890");
        o->write_data_size = strlen(o->write_data);
        o->write_size = cmd->find_command_param(c, 'S', nullptr) ? cmd->get_command_param_uint32(c, 'S', 0) : o->write_data_size;
        o->access_client.requestAccess(c, true);
        o->state = STATE_ACCESS;
        debug_msg(c, AMBRO_PSTR("//FsTest:Access\n"));
    }
    
    static void access_client_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_ACCESS)
        
        if (error) {
            return complete_command(c, AMBRO_PSTR("Error:Access\n"));
        }
        o->user_buffer.requestUserBuffer(c);
        o->state = STATE_BUFFER;
        debug_msg(c, AMBRO_PSTR("//FsTest:Buffer\n"));
    }
    
    static void user_buffer_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_BUFFER)
        
        if (error) {
            return complete_command(c, AMBRO_PSTR("Error:Buffer\n"));
        }
        o->fs_opener.init(c, o->access_client.getCurrentDirectory(c), TheFs::EntryType::FILE_TYPE, o->open_file_name, true, APRINTER_CB_STATFUNC_T(&FsTestModule::fs_opener_handler));
        o->have_opener = true;
        o->state = STATE_OPEN;
        debug_msg(c, AMBRO_PSTR("//FsTest:Open\n"));
    }
    
    static void fs_opener_handler (Context c, typename TheFs::Opener::OpenerStatus status, typename TheFs::FsEntry entry)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_OPEN)
        
        if (status != TheFs::Opener::OpenerStatus::SUCCESS) {
            return complete_command(c, AMBRO_PSTR("Error:Open\n"));
        }
        o->fs_opener.deinit(c);
        o->have_opener = false;
        o->fs_file.init(c, entry, APRINTER_CB_STATFUNC_T(&FsTestModule::fs_file_handler));
        o->have_file = true;
        o->fs_file.startOpenWritable(c);
        o->state = STATE_OPENWR;
        debug_msg(c, AMBRO_PSTR("//FsTest:OpenWr\n"));
    }
    
    static void fs_file_handler (Context c, bool io_error, size_t read_length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        switch (o->state) {
            case STATE_OPENWR: {
                if (io_error) {
                    return complete_command(c, AMBRO_PSTR("Error:OpenWr\n"));
                }
                o->write_pos = 0;
                o->buffer_pos = 0;
                return work_write(c);
            } break;
            
            case STATE_WRITE: {
                if (io_error) {
                    return complete_command(c, AMBRO_PSTR("Error:Write\n"));
                }
                o->buffer_pos = 0;
                return work_write(c);
            } break;
            
            case STATE_TRUNCATE: {
                if (io_error) {
                    return complete_command(c, AMBRO_PSTR("Error:Truncate\n"));
                }
                o->fs_file.deinit(c);
                o->have_file = false;
                o->fs_flush.init(c, APRINTER_CB_STATFUNC_T(&FsTestModule::fs_flush_handler));
                o->have_flush = true;
                o->fs_flush.requestFlush(c);
                o->state = STATE_FLUSH;
                debug_msg(c, AMBRO_PSTR("//FsTest:Flush\n"));
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
    static void work_write (Context c)
    {
        auto *o = Object::self(c);
        
        while (o->write_size > 0) {
            size_t amount = MinValue(o->write_data_size - o->write_pos, TheFs::TheBlockSize - o->buffer_pos);
            if (amount == 0) {
                break;
            }
            if (amount > o->write_size) {
                amount = o->write_size;
            }
            memcpy(o->user_buffer.getUserBuffer(c) + o->buffer_pos, o->write_data + o->write_pos, amount);
            o->write_pos += amount;
            if (o->write_pos == o->write_data_size) {
                o->write_pos = 0;
            }
            o->buffer_pos += amount;
            o->write_size -= amount;
        }
        
        if (o->buffer_pos > 0) {
            o->fs_file.startWrite(c, WrapBuffer::Make(o->user_buffer.getUserBuffer(c)), o->buffer_pos);
            o->state = STATE_WRITE;
            debug_msg(c, AMBRO_PSTR("//FsTest:Write\n"));
            return;
        }
        
        o->fs_file.startTruncate(c);
        o->state = STATE_TRUNCATE;
        debug_msg(c, AMBRO_PSTR("//FsTest:Truncate\n"));
    }
    
    static void fs_flush_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_FLUSH)
        
        return complete_command(c, error ? AMBRO_PSTR("Error:Flush\n") : nullptr);
    }
    
public:
    struct Object : public ObjBase<FsTestModule, ParentObject, MakeTypeList<
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
        char const *open_file_name;
        char const *write_data;
        size_t write_data_size;
        uint32_t write_size;
        size_t write_pos;
        size_t buffer_pos;
    };
};

struct FsTestModuleService {
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = FsTestModule<Context, ParentObject, ThePrinterMain>;
};

#include <aprinter/EndNamespace.h>

#endif
