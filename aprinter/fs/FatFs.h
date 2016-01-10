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

#ifndef AMBROLIB_FAT_FS_H
#define AMBROLIB_FAT_FS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/EnableIf.h>
#include <aprinter/meta/FunctionIf.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/misc/Utf8Encoder.h>
#include <aprinter/misc/StringTools.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/fs/BlockCache.h>
#include <aprinter/fs/BlockRange.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename TheBlockAccess, typename InitHandler, typename WriteMountHandler, typename Params>
class FatFs {
public:
    struct Object;
    static bool const FsWritable = Params::Writable;
    
private:
    static_assert(Params::NumCacheEntries >= 1, "");
    static_assert(Params::MaxFileNameSize >= 12, "");
    
    using TheDebugObject = DebugObject<Context, Object>;
    using TheBlockCache = BlockCache<Context, Object, TheBlockAccess, Params::NumCacheEntries, FsWritable>;
    
    using BlockAccessUser = typename TheBlockAccess::User;
    using BlockIndexType = typename TheBlockAccess::BlockIndexType;
    static size_t const BlockSize = TheBlockAccess::BlockSize;
    using CacheBlockRef = typename TheBlockCache::CacheRef;
    using CacheFlushRequest = typename TheBlockCache::template FlushRequest<>;
    
    static_assert(BlockSize >= 0x47, "BlockSize not enough for EBPB");
    static_assert(BlockSize % 32 == 0, "BlockSize not a multiple of 32");
    static_assert(BlockSize >= 512, "BlockSize not enough for FS Information Sector");
    
    static size_t const FatEntriesPerBlock = BlockSize / 4;
    static size_t const DirEntriesPerBlock = BlockSize / 32;
    
    using ClusterIndexType = uint32_t;
    using ClusterBlockIndexType = uint16_t;
    using DirEntriesPerBlockType = ChooseIntForMax<DirEntriesPerBlock, false>;
    using FileNameLenType = ChooseIntForMax<Params::MaxFileNameSize, false>;
    
    static size_t const EbpbStatusBitsOffset = 0x41;
    static uint8_t const StatusBitsDirty = 0x01;
    
    static ClusterIndexType const EndOfChainMarker = UINT32_C(0x0FFFFFFF);
    static ClusterIndexType const FreeClusterMarker = UINT32_C(0x00000000);
    static ClusterIndexType const EmptyFileMarker = UINT32_C(0x00000000);
    static ClusterIndexType const NormalClusterIndexEnd = UINT32_C(0x0FFFFFF8);
    
    static size_t const DirEntrySizeOffset = 0x1C;
    
    static size_t const FsInfoSig1Offset = 0x0;
    static size_t const FsInfoSig2Offset = 0x1E4;
    static size_t const FsInfoFreeClustersOffset = 0x1E8;
    static size_t const FsInfoAllocatedClusterOffset = 0x1EC;
    static size_t const FsInfoSig3Offset = 0x1FC;
    
    // Use a lesser eviction priority value for data blocks than for metadata.
    // This way data blocks will be evicted from the cache before metadata,
    // allowing reuse of the metadata and less writes.
    static uint8_t const FileDataEvictionPriority = 5;
    
    enum class FsState : uint8_t {INIT, READY, FAILED};
    enum class WriteMountState : uint8_t {NOT_MOUNTED, MOUNT_META, MOUNT_FSINFO, MOUNT_FLUSH, MOUNTED, UMOUNT_FLUSH1, UMOUNT_META, UMOUNT_FLUSH2};
    enum class AllocationState : uint8_t {IDLE, CHECK_EVENT, REQUESTING_BLOCK};
    
    template <bool Writable> class ClusterChain;
    template <bool Writable> class DirEntryRef;
    class DirectoryIterator;
    template <bool Writable> class WriteReference;
    
    APRINTER_STRUCT_IF_TEMPLATE(FsEntryExtra) {
        BlockIndexType dir_entry_block_index;
        DirEntriesPerBlockType dir_entry_block_offset;
    };
    
public:
    static size_t const TheBlockSize = BlockSize;
    
    enum class EntryType : uint8_t {DIR_TYPE, FILE_TYPE};
    
    class FsEntry : private FsEntryExtra<FsWritable> {
        friend FatFs;
        
    public:
        inline EntryType getType () const { return type; }
        inline uint32_t getFileSize () const { return file_size; }
        
    private:
        EntryType type;
        uint32_t file_size;
        ClusterIndexType cluster_index;
    };
    
    static bool isPartitionTypeSupported (uint8_t type)
    {
        return (type == 0xB || type == 0xC);
    }
    
    static void init (Context c, BlockRange<BlockIndexType> block_range)
    {
        auto *o = Object::self(c);
        
        TheBlockCache::init(c);
        
        o->block_range = block_range;
        o->state = FsState::INIT;
        
        o->init_block_ref.init(c, APRINTER_CB_STATFUNC_T(&FatFs::init_block_ref_handler));
        o->init_block_ref.requestBlock(c, get_abs_block_index(c, 0), 0, 1, CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION);
        
        fs_writable_init(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        fs_writable_deinit(c);
        
        if (o->state == FsState::INIT) {
            o->init_block_ref.deinit(c);
        }
        
        TheBlockCache::deinit(c);
    }
    
    static FsEntry getRootEntry (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FsState::READY)
        
        FsEntry entry;
        entry.type = EntryType::DIR_TYPE;
        entry.file_size = 0;
        entry.cluster_index = o->root_cluster;
        set_fs_entry_extra(&entry, 0, 0);
        return entry;
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, startWriteMount (Context c))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FsState::READY)
        AMBRO_ASSERT(o->write_mount_state == WriteMountState::NOT_MOUNTED)
        // implies
        AMBRO_ASSERT(o->num_write_references == 0)
        AMBRO_ASSERT(o->alloc_state == AllocationState::IDLE)
        
        o->write_mount_state = WriteMountState::MOUNT_META;
        o->write_block_ref.requestBlock(c, get_abs_block_index(c, 0), 0, 1, CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, startWriteUnmount (Context c))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FsState::READY)
        AMBRO_ASSERT(o->write_mount_state == WriteMountState::MOUNTED)
        AMBRO_ASSERT(o->num_write_references == 0)
        // implies
        AMBRO_ASSERT(o->alloc_state == AllocationState::IDLE)
        
        o->write_mount_state = WriteMountState::UMOUNT_FLUSH1;
        o->flush_request.requestFlush(c);
    }
    
    class DirLister {
    public:
        using DirListerHandler = Callback<void(Context c, bool is_error, char const *name, FsEntry entry)>;
        
        void init (Context c, FsEntry dir_entry, DirListerHandler handler)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(o->state == FsState::READY)
            AMBRO_ASSERT(dir_entry.type == EntryType::DIR_TYPE)
            
            m_dir_iter.init(c, dir_entry.cluster_index, handler);
        }
        
        void deinit (Context c)
        {
            TheDebugObject::access(c);
            
            m_dir_iter.deinit(c);
        }
        
        void requestEntry (Context c)
        {
            TheDebugObject::access(c);
            
            m_dir_iter.requestEntry(c);
        }
        
    private:
        DirectoryIterator m_dir_iter;
    };
    
    class Opener {
        enum class State : uint8_t {REQUESTING_ENTRY, COMPLETED};
        
    public:
        enum class OpenerStatus : uint8_t {SUCCESS, NOT_FOUND, ERROR};
        
        using OpenerHandler = Callback<void(Context c, OpenerStatus status, FsEntry entry)>;
        
        void init (Context c, FsEntry dir_entry, EntryType entry_type, char const *name, OpenerHandler handler)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(o->state == FsState::READY)
            AMBRO_ASSERT(dir_entry.type == EntryType::DIR_TYPE)
            AMBRO_ASSERT(name)
            
            m_entry_type = entry_type;
            m_name = name;
            m_handler = handler;
            m_state = State::REQUESTING_ENTRY;
            m_dir_iter.init(c, dir_entry.cluster_index, APRINTER_CB_OBJFUNC_T(&Opener::dir_iter_handler, this));
            m_dir_iter.requestEntry(c);
        }
        
        void deinit (Context c)
        {
            TheDebugObject::access(c);
            
            if (m_state != State::COMPLETED) {
                m_dir_iter.deinit(c);
            }
        }
        
    private:
        void dir_iter_handler (Context c, bool is_error, char const *name, FsEntry entry)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::REQUESTING_ENTRY)
            
            if (is_error || !name) {
                m_state = State::COMPLETED;
                m_dir_iter.deinit(c);
                OpenerStatus status = is_error ? OpenerStatus::ERROR : OpenerStatus::NOT_FOUND;
                return m_handler(c, status, FsEntry{});
            }
            if (entry.type != m_entry_type || !compare_filename_equal(name, m_name)) {
                m_dir_iter.requestEntry(c);
                return;
            }
            m_state = State::COMPLETED;
            m_dir_iter.deinit(c);
            return m_handler(c, OpenerStatus::SUCCESS, entry);
        }
        
        static bool compare_filename_equal (char const *str1, char const *str2)
        {
            return Params::CaseInsens ? AsciiCaseInsensStringEqual(str1, str2) : !strcmp(str1, str2);
        }
        
        EntryType m_entry_type;
        State m_state;
        char const *m_name;
        OpenerHandler m_handler;
        DirectoryIterator m_dir_iter;
    };
    
    APRINTER_STRUCT_IF_TEMPLATE(FileExtraMembers) {
        DirEntryRef<true> m_dir_entry;
        size_t m_write_bytes_in_block;
        BlockIndexType m_dir_entry_block_index;
        DirEntriesPerBlockType m_dir_entry_block_offset;
        bool m_no_need_to_read_for_write;
        WriteReference<true> m_write_ref;
    };
    
    template <bool Writable>
    class File : public FileExtraMembers<Writable> {
        static_assert(!Writable || FsWritable, "");
        
        enum class State : uint8_t {
            IDLE,
            READ_EVENT, READ_NEXT_CLUSTER, READ_BLOCK, READ_READY,
            OPENWR_EVENT, OPENWR_DIR_ENTRY,
            WRITE_EVENT, WRITE_NEXT_CLUSTER, WRITE_BLOCK, WRITE_READY,
            TRUNC_EVENT, TRUNC_CHAIN
        };
        
    public:
        enum class IoMode : uint8_t {USER_BUFFER, FS_BUFFER};
        
        using FileHandler = Callback<void(Context c, bool error, size_t length)>;
        
        void init (Context c, FsEntry file_entry, FileHandler handler, IoMode io_mode=IoMode::USER_BUFFER)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(o->state == FsState::READY)
            AMBRO_ASSERT(file_entry.type == EntryType::FILE_TYPE)
            AMBRO_ASSERT(io_mode == IoMode::USER_BUFFER || io_mode == IoMode::FS_BUFFER)
            
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&File::event_handler, this));
            m_chain.init(c, file_entry.cluster_index, APRINTER_CB_OBJFUNC_T(&File::chain_handler, this));
            
            if (io_mode == IoMode::USER_BUFFER) {
                m_user_buffer_mode.block_user.init(c, APRINTER_CB_OBJFUNC_T(&File::block_user_block_ref_handler, this));
            } else {
                m_fs_buffer_mode.block_ref.init(c, APRINTER_CB_OBJFUNC_T(&File::block_user_block_ref_handler, this));
            }
            
            m_handler = handler;
            m_file_size = file_entry.file_size;
            m_state = State::IDLE;
            m_io_mode = io_mode;
            m_file_pos = 0;
            m_block_in_cluster = o->blocks_per_cluster;
            
            writable_init(c, file_entry);
        }
        
        // NOTE: Not allowed when reader is busy, except when deiniting the whole FatFs and underlying storage!
        void deinit (Context c)
        {
            TheDebugObject::access(c);
            
            writable_deinit(c);
            
            if (m_io_mode == IoMode::USER_BUFFER) {
                m_user_buffer_mode.block_user.deinit(c);
            } else {
                m_fs_buffer_mode.block_ref.deinit(c);
            }
            
            m_chain.deinit(c);
            m_event.deinit(c);
        }
        
        void rewind (Context c)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            
            m_chain.rewind(c);
            m_file_pos = 0;
            m_block_in_cluster = o->blocks_per_cluster;
        }
        
        void startRead (Context c, WrapBuffer buf=WrapBuffer::Make(nullptr))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(bool(buf.ptr1) == (m_io_mode == IoMode::USER_BUFFER))
            
            if (m_io_mode == IoMode::USER_BUFFER) {
                m_user_buffer_mode.request_buf = buf;
            }
            m_state = State::READ_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        char const * getReadPointer (Context c)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::READ_READY)
            AMBRO_ASSERT(m_io_mode == IoMode::FS_BUFFER)
            
            return m_fs_buffer_mode.block_ref.getData(c, WrapBool<false>());
        }
        
        void finishRead (Context c)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::READ_READY)
            AMBRO_ASSERT(m_io_mode == IoMode::FS_BUFFER)
            
            finish_read(c, get_bytes_in_block(c));
            m_fs_buffer_mode.block_ref.reset(c);
            m_state = State::IDLE;
        }
        
        APRINTER_FUNCTION_IF(Writable, void, startOpenWritable (Context c))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(!this->m_write_ref.isTaken(c))
            
            m_state = State::OPENWR_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, closeWritable (Context c))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            
            clean_up_writability(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, startWriteUserBuf (Context c, WrapBuffer buf, size_t bytes_in_block))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(m_file_pos % BlockSize == 0)
            AMBRO_ASSERT(m_io_mode == IoMode::USER_BUFFER)
            AMBRO_ASSERT(buf.ptr1)
            AMBRO_ASSERT(bytes_in_block > 0)
            AMBRO_ASSERT(bytes_in_block <= BlockSize)
            
            m_user_buffer_mode.request_buf = buf;
            this->m_write_bytes_in_block = bytes_in_block;
            m_state = State::WRITE_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, startWriteFsBuf (Context c, bool no_need_to_read))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(m_file_pos % BlockSize == 0)
            AMBRO_ASSERT(m_io_mode == IoMode::FS_BUFFER)
            
            this->m_no_need_to_read_for_write = no_need_to_read;
            m_state = State::WRITE_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, char *, getWritePointer (Context c))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::WRITE_READY)
            AMBRO_ASSERT(m_io_mode == IoMode::FS_BUFFER)
            
            return m_fs_buffer_mode.block_ref.getData(c, WrapBool<true>());
        }
        
        APRINTER_FUNCTION_IF(Writable, void, finishWrite (Context c, size_t bytes_in_block))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::WRITE_READY)
            AMBRO_ASSERT(m_io_mode == IoMode::FS_BUFFER)
            AMBRO_ASSERT(bytes_in_block >= 0)
            AMBRO_ASSERT(bytes_in_block <= BlockSize)
            
            if (bytes_in_block > 0) {
                finish_write(c, bytes_in_block);
                m_fs_buffer_mode.block_ref.markDirty(c);
            }
            m_fs_buffer_mode.block_ref.reset(c);
            m_state = State::IDLE;
        }
        
        APRINTER_FUNCTION_IF(Writable, void, startTruncate (Context c))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            
            m_state = State::TRUNC_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
    private:
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, writable_init (Context c, FsEntry file_entry))
        {
            this->m_dir_entry.init(c, APRINTER_CB_OBJFUNC_T(&File::dir_entry_handler<>, this));
            this->m_write_ref.init(c);
            
            this->m_dir_entry_block_index = file_entry.dir_entry_block_index;
            this->m_dir_entry_block_offset = file_entry.dir_entry_block_offset;
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, writable_deinit (Context c))
        {
            this->m_write_ref.deinit(c);
            this->m_dir_entry.deinit(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, handle_event_openwr (Context c))
        {
            if (!this->m_write_ref.take(c)) {
                return complete_open_writable_request(c, true);
            }
            m_state = State::OPENWR_DIR_ENTRY;
            this->m_dir_entry.requestEntryRef(c, this->m_dir_entry_block_index, this->m_dir_entry_block_offset);
        }
        
        void handle_event_read (Context c)
        {
            auto *o = Object::self(c);
            if (m_file_pos >= m_file_size) {
                return complete_request(c, false);
            }
            if (m_block_in_cluster == o->blocks_per_cluster) {
                m_state = State::READ_NEXT_CLUSTER;
                m_chain.requestNext(c);
                return;
            }
            if (!is_cluster_idx_valid_for_data(c, m_chain.getCurrentCluster(c))) {
                return complete_request(c, true);
            }
            m_state = State::READ_BLOCK;
            BlockIndexType abs_block_idx = get_cluster_data_abs_block_index(c, m_chain.getCurrentCluster(c), m_block_in_cluster);
            if (m_io_mode == IoMode::USER_BUFFER) {
                m_user_buffer_mode.block_user.startRead(c, abs_block_idx, m_user_buffer_mode.request_buf);
            } else {
                m_fs_buffer_mode.block_ref.requestBlock(c, abs_block_idx, 0, 1, CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION, FileDataEvictionPriority);
            }
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, handle_event_write (Context c))
        {
            auto *o = Object::self(c);
            if (!this->m_write_ref.isTaken(c)) {
                return complete_request(c, true);
            }
            if (m_block_in_cluster == o->blocks_per_cluster) {
                this->m_no_need_to_read_for_write = true;
                m_state = State::WRITE_NEXT_CLUSTER;
                m_chain.requestNext(c);
                return;
            }
            if (!is_cluster_idx_valid_for_data(c, m_chain.getCurrentCluster(c))) {
                return complete_request(c, true);
            }
            m_state = State::WRITE_BLOCK;
            BlockIndexType abs_block_idx = get_cluster_data_abs_block_index(c, m_chain.getCurrentCluster(c), m_block_in_cluster);
            if (m_io_mode == IoMode::USER_BUFFER) {
                m_user_buffer_mode.block_user.startWrite(c, abs_block_idx, m_user_buffer_mode.request_buf);
            } else {
                uint8_t flags = CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION;
                if (this->m_no_need_to_read_for_write) {
                    flags |= CacheBlockRef::FLAG_NO_NEED_TO_READ;
                }
                m_fs_buffer_mode.block_ref.requestBlock(c, abs_block_idx, 0, 1, flags, FileDataEvictionPriority);
            }
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, handle_event_trunc (Context c))
        {
            if (!this->m_write_ref.isTaken(c)) {
                return complete_request(c, true);
            }
            if (m_file_size > m_file_pos) {
                m_file_size = m_file_pos;
                this->m_dir_entry.setFileSize(c, m_file_size);
            }
            m_state = State::TRUNC_CHAIN;
            m_chain.startTruncate(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, extra_first_cluster_update (Context c, bool first_cluster_changed))
        {
            AMBRO_ASSERT(!first_cluster_changed || this->m_write_ref.isTaken(c))
            
            if (first_cluster_changed) {
                AMBRO_ASSERT(m_state == State::WRITE_NEXT_CLUSTER || m_state == State::TRUNC_CHAIN)
                this->m_dir_entry.setFirstCluster(c, m_chain.getFirstCluster(c));
            }
        }
        
        void handle_chain_read_next (Context c, bool error)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_block_in_cluster == o->blocks_per_cluster)
            if (error || m_chain.endReached(c)) {
                return complete_request(c, true);
            }
            m_block_in_cluster = 0;
            m_state = State::READ_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, handle_chain_write_next (Context c, bool error))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_block_in_cluster == o->blocks_per_cluster)
            if (error) {
                return complete_request(c, true);
            }
            if (m_chain.endReached(c)) {
                m_chain.requestNew(c);
                return;
            }
            m_block_in_cluster = 0;
            m_state = State::WRITE_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        void handle_block_read (Context c, bool error)
        {
            if (error) {
                return complete_request(c, true);
            }
            size_t bytes_in_block = get_bytes_in_block(c);
            AMBRO_ASSERT(bytes_in_block > 0)
            if (m_io_mode == IoMode::USER_BUFFER) {
                finish_read(c, bytes_in_block);
                return complete_request(c, false, bytes_in_block);
            } else {
                return complete_request(c, false, bytes_in_block, State::READ_READY);
            }
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, handle_block_write (Context c, bool error))
        {
            if (error) {
                return complete_request(c, true);
            }
            if (m_io_mode == IoMode::USER_BUFFER) {
                finish_write(c, this->m_write_bytes_in_block);
                return complete_request(c, false);
            } else {
                return complete_request(c, false, 0, State::WRITE_READY);
            }
        }
        
        void complete_request (Context c, bool error, size_t length=0, State new_state=State::IDLE)
        {
            m_state = new_state;
            return m_handler(c, error, length);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, complete_open_writable_request (Context c, bool error))
        {
            if (error) {
                clean_up_writability(c);
            }
            return complete_request(c, error);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, clean_up_writability (Context c))
        {
            this->m_write_ref.release(c);
            this->m_dir_entry.reset(c);
        }
        
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_block_in_cluster <= o->blocks_per_cluster)
            
            if (m_state == State::READ_EVENT) {
                handle_event_read(c);
            }
            else if (Writable && m_state == State::OPENWR_EVENT) {
                handle_event_openwr(c);
            }
            else if (Writable && m_state == State::WRITE_EVENT) {
                handle_event_write(c);
            }
            else if (Writable && m_state == State::TRUNC_EVENT) {
                handle_event_trunc(c);
            }
            else {
                AMBRO_ASSERT(false);
            }
        }
        
        void chain_handler (Context c, bool error, bool first_cluster_changed)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            
            extra_first_cluster_update(c, first_cluster_changed);
            
            if (m_state == State::READ_NEXT_CLUSTER) {
                handle_chain_read_next(c, error);
            }
            else if (Writable && m_state == State::WRITE_NEXT_CLUSTER) {
                handle_chain_write_next(c, error);
            }
            else if (Writable && m_state == State::TRUNC_CHAIN) {
                return complete_request(c, error);
            }
            else {
                AMBRO_ASSERT(false);
            }
        }
        
        void block_user_block_ref_handler (Context c, bool error)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_block_in_cluster < o->blocks_per_cluster)
            
            if (m_state == State::READ_BLOCK) {
                handle_block_read(c, error);
            }
            else if (Writable && m_state == State::WRITE_BLOCK) {
                handle_block_write(c, error);
            }
            else {
                AMBRO_ASSERT(false);
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, dir_entry_handler (Context c, bool error))
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::OPENWR_DIR_ENTRY)
            
            if (error) {
                return complete_open_writable_request(c, true);
            }
            if (this->m_dir_entry.getFirstCluster(c) != m_chain.getFirstCluster(c)) {
                return complete_open_writable_request(c, true);
            }
            if (this->m_dir_entry.getFileSize(c) != m_file_size) {
                return complete_open_writable_request(c, true);
            }
            return complete_open_writable_request(c, false);
        }
        
        size_t get_bytes_in_block (Context c)
        {
            return MinValue((uint32_t)BlockSize, (uint32_t)(m_file_size - m_file_pos));
        }
        
        void finish_read (Context c, size_t bytes_in_block)
        {
            m_file_pos += bytes_in_block;
            m_block_in_cluster++;
        }
        
        APRINTER_FUNCTION_IF(Writable, void, finish_write (Context c, size_t bytes_in_block))
        {
            m_file_pos += bytes_in_block;
            if (m_file_size < m_file_pos) {
                m_file_size = m_file_pos;
                this->m_dir_entry.setFileSize(c, m_file_size);
            }
            m_block_in_cluster++;
        }
        
        typename Context::EventLoop::QueuedEvent m_event;
        ClusterChain<Writable> m_chain;
        FileHandler m_handler;
        uint32_t m_file_size;
        uint32_t m_file_pos;
        State m_state;
        IoMode m_io_mode;
        ClusterBlockIndexType m_block_in_cluster;
        union {
            struct {
                BlockAccessUser block_user;
                WrapBuffer request_buf;
            } m_user_buffer_mode;
            struct {
                CacheBlockRef block_ref;
            } m_fs_buffer_mode;
        };
    };
    
    template <typename Dummy=void>
    using FlushRequest = CacheFlushRequest;
    
    using CacheRefForUser = CacheBlockRef;
    
private:
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(FsWritable, static, void, fs_writable_init (Context c))
    {
        auto *o = Object::self(c);
        o->alloc_event.init(c, APRINTER_CB_STATFUNC_T(&FatFs::alloc_event_handler<>));
        o->write_block_ref.init(c, APRINTER_CB_STATFUNC_T(&FatFs::write_block_ref_handler<>));
        o->fs_info_block_ref.init(c, APRINTER_CB_STATFUNC_T(&FatFs::fs_info_block_ref_handler<>));
        o->flush_request.init(c, APRINTER_CB_STATFUNC_T(&FatFs::flush_request_handler<>));
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(FsWritable, static, void, fs_writable_deinit (Context c))
    {
        auto *o = Object::self(c);
        o->flush_request.deinit(c);
        o->fs_info_block_ref.deinit(c);
        o->write_block_ref.deinit(c);
        o->alloc_event.deinit(c);
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(FsWritable, static, void, fs_writable_init_completed (Context c, BlockIndexType fs_info_block))
    {
        auto *o = Object::self(c);
        o->write_mount_state = WriteMountState::NOT_MOUNTED;
        o->alloc_state = AllocationState::IDLE;
        o->fs_info_block = fs_info_block;
        o->allocating_chains_list.init();
        o->num_write_references = 0;
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(FsWritable, static, void, write_block_ref_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        if (o->state == FsState::READY) {
            if (o->write_mount_state == WriteMountState::MOUNT_META) {
                return write_mount_metablock_ref_handler(c, error);
            } else if (o->write_mount_state == WriteMountState::UMOUNT_META) {
                return write_unmount_metablock_ref_handler(c, error);
            } else if (o->alloc_state == AllocationState::REQUESTING_BLOCK) {
                return alloc_block_ref_handler(c, error);
            }
        }
        AMBRO_ASSERT(false)
    }
    
    static void init_block_ref_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FsState::INIT)
        
        uint8_t error_code = 99;
        do {
            if (error) {
                o->init_block_ref.deinit(c);
                error_code = 20;
                goto error;
            }
            
            char const *buffer = o->init_block_ref.getData(c, WrapBool<false>());
            
            uint16_t sector_size =          ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0xB);
            uint8_t sectors_per_cluster =   ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0xD);
            uint16_t num_reserved_sectors = ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0xE);
            uint8_t num_fats =              ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0x10);
            uint16_t max_root =             ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0x11);
            uint32_t sectors_per_fat =      ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + 0x24);
            uint32_t root_cluster =         ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + 0x2C);
            uint16_t fs_info_sector =       ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0x30);
            uint8_t sig =                   ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0x42);
            
            o->init_block_ref.deinit(c);
            
            if (sector_size == 0 || sector_size % BlockSize != 0) {
                error_code = 22;
                goto error;
            }
            uint16_t blocks_per_sector = sector_size / BlockSize;
            
            if (sectors_per_cluster > UINT16_MAX / blocks_per_sector) {
                error_code = 23;
                goto error;
            }
            o->blocks_per_cluster = blocks_per_sector * sectors_per_cluster;
            
            if ((uint32_t)num_reserved_sectors * sector_size < 0x47) {
                error_code = 24;
                goto error;
            }
            
            if (num_fats != 1 && num_fats != 2) {
                error_code = 25;
                goto error;
            }
            o->num_fats = num_fats;
            
            if (sig != 0x28 && sig != 0x29) {
                error_code = 26;
                goto error;
            }
            
            if (max_root != 0) {
                error_code = 27;
                goto error;
            }
            
            o->root_cluster = mask_cluster_entry(root_cluster);
            if (o->root_cluster < 2) {
                error_code = 28;
                goto error;
            }
            
            uint16_t entries_per_sector = sector_size / 4;
            if (sectors_per_fat == 0 || sectors_per_fat > UINT32_MAX / entries_per_sector) {
                error_code = 29;
                goto error;
            }
            o->num_fat_entries = (ClusterIndexType)sectors_per_fat * entries_per_sector;
            
            uint64_t fat_end_sectors_calc = (uint64_t)num_reserved_sectors + (uint64_t)o->num_fats * sectors_per_fat;
            if (fat_end_sectors_calc > o->block_range.getLength() / blocks_per_sector) {
                error_code = 29;
                goto error;
            }
            BlockIndexType num_reserved_blocks = (BlockIndexType)num_reserved_sectors * blocks_per_sector;
            o->fat_end_blocks = fat_end_sectors_calc * blocks_per_sector;
            
            BlockIndexType fs_info_block;
            if (fs_info_sector == 0 || fs_info_sector == UINT16_C(0xFFFF)) {
                fs_info_block = 0;
            } else {
                uint32_t fs_info_block_calc = fs_info_sector * (uint32_t)blocks_per_sector;
                if (fs_info_block_calc >= num_reserved_blocks) {
                    error_code = 31;
                    goto error;
                }
                fs_info_block = fs_info_block_calc;
            }
            
            ClusterIndexType valid_clusters_for_capacity = (o->block_range.getLength() - o->fat_end_blocks) / o->blocks_per_cluster;
            if (valid_clusters_for_capacity < 1) {
                error_code = 30;
                goto error;
            }
            o->num_valid_clusters = MinValue(valid_clusters_for_capacity, MinValue((ClusterIndexType)(o->num_fat_entries - 2), NormalClusterIndexEnd - 2));
            
            fs_writable_init_completed(c, fs_info_block);
            
            error_code = 0;
        } while (0);
        
    error:
        o->state = error_code ? FsState::FAILED : FsState::READY;
        return InitHandler::call(c, error_code);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, complete_write_mount_request (Context c, bool error))
    {
        auto *o = Object::self(c);
        o->write_block_ref.reset(c);
        o->flush_request.reset(c);
        if (error) {
            o->fs_info_block_ref.reset(c);
            o->write_mount_state = WriteMountState::NOT_MOUNTED;
        } else {
            o->write_mount_state = WriteMountState::MOUNTED;
        }
        return WriteMountHandler::call(c, error);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, complete_write_unmount_request (Context c, bool error))
    {
        auto *o = Object::self(c);
        o->write_block_ref.reset(c);
        o->flush_request.reset(c);
        if (error) {
            o->write_mount_state = WriteMountState::MOUNTED;
        } else {
            o->fs_info_block_ref.reset(c);
            o->write_mount_state = WriteMountState::NOT_MOUNTED;
        }
        return WriteMountHandler::call(c, error);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, write_mount_metablock_ref_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == FsState::READY)
        AMBRO_ASSERT(o->write_mount_state == WriteMountState::MOUNT_META)
        
        if (error) {
            return complete_write_mount_request(c, true);
        }
        if (get_fs_dirty_bit(c, &o->write_block_ref)) {
            return complete_write_mount_request(c, true);
        }
        if (!TheBlockAccess::isWritable(c)) {
            return complete_write_mount_request(c, true);
        }
        if (o->fs_info_block == 0) {
            return complete_write_mount_request(c, true);
        }
        o->write_mount_state = WriteMountState::MOUNT_FSINFO;
        o->fs_info_block_ref.requestBlock(c, get_abs_block_index(c, o->fs_info_block), 0, 1, CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, flush_request_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FsState::READY)
        
        switch (o->write_mount_state) {
            case WriteMountState::MOUNT_FLUSH: {
                if (error) {
                    update_fs_dirty_bit(c, &o->write_block_ref, false);
                    return complete_write_mount_request(c, true);
                }
                return complete_write_mount_request(c, false);
            } break;
            
            case WriteMountState::UMOUNT_FLUSH1: {
                if (error) {
                    return complete_write_unmount_request(c, true);
                }
                o->write_mount_state = WriteMountState::UMOUNT_META;
                o->write_block_ref.requestBlock(c, get_abs_block_index(c, 0), 0, 1, CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION);
            } break;
            
            case WriteMountState::UMOUNT_FLUSH2: {
                return complete_write_unmount_request(c, error);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, fs_info_block_ref_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FsState::READY)
        AMBRO_ASSERT(o->write_mount_state == WriteMountState::MOUNT_FSINFO)
        
        if (error) {
            return complete_write_mount_request(c, true);
        }
        char const *buffer = o->fs_info_block_ref.getData(c, WrapBool<false>());
        uint32_t sig1          = ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + FsInfoSig1Offset);
        uint32_t sig2          = ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + FsInfoSig2Offset);
        uint32_t sig3          = ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + FsInfoSig3Offset);
        uint32_t alloc_cluster = ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + FsInfoAllocatedClusterOffset);
        if (sig1 != UINT32_C(0x41615252) || sig2 != UINT32_C(0x61417272) || sig3 != UINT32_C(0xAA550000)) {
            return complete_write_mount_request(c, true);
        }
        o->alloc_position = 0;
        if (alloc_cluster >= 2 && alloc_cluster < 2 + o->num_valid_clusters) {
            o->alloc_position = alloc_cluster - 2;
        }
        update_fs_dirty_bit(c, &o->write_block_ref, true);
        o->write_mount_state = WriteMountState::MOUNT_FLUSH;
        o->flush_request.requestFlush(c);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, write_unmount_metablock_ref_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == FsState::READY)
        AMBRO_ASSERT(o->write_mount_state == WriteMountState::UMOUNT_META)
        
        if (error) {
            return complete_write_unmount_request(c, true);
        }
        if (!get_fs_dirty_bit(c, &o->write_block_ref)) {
            return complete_write_unmount_request(c, true);
        }
        update_fs_dirty_bit(c, &o->write_block_ref, false);
        o->write_mount_state = WriteMountState::UMOUNT_FLUSH2;
        o->flush_request.requestFlush(c);
    }
    
    static ClusterIndexType mask_cluster_entry (uint32_t entry_value)
    {
        return (entry_value & UINT32_C(0x0FFFFFFF));
    }
    
    static uint32_t update_cluster_entry (uint32_t entry_value, ClusterIndexType new_value)
    {
        return (entry_value & UINT32_C(0xF0000000)) | new_value;
    }
    
    static bool is_cluster_idx_normal (ClusterIndexType cluster_idx)
    {
        return (cluster_idx >= 2 && cluster_idx < NormalClusterIndexEnd);
    }
    
    static bool is_cluster_idx_valid_for_fat (Context c, ClusterIndexType cluster_idx)
    {
        auto *o = Object::self(c);
        return cluster_idx < o->num_fat_entries;
    }
    
    static BlockIndexType get_abs_block_index_for_fat_entry (Context c, ClusterIndexType cluster_idx)
    {
        auto *o = Object::self(c);
        BlockIndexType num_reserved_blocks = o->fat_end_blocks - o->num_fats * (o->num_fat_entries / FatEntriesPerBlock);
        return get_abs_block_index(c, num_reserved_blocks + (cluster_idx / FatEntriesPerBlock));
    }
    
    static bool request_fat_cache_block (Context c, CacheBlockRef *block_ref, ClusterIndexType cluster_idx, bool disable_immediate_completion)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(is_cluster_idx_valid_for_fat(c, cluster_idx))
        
        BlockIndexType abs_block_idx = get_abs_block_index_for_fat_entry(c, cluster_idx);
        BlockIndexType num_blocks_per_fat = o->num_fat_entries / FatEntriesPerBlock;
        return block_ref->requestBlock(c, abs_block_idx, num_blocks_per_fat, o->num_fats, disable_immediate_completion ? CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION : 0);
    }
    
    template <bool ForWriting>
    static If<ForWriting, char, char const> * get_fat_ptr_in_cache_block (Context c, CacheBlockRef *block_ref, ClusterIndexType cluster_idx)
    {
        AMBRO_ASSERT(is_cluster_idx_valid_for_fat(c, cluster_idx))
        AMBRO_ASSERT(block_ref->getAvailableBlock(c) == get_abs_block_index_for_fat_entry(c, cluster_idx))
        
        return block_ref->getData(c, WrapBool<ForWriting>()) + ((size_t)4 * (cluster_idx % FatEntriesPerBlock));
    }
    
    static ClusterIndexType read_fat_entry_in_cache_block (Context c, CacheBlockRef *block_ref, ClusterIndexType cluster_idx)
    {
        AMBRO_ASSERT(is_cluster_idx_valid_for_fat(c, cluster_idx))
        
        char const *entry_ptr = get_fat_ptr_in_cache_block<false>(c, block_ref, cluster_idx);
        return mask_cluster_entry(ReadBinaryInt<uint32_t, BinaryLittleEndian>(entry_ptr));
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, update_fat_entry_in_cache_block (Context c, CacheBlockRef *block_ref, ClusterIndexType cluster_idx, ClusterIndexType value))
    {
        AMBRO_ASSERT(is_cluster_idx_valid_for_fat(c, cluster_idx))
        
        char *entry_ptr = get_fat_ptr_in_cache_block<true>(c, block_ref, cluster_idx);
        uint32_t new_entry_value = update_cluster_entry(ReadBinaryInt<uint32_t, BinaryLittleEndian>(entry_ptr), value);
        WriteBinaryInt<uint32_t, BinaryLittleEndian>(new_entry_value, entry_ptr);
        block_ref->markDirty(c);
    }
    
    static bool is_cluster_idx_valid_for_data (Context c, ClusterIndexType cluster_idx)
    {
        auto *o = Object::self(c);
        return cluster_idx >= 2 && cluster_idx - 2 < o->num_valid_clusters;
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, bool, release_cluster (Context c, CacheBlockRef *block_ref, ClusterIndexType cluster_index, ClusterIndexType *out_next_cluster=nullptr))
    {
        ClusterIndexType next_cluster = read_fat_entry_in_cache_block(c, block_ref, cluster_index);
        if (out_next_cluster) {
            *out_next_cluster = next_cluster;
        }
        if (!is_cluster_idx_valid_for_data(c, cluster_index) || next_cluster == FreeClusterMarker) {
            return false;
        }
        update_fat_entry_in_cache_block(c, block_ref, cluster_index, FreeClusterMarker);
        update_fs_info_free_clusters(c, true);
        return true;
    }
    
    static BlockIndexType get_cluster_data_block_index (Context c, ClusterIndexType cluster_idx, ClusterBlockIndexType cluster_block_idx)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(is_cluster_idx_valid_for_data(c, cluster_idx))
        AMBRO_ASSERT(cluster_block_idx < o->blocks_per_cluster)
        
        return o->fat_end_blocks + ((BlockIndexType)(cluster_idx - 2) * o->blocks_per_cluster) + cluster_block_idx;
    }
    
    static BlockIndexType get_cluster_data_abs_block_index (Context c, ClusterIndexType cluster_idx, ClusterBlockIndexType cluster_block_idx)
    {
        return get_abs_block_index(c, get_cluster_data_block_index(c, cluster_idx, cluster_block_idx));
    }
    
    static BlockIndexType get_abs_block_index (Context c, BlockIndexType rel_block)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(rel_block < o->block_range.getLength())
        
        return o->block_range.getAbsBlockIndex(rel_block);
    }
    
    static uint32_t read_dir_entry_first_cluster (Context c, char const *entry_ptr)
    {
        return
            ((uint32_t)ReadBinaryInt<uint16_t, BinaryLittleEndian>(entry_ptr + 0x1A) << 0) |
            ((uint32_t)ReadBinaryInt<uint16_t, BinaryLittleEndian>(entry_ptr + 0x14) << 16);
    }
    
    static void write_dir_entry_first_cluster (Context c, uint32_t value, char *entry_ptr)
    {
        WriteBinaryInt<uint16_t, BinaryLittleEndian>(value >>  0, entry_ptr + 0x1A);
        WriteBinaryInt<uint16_t, BinaryLittleEndian>(value >> 16, entry_ptr + 0x14);
    }
    
    static bool get_fs_dirty_bit (Context c, CacheBlockRef *block_ref)
    {
        char const *status_bits_ptr = block_ref->getData(c, WrapBool<false>()) + EbpbStatusBitsOffset;
        uint8_t status_bits = ReadBinaryInt<uint8_t, BinaryLittleEndian>(status_bits_ptr);
        return (status_bits & StatusBitsDirty);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, update_fs_dirty_bit (Context c, CacheBlockRef *block_ref, bool set_else_clear))
    {
        char *status_bits_ptr = block_ref->getData(c, WrapBool<true>()) + EbpbStatusBitsOffset;
        uint8_t status_bits = ReadBinaryInt<uint8_t, BinaryLittleEndian>(status_bits_ptr);
        if (set_else_clear) {
            status_bits |= StatusBitsDirty;
        } else {
            status_bits &= ~StatusBitsDirty;
        }
        WriteBinaryInt<uint8_t, BinaryLittleEndian>(status_bits, status_bits_ptr);
        block_ref->markDirty(c);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, update_fs_info_allocated_cluster (Context c))
    {
        auto *o = Object::self(c);
        
        ClusterIndexType value = 2 + o->alloc_position;
        char *buffer = o->fs_info_block_ref.getData(c, WrapBool<true>());
        WriteBinaryInt<uint32_t, BinaryLittleEndian>(value, buffer + FsInfoAllocatedClusterOffset);
        o->fs_info_block_ref.markDirty(c);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, update_fs_info_free_clusters (Context c, bool inc_else_dec))
    {
        auto *o = Object::self(c);
        
        char *buffer = o->fs_info_block_ref.getData(c, WrapBool<true>());
        uint32_t free_clusters = ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + FsInfoFreeClustersOffset);
        if (free_clusters <= o->num_valid_clusters) {
            if (inc_else_dec) {
                free_clusters++;
            } else {
                free_clusters--;
            }
            WriteBinaryInt<uint32_t, BinaryLittleEndian>(free_clusters, buffer + FsInfoFreeClustersOffset);
            o->fs_info_block_ref.markDirty(c);
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, allocation_request_added (Context c))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->allocating_chains_list.isEmpty())
        AMBRO_ASSERT(o->write_mount_state == WriteMountState::MOUNTED)
        
        if (o->alloc_state == AllocationState::IDLE) {
            start_new_allocation(c);
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, allocation_request_removed (Context c))
    {
        auto *o = Object::self(c);
        if (o->alloc_state != AllocationState::IDLE && o->allocating_chains_list.isEmpty()) {
            o->alloc_state = AllocationState::IDLE;
            o->alloc_event.unset(c);
            o->write_block_ref.reset(c);
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, start_new_allocation (Context c))
    {
        auto *o = Object::self(c);
        o->alloc_state = AllocationState::CHECK_EVENT;
        o->alloc_start = o->alloc_position;
        o->alloc_event.prependNowNotAlready(c);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, complete_allocation (Context c, bool error, ClusterIndexType cluster_index=0))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->alloc_state != AllocationState::IDLE)
        AMBRO_ASSERT(!o->allocating_chains_list.isEmpty())
        
        ClusterChain<true> *complete_request = nullptr;
        bool have_more_requests = false;
        for (ClusterChain<true> *chain = o->allocating_chains_list.first(); chain; chain = o->allocating_chains_list.next(chain)) {
            AMBRO_ASSERT(chain->m_state == ClusterChain<true>::State::NEW_ALLOCATING)
            if (!complete_request) {
                complete_request = chain;
            } else {
                have_more_requests = true;
            }
        }
        AMBRO_ASSERT(complete_request)
        if (have_more_requests) {
            start_new_allocation(c);
        } else {
            o->alloc_state = AllocationState::IDLE;
            o->write_block_ref.reset(c);
        }
        complete_request->allocation_result(c, error, cluster_index);
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, alloc_event_handler (Context c))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->alloc_state == AllocationState::CHECK_EVENT)
        AMBRO_ASSERT(o->write_mount_state == WriteMountState::MOUNTED)
        
        while (true) {
            ClusterIndexType current_cluster = 2 + o->alloc_position;
            
            if (!request_fat_cache_block(c, &o->write_block_ref, current_cluster, false)) {
                o->alloc_state = AllocationState::REQUESTING_BLOCK;
                return;
            }
            
            o->alloc_position++;
            if (o->alloc_position == o->num_valid_clusters) {
                o->alloc_position = 0;
            }
            
            ClusterIndexType fat_value = read_fat_entry_in_cache_block(c, &o->write_block_ref, current_cluster);
            if (fat_value == FreeClusterMarker) {
                update_fat_entry_in_cache_block(c, &o->write_block_ref, current_cluster, EndOfChainMarker);
                update_fs_info_free_clusters(c, false);
                update_fs_info_allocated_cluster(c);
                return complete_allocation(c, false, current_cluster);
            }
            
            if (o->alloc_position == o->alloc_start) {
                return complete_allocation(c, true);
            }
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(FsWritable, static, void, alloc_block_ref_handler (Context c, bool error))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->alloc_state == AllocationState::REQUESTING_BLOCK)
        
        if (error) {
            return complete_allocation(c, true);
        }
        o->alloc_state = AllocationState::CHECK_EVENT;
        o->alloc_event.prependNowNotAlready(c);
    }
    
    APRINTER_STRUCT_IF_TEMPLATE(ClusterChainExtraMembers) {
        CacheBlockRef m_fat_cache_ref2;
        DoubleEndedListNode<ClusterChain<true>> m_allocating_chains_node;
        ClusterIndexType m_prev_cluster;
    };
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(FsWritable, static, void, set_fs_entry_extra (FsEntry *entry, BlockIndexType dir_entry_block_index, DirEntriesPerBlockType dir_entry_block_offset))
    {
        entry->dir_entry_block_index = dir_entry_block_index;
        entry->dir_entry_block_offset = dir_entry_block_offset;
    }
    
    template <bool Writable>
    class ClusterChain : public ClusterChainExtraMembers<Writable> {
        static_assert(!Writable || FsWritable, "");
        
        enum class State : uint8_t {
            IDLE,
            NEXT_CHECK, NEXT_REQUESTING_FAT,
            NEW_CHECK, NEW_REQUESTING_FAT, NEW_ALLOCATING,
            TRUNCATE_CHECK, TRUNCATE_REQUESTING_FAT, TRUNCATE_REQUESTING_FAT2
        };
        enum class IterState : uint8_t {START, CLUSTER, END};
        
    public:
        using ClusterChainHandler = Callback<void(Context c, bool error, bool first_cluster_changed)>;
        
        void init (Context c, ClusterIndexType first_cluster, ClusterChainHandler handler)
        {
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&ClusterChain::event_handler, this));
            m_fat_cache_ref1.init(c, APRINTER_CB_OBJFUNC_T(&ClusterChain::fat_cache_ref_handler, this));
            
            m_handler = handler;
            m_state = State::IDLE;
            m_first_cluster = first_cluster;
            
            extra_init(c);
            
            rewind_internal(c);
        }
        
        void deinit (Context c)
        {
            extra_deinit(c);
            m_fat_cache_ref1.deinit(c);
            m_event.deinit(c);
        }
        
        void rewind (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            
            rewind_internal(c);
        }
        
        void requestNext (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            
            m_state = State::NEXT_CHECK;
            m_event.prependNowNotAlready(c);
        }
        
        bool endReached (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            
            return m_iter_state == IterState::END;
        }
        
        ClusterIndexType getCurrentCluster (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(m_iter_state == IterState::CLUSTER)
            
            return m_current_cluster;
        }
        
        APRINTER_FUNCTION_IF(Writable, void, requestNew (Context c))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->write_mount_state == WriteMountState::MOUNTED)
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(m_iter_state == IterState::END)
            
            m_state = State::NEW_CHECK;
            m_event.prependNowNotAlready(c);
        }
        
        ClusterIndexType getFirstCluster (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            
            return m_first_cluster;
        }
        
        APRINTER_FUNCTION_IF(Writable, void, startTruncate (Context c))
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            
            m_state = State::TRUNCATE_CHECK;
            m_event.prependNowNotAlready(c);
        }
        
    private:
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, extra_init (Context c))
        {
            this->m_fat_cache_ref2.init(c, APRINTER_CB_OBJFUNC_T(&ClusterChain::fat_cache_ref_handler, this));
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, extra_deinit (Context c))
        {
            auto *o = Object::self(c);
            if (m_state == State::NEW_ALLOCATING) {
                o->allocating_chains_list.remove(this);
                allocation_request_removed(c);
            }
            this->m_fat_cache_ref2.deinit(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, extra_complete_request (Context c))
        {
            this->m_fat_cache_ref2.reset(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, handle_event_new_check (Context c))
        {
            auto *o = Object::self(c);
            if (is_cluster_idx_normal(this->m_prev_cluster)) {
                AMBRO_ASSERT(is_cluster_idx_valid_for_fat(c, this->m_prev_cluster))
                if (!request_fat_cache_block(c, &m_fat_cache_ref1, this->m_prev_cluster, false)) {
                    m_state = State::NEW_REQUESTING_FAT;
                    return;
                }
            }
            m_state = State::NEW_ALLOCATING;
            o->allocating_chains_list.append(this);
            allocation_request_added(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, handle_event_truncate_check (Context c))
        {
            if (!is_cluster_idx_normal(m_current_cluster)) {
                return complete_request(c, false);
            }
            AMBRO_ASSERT(m_iter_state != IterState::END)
            if (!is_cluster_idx_valid_for_fat(c, m_current_cluster)) {
                return complete_request(c, true);
            }
            if (!request_fat_cache_block(c, &m_fat_cache_ref1, m_current_cluster, false)) {
                m_state = State::TRUNCATE_REQUESTING_FAT;
                return;
            }
            ClusterIndexType next_cluster = read_fat_entry_in_cache_block(c, &m_fat_cache_ref1, m_current_cluster);
            if (!is_cluster_idx_normal(next_cluster)) {
                bool changing_first_cluster = false;
                if (m_iter_state == IterState::START) {
                    if (!release_cluster(c, &m_fat_cache_ref1, m_current_cluster)) {
                        return complete_request(c, true);
                    }
                    m_first_cluster = EmptyFileMarker;
                    m_current_cluster = m_first_cluster;
                    changing_first_cluster = true;
                }
                return complete_request(c, false, changing_first_cluster);
            }
            if (!is_cluster_idx_valid_for_fat(c, next_cluster)) {
                return complete_request(c, true);
            }
            if (!request_fat_cache_block(c, &this->m_fat_cache_ref2, next_cluster, false)) {
                m_state = State::TRUNCATE_REQUESTING_FAT2;
                return;
            }
            ClusterIndexType after_next_cluster;
            if (!release_cluster(c, &this->m_fat_cache_ref2, next_cluster, &after_next_cluster)) {
                return complete_request(c, true);
            }
            update_fat_entry_in_cache_block(c, &m_fat_cache_ref1, m_current_cluster, after_next_cluster);
            m_event.prependNowNotAlready(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, extra_set_prev_cluster (Context c, ClusterIndexType value))
        {
            this->m_prev_cluster = value;
        }
        
        void rewind_internal (Context c)
        {
            m_iter_state = IterState::START;
            m_current_cluster = m_first_cluster;
            extra_set_prev_cluster(c, 0);
        }
        
        void complete_request (Context c, bool error, bool first_cluster_changed=false)
        {
            m_state = State::IDLE;
            m_fat_cache_ref1.reset(c);
            extra_complete_request(c);
            return m_handler(c, error, first_cluster_changed);
        }
        
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            
            if (m_state == State::NEXT_CHECK) {
                if (m_iter_state == IterState::CLUSTER) {
                    if (!is_cluster_idx_valid_for_fat(c, m_current_cluster)) {
                        return complete_request(c, true);
                    }
                    if (!request_fat_cache_block(c, &m_fat_cache_ref1, m_current_cluster, false)) {
                        m_state = State::NEXT_REQUESTING_FAT;
                        return;
                    }
                    extra_set_prev_cluster(c, m_current_cluster);
                    m_current_cluster = read_fat_entry_in_cache_block(c, &m_fat_cache_ref1, m_current_cluster);
                }
                if (m_iter_state != IterState::END) {
                    m_iter_state = is_cluster_idx_normal(m_current_cluster) ? IterState::CLUSTER : IterState::END;
                }
                return complete_request(c, false);
            }
            else if (Writable && m_state == State::NEW_CHECK) {
                handle_event_new_check(c);
            }
            else if (Writable && m_state == State::TRUNCATE_CHECK) {
                handle_event_truncate_check(c);
            }
            else {
                AMBRO_ASSERT(false);
            }
        }
        
        void fat_cache_ref_handler (Context c, bool error)
        {
            TheDebugObject::access(c);
            
            State success_state;
            switch (m_state) {
                case State::NEXT_REQUESTING_FAT:      success_state = State::NEXT_CHECK;     break;
                case State::NEW_REQUESTING_FAT:       success_state = State::NEW_CHECK;      break;
                case State::TRUNCATE_REQUESTING_FAT:  success_state = State::TRUNCATE_CHECK; break;
                case State::TRUNCATE_REQUESTING_FAT2: success_state = State::TRUNCATE_CHECK; break;
                default: AMBRO_ASSERT(false);
            }
            if (error) {
                return complete_request(c, true);
            }
            m_state = success_state;
            m_event.prependNowNotAlready(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, allocation_result (Context c, bool error, ClusterIndexType new_cluster_index))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::NEW_ALLOCATING)
            AMBRO_ASSERT(error || is_cluster_idx_valid_for_fat(c, new_cluster_index))
            AMBRO_ASSERT(error || is_cluster_idx_normal(new_cluster_index))
            AMBRO_ASSERT(m_iter_state == IterState::END)
            AMBRO_ASSERT(!is_cluster_idx_normal(m_current_cluster))
            AMBRO_ASSERT(is_cluster_idx_normal(m_first_cluster) == is_cluster_idx_normal(this->m_prev_cluster))
            
            o->allocating_chains_list.remove(this);
            if (error) {
                return complete_request(c, error);
            }
            m_current_cluster = new_cluster_index;
            bool changing_first_cluster = !is_cluster_idx_normal(this->m_prev_cluster);
            if (changing_first_cluster) {
                m_first_cluster = m_current_cluster;
            } else {
                update_fat_entry_in_cache_block(c, &m_fat_cache_ref1, this->m_prev_cluster, m_current_cluster);
            }
            m_iter_state = IterState::CLUSTER;
            return complete_request(c, false, changing_first_cluster);
        }
        
        typename Context::EventLoop::QueuedEvent m_event;
        CacheBlockRef m_fat_cache_ref1;
        ClusterChainHandler m_handler;
        State m_state;
        IterState m_iter_state;
        ClusterIndexType m_first_cluster;
        ClusterIndexType m_current_cluster;
    };
    
    template <bool Writable>
    class DirEntryRef {
        static_assert(Writable && FsWritable, "");
        
        enum class State : uint8_t {INVALID, REQUESTING_BLOCK, READY};
        
    public:
        using DirEntryRefHandler = Callback<void(Context c, bool error)>;
        
        void init (Context c, DirEntryRefHandler handler)
        {
            m_block_ref.init(c, APRINTER_CB_OBJFUNC_T(&DirEntryRef::block_ref_handler, this));
            m_handler = handler;
            m_state = State::INVALID;
        }
        
        void deinit (Context c)
        {
            m_block_ref.deinit(c);
        }
        
        void reset (Context c)
        {
            m_block_ref.reset(c);
            m_state = State::INVALID;
        }
        
        void requestEntryRef (Context c, BlockIndexType block_index, DirEntriesPerBlockType block_offset)
        {
            AMBRO_ASSERT(m_state == State::INVALID)
            
            m_state = State::REQUESTING_BLOCK;
            m_block_offset = block_offset;
            m_block_ref.requestBlock(c, get_abs_block_index(c, block_index), 0, 1, CacheBlockRef::FLAG_NO_IMMEDIATE_COMPLETION);
        }
        
        ClusterIndexType getFirstCluster (Context c)
        {
            AMBRO_ASSERT(m_state == State::READY)
            
            return mask_cluster_entry(read_dir_entry_first_cluster(c, get_entry_ptr<false>(c)));
        }
        
        void setFirstCluster (Context c, ClusterIndexType value)
        {
            AMBRO_ASSERT(m_state == State::READY)
            
            char *buffer = get_entry_ptr<true>(c);
            uint32_t write_value = update_cluster_entry(read_dir_entry_first_cluster(c, buffer), value);
            write_dir_entry_first_cluster(c, write_value, buffer);
            m_block_ref.markDirty(c);
        }
        
        uint32_t getFileSize (Context c)
        {
            AMBRO_ASSERT(m_state == State::READY)
            
            return ReadBinaryInt<uint32_t, BinaryLittleEndian>(get_entry_ptr<false>(c) + DirEntrySizeOffset);
        }
        
        void setFileSize (Context c, uint32_t value)
        {
            AMBRO_ASSERT(m_state == State::READY)
            
            WriteBinaryInt<uint32_t, BinaryLittleEndian>(value, get_entry_ptr<true>(c) + DirEntrySizeOffset);
            m_block_ref.markDirty(c);
        }
        
    private:
        void block_ref_handler (Context c, bool error)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::REQUESTING_BLOCK)
            
            m_state = error ? State::INVALID : State::READY;
            return m_handler(c, error);
        }
        
        template <bool ForWriting>
        If<ForWriting, char, char const> * get_entry_ptr (Context c)
        {
            return m_block_ref.getData(c, WrapBool<ForWriting>()) + ((size_t)m_block_offset * 32);
        }
        
        CacheBlockRef m_block_ref;
        DirEntryRefHandler m_handler;
        State m_state;
        DirEntriesPerBlockType m_block_offset;
    };
    
    class DirectoryIterator {
        enum class State : uint8_t {WAIT_REQUEST, CHECK_NEXT_EVENT, REQUESTING_CLUSTER, REQUESTING_BLOCK};
        
    public:
        using DirectoryIteratorHandler = Callback<void(Context c, bool is_error, char const *name, FsEntry entry)>;
        
        void init (Context c, ClusterIndexType first_cluster, DirectoryIteratorHandler handler)
        {
            auto *o = Object::self(c);
            
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&DirectoryIterator::event_handler, this));
            m_chain.init(c, first_cluster, APRINTER_CB_OBJFUNC_T(&DirectoryIterator::chain_handler, this));
            m_dir_block_ref.init(c, APRINTER_CB_OBJFUNC_T(&DirectoryIterator::dir_block_ref_handler, this));
            
            m_handler = handler;
            m_state = State::WAIT_REQUEST;
            m_block_in_cluster = o->blocks_per_cluster;
            m_block_entry_pos = DirEntriesPerBlock;
            m_vfat_seq = -1;
        }
        
        void deinit (Context c)
        {
            m_dir_block_ref.deinit(c);
            m_chain.deinit(c);
            m_event.deinit(c);
        }
        
        void requestEntry (Context c)
        {
            AMBRO_ASSERT(m_state == State::WAIT_REQUEST)
            
            schedule_event(c);
        }
        
    private:
        void complete_request (Context c, bool error, char const *name=nullptr, FsEntry entry=FsEntry{})
        {
            m_state = State::WAIT_REQUEST;
            return m_handler(c, error, name, entry);
        }
        
        void schedule_event (Context c)
        {
            m_state = State::CHECK_NEXT_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::CHECK_NEXT_EVENT)
            
            if (m_block_entry_pos == DirEntriesPerBlock) {
                if (m_block_in_cluster == o->blocks_per_cluster) {
                    m_dir_block_ref.reset(c);
                    m_chain.requestNext(c);
                    m_state = State::REQUESTING_CLUSTER;
                    return;
                }
                
                if (!is_cluster_idx_valid_for_data(c, m_chain.getCurrentCluster(c))) {
                    return complete_request(c, true);
                }
                
                BlockIndexType abs_block_idx = get_cluster_data_abs_block_index(c, m_chain.getCurrentCluster(c), m_block_in_cluster);
                if (!m_dir_block_ref.requestBlock(c, abs_block_idx, 0, 1, 0)) {
                    m_state = State::REQUESTING_BLOCK;
                    return;
                }
                
                m_block_in_cluster++;
                m_block_entry_pos = 0;
            }
            
            char const *entry_ptr = m_dir_block_ref.getData(c, WrapBool<false>()) + ((size_t)m_block_entry_pos * 32);
            
            uint8_t first_byte =    ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0x0);
            uint8_t attrs =         ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0xB);
            uint8_t type_byte =     ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0xC);
            uint8_t checksum_byte = ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0xD);
            uint32_t file_size =    ReadBinaryInt<uint32_t, BinaryLittleEndian>(entry_ptr + DirEntrySizeOffset);
            
            if (first_byte == 0) {
                return complete_request(c, false);
            }
            
            m_block_entry_pos++;
            
            // VFAT entry
            if (first_byte != 0xE5 && attrs == 0xF && type_byte == 0 && file_size != 0) {
                int8_t entry_vfat_seq = first_byte & 0x1F;
                if ((first_byte & 0x60) == 0x40) {
                    // Start collection.
                    m_vfat_seq = entry_vfat_seq;
                    m_vfat_csum = checksum_byte;
                    m_filename_pos = Params::MaxFileNameSize;
                }
                
                if (entry_vfat_seq > 0 && m_vfat_seq != -1 && entry_vfat_seq == m_vfat_seq && checksum_byte == m_vfat_csum) {
                    // Collect entry.
                    char name_data[26];
                    memcpy(name_data + 0, entry_ptr + 0x1, 10);
                    memcpy(name_data + 10, entry_ptr + 0xE, 12);
                    memcpy(name_data + 22, entry_ptr + 0x1C, 4);
                    size_t chunk_len = 0;
                    for (size_t i = 0; i < sizeof(name_data); i += 2) {
                        uint16_t ch = ReadBinaryInt<uint16_t, BinaryLittleEndian>(name_data + i);
                        if (ch == 0) {
                            break;
                        }
                        char enc_buf[4];
                        int enc_len = Utf8EncodeChar(ch, enc_buf);
                        if (enc_len > m_filename_pos - chunk_len) {
                            goto cancel_vfat;
                        }
                        memcpy(m_filename + chunk_len, enc_buf, enc_len);
                        chunk_len += enc_len;
                    }
                    memmove(m_filename + (m_filename_pos - chunk_len), m_filename, chunk_len);
                    m_filename_pos -= chunk_len;
                    m_vfat_seq--;
                } else {
                cancel_vfat:
                    // Cancel any collection.
                    m_vfat_seq = -1;
                }
                
                // Go on reading directory entries.
                return schedule_event(c);
            }
            
            // Forget VFAT state but remember for use in this entry.
            int8_t cur_vfat_seq = m_vfat_seq;
            m_vfat_seq = -1;
            
            // Free marker.
            if (first_byte == 0xE5) {
                return schedule_event(c);
            }
            
            // Ignore: volume label or device.
            if ((attrs & 0x8) || (attrs & 0x40)) {
                return schedule_event(c);
            }
            
            bool is_dir = (attrs & 0x10);
            bool is_dot_entry = (first_byte == (uint8_t)'.');
            
            ClusterIndexType first_cluster = mask_cluster_entry(read_dir_entry_first_cluster(c, entry_ptr));
            
            if (is_dot_entry && first_cluster == 0) {
                first_cluster = o->root_cluster;
            }
            
            char const *filename;
            if (!is_dot_entry && cur_vfat_seq == 0 && vfat_checksum(entry_ptr) == m_vfat_csum) {
                filename = m_filename + m_filename_pos;
                m_filename[Params::MaxFileNameSize] = 0;
            } else {
                char name_temp[8];
                memcpy(name_temp, entry_ptr + 0, 8);
                if (name_temp[0] == 0x5) {
                    name_temp[0] = 0xE5;
                }
                size_t name_len = fixup_83_name(name_temp, 8, bool(type_byte & 0x8));
                
                char ext_temp[3];
                memcpy(ext_temp, entry_ptr + 8, 3);
                size_t ext_len = fixup_83_name(ext_temp, 3, bool(type_byte & 0x10));
                
                size_t filename_len = 0;
                memcpy(m_filename + filename_len, name_temp, name_len);
                filename_len += name_len;
                if (ext_len > 0) {
                    m_filename[filename_len++] = '.';
                    memcpy(m_filename + filename_len, ext_temp, ext_len);
                    filename_len += ext_len;
                }
                m_filename[filename_len] = '\0';
                filename = m_filename;
            }
            
            FsEntry entry;
            entry.type = is_dir ? EntryType::DIR_TYPE : EntryType::FILE_TYPE;
            entry.file_size = file_size;
            entry.cluster_index = first_cluster;
            set_fs_entry_extra(&entry,
                get_cluster_data_block_index(c, m_chain.getCurrentCluster(c), m_block_in_cluster - 1),
                m_block_entry_pos - 1);
            
            return complete_request(c, false, filename, entry);
        }
        
        void chain_handler (Context c, bool error, bool first_cluster_changed)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::REQUESTING_CLUSTER)
            
            if (error || m_chain.endReached(c)) {
                return complete_request(c, error);
            }
            m_block_in_cluster = 0;
            schedule_event(c);
        }
        
        void dir_block_ref_handler (Context c, bool error)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::REQUESTING_BLOCK)
            
            if (error) {
                return complete_request(c, error);
            }
            schedule_event(c);
        }
        
        static uint8_t vfat_checksum (char const *data)
        {
            uint8_t csum = 0;
            for (int i = 0; i < 11; i++) {
                csum = (uint8_t)((uint8_t)((csum & 1) << 7) + (csum >> 1)) + (uint8_t)data[i];
            }
            return csum;
        }
        
        static size_t fixup_83_name (char *data, size_t length, bool lowercase)
        {
            while (length > 0 && data[length - 1] == ' ') {
                length--;
            }
            if (lowercase) {
                for (size_t i = 0; i < length; i++) {
                    if (data[i] >= 'A' && data[i] <= 'Z') {
                        data[i] += 32;
                    }
                }
            }
            return length;
        }
        
        typename Context::EventLoop::QueuedEvent m_event;
        ClusterChain<false> m_chain;
        CacheBlockRef m_dir_block_ref;
        DirectoryIteratorHandler m_handler;
        ClusterBlockIndexType m_block_in_cluster;
        DirEntriesPerBlockType m_block_entry_pos;
        State m_state;
        int8_t m_vfat_seq;
        uint8_t m_vfat_csum;
        FileNameLenType m_filename_pos;
        char m_filename[Params::MaxFileNameSize + 1];
    };
    
    template <bool Writable>
    class WriteReference {
        static_assert(Writable && FsWritable, "");
        
    public:
        void init (Context c)
        {
            m_taken = false;
        }
        
        void deinit (Context c)
        {
            release(c);
        }
        
        bool isTaken (Context c)
        {
            return m_taken;
        }
        
        bool take (Context c)
        {
            auto *o = Object::self(c);
            if (!m_taken) {
                if (o->write_mount_state != WriteMountState::MOUNTED) {
                    return false;
                }
                o->num_write_references++;
                m_taken = true;
            }
            return true;
        }
        
        void release (Context c)
        {
            auto *o = Object::self(c);
            if (m_taken) {
                AMBRO_ASSERT(o->num_write_references > 0)
                o->num_write_references--;
                m_taken = false;
            }
        }
        
    private:
        bool m_taken;
    };
    
    APRINTER_STRUCT_IF_TEMPLATE(FsWritableMembers) {
        typename Context::EventLoop::QueuedEvent alloc_event;
        CacheBlockRef write_block_ref;
        CacheBlockRef fs_info_block_ref;
        CacheFlushRequest flush_request;
        WriteMountState write_mount_state;
        AllocationState alloc_state;
        BlockIndexType fs_info_block;
        DoubleEndedListForBase<ClusterChain<true>, ClusterChainExtraMembers<true>, &ClusterChain<true>::m_allocating_chains_node> allocating_chains_list;
        ClusterIndexType alloc_position;
        ClusterIndexType alloc_start;
        size_t num_write_references;
    };
    
public:
    struct Object : public ObjBase<FatFs, ParentObject, MakeTypeList<
        TheDebugObject,
        TheBlockCache
    >>, public FsWritableMembers<FsWritable> {
        BlockRange<BlockIndexType> block_range;
        FsState state;
        union {
            CacheBlockRef init_block_ref;
            struct {
                uint8_t num_fats;
                ClusterBlockIndexType blocks_per_cluster;
                ClusterIndexType root_cluster;
                ClusterIndexType num_fat_entries;
                BlockIndexType fat_end_blocks;
                ClusterIndexType num_valid_clusters;
            };
        };
    };
};

APRINTER_ALIAS_STRUCT_EXT(FatFsService, (
    APRINTER_AS_VALUE(int, MaxFileNameSize),
    APRINTER_AS_VALUE(int, NumCacheEntries),
    APRINTER_AS_VALUE(bool, CaseInsens),
    APRINTER_AS_VALUE(bool, Writable)
), (
    template <typename Context, typename ParentObject, typename TheBlockAccess, typename InitHandler, typename WriteMountHandler>
    using Fs = FatFs<Context, ParentObject, TheBlockAccess, InitHandler, WriteMountHandler, FatFsService>;
))

#include <aprinter/EndNamespace.h>

#endif
