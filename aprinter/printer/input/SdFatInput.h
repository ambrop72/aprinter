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
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/fs/FatFs.h>
#include <aprinter/fs/BlockAccess.h>

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
        auto *o = Object::self(c);
        
        TheBlockAccess::init(c);
        set_default_states(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        cleanup(c);
        TheBlockAccess::deinit(c);
    }
    
    static bool startingIo (Context c, typename ThePrinterMain::CommandType *cmd)
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
    
    static bool rewind (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->file_state == FILE_STATE_INACTIVE || o->file_state == FILE_STATE_PAUSED)
        
        if (!check_file_paused(c, cmd)) {
            return false;
        }
        fs_o->file_reader.rewind(c);
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
        
        fs_o->file_reader.startRead(c, buf);
        o->file_state = FILE_STATE_READING;
    }
    
    static bool checkCommand (Context c, typename ThePrinterMain::CommandType *cmd)
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
        if (TheFs::FsWritable && (cmd_num == 931 || cmd_num == 932)) {
            handle_write_mount_command(c, cmd, (cmd_num == 931));
            return false;
        }
        return true;
    }
    
    using GetSdCard = typename TheBlockAccess::GetSd;
    
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
            fs_o->file_reader.deinit(c);
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
    
    static bool check_file_paused (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        
        if (o->init_state != INIT_STATE_DONE) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdNotInited\n"));
            return false;
        }
        if (o->file_state != FILE_STATE_PAUSED) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("Error:FileNotOpened\n"));
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
            return complete_mount_command(c, error_code);
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
            
            uint16_t signature = ReadBinaryInt<uint16_t, BinaryLittleEndian>(mbr_o->block_buffer + 510);
            if (signature != UINT16_C(0xAA55)) {
                error_code = 41;
                goto error;
            }
            
            auto capacity = TheBlockAccess::getCapacityBlocks(c);
            bool part_found = false;
            typename TheBlockAccess::BlockRange part_range;
            
            for (int partNum = 0; partNum < 4; partNum++) {
                char const *part_entry_buf = mbr_o->block_buffer + (446 + partNum * 16);
                uint8_t part_type =           ReadBinaryInt<uint8_t,  BinaryLittleEndian>(part_entry_buf + 0x4);
                uint32_t part_start_blocks =  ReadBinaryInt<uint32_t, BinaryLittleEndian>(part_entry_buf + 0x8);
                uint32_t part_length_blocks = ReadBinaryInt<uint32_t, BinaryLittleEndian>(part_entry_buf + 0xC);
                
                if (!(part_start_blocks <= capacity && part_length_blocks <= capacity - part_start_blocks && part_length_blocks > 0)) {
                    continue;
                }
                
                if (TheFs::isPartitionTypeSupported(part_type)) {
                    part_found = true;
                    part_range = typename TheBlockAccess::BlockRange{part_start_blocks, part_start_blocks + part_length_blocks};
                    break;
                }
            }
            
            if (!part_found) {
                error_code = 42;
                goto error;
            }
            
            mbr_o->block_user.deinit(c);
            
            TheFs::init(c, part_range);
            o->init_state = INIT_STATE_INIT_FS;
            return;
        } while (0);
        
    error:
        cleanup(c);
        return complete_mount_command(c, error_code);
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
        return complete_mount_command(c, error_code);
    }
    struct FsInitHandler : public AMBRO_WFUNC_TD(&SdFatInput::fs_init_handler) {};
    
    static void handle_mount_command (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        if (o->init_state != INIT_STATE_INACTIVE) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdAlreadyInited\n"));
            cmd->finishCommand(c);
            return;
        }
        TheBlockAccess::activate(c);
        o->init_state = INIT_STATE_ACTIVATE_SD;
    }
    
    static void complete_mount_command (Context c, uint8_t error_code)
    {
        typename ThePrinterMain::CommandType *cmd = ThePrinterMain::get_locked(c);
        if (error_code) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("SD error "));
            cmd->reply_append_uint8(c, error_code);
        } else {
            cmd->reply_append_pstr(c, AMBRO_PSTR("SD mounted"));
        }
        cmd->reply_append_ch(c, '\n');
        cmd->finishCommand(c);
    }
    
    static void handle_unmount_command (Context c, typename ThePrinterMain::CommandType *cmd)
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        do {
            if (o->init_state != INIT_STATE_DONE) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdNotInited\n"));
                break;
            }
            if (o->file_state >= FILE_STATE_RUNNING) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdPrintRunning\n"));
                break;
            }
            AMBRO_ASSERT(o->listing_state == LISTING_STATE_INACTIVE)
            AMBRO_ASSERT(o->write_mount_state == WRITEMOUNT_STATE_MOUNTED || o->write_mount_state == WRITEMOUNT_STATE_NOT_MOUNTED)
            cleanup(c);
            ClientParams::ClearBufferHandler::call(c);
            cmd->reply_append_pstr(c, AMBRO_PSTR("SD unmounted\n"));
        } while (false);
        cmd->finishCommand(c);
    }
    
    static void handle_navigation_command (Context c, typename ThePrinterMain::CommandType *cmd, bool is_dirlist)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        
        do {
            if (o->init_state != INIT_STATE_DONE || o->listing_state != LISTING_STATE_INACTIVE) {
                AMBRO_PGM_P errstr = (o->init_state != INIT_STATE_DONE) ? AMBRO_PSTR("Error:SdNotInited\n") : AMBRO_PSTR("Error:SdNavBusy\n");
                cmd->reply_append_pstr(c, errstr);
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
                
                uint8_t listing_state;
                typename TheFs::EntryType entry_type;
                char const *find_name;
                if ((find_name = cmd->get_command_param_str(c, 'D', nullptr))) {
                    listing_state = LISTING_STATE_CHDIR;
                    entry_type = TheFs::EntryType::DIR_TYPE;
                }
                else if ((find_name = cmd->get_command_param_str(c, 'F', nullptr))) {
                    listing_state = LISTING_STATE_OPEN;
                    entry_type = TheFs::EntryType::FILE_TYPE;
                }
                else {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("Error:BadParams\n"));
                    break;
                }
                
                o->listing_state = listing_state;
                o->listing_u.open_or_chdir.opener.init(c, fs_o->current_directory, entry_type, find_name, Params::CaseInsensFileName, APRINTER_CB_STATFUNC_T(&SdFatInput::opener_handler));
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
        
        if (!is_error && name) {
            AMBRO_ASSERT(!o->listing_u.dirlist.cur_name)
            
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
            errstr = AMBRO_PSTR("error:InputOutput\n");
        } else if (o->listing_u.dirlist.length_error) {
            errstr = AMBRO_PSTR("error:NameTooLong\n");
        }
        
        complete_navigation(c, errstr);
    }
    
    static void opener_handler (Context c, typename TheFs::Opener::OpenerStatus status, typename TheFs::FsEntry entry)
    {
        auto *o = Object::self(c);
        auto *fs_o = UnionFsPart::Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->listing_state == LISTING_STATE_OPEN || o->listing_state == LISTING_STATE_CHDIR)
        
        AMBRO_PGM_P errstr = nullptr;
        do {
            if (status != TheFs::Opener::OpenerStatus::SUCCESS) {
                errstr = (status == TheFs::Opener::OpenerStatus::NOT_FOUND) ? AMBRO_PSTR("error:NotFound\n") : AMBRO_PSTR("error:InputOutput\n");
                break;
            }
            
            if (o->listing_state == LISTING_STATE_CHDIR) {
                fs_o->current_directory = entry;
            } else {
                if (o->file_state >= FILE_STATE_RUNNING) {
                    errstr = AMBRO_PSTR("error:SdPrintRunning\n");
                    break;
                }
                
                if (o->file_state != FILE_STATE_INACTIVE) {
                    fs_o->file_reader.deinit(c);
                }
                fs_o->file_reader.init(c, entry, APRINTER_CB_STATFUNC_T(&SdFatInput::file_reader_handler));
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
    
    static void file_reader_handler (Context c, bool is_error, size_t length)
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
    
    static void complete_navigation (Context c, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->listing_state != LISTING_STATE_INACTIVE)
        
        auto *cmd = ThePrinterMain::get_locked(c);
        if (errstr) {
            cmd->reply_append_pstr(c, errstr);
        }
        cmd->finishCommand(c);
        
        if (o->listing_state == LISTING_STATE_DIRLIST) {
            o->listing_u.dirlist.dir_lister.deinit(c);
        } else {
            o->listing_u.open_or_chdir.opener.deinit(c);
        }
        o->listing_state = LISTING_STATE_INACTIVE;
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(TheFs::FsWritable, static, void, handle_write_mount_command (Context c, typename ThePrinterMain::CommandType *cmd, bool is_mount))
    {
        auto *o = Object::self(c);
        
        if (!cmd->tryLockedCommand(c)) {
            return;
        }
        do {
            if (o->init_state != INIT_STATE_DONE) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:SdNotInited\n"));
                break;
            }
            WriteMountState expected = is_mount ? WRITEMOUNT_STATE_NOT_MOUNTED : WRITEMOUNT_STATE_MOUNTED;
            if (o->write_mount_state != expected) {
                AMBRO_PGM_P errstr = is_mount ? AMBRO_PSTR("Error:SdWriteAlreadyMounted\n") : AMBRO_PSTR("Error:SdWriteNotMounted\n");
                cmd->reply_append_pstr(c, errstr);
                break;
            }
            o->write_mount_state = is_mount ? WRITEMOUNT_STATE_MOUNTING : WRITEMOUNT_STATE_UNMOUNTING;
            if (is_mount) {
                TheFs::startWriteMount(c);
            } else {
                TheFs::startWriteUnmount(c);
            }
            return;
        } while (false);
        cmd->finishCommand(c);
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(TheFs::FsWritable, static, void, fs_write_mount_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == INIT_STATE_DONE)
        AMBRO_ASSERT(o->write_mount_state == WRITEMOUNT_STATE_MOUNTING ||
                     o->write_mount_state == WRITEMOUNT_STATE_UNMOUNTING)
        
        bool is_mount = (o->write_mount_state == WRITEMOUNT_STATE_MOUNTING);
        auto *cmd = ThePrinterMain::get_locked(c);
        if (error) {
            o->write_mount_state = is_mount ? WRITEMOUNT_STATE_NOT_MOUNTED : WRITEMOUNT_STATE_MOUNTED;
            AMBRO_PGM_P errstr = is_mount ? AMBRO_PSTR("Error:SdWriteMountError\n") : AMBRO_PSTR("Error:SdWriteUnmountError\n");
            cmd->reply_append_pstr(c, errstr);
        } else {
            o->write_mount_state = is_mount ? WRITEMOUNT_STATE_MOUNTED : WRITEMOUNT_STATE_NOT_MOUNTED;
        }
        cmd->finishCommand(c);
    }
    struct FsWriteMountHandler : public AMBRO_WFUNC_TD(&SdFatInput::fs_write_mount_handler<>) {};
    
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
            typename TheFs::template File<false> file_reader;
        };
    };
    
public:
    struct Object : public ObjBase<SdFatInput, ParentObject, MakeTypeList<
        TheDebugObject,
        TheBlockAccess,
        InitUnion
    >> {
        uint8_t init_state : 3;
        uint8_t listing_state : 2;
        uint8_t file_state : 2;
        uint8_t file_eof : 1;
        uint8_t write_mount_state : 2;
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
