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

#ifndef APRINTER_LINUX_SDCARD_H
#define APRINTER_LINUX_SDCARD_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <atomic>
#include <limits>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/TransferVector.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/platform/linux/linux_support.h>

#include <aprinter/BeginNamespace.h>

// NOTE: The existing SD-card API does not support asynchonous execution
// of deactivate (it assumed that any cleanup can be done immediately),
// but we have to wait for any operations in the I/O thread to complete.
// Since the Linux port is for testing only, waiting synchronously in
// deactivate and deinit is fine.

template <typename Arg>
class LinuxSdCard {
    APRINTER_USE_TYPES1(Arg, (Context, ParentObject, InitHandler, CommandHandler, Params))
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    using CompletedFastEvent = typename Context::EventLoop::template FastEventSpec<LinuxSdCard>;
    
    enum class InitState : uint8_t {Inactive, Initing, Running};
    enum class IoState : uint8_t {Inactive, Reading, Writing};
    
    enum class ErrorCode : uint8_t {
        Success = 0,
        Impossible = 1,
        OpenFailed = 2,
        StatFailed = 3,
        BadFileSize = 4,
        IoFailed = 5,
        BadIoResLen = 6
    };
    
    static char const * FilePath() { return "sdcard.bin"; }
    
public:
    using BlockIndexType = uint32_t;
    static size_t const BlockSize = Params::BlockSize;
    using DataWordType = uint32_t;
    static size_t const MaxIoBlocks = Params::MaxIoBlocks;
    static int const MaxIoDescriptors = Params::MaxIoDescriptors;
    
private:
    static_assert(BlockSize > 0, "");
    static_assert(BlockSize % sizeof(DataWordType) == 0, "");
    static_assert(MaxIoBlocks > 0, "");
    static_assert(MaxIoDescriptors > 0, "");
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->init_state = InitState::Inactive;
        o->io_state = IoState::Inactive;
        o->stop_thread = false;
        o->cmd_in_progress = false;
        o->file_fd = -1;
        
        Context::EventLoop::template initFastEvent<CompletedFastEvent>(c, LinuxSdCard::completed_event_handler);
        
        AMBRO_ASSERT_FORCE_MSG(::sem_init(&o->start_cmd_sem, 0, 0) == 0, "sem_init failed")
        
        AMBRO_ASSERT_FORCE_MSG(::sem_init(&o->end_cmd_sem, 0, 1) == 0, "sem_init failed")
        
        {
            LinuxBlockSignals block_signals;
            AMBRO_ASSERT_FORCE_MSG(::pthread_create(&o->io_thread, nullptr, LinuxSdCard::io_thread_func, nullptr) == 0, "pthread_create failed")
        }
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        if (o->cmd_in_progress) {
            wait_for_cmd(c);
        }
        
        if (o->file_fd >= 0) {
            ::close(o->file_fd);
        }
        
        o->stop_thread = true;
        AMBRO_ASSERT_FORCE_MSG(::sem_post(&o->start_cmd_sem) == 0, "sem_post failed")
        AMBRO_ASSERT_FORCE_MSG(::pthread_join(o->io_thread, nullptr) == 0, "pthread_join failed")
        
        AMBRO_ASSERT_FORCE_MSG(::sem_destroy(&o->end_cmd_sem) == 0, "sem_destroy failed")
        
        AMBRO_ASSERT_FORCE_MSG(::sem_destroy(&o->start_cmd_sem) == 0, "sem_destroy failed")
        
        Context::EventLoop::template resetFastEvent<CompletedFastEvent>(c);
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == InitState::Inactive)
        // implies
        AMBRO_ASSERT(o->io_state == IoState::Inactive)
        AMBRO_ASSERT(!o->cmd_in_progress)
        AMBRO_ASSERT(o->file_fd == -1)
        
        o->init_state = InitState::Initing;
        o->cmd_in_progress = true;
        AMBRO_ASSERT_FORCE_MSG(::sem_post(&o->start_cmd_sem) == 0, "sem_post failed")
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state != InitState::Inactive)
        
        if (o->cmd_in_progress) {
            wait_for_cmd(c);
        }
        
        if (o->file_fd >= 0) {
            ::close(o->file_fd);
            o->file_fd = -1;
        }
        
        o->init_state = InitState::Inactive;
        o->io_state = IoState::Inactive;
    }
    
    static BlockIndexType getCapacityBlocks (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == InitState::Running)
        
        return o->capacity_blocks;
    }
    
    static bool isWritable (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == InitState::Running)
        
        return true;
    }
    
    static void startReadOrWrite (Context c, bool is_write, BlockIndexType block, size_t num_blocks, TransferVector<DataWordType> data_vector)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->init_state == InitState::Running)
        AMBRO_ASSERT(o->io_state == IoState::Inactive)
        AMBRO_ASSERT(block <= o->capacity_blocks)
        AMBRO_ASSERT(num_blocks > 0)
        AMBRO_ASSERT(num_blocks <= o->capacity_blocks - block)
        AMBRO_ASSERT(num_blocks <= MaxIoBlocks)
        AMBRO_ASSERT(data_vector.num_descriptors <= MaxIoDescriptors)
        AMBRO_ASSERT(CheckTransferVector(data_vector, num_blocks * (BlockSize/sizeof(DataWordType))))
        // implies
        AMBRO_ASSERT(!o->cmd_in_progress)
        AMBRO_ASSERT(o->file_fd >= 0)
        
        o->io_state = is_write ? IoState::Writing : IoState::Reading;
        o->io_block = block;
        o->io_num_blocks = num_blocks;
        o->io_vector = data_vector;
        o->cmd_in_progress = true;
        AMBRO_ASSERT_FORCE_MSG(::sem_post(&o->start_cmd_sem) == 0, "sem_post failed")
    }
    
    using EventLoopFastEvents = MakeTypeList<CompletedFastEvent>;
    
private:
    static void * io_thread_func (void *)
    {
        Context c;
        auto *o = Object::self(c);
        
        while (true) {
            AMBRO_ASSERT_FORCE_MSG(::sem_wait(&o->start_cmd_sem) == 0, "sem_wait failed")
            
            if (o->stop_thread) {
                break;
            }
            
            AMBRO_ASSERT(o->cmd_in_progress)
            
            ErrorCode err = ErrorCode::Impossible;
            
            if (o->init_state == InitState::Initing) {
                err = process_init(c);
            }
            else if (o->io_state == OneOf(IoState::Reading, IoState::Writing)) {
                err = process_read_or_write(c, o->io_state == IoState::Writing);
            }
            else {
                AMBRO_ASSERT(false)
            }
            
            o->error_code = err;
            
            Context::EventLoop::template triggerFastEvent<CompletedFastEvent>(c);
            
            AMBRO_ASSERT_FORCE_MSG(::sem_post(&o->end_cmd_sem) == 0, "sem_post failed")
        }
        
        return nullptr;
    }
    
    static ErrorCode process_init (Context c)
    {
        auto *o = Object::self(c);
        int res;
        
        int fd = ::open(FilePath(), O_RDWR);
        if (fd < 0) {
            return ErrorCode::OpenFailed;
        }
        
        struct stat stat_buf;
        res = ::fstat(fd, &stat_buf);
        if (res != 0) {
            ::close(fd);
            return ErrorCode::StatFailed;
        }
        
        auto capacity_blocks = stat_buf.st_size / BlockSize;
        if (capacity_blocks == 0 || capacity_blocks > (BlockIndexType)-1 ||
            capacity_blocks > std::numeric_limits<off_t>::max() / BlockSize 
        ) {
            ::close(fd);
            return ErrorCode::BadFileSize;
        }
        
        o->file_fd = fd;
        o->capacity_blocks = capacity_blocks;
        
        return ErrorCode::Success;
    }
    
    static ErrorCode process_read_or_write (Context c, bool is_write)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::Running)
        
        auto num_descriptors = o->io_vector.num_descriptors;
        AMBRO_ASSERT(num_descriptors <= MaxIoDescriptors)
        
        for (auto i : LoopRangeAuto(num_descriptors)) {
            o->iov[i].iov_base = o->io_vector.descriptors[i].buffer_ptr;
            o->iov[i].iov_len = o->io_vector.descriptors[i].num_words * sizeof(DataWordType);
        }
        
        off_t offset = o->io_block * (off_t)BlockSize;
        
        ssize_t res;
        if (is_write) {
            res = ::pwritev(o->file_fd, o->iov, num_descriptors, offset);
        } else {
            res = ::preadv(o->file_fd, o->iov, num_descriptors, offset);
        }
        
        if (res < 0) {
            return ErrorCode::IoFailed;
        }
        
        if ((size_t)res != o->io_num_blocks * BlockSize) {
            return ErrorCode::BadIoResLen;
        }
        
        return ErrorCode::Success;
    }
    
    static void completed_event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->cmd_in_progress)
        
        AMBRO_ASSERT_FORCE_MSG(::sem_trywait(&o->end_cmd_sem) == 0, "sem_trywait failed")
        
        o->cmd_in_progress = false;
        
        if (o->init_state == InitState::Initing) {
            AMBRO_ASSERT(o->io_state == IoState::Inactive)
            
            if (o->error_code == ErrorCode::Success) {
                AMBRO_ASSERT(o->file_fd >= 0)
                o->init_state = InitState::Running;
            } else {
                AMBRO_ASSERT(o->file_fd == -1)
                o->init_state = InitState::Inactive;
            }
            
            return InitHandler::call(c, (uint8_t)o->error_code);
        }
        else if (o->io_state == OneOf(IoState::Reading, IoState::Writing)) {
            AMBRO_ASSERT(o->init_state == InitState::Running)
            
            o->io_state = IoState::Inactive;
            
            bool error = o->error_code != ErrorCode::Success;
            return CommandHandler::call(c, error);
        }
        else {
            AMBRO_ASSERT(false)
        }
    }
    
    static void wait_for_cmd (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->cmd_in_progress)
        
        AMBRO_ASSERT_FORCE_MSG(::sem_wait(&o->end_cmd_sem) == 0, "sem_wait failed")
        AMBRO_ASSERT_FORCE_MSG(::sem_wait(&o->end_cmd_sem) == 0, "sem_wait failed")
        AMBRO_ASSERT_FORCE_MSG(::sem_post(&o->end_cmd_sem) == 0, "sem_post failed")
        
        Context::EventLoop::template resetFastEvent<CompletedFastEvent>(c);
        o->cmd_in_progress = false;
    }
    
public:
    struct Object : public ObjBase<LinuxSdCard, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        sem_t start_cmd_sem;
        sem_t end_cmd_sem;
        pthread_t io_thread;
        InitState init_state;
        IoState io_state;
        std::atomic_bool stop_thread;
        bool cmd_in_progress;
        ErrorCode error_code;
        int file_fd;
        BlockIndexType capacity_blocks;
        BlockIndexType io_block;
        size_t io_num_blocks;
        TransferVector<DataWordType> io_vector;
        struct iovec iov[MaxIoDescriptors];
    };
};

APRINTER_ALIAS_STRUCT_EXT(LinuxSdCardService, (
    APRINTER_AS_VALUE(size_t, BlockSize),
    APRINTER_AS_VALUE(size_t, MaxIoBlocks),
    APRINTER_AS_VALUE(int, MaxIoDescriptors)
), (
    APRINTER_ALIAS_STRUCT_EXT(SdCard, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(InitHandler),
        APRINTER_AS_TYPE(CommandHandler)
    ), (
        using Params = LinuxSdCardService;
        APRINTER_DEF_INSTANCE(SdCard, LinuxSdCard)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
