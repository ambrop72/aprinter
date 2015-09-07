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
#include <aprinter/meta/FunctionIf.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/fs/FatFs.h>
#include <aprinter/fs/BlockAccess.h>
#include <aprinter/fs/PartitionTable.h>
#include <aprinter/fs/BlockRange.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class SdFatInput {
public:
    struct Object;
    
private:
    struct UnionMbrPart;
    struct UnionFsPart;
    using ThePrinterMain = typename ClientParams::ThePrinterMain;
    using TheDebugObject = DebugObject<Context, Object>;
    struct BlockAccessActivateHandler;
    using TheBlockAccess = typename BlockAccessService<typename Params::SdCardService>::template Access<Context, Object, BlockAccessActivateHandler>;
    struct FsInitHandler;
    struct FsWriteMountHandler;
    using TheFs = typename Params::FsService::template Fs<Context, typename UnionFsPart::Object, TheBlockAccess, FsInitHandler, FsWriteMountHandler>;
    
    static size_t const DirListReplyRequestExtra = 24;
    static_assert(TheBlockAccess::BlockSize == 512, "BlockSize must be 512");
    
    // NOTE: Check bit field widths at the bottom before adding new state values.
    enum InitState {
        INIT_STATE_INACTIVE,
        INIT_STATE_ACTIVATE_SD,
        INIT_STATE_READ_MBR,
        INIT_STATE_INIT_FS,
        INIT_STATE_DONE
    };
    enum ListingState {
        LISTING_STATE_INACTIVE,
        LISTING_STATE_DIRLIST,
        LISTING_STATE_CHDIR,
        LISTING_STATE_OPEN
    };
    enum FileState {
        FILE_STATE_INACTIVE,
        FILE_STATE_PAUSED,
        FILE_STATE_RUNNING,
        FILE_STATE_READING
    };
    enum WriteMountState {
        WRITEMOUNT_STATE_NOT_MOUNTED,
        WRITEMOUNT_STATE_MOUNTING,
        WRITEMOUNT_STATE_MOUNTED,
        WRITEMOUNT_STATE_UNMOUNTING
    };
    
    // Note regarding calling the ClearBufferHandler. A non-obvious precondition for clearing the buffer
    // is that it does not contain a command currently possessing the printer lock. We guarantee this by
    // always executing ClearBufferHandler as part of commands that take the printer lock.
    
public:
    static size_t const NeedBufAvail = TheBlockAccess::BlockSize;
    
    static void init (Context c)
    {
        TheBlockAccess::init(c);
        set_default_states(c);
        AccessInterface::init(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        AccessInterface::deinit(c);
        cleanup(c);
        TheBlockAccess::deinit(c);
    }
    
    static bool startingIo (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->file_state == FILE_STATE_INACTIVE || o->file_state == FILE_STATE_PAUSED)
        
        if (!check_file_paused(c, cmd)) {
            return false;
        }
        o->file_state = FILE_STATE_RUNNING;
        return true;
    }
    
    static void pausingIo (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->file_state == FILE_STATE_RUNNING) // not FILE_STATE_READING!
        
        o->file_state = FILE_STATE_PAUSED;
    }
    
    static bool rewind (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->file_state == FILE_STATE_INACTIVE || o->file_state == FILE_STATE_PAUSED)
        
        if (!check_file_paused(c, cmd)) {
            return false;
        }
        fs_o->file.rewind(c);
        o->file_eof = false;
        ClientParams::ClearBufferHandler::call(c);
        return true;
    }
    
    static bool eofReached (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->file_state == FILE_STATE_RUNNING || o->file_state == FILE_STATE_READING)
        
        return o->file_eof;
    }
    
    static bool canRead (Context c, size_t buf_avail)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->file_state == FILE_STATE_RUNNING)
        
        return (!o->file_eof && buf_avail >= TheBlockAccess::BlockSize);
    }
    
    static void startRead (Context c, size_t buf_avail, WrapBuffer buf)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->file_state == FILE_STATE_RUNNING)
        AMBRO_ASSERT(!o->file_eof)
        AMBRO_ASSERT(buf_avail >= TheBlockAccess::BlockSize)
        AMBRO_ASSERT(buf.wrap > 0)
        
        fs_o->file.startRead(c, buf);
        o->file_state = FILE_STATE_READING;
    }
    
    static bool checkCommand (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        TheDebugObject::access(c);
        
        auto cmd_num = cmd->getCmdNumber(c);
        if (cmd_num == 21) {
            handle_mount_command(c, cmd);
            return false;
        }
        if (cmd_num == 22) {
            handle_unmount_command(c, cmd);
            return false;
        }
        if (cmd_num == 20 || cmd_num == 23) {
            handle_navigation_command(c, cmd, (cmd_num == 20));
            return false;
        }
        return true;
    }
    
    using GetSdCard = typename TheBlockAccess::GetSd;
    
    template <typename This=SdFatInput>
    using GetFsAccess = typename This::AccessInterface;
    
private:
    static void set_default_states (Context c)
    {
        auto *o = Object::self(c);
        o->init_state = INIT_STATE_INACTIVE;
        o->listing_state = LISTING_STATE_INACTIVE;
        o->file_state = FILE_STATE_INACTIVE;
        o->write_mount_state = WRITEMOUNT_STATE_NOT_MOUNTED;
    }
    
    static void cleanup (Context c)
    {
        auto *o = Object::self(c);
        auto *mbr_o = UnionMbrPart::Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        
        if (o->listing_state == LISTING_STATE_DIRLIST) {
            o->listing_u.dirlist.dir_lister.deinit(c);
        }
        if (o->listing_state == LISTING_STATE_OPEN || o->listing_state == LISTING_STATE_CHDIR) {
            o->listing_u.open_or_chdir.opener.deinit(c);
        }
        if (o->file_state != FILE_STATE_INACTIVE) {
            fs_o->file.deinit(c);
        }
        if (o->init_state >= INIT_STATE_INIT_FS) {
            TheFs::deinit(c);
        }
        if (o->init_state == INIT_STATE_READ_MBR) {
            mbr_o->block_user.deinit(c);
        }
        if (o->init_state >= INIT_STATE_ACTIVATE_SD) {
            TheBlockAccess::deactivate(c);
        }
        
        set_default_states(c);
    }
    
    static bool check_file_paused (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (o->init_state != INIT_STATE_DONE) {
            cmd->reportError(c, AMBRO_PSTR("SdNotInited"));
            return false;
        }
        if (o->file_state != FILE_STATE_PAUSED) {
            cmd->reportError(c, AMBRO_PSTR("FileNotOpened"));
            return false;
        }
        return true;
    }
    
    static void block_access_activate_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        auto *mbr_o = UnionMbrPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_ACTIVATE_SD)
        
        if (error_code) {
            o->init_state = INIT_STATE_INACTIVE;
            return mount_completed(c, error_code);
        }
        
        o->init_state = INIT_STATE_READ_MBR;
        mbr_o->block_user.init(c, APRINTER_CB_STATFUNC_T(&SdFatInput::block_user_handler));
        mbr_o->block_user.startRead(c, 0, WrapBuffer::Make(mbr_o->block_buffer));
    }
    struct BlockAccessActivateHandler : public AMBRO_WFUNC_TD(&SdFatInput::block_access_activate_handler) {};
    
    static void block_user_handler (Context c, bool read_error)
    {
        auto *o = Object::self(c);
        auto *mbr_o = UnionMbrPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_READ_MBR)
        
        uint8_t error_code = 99;
        do {
            if (read_error) {
                error_code = 40;
                goto error;
            }
            
            BlockRange<typename TheBlockAccess::BlockIndexType> part_range;
            if (!FindMbrPartition<TheFs>(mbr_o->block_buffer, TheBlockAccess::getCapacityBlocks(c), &part_range)) {
                error_code = 42;
                goto error;
            }
            
            mbr_o->block_user.deinit(c);
            
            TheFs::init(c, part_range);
            o->init_state = INIT_STATE_INIT_FS;
            return;
        } while (false);
        
    error:
        cleanup(c);
        return mount_completed(c, error_code);
    }
    
    static void fs_init_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_INIT_FS)
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_INACTIVE)
        AMBRO_ASSERT(o->file_state == FILE_STATE_INACTIVE)
        AMBRO_ASSERT(o->write_mount_state == WRITEMOUNT_STATE_NOT_MOUNTED)
        
        if (error_code) {
            cleanup(c);
        } else {
            o->init_state = INIT_STATE_DONE;
            fs_o->current_directory = TheFs::getRootEntry(c);
        }
        return mount_completed(c, error_code);
    }
    struct FsInitHandler : public AMBRO_WFUNC_TD(&SdFatInput::fs_init_handler) {};
    
    static void handle_mount_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        bool mount_writable = TheFs::FsWritable && cmd->find_command_param(c, 'W', nullptr);
        if (!start_mount(c, true, mount_writable)) {
            AMBRO_PGM_P errstr = (TheFs::FsWritable && mount_writable && o->write_mount_state == WRITEMOUNT_STATE_UNMOUNTING) ?
                AMBRO_PSTR("SdWriteUnmountInProgress") : AMBRO_PSTR("SdAlreadyMounted");
            cmd->reportError(c, errstr);
            cmd->finishCommand(c);
        }
    }
    
    static bool start_mount (Context c, bool for_command, bool mount_writable)
    {
        auto *o = Object::self(c);
        
        mount_writable = TheFs::FsWritable && mount_writable;
        
        if (o->init_state == INIT_STATE_INACTIVE) {
            o->for_command = for_command;
            o->mount_writable = mount_writable;
            TheBlockAccess::activate(c);
            o->init_state = INIT_STATE_ACTIVATE_SD;
            return true;
        }
        if (mount_writable && o->init_state == INIT_STATE_DONE && o->write_mount_state == WRITEMOUNT_STATE_NOT_MOUNTED) {
            o->for_command = for_command;
            o->mount_writable = mount_writable; // just so that |= below is not undefined behavior
            start_write_mount(c, true);
            return true;
        }
        if (o->init_state != INIT_STATE_DONE || (mount_writable && o->write_mount_state == WRITEMOUNT_STATE_MOUNTING)) {
            o->for_command |= for_command;
            o->mount_writable |= mount_writable;
            return true;
        }
        return false;
    }
    
    static void mount_completed (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        
        auto *output = ThePrinterMain::get_msg_output(c);
        if (error_code) {
            if (o->for_command) {
                auto *cmd = ThePrinterMain::get_locked(c);
                cmd->reportError(c, nullptr);
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdMount:"));
                cmd->reply_append_uint8(c, error_code);
                cmd->reply_append_ch(c, '\n');
            }
            if (!o->for_command || output != ThePrinterMain::get_locked(c)) {
                output->reply_append_pstr(c, AMBRO_PSTR("//Error:SdMount:"));
                output->reply_append_uint8(c, error_code);
                output->reply_append_ch(c, '\n');
            }
        } else {
            output->reply_append_pstr(c, AMBRO_PSTR("//SD mounted\n"));
        }
        output->reply_poke(c);
        
        if (TheFs::FsWritable && !error_code && o->mount_writable) {
            AccessInterface::complete_mount_requests(c, false);
            start_write_mount(c, true);
        } else {
            report_mount_result(c, true);
        }
    }
    
    static void write_mount_completed (Context c, bool error, bool is_mount)
    {
        auto *o = Object::self(c);
        
        auto *output = ThePrinterMain::get_msg_output(c);
        if (error) {
            AMBRO_PGM_P errstr = is_mount ? AMBRO_PSTR("Error:SdWriteMount\n") : AMBRO_PSTR("Error:SdWriteUnmount\n");
            if (o->for_command) {
                auto *cmd = ThePrinterMain::get_locked(c);
                cmd->reportError(c, nullptr);
                cmd->reply_append_pstr(c, errstr);
            }
            if (!o->for_command || output != ThePrinterMain::get_locked(c)) {
                output->reply_append_pstr(c, AMBRO_PSTR("//"));
                output->reply_append_pstr(c, errstr);
            }
        } else {
            AMBRO_PGM_P msgstr = is_mount ? AMBRO_PSTR("//SD write-mounted\n") : AMBRO_PSTR("//SD write-unmounted\n");
            output->reply_append_pstr(c, msgstr);
        }
        output->reply_poke(c);
        
        if (is_mount || (error && !o->unmount_force) || o->unmount_readonly) {
            report_mount_result(c, is_mount);
        } else {
            AMBRO_ASSERT(o->for_command)
            
            if (!can_unmount(c)) {
                auto *cmd = ThePrinterMain::get_locked(c);
                cmd->reportError(c, AMBRO_PSTR("SdInUse"));
                cmd->finishCommand(c);
                return;
            }
            
            complete_unmount(c);
        }
    }
    
    static void report_mount_result (Context c, bool is_mount)
    {
        auto *o = Object::self(c);
        
        if (is_mount) {
            AccessInterface::complete_mount_requests(c, true);
        }
        if (o->for_command) {
            auto *cmd = ThePrinterMain::get_locked(c);
            cmd->finishCommand(c);
        }
    }
    
    static void handle_unmount_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        
        do {
            if (o->init_state != INIT_STATE_DONE) {
                cmd->reportError(c, AMBRO_PSTR("SdNotMounted"));
                break;
            }
            
            bool unmount_readonly = TheFs::FsWritable && cmd->find_command_param(c, 'R', nullptr);
            if (TheFs::FsWritable && o->write_mount_state != WRITEMOUNT_STATE_NOT_MOUNTED) {
                if (AccessInterface::has_references(c, true)) {
                    cmd->reportError(c, AMBRO_PSTR("SdWriteInUse"));
                    break;
                }
                if (o->write_mount_state == WRITEMOUNT_STATE_MOUNTING) {
                    cmd->reportError(c, AMBRO_PSTR("SdWriteMountInProgress"));
                    break;
                }
                o->for_command = true;
                o->unmount_readonly = unmount_readonly;
                o->unmount_force = cmd->find_command_param(c, 'F', nullptr);
                if (o->write_mount_state != WRITEMOUNT_STATE_UNMOUNTING) {
                    start_write_mount(c, false);
                }
                return;
            }
            
            if (unmount_readonly) {
                cmd->reportError(c, AMBRO_PSTR("SdNotWriteMounted"));
                break;
            }
            
            if (!can_unmount(c)) {
                cmd->reportError(c, AMBRO_PSTR("SdInUse"));
                break;
            }
            
            complete_unmount(c);
            return;
        } while (false);
        
        cmd->finishCommand(c);
    }
    
    static void write_reference_removed (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->write_mount_state == WRITEMOUNT_STATE_MOUNTED)
        
        if (!AccessInterface::has_references(c, true)) {
            o->for_command = false;
            o->unmount_readonly = true;
            o->unmount_force = false;
            start_write_mount(c, false);
        }
    }
    
    static bool can_unmount (Context c)
    {
        auto *o = Object::self(c);
        return (!(o->file_state >= FILE_STATE_RUNNING) && !AccessInterface::has_references(c, false));
    }
    
    static void complete_unmount (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_INACTIVE)
        AMBRO_ASSERT(o->write_mount_state == WRITEMOUNT_STATE_MOUNTED || o->write_mount_state == WRITEMOUNT_STATE_NOT_MOUNTED)
        AMBRO_ASSERT(!(o->file_state >= FILE_STATE_RUNNING))
        AMBRO_ASSERT(!AccessInterface::has_references(c, false))
        
        cleanup(c);
        ClientParams::ClearBufferHandler::call(c);
        
        auto *output = ThePrinterMain::get_msg_output(c);
        output->reply_append_pstr(c, AMBRO_PSTR("//SD unmounted\n"));
        output->reply_poke(c);
        
        auto *cmd = ThePrinterMain::get_locked(c);
        cmd->finishCommand(c);
    }
    
    static void handle_navigation_command (Context c, typename ThePrinterMain::TheCommand *cmd, bool is_dirlist)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        
        do {
            AMBRO_ASSERT(o->listing_state == LISTING_STATE_INACTIVE)
            
            if (o->init_state != INIT_STATE_DONE) {
                cmd->reportError(c, AMBRO_PSTR("SdNotInited"));
                break;
            }
            
            if (is_dirlist) {
                o->listing_state = LISTING_STATE_DIRLIST;
                o->listing_u.dirlist.cur_name = nullptr;
                o->listing_u.dirlist.length_error = false;
                o->listing_u.dirlist.dir_lister.init(c, fs_o->current_directory, APRINTER_CB_STATFUNC_T(&SdFatInput::dir_lister_handler));
                o->listing_u.dirlist.dir_lister.requestEntry(c);
            } else {
                if (cmd->find_command_param(c, 'R', nullptr)) {
                    fs_o->current_directory = TheFs::getRootEntry(c);
                    break;
                }
                
                char const *find_name;
                uint8_t listing_state;
                typename TheFs::EntryType entry_type;
                
                if ((find_name = cmd->get_command_param_str(c, 'D', nullptr))) {
                    listing_state = LISTING_STATE_CHDIR;
                    entry_type = TheFs::EntryType::DIR_TYPE;
                }
                else if ((find_name = cmd->get_command_param_str(c, 'F', nullptr))) {
                    listing_state = LISTING_STATE_OPEN;
                    entry_type = TheFs::EntryType::FILE_TYPE;
                }
                else {
                    cmd->reportError(c, AMBRO_PSTR("BadParams"));
                    break;
                }
                
                o->listing_state = listing_state;
                o->listing_u.open_or_chdir.opener.init(c, fs_o->current_directory, entry_type, find_name, APRINTER_CB_STATFUNC_T(&SdFatInput::opener_handler));
            }
            
            return;
        } while (false);
        
        cmd->finishCommand(c);
    }
    
    static void dir_lister_handler (Context c, bool is_error, char const *name, typename TheFs::FsEntry entry)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_DIRLIST)
        AMBRO_ASSERT(!o->listing_u.dirlist.cur_name)
        
        if (!is_error && name) {
            // DirListReplyRequestExtra is to make sure we have space for a possible error reply at the end
            size_t req_len = (2 + strlen(name) + 1) + DirListReplyRequestExtra;
            auto *cmd = ThePrinterMain::get_locked(c);
            if (!cmd->requestSendBufEvent(c, req_len, SdFatInput::send_buf_event_handler)) {
                o->listing_u.dirlist.length_error = true;
                o->listing_u.dirlist.dir_lister.requestEntry(c);
                return;
            }
            
            o->listing_u.dirlist.cur_name = name;
            o->listing_u.dirlist.cur_is_dir = (entry.getType() == TheFs::EntryType::DIR_TYPE);
            return;
        }
        
        AMBRO_PGM_P errstr = nullptr;
        if (is_error) {
            errstr = AMBRO_PSTR("InputOutput");
        } else if (o->listing_u.dirlist.length_error) {
            errstr = AMBRO_PSTR("NameTooLong");
        }
        
        complete_navigation(c, errstr);
    }
    
    static void opener_handler (Context c, typename TheFs::Opener::OpenerStatus status, typename TheFs::FsEntry entry)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_CHDIR || o->listing_state == LISTING_STATE_OPEN)
        
        AMBRO_PGM_P errstr = nullptr;
        do {
            if (status != TheFs::Opener::OpenerStatus::SUCCESS) {
                errstr = (status == TheFs::Opener::OpenerStatus::NOT_FOUND) ? AMBRO_PSTR("NotFound") : AMBRO_PSTR("InputOutput");
                break;
            }
            
            if (o->listing_state == LISTING_STATE_CHDIR) {
                fs_o->current_directory = entry;
            } else {
                if (o->file_state >= FILE_STATE_RUNNING) {
                    errstr = AMBRO_PSTR("SdPrintRunning");
                    break;
                }
                
                if (o->file_state != FILE_STATE_INACTIVE) {
                    fs_o->file.deinit(c);
                }
                
                fs_o->file.init(c, entry, APRINTER_CB_STATFUNC_T(&SdFatInput::file_handler));
                o->file_state = FILE_STATE_PAUSED;
                o->file_eof = false;
                ClientParams::ClearBufferHandler::call(c);
            }
        } while (false);
        
        complete_navigation(c, errstr);
    }
    
    static void send_buf_event_handler (Context c)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_DIRLIST)
        AMBRO_ASSERT(o->listing_u.dirlist.cur_name)
        
        auto *cmd = ThePrinterMain::get_locked(c);
        cmd->reply_append_pstr(c, o->listing_u.dirlist.cur_is_dir ? AMBRO_PSTR("d ") : AMBRO_PSTR("f "));
        cmd->reply_append_str(c, o->listing_u.dirlist.cur_name);
        cmd->reply_append_ch(c, '\n');
        cmd->reply_poke(c);
        
        o->listing_u.dirlist.dir_lister.requestEntry(c);
        o->listing_u.dirlist.cur_name = nullptr;
    }
    
    static void complete_navigation (Context c, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->listing_state != LISTING_STATE_INACTIVE)
        
        auto *cmd = ThePrinterMain::get_locked(c);
        if (errstr) {
            cmd->reportError(c, errstr);
        }
        cmd->finishCommand(c);
        
        if (o->listing_state == LISTING_STATE_DIRLIST) {
            o->listing_u.dirlist.dir_lister.deinit(c);
        } else {
            o->listing_u.open_or_chdir.opener.deinit(c);
        }
        o->listing_state = LISTING_STATE_INACTIVE;
    }
    
    static void file_handler (Context c, bool is_error, size_t length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->file_state == FILE_STATE_READING)
        AMBRO_ASSERT(!o->file_eof)
        
        if (!is_error && length == 0) {
            o->file_eof = true;
        }
        o->file_state = FILE_STATE_RUNNING;
        return ClientParams::ReadHandler::call(c, is_error, length);
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(TheFs::FsWritable, static, void, start_write_mount (Context c, bool is_mount))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->write_mount_state == (is_mount ? WRITEMOUNT_STATE_NOT_MOUNTED : WRITEMOUNT_STATE_MOUNTED))
        
        o->write_mount_state = is_mount ? WRITEMOUNT_STATE_MOUNTING : WRITEMOUNT_STATE_UNMOUNTING;
        if (is_mount) {
            TheFs::startWriteMount(c);
        } else {
            TheFs::startWriteUnmount(c);
        }
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(TheFs::FsWritable, static, void, fs_write_mount_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->write_mount_state == WRITEMOUNT_STATE_MOUNTING ||
                     o->write_mount_state == WRITEMOUNT_STATE_UNMOUNTING)
        
        bool is_mount = (o->write_mount_state == WRITEMOUNT_STATE_MOUNTING);
        o->write_mount_state = (is_mount == error) ? WRITEMOUNT_STATE_NOT_MOUNTED : WRITEMOUNT_STATE_MOUNTED;
        return write_mount_completed(c, error, is_mount);
    }
    struct FsWriteMountHandler : public AMBRO_WFUNC_TD(&SdFatInput::fs_write_mount_handler<>) {};
    
    AMBRO_STRUCT_IF(AccessInterface, Params::HaveAccessInterface) {
    private:
        friend SdFatInput;
        static_assert(TheFs::FsWritable, "");
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->num_ro_refs = 0;
            o->num_rw_refs = 0;
            o->clients_list.init();
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->num_ro_refs == 0)
            AMBRO_ASSERT(o->num_rw_refs == 0)
            AMBRO_ASSERT(o->clients_list.isEmpty())
        }
        
        static void complete_mount_requests (Context c, bool complete_writable_requests)
        {
            auto *o = Object::self(c);
            
            Client *client = o->clients_list.first();
            while (client) {
                Client *next_client = o->clients_list.next(client);
                AMBRO_ASSERT(client->m_state == Client::STATE_REQUESTING)
                if (!client->m_writable || complete_writable_requests) {
                    o->clients_list.remove(client);
                    client->complete_request(c);
                }
                client = next_client;
            }
        }
        
        static bool has_references (Context c, bool count_writable_refs_only)
        {
            auto *o = Object::self(c);
            return (!count_writable_refs_only && o->num_ro_refs != 0) || o->num_rw_refs != 0;
        }
        
    public:
        using TheFileSystem = TheFs;
        
        class Client : private SimpleDebugObject<Context> {
            friend AccessInterface;
            enum {STATE_IDLE, STATE_REQUESTING, STATE_REPORTING, STATE_COMPLETED};
            
        public:
            using ClientHandler = Callback<void(Context c, bool error)>;
            
            void init (Context c, ClientHandler handler)
            {
                m_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::event_handler, this));
                m_handler = handler;
                m_state = STATE_IDLE;
                m_have_reference = false;
                this->debugInit(c);
            }
            
            void deinit (Context c)
            {
                this->debugDeinit(c);
                reset_internal(c);
                m_event.deinit(c);
            }
            
            void reset (Context c)
            {
                this->debugAccess(c);
                reset_internal(c);
            }
            
            void requestAccess (Context c, bool writable)
            {
                this->debugAccess(c);
                AMBRO_ASSERT(m_state == STATE_IDLE)
                TheDebugObject::access(c);
                auto *o = Object::self(c);
                
                m_writable = writable;
                if (start_mount(c, false, writable)) {
                    m_state = STATE_REQUESTING;
                    o->clients_list.prepend(this);
                } else {
                    complete_request(c);
                }
            }
            
            typename TheFs::FsEntry getCurrentDirectory (Context c)
            {
                this->debugAccess(c);
                AMBRO_ASSERT(m_state == STATE_COMPLETED)
                
                auto *fs_o = UnionFsPart::Object::self(c);
                return fs_o->current_directory;
            }
            
        private:
            void reset_internal (Context c)
            {
                if (m_have_reference) {
                    auto *o = Object::self(c);
                    if (m_writable) {
                        AMBRO_ASSERT(o->num_rw_refs > 0)
                        o->num_rw_refs--;
                        write_reference_removed(c);
                    } else {
                        AMBRO_ASSERT(o->num_ro_refs > 0)
                        o->num_ro_refs--;
                    }
                }
                if (m_state == STATE_REQUESTING) {
                    auto *o = Object::self(c);
                    o->clients_list.remove(this);
                }
                m_event.unset(c);
                m_state = STATE_IDLE;
                m_have_reference = false;
            }
            
            void complete_request (Context c)
            {
                auto *o = Object::self(c);
                auto *mo = SdFatInput::Object::self(c);
                AMBRO_ASSERT(!m_have_reference)
                
                if (mo->init_state == INIT_STATE_DONE && (!m_writable || mo->write_mount_state == WRITEMOUNT_STATE_MOUNTED)) {
                    if (m_writable) {
                        o->num_rw_refs++;
                    } else {
                        o->num_ro_refs++;
                    }
                    m_have_reference = true;
                }
                
                m_state = Client::STATE_REPORTING;
                m_event.prependNowNotAlready(c);
            }
            
            void event_handler (Context c)
            {
                this->debugAccess(c);
                AMBRO_ASSERT(m_state == STATE_REPORTING)
                
                m_state = m_have_reference ? STATE_COMPLETED : STATE_IDLE;
                return m_handler(c, !m_have_reference);
            }
            
            typename Context::EventLoop::QueuedEvent m_event;
            ClientHandler m_handler;
            DoubleEndedListNode<Client> m_list_node;
            uint8_t m_state : 3;
            bool m_have_reference : 1;
            bool m_writable : 1;
        };
        
        class UserBuffer {
        public:
            using UserBufferHandler = Callback<void(Context c, bool error)>;
            
            void init (Context c, UserBufferHandler handler)
            {
                m_cache_ref.init(c, handler);
            }
            
            void deinit (Context c)
            {
                m_cache_ref.deinit(c);
            }
            
            void reset (Context c)
            {
                m_cache_ref.reset(c);
            }
            
            void requestUserBuffer (Context c)
            {
                m_cache_ref.requestUserBuffer(c);
            }
            
            char * getUserBuffer (Context c)
            {
                return m_cache_ref.getUserBuffer(c);
            }
            
        private:
            typename TheFs::CacheRefForUser m_cache_ref;
        };
        
    public:
        struct Object : public ObjBase<AccessInterface, typename SdFatInput::Object, EmptyTypeList> {
            size_t num_ro_refs;
            size_t num_rw_refs;
            DoubleEndedList<Client, &Client::m_list_node, false> clients_list;
        };
    }
    AMBRO_STRUCT_ELSE(AccessInterface) {
    private:
        friend SdFatInput;
        static void init (Context c) {}
        static void deinit (Context c) {}
        static void complete_mount_requests (Context c, bool complete_writable_requests) {}
        static bool has_references (Context c, bool count_writable_refs_only) { return false; }
    public:
        struct Object {};
    };
    
    struct InitUnion {
        struct Object : public ObjUnionBase<InitUnion, typename SdFatInput::Object, MakeTypeList<
            UnionMbrPart,
            UnionFsPart
        >> {};
    };
    
    struct UnionMbrPart {
        struct Object : public ObjBase<UnionMbrPart, typename InitUnion::Object, EmptyTypeList> {
            typename TheBlockAccess::User block_user;
            char block_buffer[TheBlockAccess::BlockSize];
        };
    };
    
    struct UnionFsPart {
        struct Object : public ObjBase<UnionFsPart, typename InitUnion::Object, MakeTypeList<
            TheFs
        >> {
            typename TheFs::FsEntry current_directory;
            typename TheFs::template File<false> file;
        };
    };
    
public:
    struct Object : public ObjBase<SdFatInput, ParentObject, MakeTypeList<
        TheDebugObject,
        TheBlockAccess,
        InitUnion,
        AccessInterface
    >> {
        uint8_t init_state : 3;
        uint8_t listing_state : 2;
        uint8_t file_state : 2;
        uint8_t file_eof : 1;
        uint8_t write_mount_state : 2;
        uint8_t for_command : 1;
        uint8_t mount_writable : 1;
        uint8_t unmount_readonly : 1;
        uint8_t unmount_force : 1;
        union {
            struct {
                typename TheFs::DirLister dir_lister;
                char const *cur_name;
                bool cur_is_dir : 1;
                bool length_error : 1;
            } dirlist;
            struct {
                typename TheFs::Opener opener;
            } open_or_chdir;
        } listing_u;
    };
};

template <typename TSdCardService, typename TFsService, bool THaveAccessInterface>
struct SdFatInputService {
    using SdCardService = TSdCardService;
    using FsService = TFsService;
    static bool const HaveAccessInterface = THaveAccessInterface;
    
    static bool const ProvidesFsAccess = HaveAccessInterface;
    
    template <typename Context, typename ParentObject, typename ClientParams>
    using Input = SdFatInput<Context, ParentObject, ClientParams, SdFatInputService>;
};

#include <aprinter/EndNamespace.h>

#endif
