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

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/misc/Utf8Encoder.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/fs/BlockCache.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename TheBlockAccess, typename InitHandler, typename Params>
class FatFs {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using BlockAccessUser = typename TheBlockAccess::User;
    using BlockIndexType = typename TheBlockAccess::BlockIndexType;
    static size_t const BlockSize = TheBlockAccess::BlockSize;
    static_assert(BlockSize >= 0x47, "BlockSize not enough for EBPB");
    static_assert(BlockSize % 32 == 0, "BlockSize not a multiple of 32");
    using SectorIndexType = BlockIndexType;
    using ClusterIndexType = uint32_t;
    using ClusterBlockIndexType = uint16_t;
    static size_t const FatEntriesPerBlock = BlockSize / 4;
    static size_t const DirEntriesPerBlock = BlockSize / 32;
    using DirEntriesPerBlockType = ChooseIntForMax<DirEntriesPerBlock, false>;
    static_assert(Params::MaxFileNameSize >= 12, "");
    using FileNameLenType = ChooseIntForMax<Params::MaxFileNameSize, false>;
    static_assert(Params::NumCacheEntries >= 2, "");
    using TheBlockCache = BlockCache<Context, Object, TheBlockAccess, Params::NumCacheEntries>;
    using CacheBlockRef = typename TheBlockCache::CacheRef;
    
    enum {FS_STATE_INIT, FS_STATE_READY, FS_STATE_FAILED};
    
    class BaseReader;
    class ClusterChain;
    
public:
    enum EntryType {ENTRYTYPE_DIR, ENTRYTYPE_FILE};
    
    class FsEntry {
    public:
        inline EntryType getType () const { return (EntryType)type; }
        inline uint32_t getFileSize () const { return file_size; }
        
    private:
        friend FatFs;
        
        uint8_t type;
        uint32_t file_size;
        ClusterIndexType cluster_index;
    };
    
    struct SharedBuffer {
        char buffer[BlockSize];
    };
    
    static bool isPartitionTypeSupported (uint8_t type)
    {
        return (type == 0xB || type == 0xC);
    }
    
    static void init (Context c, SharedBuffer *init_buffer, typename TheBlockAccess::BlockRange block_range)
    {
        auto *o = Object::self(c);
        
        o->block_range = block_range;
        
        o->state = FS_STATE_INIT;
        o->u.init.block_user.init(c, APRINTER_CB_STATFUNC_T(&FatFs::init_block_read_handler));
        o->u.init.block_user.startRead(c, get_abs_block_index(c, 0), WrapBuffer::Make(init_buffer->buffer));
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        if (o->state == FS_STATE_INIT) {
            o->u.init.block_user.deinit(c);
        } else if (o->state == FS_STATE_READY) {
            TheBlockCache::deinit(c);
        }
    }
    
    static FsEntry getRootEntry (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FS_STATE_READY)
        
        FsEntry entry;
        entry.type = ENTRYTYPE_DIR;
        entry.file_size = 0;
        entry.cluster_index = o->u.fs.root_cluster;
        return entry;
    }
    
    class DirLister {
    private:
        enum {DIRLISTER_STATE_WAITREQ, DIRLISTER_STATE_READING, DIRLISTER_STATE_EVENT};
        
    public:
        using DirListerHandler = Callback<void(Context c, bool is_error, char const *name, FsEntry entry)>;
        
        void init (Context c, FsEntry dir_entry, SharedBuffer *buffer, DirListerHandler handler)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(dir_entry.type == ENTRYTYPE_DIR)
            
            m_buffer = buffer;
            m_handler = handler;
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&DirLister::event_handler, this));
            m_reader.init(c, dir_entry.cluster_index, APRINTER_CB_OBJFUNC_T(&DirLister::reader_handler, this));
            m_state = DIRLISTER_STATE_WAITREQ;
            m_block_entry_pos = DirEntriesPerBlock;
            m_vfat_seq = -1;
        }
        
        // WARNING: Only allowed together with deiniting the whole FatFs and underlying storage!
        void deinit (Context c)
        {
            TheDebugObject::access(c);
            
            m_reader.deinit(c);
            m_event.deinit(c);
        }
        
        void requestEntry (Context c)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == DIRLISTER_STATE_WAITREQ)
            
            next_entry(c);
        }
        
    private:
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == DIRLISTER_STATE_EVENT)
            AMBRO_ASSERT(m_block_entry_pos < DirEntriesPerBlock)
            
            char const *entry_ptr = m_buffer->buffer + ((size_t)m_block_entry_pos * 32);
            uint8_t first_byte =    ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0x0);
            uint8_t attrs =         ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0xB);
            uint8_t type_byte =     ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0xC);
            uint8_t checksum_byte = ReadBinaryInt<uint8_t, BinaryLittleEndian>(entry_ptr + 0xD);
            
            if (first_byte == 0) {
                m_state = DIRLISTER_STATE_WAITREQ;
                return m_handler(c, false, nullptr, FsEntry());
            }
            
            m_block_entry_pos++;
            
            // VFAT entry
            if (first_byte != 0xE5 && attrs == 0xF && type_byte == 0) {
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
                return next_entry(c);
            }
            
            // Forget VFAT state but remember for use in this entry.
            int8_t cur_vfat_seq = m_vfat_seq;
            m_vfat_seq = -1;
            
            // Free marker.
            if (first_byte == 0xE5) {
                return next_entry(c);
            }
            
            // Ignore: volume label or device.
            if ((attrs & 0x8) || (attrs & 0x40)) {
                return next_entry(c);
            }
            
            bool is_dir = (attrs & 0x10);
            bool is_dot_entry = (first_byte == (uint8_t)'.');
            uint32_t file_size = ReadBinaryInt<uint32_t, BinaryLittleEndian>(entry_ptr + 0x1C);
            
            ClusterIndexType first_cluster = mask_cluster_entry(
                ReadBinaryInt<uint16_t, BinaryLittleEndian>(entry_ptr + 0x1A) |
                ((uint32_t)ReadBinaryInt<uint16_t, BinaryLittleEndian>(entry_ptr + 0x14) << 16));
            
            if (is_dot_entry && first_cluster == 0) {
                first_cluster = o->u.fs.root_cluster;
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
            entry.type = is_dir ? ENTRYTYPE_DIR : ENTRYTYPE_FILE;
            entry.file_size = file_size;
            entry.cluster_index = first_cluster;
            
            m_state = DIRLISTER_STATE_WAITREQ;
            return m_handler(c, false, filename, entry);
        }
        
        void reader_handler (Context c, uint8_t status)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == DIRLISTER_STATE_READING)
            
            if (status != BaseReader::BASEREAD_STATUS_OK) {
                bool is_error = (status != BaseReader::BASEREAD_STATUS_EOF);
                m_state = DIRLISTER_STATE_WAITREQ;
                return m_handler(c, is_error, nullptr, FsEntry());
            }
            
            m_block_entry_pos = 0;
            m_event.appendNowNotAlready(c);
            m_state = DIRLISTER_STATE_EVENT;
        }
        
        void next_entry (Context c)
        {
            if (m_block_entry_pos == DirEntriesPerBlock) {
                m_reader.requestBlock(c, WrapBuffer::Make(m_buffer->buffer));
                m_state = DIRLISTER_STATE_READING;
            } else {
                m_event.appendNowNotAlready(c);
                m_state = DIRLISTER_STATE_EVENT;
            }
        }
        
        SharedBuffer *m_buffer;
        DirListerHandler m_handler;
        typename Context::EventLoop::QueuedEvent m_event;
        BaseReader m_reader;
        uint8_t m_state;
        DirEntriesPerBlockType m_block_entry_pos;
        int8_t m_vfat_seq;
        uint8_t m_vfat_csum;
        FileNameLenType m_filename_pos;
        char m_filename[Params::MaxFileNameSize + 1];
    };
    
    class FileReader {
    public:
        using FileReaderHandler = Callback<void(Context c, bool is_error, size_t length)>;
        
        void init (Context c, FsEntry file_entry, FileReaderHandler handler)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(file_entry.type == ENTRYTYPE_FILE)
            
            m_handler = handler;
            m_first_cluster = file_entry.cluster_index;
            m_file_size = file_entry.file_size;
            init_reader(c);
        }
        
        // WARNING: Only allowed together with deiniting the whole FatFs and underlying storage!
        void deinit (Context c)
        {
            TheDebugObject::access(c);
            
            m_reader.deinit(c);
        }
        
        void rewind (Context c)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_reader.isIdle(c))
            
            m_reader.deinit(c);
            init_reader(c);
        }
        
        void requestBlock (Context c, WrapBuffer buf)
        {
            TheDebugObject::access(c);
            
            m_reader.requestBlock(c, buf);
        }
        
    private:
        void init_reader (Context c)
        {
            m_reader.init(c, m_first_cluster, APRINTER_CB_OBJFUNC_T(&FileReader::reader_handler, this));
            m_rem_file_size = m_file_size;
        }
        
        void reader_handler (Context c, uint8_t status)
        {
            TheDebugObject::access(c);
            
            bool is_error = true;
            size_t read_length = 0;
            
            do {
                if (status != BaseReader::BASEREAD_STATUS_OK) {
                    is_error = (status != BaseReader::BASEREAD_STATUS_EOF || m_rem_file_size > 0);
                    break;
                }
                
                is_error = false;
                read_length = MinValue((uint32_t)BlockSize, m_rem_file_size);
                m_rem_file_size -= read_length;
            } while (0);
            
            return m_handler(c, is_error, read_length);
        }
        
        FileReaderHandler m_handler;
        ClusterIndexType m_first_cluster;
        uint32_t m_file_size;
        BaseReader m_reader;
        uint32_t m_rem_file_size;
    };
    
private:
    static void init_block_read_handler (Context c, bool read_error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FS_STATE_INIT)
        
        char *buffer = o->u.init.block_user.getBuffer(c).ptr1;
        o->u.init.block_user.deinit(c);
        
        uint8_t error_code = 99;
        do {
            if (read_error) {
                error_code = 20;
                goto error;
            }
            
            uint16_t sector_size =          ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0xB);
            uint8_t sectors_per_cluster =   ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0xD);
            uint16_t num_reserved_sectors = ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0xE);
            o->u.fs.num_fats =              ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0x10);
            uint16_t max_root =             ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0x11);
            uint32_t sectors_per_fat =      ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + 0x24);
            uint32_t root_cluster =         ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + 0x2C);
            uint8_t sig =                   ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0x42);
            
            if (sector_size == 0 || sector_size % BlockSize != 0) {
                error_code = 22;
                goto error;
            }
            uint16_t blocks_per_sector = sector_size / BlockSize;
            
            if (sectors_per_cluster > UINT16_MAX / blocks_per_sector) {
                error_code = 23;
                goto error;
            }
            o->u.fs.blocks_per_cluster = blocks_per_sector * sectors_per_cluster;
            
            if ((uint32_t)num_reserved_sectors * sector_size < 0x47) {
                error_code = 24;
                goto error;
            }
            
            if (o->u.fs.num_fats != 1 && o->u.fs.num_fats != 2) {
                error_code = 25;
                goto error;
            }
            
            if (sig != 0x28 && sig != 0x29) {
                error_code = 26;
                goto error;
            }
            
            if (max_root != 0) {
                error_code = 27;
                goto error;
            }
            
            o->u.fs.root_cluster = mask_cluster_entry(root_cluster);
            if (o->u.fs.root_cluster < 2) {
                error_code = 28;
                goto error;
            }
            
            uint16_t entries_per_sector = sector_size / 4;
            if (sectors_per_fat == 0 || sectors_per_fat > UINT32_MAX / entries_per_sector) {
                error_code = 29;
                goto error;
            }
            o->u.fs.num_fat_entries = (ClusterIndexType)sectors_per_fat * entries_per_sector;
            
            uint64_t fat_end_sectors_calc = (uint64_t)num_reserved_sectors + (uint64_t)o->u.fs.num_fats * sectors_per_fat;
            if (fat_end_sectors_calc > o->block_range.getLength() / blocks_per_sector) {
                error_code = 29;
                goto error;
            }
            o->u.fs.num_reserved_blocks = (BlockIndexType)num_reserved_sectors * blocks_per_sector;
            o->u.fs.fat_end_blocks = fat_end_sectors_calc * blocks_per_sector;
            
            TheBlockCache::init(c);
            
#if 0
            o->u.fs.allocator_list.init(c);
            o->u.fs.allocator_position = 0;
#endif
            
            error_code = 0;
        } while (0);
        
    error:
        o->state = error_code ? FS_STATE_FAILED : FS_STATE_READY;
        return InitHandler::call(c, error_code);
    }
    
    static ClusterIndexType mask_cluster_entry (uint32_t entry_value)
    {
        return (entry_value & UINT32_C(0x0FFFFFFF));
    }
    
    static uint32_t update_cluster_entry (uint32_t entry_value, ClusterIndexType new_value)
    {
        return (entry_value & UINT32_C(0xF0000000)) | new_value;
    }
    
    static bool is_cluster_idx_valid (ClusterIndexType cluster_idx)
    {
        return (cluster_idx >= 2 && cluster_idx < UINT32_C(0xFFFFFF8));
    }
    
    static BlockIndexType get_abs_block_index (Context c, BlockIndexType rel_block)
    {
        auto *o = Object::self(c);
        return o->block_range.start_block + rel_block;
    }
    
    static BlockIndexType num_blocks_per_fat (Context c)
    {
        auto *o = Object::self(c);
        return o->u.fs.num_fat_entries / FatEntriesPerBlock;
    }
    
    static bool get_cluster_block_idx (Context c, ClusterIndexType cluster_idx, ClusterBlockIndexType cluster_block_idx, BlockIndexType *out_block_idx)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(is_cluster_idx_valid(cluster_idx))
        AMBRO_ASSERT(cluster_block_idx < o->u.fs.blocks_per_cluster)
        
        uint64_t blocks_after_fat_end = (uint64_t)(cluster_idx - 2) * o->u.fs.blocks_per_cluster + cluster_block_idx;
        if (blocks_after_fat_end >= o->block_range.getLength() - o->u.fs.fat_end_blocks) {
            return false;
        }
        *out_block_idx = o->u.fs.fat_end_blocks + blocks_after_fat_end;
        return true;
    }
    
    static bool get_fat_entry_block_idx (Context c, ClusterIndexType cluster_idx, BlockIndexType *out_block_idx, size_t *out_block_offset)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(is_cluster_idx_valid(cluster_idx))
        
        if (cluster_idx >= o->u.fs.num_fat_entries) {
            return false;
        }
        *out_block_idx = o->u.fs.num_reserved_blocks + (cluster_idx / FatEntriesPerBlock);
        *out_block_offset = (size_t)4 * (cluster_idx % FatEntriesPerBlock);
        return true;
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
    
    class BaseReader {
    public:
        enum {BASEREAD_STATUS_ERR, BASEREAD_STATUS_EOF, BASEREAD_STATUS_OK};
        
        using BaseReadHandler = Callback<void(Context c, uint8_t status)>;
        
        void init (Context c, ClusterIndexType first_cluster, BaseReadHandler handler)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(o->state == FS_STATE_READY)
            
            m_handler = handler;
            m_state = State::IDLE;
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&BaseReader::event_handler, this));
            m_chain.init(c, first_cluster, APRINTER_CB_OBJFUNC_T(&BaseReader::chain_handler, this));
            m_block_user.init(c, APRINTER_CB_OBJFUNC_T(&BaseReader::read_handler, this));
            m_block_in_cluster = o->u.fs.blocks_per_cluster;
        }
        
        // WARNING: Only allowed together with deiniting the whole FatFs and underlying storage or when not reading!
        void deinit (Context c)
        {
            m_block_user.deinit(c);
            m_chain.deinit(c);
            m_event.deinit(c);
        }
        
        void requestBlock (Context c, WrapBuffer buf)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            
            m_state = State::CHECK_EVENT;
            m_req_buf = buf;
            m_event.appendNowNotAlready(c);
        }
        
        bool isIdle (Context c)
        {
            return (m_state == State::IDLE);
        }
        
    private:
        enum class State : uint8_t {IDLE, CHECK_EVENT, NEXT_CLUSTER, READING_DATA};
        
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::CHECK_EVENT)
            
            if (m_block_in_cluster == o->u.fs.blocks_per_cluster) {
                m_chain.requestNext(c);
                m_state = State::NEXT_CLUSTER;
                return;
            }
            
            BlockIndexType block_idx;
            if (!get_cluster_block_idx(c, m_chain.getCurrentCluster(c), m_block_in_cluster, &block_idx)) {
                return complete_request(c, BASEREAD_STATUS_ERR);
            }
            
            m_block_user.startRead(c, get_abs_block_index(c, block_idx), m_req_buf);
            m_state = State::READING_DATA;
        }
        
        void chain_handler (Context c, bool error)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::NEXT_CLUSTER)
            AMBRO_ASSERT(m_block_in_cluster == o->u.fs.blocks_per_cluster)
             
            if (error || m_chain.endReached(c)) {
                return complete_request(c, error ? BASEREAD_STATUS_ERR : BASEREAD_STATUS_EOF);
            }
            
            m_block_in_cluster = 0;
            m_state = State::CHECK_EVENT;
            m_event.appendNowNotAlready(c);
        }
        
        void read_handler (Context c, bool is_read_error)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::READING_DATA)
            AMBRO_ASSERT(m_block_in_cluster < o->u.fs.blocks_per_cluster)
            
            if (is_read_error) {
                return complete_request(c, BASEREAD_STATUS_ERR);
            }
            
            m_block_in_cluster++;
            
            return complete_request(c, BASEREAD_STATUS_OK);
        }
        
        void complete_request (Context c, uint8_t status)
        {
            m_state = State::IDLE;
            
            return m_handler(c, status);
        }
        
        BaseReadHandler m_handler;
        State m_state;
        typename Context::EventLoop::QueuedEvent m_event;
        ClusterChain m_chain;
        BlockAccessUser m_block_user;
        ClusterBlockIndexType m_block_in_cluster;
        WrapBuffer m_req_buf;
    };
    
    class ClusterChain {
    public:
        using ClusterChainHandler = Callback<void(Context c, bool error)>;
        
        void init (Context c, ClusterIndexType first_cluster, ClusterChainHandler handler)
        {
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&ClusterChain::event_handler, this));
            m_fat_cache_ref.init(c, APRINTER_CB_OBJFUNC_T(&ClusterChain::fat_cache_ref_handler, this));
            
            m_handler = handler;
            m_state = State::IDLE;
            m_first_cluster = first_cluster;
            
            rewind_internal(c);
        }
        
        void deinit (Context c)
        {
            m_fat_cache_ref.deinit(c);
            m_event.deinit(c);
        }
        
        ClusterIndexType getFirstCluster (Context c)
        {
            return m_first_cluster;
        }
        
        void rewind (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            
            rewind_internal(c);
        }
        
        void requestNext (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(m_iter_state != IterState::END)
            
            m_state = State::REQUEST_NEXT_CHECK;
            m_event.prependNowNotAlready(c);
        }
        
        bool endReached (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            
            return (m_iter_state == IterState::END);
        }
        
        ClusterIndexType getCurrentCluster (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(m_iter_state == IterState::CLUSTER)
            
            return m_current_cluster;
        }
        
    private:
        enum class State : uint8_t {IDLE, REQUEST_NEXT_CHECK, READING_FAT_FOR_NEXT};
        
        enum class IterState : uint8_t {START, CLUSTER, END};
        
        void rewind_internal (Context c)
        {
            m_iter_state = IterState::START;
            m_current_cluster = m_first_cluster;
            m_prev_cluster = 0;
        }
        
        void complete_request (Context c, bool error)
        {
            m_state = State::IDLE;
            return m_handler(c, error);
        }
        
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            
            switch (m_state) {
                case State::REQUEST_NEXT_CHECK: {
                    AMBRO_ASSERT(m_iter_state != IterState::END)
                    
                    if (m_iter_state != IterState::START) {
                        BlockIndexType fat_block_idx;
                        size_t fat_block_offset;
                        if (!get_fat_entry_block_idx(c, m_current_cluster, &fat_block_idx, &fat_block_offset)) {
                            return complete_request(c, true);
                        }
                        
                        if (!m_fat_cache_ref.isThisBlockSelected(c, fat_block_idx)) {
                            m_state = State::READING_FAT_FOR_NEXT;
                            m_fat_cache_ref.requestBlock(c, fat_block_idx, num_blocks_per_fat(c), o->u.fs.num_fats);
                            return;
                        }
                        
                        m_prev_cluster = m_current_cluster;
                        m_current_cluster = mask_cluster_entry(ReadBinaryInt<uint32_t, BinaryLittleEndian>(m_fat_cache_ref.getData(c) + fat_block_offset));
                    }
                    
                    if (is_cluster_idx_valid(m_current_cluster)) {
                        m_iter_state = IterState::CLUSTER;
                    } else {
                        m_iter_state = IterState::END;
                        m_fat_cache_ref.reset(c);
                    }
                    
                    return complete_request(c, false);
                } break;
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        void fat_cache_ref_handler (Context c, bool error)
        {
            switch (m_state) {
                case State::READING_FAT_FOR_NEXT: {
                    if (error) {
                        return complete_request(c, true);
                    }
                    
                    m_state = State::REQUEST_NEXT_CHECK;
                    m_event.prependNowNotAlready(c);
                } break;
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        typename Context::EventLoop::QueuedEvent m_event;
        CacheBlockRef m_fat_cache_ref;
        ClusterChainHandler m_handler;
        State m_state;
        IterState m_iter_state;
        ClusterIndexType m_first_cluster;
        ClusterIndexType m_current_cluster;
        ClusterIndexType m_prev_cluster;
    };
    
#if 0
    class ClusterAllocator {
    public:
        using ClusterAllocatorHandler = Callback<void(Context c, bool error)>;
        
        void init (Context c, ClusterAllocatorHandler handler)
        {
            auto *o = Object::self(c);
            
            m_handler = handler;
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&ClusterAllocator::event_handler, this));
            m_fat_cache_ref.init(c, APRINTER_CB_OBJFUNC_T(&ClusterAllocator::fat_cache_ref_handler, this));
            m_state = State::INVALID;
        }
        
        void deinit (Context c)
        {
            auto *o = Object::self(c);
            
            release_locking(c);
            m_fat_cache_ref.deinit(c);
            m_event.deinit(c);
        }
        
        void reset (Context c)
        {
            release_locking(c);
            m_fat_cache_ref.reset(c);
            m_event.unset(c);
            m_state = State::INVALID;
        }
        
        void requestAllocation (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::INVALID)
            
            bool was_empty = o->u.fs.allocator_list.isEmpty(c);
            o->u.fs.allocator_list.append(this);
            
            if (was_empty) {
                locking_completed(c);
            } else {
                m_state = State::QUEUED;
            }
        }
        
        ClusterIndexType getAllocatedCluster (Context c)
        {
            AMBRO_ASSERT(m_state == State::ALLOCATED)
            
            // TBD
        }
        
        void setNextCluster (Context c, ClusterIndexType next_cluster)
        {
            AMBRO_ASSERT(m_state == State::ALLOCATED)
            
            // TBD
            
            
        }
        
    private:
        enum class State : uint8_t {INVALID, QUEUED, ALLOCATING, ALLOCATED};
        enum class AllocatingState : uint8_t {CHECK_EVENT, REQUESTING_BLOCK};
        
        void locking_completed (Context c)
        {
            auto *o = Object::self(c);
            
            o->u.fs.allocator_count = 0;
            m_state = State::ALLOCATING;
            m_allocating_state = AllocatingState::CHECK_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        void release_locking (Context c)
        {
            auto *o = Object::self(c);
            
            if (m_state == State::QUEUED || m_state == State::ALLOCATING) {
                o->u.fs.allocator_list.remove(this);
            }
            
            if (m_state == State::ALLOCATING) {
                ClusterAllocator *allocator = o->u.fs.allocator_list.first();
                AMBRO_ASSERT(allocator->m_state == State::QUEUED)
                allocator->locking_completed(c);
            }
        }
        
        void complete_request (Context c, State new_state, bool error)
        {
            release_locking(c);
            m_state = new_state;
            return m_handler(c, error);
        }
        
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            
            switch (m_state) {
                case State::ALLOCATING: {
                    AMBRO_ASSERT(m_allocating_state == AllocatingState::CHECK_EVENT)
                    
                    if (o->u.fs.allocator_count == o->u.fs.num_fat_entries) {
                        return complete_request(c, State::INVALID, true);
                    }
                    
                    BlockIndexType fat_block_idx;
                    size_t fat_block_offset;
                    if (!get_fat_entry_block_idx(c, o->u.fs.allocator_position, &fat_block_idx, &fat_block_offset)) {
                        return complete_request(c, State::INVALID, true);
                    }
                    
                    if (!m_fat_cache_ref.isThisBlockSelected(c, fat_block_idx)) {
                        m_fat_cache_ref.requestBlock(c, fat_block_idx);
                        m_allocating_state = AllocatingState::REQUESTING_BLOCK;
                        return;
                    }
                    
                    ClusterIndexType current_cluster = o->u.fs.allocator_position;
                    o->u.fs.allocator_position = (o->u.fs.allocator_position == o->u.fs.num_fat_entries - 1) ? 0 : (o->u.fs.allocator_position + 1);
                    o->u.fs.allocator_count++;
                    
                    char *entry_ptr = m_fat_cache_ref.getData(c) + fat_block_offset;
                    uint32_t entry_value = ReadBinaryInt<uint32_t, BinaryLittleEndian>(entry_ptr);
                    ClusterIndexType entry_index = mask_cluster_entry(entry_value);
                    
                    if (entry_index == 0) {
                        uint32_t new_entry_value = update_cluster_entry(entry_value, UINT32_C(0x0FFFFFFF));
                        WriteBinaryInt<uint32_t, BinaryLittleEndian>(new_entry_value, entry_ptr);
                        m_fat_cache_ref.markDirty(c);
                        m_allocated_cluster = current_cluster;
                        
                        return complete_request(c, State::ALLOCATED);
                    }
                    
                    
                } break;
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        void fat_cache_ref_handler (Context c, bool error)
        {
            switch (m_state) {
                case State::ALLOCATING: {
                    AMBRO_ASSERT(m_allocating_state == AllocatingState::REQUESTING_BLOCK)
                    
                    if (error) {
                        return complete_request(c, State::INVALID, true);
                    }
                    
                    m_allocating_state = AllocatingState::CHECK_EVENT;
                    m_event.prependNowNotAlready(c);
                } break;
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        ClusterAllocatorHandler m_handler;
        typename Context::EventLoop::QueuedEvent m_event;
        CacheRef m_fat_cache_ref;
        State m_state;
        AllocatingState m_allocating_state;
        ClusterIndexType m_allocated_cluster;
        DoubleEndedListNode<ClusterAllocator> m_allocators_list_node;
    };
#endif
    
public:
    struct Object : public ObjBase<FatFs, ParentObject, MakeTypeList<
        TheDebugObject,
        TheBlockCache
    >> {
        typename TheBlockAccess::BlockRange block_range;
        uint8_t state;
        union {
            struct {
                BlockAccessUser block_user;
            } init;
            struct {
                uint8_t num_fats;
                ClusterIndexType root_cluster;
                ClusterBlockIndexType blocks_per_cluster;
                ClusterIndexType num_fat_entries;
                BlockIndexType num_reserved_blocks;
                BlockIndexType fat_end_blocks;
#if 0
                DoubleEndedList<ClusterAllocator, &ClusterAllocator::m_allocators_list_node> allocator_list;
                ClusterIndexType allocator_position;
                ClusterIndexType allocator_count;
#endif
            } fs;
        } u;
    };
};

template <
    int TMaxFileNameSize,
    int TNumCacheEntries
>
struct FatFsService {
    static int const MaxFileNameSize = TMaxFileNameSize;
    static int const NumCacheEntries = TNumCacheEntries;
    
    template <typename Context, typename ParentObject, typename TheBlockAccess, typename InitHandler>
    using Fs = FatFs<Context, ParentObject, TheBlockAccess, InitHandler, FatFsService>;
};

#include <aprinter/EndNamespace.h>

#endif
