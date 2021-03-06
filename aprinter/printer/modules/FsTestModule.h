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

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Callback.h>
#include <aprinter/fs/BufferedFile.h>
#include <aprinter/printer/utils/ModuleUtils.h>

namespace APrinter {

template <typename ModuleArg>
class FsTestModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using TheFsAccess = typename ThePrinterMain::template GetFsAccess<>;
    using TheBufferedFile = BufferedFile<Context, TheFsAccess>;
    
    enum class State : uint8_t {IDLE, WRITE_OPEN, WRITE_DATA, WRITE_EOF, READ_OPEN, READ_DATA};
    
    static size_t const ReadBufferSize = 128;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->buffered_file.init(c, APRINTER_CB_STATFUNC_T(&FsTestModule::file_handler));
        o->state = State::IDLE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->buffered_file.deinit(c);
    }
    
    static bool check_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        TheDebugObject::access(c);
        
        auto cmd_number = cmd->getCmdNumber(c);
        if (cmd_number == 935 || cmd_number == 936) {
            handle_read_write_command(c, cmd, cmd_number == 935);
            return false;
        }
        return true;
    }
    
private:
    static void complete_command (Context c, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        
        o->buffered_file.reset(c);
        o->state = State::IDLE;
        
        auto *cmd = ThePrinterMain::get_locked(c);
        if (errstr) {
            cmd->reportError(c, errstr);
        }
        cmd->finishCommand(c);
    }
    
    static void handle_read_write_command (Context c, typename ThePrinterMain::TheCommand *cmd, bool is_write)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        AMBRO_ASSERT(o->state == State::IDLE)
        
        char const *open_file_name = cmd->get_command_param_str(c, 'F', nullptr);
        if (!open_file_name) {
            return complete_command(c, AMBRO_PSTR("NoFileSpecified"));
        }
        
        if (is_write) {
            o->write_data = cmd->get_command_param_str(c, 'D', "1234567890");
            o->write_data_size = strlen(o->write_data);
            o->write_size = cmd->find_command_param(c, 'S', nullptr) ? cmd->get_command_param_uint32(c, 'S', 0) : o->write_data_size;
            if (o->write_data_size == 0) {
                o->write_size = 0;
            }
        }
        
        auto mode = is_write ? TheBufferedFile::OpenMode::OPEN_WRITE : TheBufferedFile::OpenMode::OPEN_READ;
        o->buffered_file.startOpen(c, open_file_name, true, mode);
        o->state = is_write ? State::WRITE_OPEN : State::READ_OPEN;
    }
    
    static void file_handler (Context c, typename TheBufferedFile::Error error, size_t read_length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        switch (o->state) {
            case State::WRITE_OPEN:
            case State::READ_OPEN: {
                if (error != TheBufferedFile::Error::NO_ERROR) {
                    return complete_command(c, AMBRO_PSTR("Open"));
                }
                if (o->state == State::WRITE_OPEN) {
                    work_write(c);
                } else {
                    work_read(c);
                }
            } break;
            
            case State::WRITE_DATA: {
                if (error != TheBufferedFile::Error::NO_ERROR) {
                    return complete_command(c, AMBRO_PSTR("WriteData"));
                }
                work_write(c);
            } break;
            
            case State::WRITE_EOF: {
                if (error != TheBufferedFile::Error::NO_ERROR) {
                    return complete_command(c, AMBRO_PSTR("WriteEof"));
                }
                return complete_command(c, nullptr);
            } break;
            
            case State::READ_DATA: {
                if (error != TheBufferedFile::Error::NO_ERROR) {
                    return complete_command(c, AMBRO_PSTR("ReadData"));
                }
                if (read_length == 0) {
                    return complete_command(c, nullptr);
                }
                work_read(c);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
    static void work_write (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->write_size == 0) {
            o->buffered_file.startWriteEof(c);
            o->state = State::WRITE_EOF;
            return;
        }
        
        size_t amount = o->write_data_size;
        if (amount > o->write_size) {
            amount = o->write_size;
        }
        o->write_size -= amount;
        o->buffered_file.startWriteData(c, o->write_data, amount);
        o->state = State::WRITE_DATA;
    }
    
    static void work_read (Context c)
    {
        auto *o = Object::self(c);
        
        o->buffered_file.startReadData(c, o->read_buffer, ReadBufferSize);
        o->state = State::READ_DATA;
    }
    
public:
    struct Object : public ObjBase<FsTestModule, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        TheBufferedFile buffered_file;
        State state;
        union {
            struct {
                char const *write_data;
                size_t write_data_size;
                uint32_t write_size;
            };
            struct {
                char read_buffer[ReadBufferSize];
            };
        };
    };
};

struct FsTestModuleService {
    APRINTER_MODULE_TEMPLATE(FsTestModuleService, FsTestModule)
};

}

#endif
