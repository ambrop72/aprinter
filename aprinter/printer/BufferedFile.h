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

#ifndef APRINTER_BUFFERED_FILE_H
#define APRINTER_BUFFERED_FILE_H

#include <stddef.h>
#include <string.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/WrapBuffer.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename TheFsAccess>
class BufferedFile {
private:
    using TheFs = typename TheFsAccess::TheFileSystem;
    
    enum class State {
        IDLE,
        WRITE_ACCESS, WRITE_BUFFER, WRITE_OPEN, WRITE_OPENWR, WRITE_READY,
        WRITE_EVENT, WRITE_WRITE, WRITE_TRUNCATE, WRITE_FLUSH
    };
    
public:
    enum class Error {NO_ERROR, OTHER_ERROR, NOT_FOUND};
    
    using CompletionHandler = Callback<void(Context c, Error error)>;
    
    void init (Context c, CompletionHandler completion_handler)
    {
        m_completion_handler = completion_handler;
        m_event.init(c, APRINTER_CB_OBJFUNC_T(&BufferedFile::event_handler, this));
        m_access_client.init(c, APRINTER_CB_OBJFUNC_T(&BufferedFile::access_client_handler, this));
        m_user_buffer.init(c, APRINTER_CB_OBJFUNC_T(&BufferedFile::user_buffer_handler, this));
        m_state = State::IDLE;
        m_have_opener = false;
        m_have_file = false;
        m_have_flush = false;
    }
    
    void deinit (Context c)
    {
        reset_internal(c);
        m_user_buffer.deinit(c);
        m_access_client.deinit(c);
        m_event.deinit(c);
    }
    
    void reset (Context c)
    {
        reset_internal(c);
    }
    
    void startOpen (Context c, char const *filename, bool in_current_dir)
    {
        AMBRO_ASSERT(m_state == State::IDLE)
        
        m_state = State::WRITE_ACCESS;
        m_filename = filename;
        m_in_current_dir = in_current_dir;
        m_access_client.requestAccess(c, true);
    }
    
    void startWriteData (Context c, char const *data, size_t length)
    {
        AMBRO_ASSERT(m_state == State::WRITE_READY)
        AMBRO_ASSERT(!m_write_eof)
        
        m_write_data = data;
        m_write_length = length;
        m_state = State::WRITE_EVENT;
        m_event.prependNowNotAlready(c);
    }
    
    void startWriteEof (Context c)
    {
        AMBRO_ASSERT(m_state == State::WRITE_READY)
        AMBRO_ASSERT(!m_write_eof)
        
        m_write_eof = true;
        m_write_length = 0;
        m_state = State::WRITE_EVENT;
        m_event.prependNowNotAlready(c);
    }
    
    bool isWriteReady (Context c)
    {
        return (m_state == State::WRITE_READY);
    }
    
private:
    void reset_internal (Context c)
    {
        if (m_have_flush) {
            m_fs_flush.deinit(c);
            m_have_flush = false;
        }
        if (m_have_file) {
            m_fs_file.deinit(c);
            m_have_file = false;
        }
        if (m_have_opener) {
            m_fs_opener.deinit(c);
            m_have_opener = false;
        }
        m_user_buffer.reset(c);
        m_access_client.reset(c);
        m_event.unset(c);
        m_state = State::IDLE;
    }
    
    void reset_and_complete (Context c, Error error)
    {
        reset_internal(c);
        return m_completion_handler(c, error);
    }
    
    void access_client_handler (Context c, bool error)
    {
        AMBRO_ASSERT(m_state == State::WRITE_ACCESS)
        
        if (error) {
            return reset_and_complete(c, Error::OTHER_ERROR);
        }
        
        m_state = State::WRITE_BUFFER;
        m_user_buffer.requestUserBuffer(c);
    }
    
    void user_buffer_handler (Context c, bool error)
    {
        AMBRO_ASSERT(m_state == State::WRITE_BUFFER)
        AMBRO_ASSERT(!m_have_opener)
        
        if (error) {
            return reset_and_complete(c, Error::OTHER_ERROR);
        }
        
        m_state = State::WRITE_OPEN;
        auto dir_entry = m_in_current_dir ? m_access_client.getCurrentDirectory(c) : TheFs::getRootEntry(c);
        m_fs_opener.init(c, dir_entry, TheFs::EntryType::FILE_TYPE, m_filename, APRINTER_CB_OBJFUNC_T(&BufferedFile::fs_opener_handler, this));
        m_have_opener = true;
    }
    
    void fs_opener_handler (Context c, typename TheFs::Opener::OpenerStatus status, typename TheFs::FsEntry entry)
    {
        AMBRO_ASSERT(m_state == State::WRITE_OPEN)
        AMBRO_ASSERT(m_have_opener)
        AMBRO_ASSERT(!m_have_file)
        
        if (status != TheFs::Opener::OpenerStatus::SUCCESS) {
            Error user_error = (status == TheFs::Opener::OpenerStatus::NOT_FOUND) ? Error::NOT_FOUND : Error::OTHER_ERROR;
            return reset_and_complete(c, user_error);
        }
        
        m_fs_opener.deinit(c);
        m_have_opener = false;
        
        m_fs_file.init(c, entry, APRINTER_CB_OBJFUNC_T(&BufferedFile::fs_file_handler, this));
        m_have_file = true;
        
        m_state = State::WRITE_OPENWR;
        m_fs_file.startOpenWritable(c);
    }
    
    void fs_file_handler (Context c, bool io_error, size_t read_length)
    {
        AMBRO_ASSERT(m_state == State::WRITE_OPENWR || m_state == State::WRITE_WRITE || m_state == State::WRITE_TRUNCATE)
        
        if (io_error) {
            return reset_and_complete(c, Error::OTHER_ERROR);
        }
        
        if (m_state == State::WRITE_OPENWR) {
            m_state = State::WRITE_READY;
            m_write_eof = false;
            m_buffer_pos = 0;
            return m_completion_handler(c, Error::NO_ERROR);
        }
        else if (m_state == State::WRITE_WRITE) {
            m_state = State::WRITE_EVENT;
            m_buffer_pos = 0;
            m_event.prependNowNotAlready(c);
        }
        else { // m_state == State::WRITE_TRUNCATE
            AMBRO_ASSERT(m_have_file)
            AMBRO_ASSERT(!m_have_flush)
            
            m_fs_file.deinit(c);
            m_have_file = false;
            
            m_fs_flush.init(c, APRINTER_CB_OBJFUNC_T(&BufferedFile::fs_flush_handler, this));
            m_have_flush = true;
            
            m_state = State::WRITE_FLUSH;
            m_fs_flush.requestFlush(c);
        }
    }
    
    void fs_flush_handler (Context c, bool error)
    {
        AMBRO_ASSERT(m_state == State::WRITE_FLUSH)
        
        Error user_error = error ? Error::OTHER_ERROR : Error::NO_ERROR;
        return reset_and_complete(c, user_error);
    }
    
    void event_handler (Context c)
    {
        AMBRO_ASSERT(m_state == State::WRITE_EVENT)
        
        size_t to_copy = MinValue(m_write_length, (size_t)(TheFs::BlockSize - m_buffer_pos));
        if (to_copy > 0) {
            memcpy(m_user_buffer.getUserBuffer(c) + m_buffer_pos, m_write_data, to_copy);
            m_write_data += to_copy;
            m_write_length -= to_copy;
            m_buffer_pos += to_copy;
        }
        
        if (m_buffer_pos == TheFs::BlockSize || (m_write_eof && m_buffer_pos > 0)) {
            m_state = State::WRITE_WRITE;
            m_fs_file.startWrite(c, WrapBuffer::Make(m_user_buffer.getUserBuffer(c)), m_buffer_pos);
            return;
        }
        
        if (m_write_eof) {
            m_fs_file.startTruncate(c);
            m_state = State::WRITE_TRUNCATE;
            return;
        }
        
        m_state = State::WRITE_READY;
        return m_completion_handler(c, Error::NO_ERROR);
    }
    
public:
    CompletionHandler m_completion_handler;
    typename Context::EventLoop::QueuedEvent m_event;
    typename TheFsAccess::Client m_access_client;
    typename TheFsAccess::UserBuffer m_user_buffer;
    union {
        typename TheFs::Opener m_fs_opener;
        typename TheFs::template File<true> m_fs_file;
        typename TheFs::template FlushRequest<> m_fs_flush;
    };
    State m_state;
    bool m_have_opener : 1;
    bool m_have_file : 1;
    bool m_have_flush : 1;
    bool m_in_current_dir : 1;
    bool m_write_eof : 1;
    union {
        struct {
            char const *m_filename;
        };
        struct {
            char const *m_write_data;
            size_t m_write_length;
            size_t m_buffer_pos;
        };
    };
};

#include <aprinter/EndNamespace.h>

#endif
