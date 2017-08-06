/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_DIR_LISTER_H
#define APRINTER_DIR_LISTER_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/OneOf.h>

namespace APrinter {

template <typename Context, typename TheFsAccess>
class DirLister {
private:
    using TheFs = typename TheFsAccess::TheFileSystem;
    using FsEntry = typename TheFs::FsEntry;
    using Opener  = typename TheFs::Opener;
    
    enum class State : uint8_t {IDLE, OPEN_ACCESS, OPEN_BASEDIR, OPEN_DIR, READY, REQUEST_ENTRY};
    
public:
    enum class Error {NO_ERROR, OTHER_ERROR, NOT_FOUND};
    
    using CompletionHandler = Callback<void(Context c, Error error, char const *name, FsEntry entry)>;
    
    void init (Context c, CompletionHandler completion_handler)
    {
        m_completion_handler = completion_handler;
        m_access_client.init(c, APRINTER_CB_OBJFUNC_T(&DirLister::access_client_handler, this));
        m_state = State::IDLE;
    }
    
    void deinit (Context c)
    {
        reset_internal(c);
        m_access_client.deinit(c);
    }
    
    void reset (Context c)
    {
        reset_internal(c);
    }
    
    void startOpen (Context c, char const *filename, bool in_current_dir, char const *basedir=nullptr)
    {
        AMBRO_ASSERT(m_state == State::IDLE)
        AMBRO_ASSERT(filename)
        
        m_state = State::OPEN_ACCESS;
        m_filename = filename;
        m_basedir = basedir;
        m_in_current_dir = in_current_dir;
        m_access_client.requestAccess(c, false);
    }
    
    void requestEntry (Context c)
    {
        AMBRO_ASSERT(m_state == State::READY)
        
        m_state = State::REQUEST_ENTRY;
        m_fs_lister.requestEntry(c);
    }
    
private:
    void reset_internal (Context c)
    {
        if (m_state == OneOf(State::OPEN_BASEDIR, State::OPEN_DIR)) {
            m_fs_opener.deinit(c);
        }
        else if (m_state == OneOf(State::READY, State::REQUEST_ENTRY)) {
            m_fs_lister.deinit(c);
        }
        m_access_client.reset(c);
        m_state = State::IDLE;
    }
    
    void reset_and_complete (Context c, Error error)
    {
        reset_internal(c);
        
        return m_completion_handler(c, error, nullptr, FsEntry{});
    }
    
    void access_client_handler (Context c, bool error)
    {
        AMBRO_ASSERT(m_state == State::OPEN_ACCESS)
        
        if (error) {
            return reset_and_complete(c, Error::OTHER_ERROR);
        }
        
        auto dir_entry = m_in_current_dir ? m_access_client.getCurrentDirectory(c) : TheFs::getRootEntry(c);
        if (m_basedir) {
            m_state = State::OPEN_BASEDIR;
            m_fs_opener.init(c, dir_entry, TheFs::EntryType::DIR_TYPE, m_basedir, APRINTER_CB_OBJFUNC_T(&DirLister::fs_opener_handler, this));
        } else {
            basedir_entry_found(c, dir_entry);
        }
    }
    
    void fs_opener_handler (Context c, typename Opener::OpenerStatus status, FsEntry entry)
    {
        AMBRO_ASSERT(m_state == OneOf(State::OPEN_BASEDIR, State::OPEN_DIR))
        
        if (status != Opener::OpenerStatus::SUCCESS) {
            Error user_error = (status == Opener::OpenerStatus::NOT_FOUND) ? Error::NOT_FOUND : Error::OTHER_ERROR;
            return reset_and_complete(c, user_error);
        }
        
        m_fs_opener.deinit(c);
        
        if (m_state == State::OPEN_BASEDIR) {
            return basedir_entry_found(c, entry);
        }
        
        m_state = State::READY;
        m_fs_lister.init(c, entry, APRINTER_CB_OBJFUNC_T(&DirLister::fs_lister_handler, this));
        
        return m_completion_handler(c, Error::NO_ERROR, nullptr, FsEntry{});
    }
    
    void basedir_entry_found (Context c, FsEntry basedir_entry)
    {
        m_state = State::OPEN_DIR;
        m_fs_opener.init(c, basedir_entry, TheFs::EntryType::DIR_TYPE, m_filename, APRINTER_CB_OBJFUNC_T(&DirLister::fs_opener_handler, this));
    }
    
    void fs_lister_handler (Context c, bool is_error, char const *name, FsEntry entry)
    {
        AMBRO_ASSERT(m_state == State::REQUEST_ENTRY)
        
        if (is_error) {
            return reset_and_complete(c, Error::OTHER_ERROR);
        }
        
        m_state = State::READY;
        
        return m_completion_handler(c, Error::NO_ERROR, name, entry);
    }
    
private:
    CompletionHandler m_completion_handler;
    typename TheFsAccess::Client m_access_client;
    State m_state;
    bool m_in_current_dir;
    char const *m_filename;
    char const *m_basedir;
    union {
        Opener m_fs_opener;
        typename TheFs::DirLister m_fs_lister;
    };
};

}

#endif
