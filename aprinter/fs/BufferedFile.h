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

#include <aprinter/BeginNamespace.h>

template <typename Context, typename TheFsAccess>
class BufferedFile {
private:
    using TheFs = typename TheFsAccess::TheFileSystem;
    using TheOpener = typename TheFs::Opener;
    using TheFile = typename TheFs::template File<true>;
    
    enum class State {
        IDLE,
        OPEN_ACCESS, OPEN_BASEDIR, OPEN_OPEN, OPEN_OPENWR,
        READY,
        WRITE_EVENT, WRITE_WRITE, WRITE_TRUNCATE, WRITE_FLUSH,
        READ_EVENT, READ_READ
    };
    
public:
    enum class OpenMode {OPEN_READ, OPEN_WRITE};
    enum class Error {NO_ERROR, OTHER_ERROR, NOT_FOUND};
    
    using CompletionHandler = Callback<void(Context c, Error error, size_t read_length)>;
    
    void init (Context c, CompletionHandler completion_handler)
    {
        m_completion_handler = completion_handler;
        m_event.init(c, APRINTER_CB_OBJFUNC_T(&BufferedFile::event_handler, this));
        m_access_client.init(c, APRINTER_CB_OBJFUNC_T(&BufferedFile::access_client_handler, this));
        m_state = State::IDLE;
        m_have_opener = false;
        m_have_file = false;
        m_have_flush = false;
    }
    
    void deinit (Context c)
    {
        reset_internal(c);
        m_access_client.deinit(c);
        m_event.deinit(c);
    }
    
    void reset (Context c)
    {
        reset_internal(c);
    }
    
    void startOpen (Context c, char const *filename, bool in_current_dir, OpenMode mode, char const *basedir=nullptr)
    {
        AMBRO_ASSERT(m_state == State::IDLE)
        AMBRO_ASSERT(filename)
        AMBRO_ASSERT(mode == OpenMode::OPEN_READ || mode == OpenMode::OPEN_WRITE)
        
        m_state = State::OPEN_ACCESS;
        m_filename = filename;
        m_basedir = basedir;
        m_in_current_dir = in_current_dir;
        m_write_mode = (mode == OpenMode::OPEN_WRITE);
        m_access_client.requestAccess(c, m_write_mode);
    }
    
    void startWriteData (Context c, char const *data, size_t length)
    {
        AMBRO_ASSERT(m_state == State::READY)
        AMBRO_ASSERT(m_write_mode)
        AMBRO_ASSERT(!m_write_eof)
        
        m_write_data = data;
        m_write_length = length;
        m_state = State::WRITE_EVENT;
        m_event.prependNowNotAlready(c);
    }
    
    void startWriteEof (Context c)
    {
        AMBRO_ASSERT(m_state == State::READY)
        AMBRO_ASSERT(m_write_mode)
        AMBRO_ASSERT(!m_write_eof)
        
        m_write_eof = true;
        m_write_length = 0;
        m_state = State::WRITE_EVENT;
        m_event.prependNowNotAlready(c);
    }
    
    void startReadData (Context c, char *data, size_t avail)
    {
        AMBRO_ASSERT(m_state == State::READY)
        AMBRO_ASSERT(!m_write_mode)
        
        m_read_data = data;
        m_read_avail = avail;
        m_read_pos = 0;
        m_state = State::READ_EVENT;
        m_event.prependNowNotAlready(c);
    }
    
    bool isReady (Context c)
    {
        return (m_state == State::READY);
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
        m_access_client.reset(c);
        m_event.unset(c);
        m_state = State::IDLE;
    }
    
    void reset_and_complete (Context c, Error error)
    {
        reset_internal(c);
        return m_completion_handler(c, error, 0);
    }
    
    void access_client_handler (Context c, bool error)
    {
        AMBRO_ASSERT(m_state == State::OPEN_ACCESS)
        AMBRO_ASSERT(!m_have_opener)
        
        if (error) {
            return reset_and_complete(c, Error::OTHER_ERROR);
        }
        
        auto dir_entry = m_in_current_dir ? m_access_client.getCurrentDirectory(c) : TheFs::getRootEntry(c);
        if (m_basedir) {
            m_state = State::OPEN_BASEDIR;
            m_fs_opener.init(c, dir_entry, TheFs::EntryType::DIR_TYPE, m_basedir, APRINTER_CB_OBJFUNC_T(&BufferedFile::fs_opener_handler, this));
        } else {
            m_state = State::OPEN_OPEN;
            m_fs_opener.init(c, dir_entry, TheFs::EntryType::FILE_TYPE, m_filename, APRINTER_CB_OBJFUNC_T(&BufferedFile::fs_opener_handler, this));
        }
        m_have_opener = true;
    }
    
    void fs_opener_handler (Context c, typename TheOpener::OpenerStatus status, typename TheFs::FsEntry entry)
    {
        AMBRO_ASSERT(m_state == State::OPEN_BASEDIR || m_state == State::OPEN_OPEN)
        AMBRO_ASSERT(m_have_opener)
        AMBRO_ASSERT(!m_have_file)
        
        if (status != TheOpener::OpenerStatus::SUCCESS) {
            Error user_error = (status == TheOpener::OpenerStatus::NOT_FOUND) ? Error::NOT_FOUND : Error::OTHER_ERROR;
            return reset_and_complete(c, user_error);
        }
        
        m_fs_opener.deinit(c);
        
        if (m_state == State::OPEN_BASEDIR) {
            m_state = State::OPEN_OPEN;
            m_fs_opener.init(c, entry, TheFs::EntryType::FILE_TYPE, m_filename, APRINTER_CB_OBJFUNC_T(&BufferedFile::fs_opener_handler, this));
            return;
        }
        
        m_have_opener = false;
        
        m_fs_file.init(c, entry, APRINTER_CB_OBJFUNC_T(&BufferedFile::fs_file_handler, this), TheFile::IoMode::FS_BUFFER);
        m_have_file = true;
        
        if (m_write_mode) {
            m_state = State::OPEN_OPENWR;
            m_fs_file.startOpenWritable(c);
        } else {
            m_state = State::READY;
            m_read_buffer_pos = TheFs::BlockSize;
            m_read_buffer_length = TheFs::BlockSize;
            return m_completion_handler(c, Error::NO_ERROR, 0);
        }
    }
    
    void fs_file_handler (Context c, bool io_error, size_t read_length)
    {
        AMBRO_ASSERT(m_state == State::OPEN_OPENWR || m_state == State::WRITE_WRITE || m_state == State::READ_READ || m_state == State::WRITE_TRUNCATE)
        AMBRO_ASSERT(m_have_file)
        
        if (io_error) {
            return reset_and_complete(c, Error::OTHER_ERROR);
        }
        
        if (m_state == State::OPEN_OPENWR) {
            m_state = State::READY;
            m_write_eof = false;
            m_write_buffer_pos = TheFs::BlockSize;
            return m_completion_handler(c, Error::NO_ERROR, 0);
        }
        else if (m_state == State::WRITE_WRITE) {
            m_state = State::WRITE_EVENT;
            m_write_buffer_pos = 0;
            m_event.prependNowNotAlready(c);
        }
        else if (m_state == State::READ_READ) {
            AMBRO_ASSERT(read_length <= TheFs::BlockSize)
            
            m_state = State::READ_EVENT;
            m_read_buffer_pos = 0;
            m_read_buffer_length = read_length;
            m_event.prependNowNotAlready(c);
        }
        else { // m_state == State::WRITE_TRUNCATE
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
        if (m_state == State::WRITE_EVENT) {
            handle_event_write(c);
        } else {
            AMBRO_ASSERT(m_state == State::READ_EVENT)
            handle_event_read(c);
        }
    }
    
    void handle_event_write (Context c)
    {
        if (m_write_eof) {
            if (m_write_buffer_pos < TheFs::BlockSize) {
                m_fs_file.finishWrite(c, m_write_buffer_pos);
            }
            m_fs_file.startTruncate(c);
            m_state = State::WRITE_TRUNCATE;
            return;
        }
        
        size_t to_copy = MinValue(m_write_length, (size_t)(TheFs::BlockSize - m_write_buffer_pos));
        if (to_copy > 0) {
            memcpy(m_fs_file.getWritePointer(c) + m_write_buffer_pos, m_write_data, to_copy);
            m_write_data += to_copy;
            m_write_length -= to_copy;
            m_write_buffer_pos += to_copy;
            
            if (m_write_buffer_pos == TheFs::BlockSize) {
                m_fs_file.finishWrite(c, m_write_buffer_pos);
            }
        }
        
        if (m_write_length > 0) {
            AMBRO_ASSERT(m_write_buffer_pos == TheFs::BlockSize)
            m_state = State::WRITE_WRITE;
            m_fs_file.startWrite(c, true);
            return;
        }
        
        m_state = State::READY;
        return m_completion_handler(c, Error::NO_ERROR, 0);
    }
    
    void handle_event_read (Context c)
    {
        size_t to_copy = MinValue(m_read_avail, (size_t)(m_read_buffer_length - m_read_buffer_pos));
        if (to_copy > 0) {
            memcpy(m_read_data, m_fs_file.getReadPointer(c) + m_read_buffer_pos, to_copy);
            m_read_data += to_copy;
            m_read_avail -= to_copy;
            m_read_pos += to_copy;
            m_read_buffer_pos += to_copy;
            
            if (m_read_buffer_pos == m_read_buffer_length) {
                m_fs_file.finishRead(c);
            }
        }
        
        if (m_read_avail > 0 && m_read_buffer_pos == TheFs::BlockSize) {
            m_state = State::READ_READ;
            m_fs_file.startRead(c);
            return;
        }
        
        m_state = State::READY;
        return m_completion_handler(c, Error::NO_ERROR, m_read_pos);
    }
    
public:
    CompletionHandler m_completion_handler;
    typename Context::EventLoop::QueuedEvent m_event;
    typename TheFsAccess::Client m_access_client;
    union {
        TheOpener m_fs_opener;
        TheFile m_fs_file;
        typename TheFs::template FlushRequest<> m_fs_flush;
    };
    State m_state;
    bool m_have_opener : 1;
    bool m_have_file : 1;
    bool m_have_flush : 1;
    bool m_write_mode : 1;
    bool m_in_current_dir : 1;
    bool m_write_eof : 1;
    union {
        struct {
            char const *m_filename;
            char const *m_basedir;
        };
        union {
            struct {
                char const *m_write_data;
                size_t m_write_length;
                size_t m_write_buffer_pos;
            };
            struct {
                char *m_read_data;
                size_t m_read_avail;
                size_t m_read_pos;
                size_t m_read_buffer_pos;
                size_t m_read_buffer_length;
            };
        };
    };
};

#include <aprinter/EndNamespace.h>

#endif
