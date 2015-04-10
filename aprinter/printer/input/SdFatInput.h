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
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/misc/AsciiTools.h>
#include <aprinter/devices/FatFs.h>
#include <aprinter/devices/BlockAccess.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class SdFatInput {
public:
    struct Object;
    
private:
    using ThePrinterMain = typename ClientParams::ThePrinterMain;
    using TheDebugObject = DebugObject<Context, Object>;
    struct BlockAccessActivateHandler;
    using TheBlockAccess = typename BlockAccessService<typename Params::SdCardService>::template Access<Context, Object, BlockAccessActivateHandler>;
    struct FsInitHandler;
    using TheFs = typename Params::FsService::template Fs<Context, Object, TheBlockAccess, FsInitHandler>;
    enum InitState {
        INIT_STATE_INACTIVE,
        INIT_STATE_ACTIVATE_SD,
        INIT_STATE_INIT_FS,
        INIT_STATE_DONE
    };
    enum ListingState {
        LISTING_STATE_INACTIVE,
        LISTING_STATE_DIRLIST,
        LISTING_STATE_CHDIR,
        LISTING_STATE_OPEN
    };
    
    static size_t const ReplyRequestExtra = 24;
    
public:
    static size_t const NeedBufAvail = TheBlockAccess::BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheBlockAccess::init(c);
        o->init_state = INIT_STATE_INACTIVE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        cleanup(c);
        TheBlockAccess::deinit(c);
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_INACTIVE)
        
        TheBlockAccess::activate(c);
        o->init_state = INIT_STATE_ACTIVATE_SD;
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state != INIT_STATE_INACTIVE)
        // due to locking in all of M20,M22,M23 we cannot be listing at this point
        AMBRO_ASSERT(o->init_state != INIT_STATE_DONE || o->listing_state == LISTING_STATE_INACTIVE)
        
        cleanup(c);
        o->init_state = INIT_STATE_INACTIVE;
    }
    
    static bool eofReached (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        //
        return false;
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
        
        AMBRO_ASSERT(0)
        //
    }
    
    static bool checkCommand (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        auto cmd_num = cmd->getCmdNumber(c);
        
        if (cmd_num == 20 || cmd_num == 23) {
            if (!cmd->tryLockedCommand(c)) {
                return false;
            }
            
            do {
                if (o->init_state != INIT_STATE_DONE || o->listing_state != LISTING_STATE_INACTIVE) {
                    AMBRO_PGM_P errstr = (o->init_state != INIT_STATE_DONE) ? AMBRO_PSTR("Error:SdNotInited\n") : AMBRO_PSTR("Error:SdNavBusy\n");
                    cmd->reply_append_pstr(c, errstr);
                    break;
                }
                
                if (cmd_num == 20) {
                    o->listing_state = LISTING_STATE_DIRLIST;
                    o->listing_u.dirlist.cur_name = nullptr;
                    o->listing_u.dirlist.length_error = false;
                } 
                else { // cmd_num == 23
                    if (cmd->find_command_param(c, 'R', nullptr)) {
                        o->current_directory = TheFs::getRootEntry(c);
                        break;
                    }
                    
                    char const *find_name;
                    if ((find_name = cmd->get_command_param_str(c, 'D', nullptr))) {
                        o->listing_state = LISTING_STATE_CHDIR;
                    }
                    else if ((find_name = cmd->get_command_param_str(c, 'F', nullptr))) {
                        o->listing_state = LISTING_STATE_OPEN;
                    }
                    else {
                        cmd->reply_append_pstr(c, AMBRO_PSTR("Error:BadParams\n"));
                        break;
                    }
                    
                    o->listing_u.open_or_chdir.find_name = find_name;
                    o->listing_u.open_or_chdir.entry_found = false;
                }
                
                o->dir_lister.init(c, o->current_directory, &o->fs_buffer, APRINTER_CB_STATFUNC_T(&SdFatInput::dir_lister_handler));
                o->dir_lister.requestEntry(c);
                return false;
            } while (0);
            
            cmd->finishCommand(c);
            return false;
        }
        
        return true;
    }
    
    static bool startingIo (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        
        cmd->reply_append_pstr(c, AMBRO_PSTR("Error:TBD\n"));
        return false;
    }
    
    static void pausingIo (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        
        
    }
    
    using GetSdCard = typename TheBlockAccess::GetSd;
    
private:
    static void cleanup (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->init_state == INIT_STATE_DONE && o->listing_state != LISTING_STATE_INACTIVE) {
            o->dir_lister.deinit(c);
        }
        if (o->init_state >= INIT_STATE_INIT_FS) {
            TheFs::deinit(c);
        }
        if (o->init_state >= INIT_STATE_ACTIVATE_SD) {
            TheBlockAccess::deactivate(c);
        }
    }
    
    static void block_access_activate_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_ACTIVATE_SD)
        
        if (error_code) {
            o->init_state = INIT_STATE_INACTIVE;
            return ClientParams::ActivateHandler::call(c, error_code);
        }
        TheFs::init(c, &o->fs_buffer);
        o->init_state = INIT_STATE_INIT_FS;
    }
    struct BlockAccessActivateHandler : public AMBRO_WFUNC_TD(&SdFatInput::block_access_activate_handler) {};
    
    static void fs_init_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_INIT_FS)
        
        if (error_code) {
            cleanup(c);
            o->init_state = INIT_STATE_INACTIVE;
        } else {
            o->init_state = INIT_STATE_DONE;
            o->listing_state = LISTING_STATE_INACTIVE;
            o->current_directory = TheFs::getRootEntry(c);
        }
        return ClientParams::ActivateHandler::call(c, error_code);
    }
    struct FsInitHandler : public AMBRO_WFUNC_TD(&SdFatInput::fs_init_handler) {};
    
    static void dir_lister_handler (Context c, bool is_error, char const *name, typename TheFs::FsEntry entry)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_DIRLIST || o->listing_state == LISTING_STATE_CHDIR || o->listing_state == LISTING_STATE_OPEN)
        
        auto *cmd = ThePrinterMain::get_locked(c);
        
        if (!is_error && name) {
            bool stop_listing = false;
            do {
                if (o->listing_state == LISTING_STATE_DIRLIST) {
                    AMBRO_ASSERT(!o->listing_u.dirlist.cur_name)
                    
                    // ReplyRequestExtra is to make sure we have space for a possible error reply at the end
                    size_t name_len = strlen(name);
                    size_t req_len = (2 + name_len + 1) + ReplyRequestExtra;
                    if (!cmd->requestSendBufEvent(c, req_len, SdFatInput::send_buf_event_handler)) {
                        o->listing_u.dirlist.length_error = true;
                        break;
                    }
                    
                    o->listing_u.dirlist.cur_name = name;
                    o->listing_u.dirlist.cur_is_dir = (entry.getType() == TheFs::ENTRYTYPE_DIR);
                    return;
                }
                else if (o->listing_state == LISTING_STATE_CHDIR) {
                    if (compare_filename_equal(name, o->listing_u.open_or_chdir.find_name) && entry.getType() == TheFs::ENTRYTYPE_DIR) {
                        o->current_directory = entry;
                        o->listing_u.open_or_chdir.entry_found = true;
                        stop_listing = true;
                    }
                }
                else { // o->listing_state == LISTING_STATE_OPEN
                    if (compare_filename_equal(name, o->listing_u.open_or_chdir.find_name) && entry.getType() == TheFs::ENTRYTYPE_FILE) {
                        // TBD
                        o->listing_u.open_or_chdir.entry_found = true;
                        stop_listing = true;
                    }
                }
            } while (0);
            
            if (!stop_listing) {
                o->dir_lister.requestEntry(c);
                return;
            }
        }
        
        AMBRO_PGM_P errstr = nullptr;
        if (is_error) {
            errstr = AMBRO_PSTR("error:InputOutput\n");
        } else {
            if (o->listing_state == LISTING_STATE_DIRLIST) {
                if (o->listing_u.dirlist.length_error) {
                    errstr = AMBRO_PSTR("error:NameTooLong\n");
                }
            }
            else {
                if (!o->listing_u.open_or_chdir.entry_found) {
                    errstr = AMBRO_PSTR("error:NotFound\n");
                }
            }
        }
        
        if (errstr) {
            cmd->reply_append_pstr(c, errstr);
        }
        cmd->finishCommand(c);
        
        o->dir_lister.deinit(c);
        o->listing_state = LISTING_STATE_INACTIVE;
    }
    
    static void send_buf_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_DIRLIST)
        AMBRO_ASSERT(o->listing_u.dirlist.cur_name)
        
        auto *cmd = ThePrinterMain::get_locked(c);
        cmd->reply_append_pstr(c, o->listing_u.dirlist.cur_is_dir ? AMBRO_PSTR("d ") : AMBRO_PSTR("f "));
        cmd->reply_append_str(c, o->listing_u.dirlist.cur_name);
        cmd->reply_append_ch(c, '\n');
        cmd->reply_poke(c);
        
        o->dir_lister.requestEntry(c);
        o->listing_u.dirlist.cur_name = nullptr;
    }
    
    static bool compare_filename_equal (char const *str1, char const *str2)
    {
        return Params::CaseInsensFileName ? AsciiCaseInsensStringEqual(str1, str2) : !strcmp(str1, str2);
    }
    
public:
    struct Object : public ObjBase<SdFatInput, ParentObject, MakeTypeList<
        TheDebugObject,
        TheBlockAccess,
        TheFs
    >> {
        uint8_t init_state;
        uint8_t listing_state;
        typename TheFs::FsEntry current_directory;
        typename TheFs::DirLister dir_lister;
        union {
            struct {
                char const *cur_name;
                bool cur_is_dir;
                bool length_error;
            } dirlist;
            struct {
                char const *find_name;
                bool entry_found;
            } open_or_chdir;
        } listing_u;
        typename TheFs::SharedBuffer fs_buffer;
    };
};

template <typename TSdCardService, typename TFsService, bool TCaseInsensFileName>
struct SdFatInputService {
    using SdCardService = TSdCardService;
    using FsService = TFsService;
    static bool const CaseInsensFileName = TCaseInsensFileName;
    
    template <typename Context, typename ParentObject, typename ClientParams>
    using Input = SdFatInput<Context, ParentObject, ClientParams, SdFatInputService>;
};

#include <aprinter/EndNamespace.h>

#endif
