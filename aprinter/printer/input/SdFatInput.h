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

#ifndef AMBROLIB_SDFAT_INPUT_H
#define AMBROLIB_SDFAT_INPUT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/devices/FatFs.h>
#include <aprinter/devices/BlockAccess.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class SdFatInput {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using ThePrinterMain = typename ClientParams::ThePrinterMain;
    struct BlockAccessActivateHandler;
    using TheBlockAccess = typename BlockAccessService<typename Params::SdCardService>::template Access<Context, Object, BlockAccessActivateHandler>;
    struct FsInitHandler;
    using TheFs = typename Params::FsService::template Fs<Context, Object, TheBlockAccess, FsInitHandler>;
    using FsEntry = typename TheFs::FsEntry;
    using FsDirLister = typename TheFs::DirLister;
    enum {
        STATE_INACTIVE,
        STATE_ACTIVATE_SD,
        STATE_INIT_FS,
        STATE_NOFILE,
        STATE_LISTING
    };
    
public:
    static size_t const NeedBufAvail = BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheBlockAccess::init(c);
        o->state = STATE_INACTIVE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        if (o->state == STATE_LISTING) {
            o->dir_lister.deinit(c);
        }
        if (o->state >= STATE_INIT_FS) {
            TheFs::deinit(c);
        }
        TheBlockAccess::deinit(c);
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_INACTIVE)
        
        TheBlockAccess::activate(c);
        o->state = STATE_ACTIVATE_SD;
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state != STATE_INACTIVE)
        
        if (o->state == STATE_LISTING) {
            o->dir_lister.deinit(c);
        }
        if (o->state >= STATE_INIT_FS) {
            TheFs::deinit(c);
        }
        TheBlockAccess::deactivate(c);
        o->state = STATE_INACTIVE;
    }
    
    static bool eofReached (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        //
        return true;
    }
    
    static bool canRead (Context c, size_t buf_avail)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        //
        return false;
    }
    
    static void startRead (Context c, size_t buf_avail, size_t buf_wrap, uint8_t *buf1, uint8_t *buf2)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        //
    }
    
    static bool checkCommand (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        /*
        if (cmd->getCmdNumber(c) == 23) {
            if (o->state != STATE_NOFILE) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:Incorrect state"));
                cmd->finishCommand(c);
                return false;
            }
            o->file_path = cmd->get_command_param_str(c, 'F', "");
            return false;
        }
        */
        
        if (cmd->getCmdNumber(c) == 20) {
            if (!cmd->tryLockedCommand(c)) {
                return false;
            }
            if (o->state != STATE_NOFILE) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:Incorrect state"));
                cmd->finishCommand(c);
                return false;
            }
            o->dir_lister.init(c, o->current_directory, APRINTER_CB_STATFUNC_T(&SdFatInput::dir_lister_handler));
            o->dir_lister.requestEntry(c);
            o->state = STATE_LISTING;
            return false;
        }
        
        return true;
    }
    
    using GetSdCard = TheSdCard;
    
private:
    static void block_access_activate_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_ACTIVATE_SD)
        
        if (error_code) {
            o->state = STATE_INACTIVE;
            return ClientParams::ActivateHandler::call(c, error_code);
        }
        TheFs::init(c);
        o->state = STATE_INIT_FS;
    }
    struct BlockAccessActivateHandler : public AMBRO_WFUNC_TD(&SdFatInput::block_access_activate_handler) {};
    
    static void fs_init_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_INIT_FS)
        
        if (error_code) {
            TheFs::deinit(c);
            TheBlockAccess::deactivate(c);
            o->state = STATE_INACTIVE;
        } else {
            o->state = STATE_NOFILE;
            o->current_directory = TheFs::getRootEntry(c);
        }
        return ClientParams::ActivateHandler::call(c, error_code);
    }
    struct FsInitHandler : public AMBRO_WFUNC_TD(&SdFatInput::fs_init_handler) {};
    
    static void dir_lister_handler (Context c, bool is_error, char const *name, FsEntry entry)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_LISTING)
        
        auto *cmd = ThePrinterMain::get_locked(c);
        
        if (!is_error && name) {
            // TBD
            o->dir_lister.requestEntry(c);
            return;
        }
        
        o->dir_lister.deinit(c);
        o->state = STATE_NOFILE;
        
        if (is_error) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("error:FsDirList\n"));
        } else {
            cmd->reply_append_pstr(c, AMBRO_PSTR("end\n"));
        }
        cmd->finishCommand(c);
    }
    
public:
    struct Object : public ObjBase<SdFatInput, ParentObject, MakeTypeList<
        TheDebugObject,
        TheBlockAccess,
        TheFs
    >> {
        uint8_t state;
        FsEntry current_directory;
        FsDirLister dir_lister;
    };
};

template <typename TSdCardService, typename TFsService>
struct SdFatInputService {
    using SdCardService = TSdCardService;
    using FsService = TFsService;
    
    template <typename Context, typename ParentObject, typename ClientParams>
    using Input = SdFatInput<Context, ParentObject, ClientParams, SdFatInputService>;
};

#include <aprinter/EndNamespace.h>

#endif
