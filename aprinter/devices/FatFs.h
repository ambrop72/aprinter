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
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/misc/Utf8Encoder.h>

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
    using SectorIndexType = uint64_t;
    using ClusterIndexType = uint32_t;
    using ClusterBlockIndexType = uint16_t;
    static size_t const FatEntriesPerBlock = BlockSize / 4;
    static size_t const DirEntriesPerBlock = BlockSize / 32;
    static_assert(Params::MaxFileNameSize >= 12, "");
    enum {FS_STATE_INIT, FS_STATE_READY, FS_STATE_FAILED};
    class BaseReader;
    
public:
    enum EntryType {ENTRYTYPE_DIR, ENTRYTYPE_FILE};
    
    class FsEntry {
    public:
        inline EntryType getType () const { return type; }
        inline uint32_t getFileSize () const { return file_size; }
        
    private:
        friend FatFs;
        
        EntryType type;
        uint32_t file_size;
        ClusterIndexType cluster_index;
    };
    
    struct SharedBuffer {
        char buffer[BlockSize];
    };
    
    static void init (Context c, SharedBuffer *init_buffer)
    {
        auto *o = Object::self(c);
        
        o->init_buffer = init_buffer;
        o->init_block_user.init(c, APRINTER_CB_STATFUNC_T(&FatFs::init_block_read_handler));
        o->state = FS_STATE_INIT;
        o->init_block_user.startRead(c, 0, WrapBuffer(o->init_buffer->buffer));
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->init_block_user.deinit(c);
    }
    
    static FsEntry getRootEntry (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FS_STATE_READY)
        
        FsEntry entry;
        entry.type = ENTRYTYPE_DIR;
        entry.file_size = 0;
        entry.cluster_index = o->root_cluster;
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
            
            char const *entry_ptr = m_buffer->buffer + (m_block_entry_pos * 32);
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
            
            ClusterIndexType first_cluster =
                ReadBinaryInt<uint16_t, BinaryLittleEndian>(entry_ptr + 0x1A) |
                ((uint32_t)ReadBinaryInt<uint16_t, BinaryLittleEndian>(entry_ptr + 0x14) << 16);
            
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
                m_reader.requestBlock(c, WrapBuffer(m_buffer->buffer));
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
        size_t m_block_entry_pos;
        int8_t m_vfat_seq;
        uint8_t m_vfat_csum;
        size_t m_filename_pos;
        char m_filename[Params::MaxFileNameSize + 1];
    };
    
    class FileReader {
    private:
        enum {FILEREADER_STATE_WAITREQ, FILEREADER_STATE_READING};
        
    public:
        using FileReaderHandler = Callback<void(Context c, bool is_error, size_t length)>;
        
        void init (Context c, FsEntry file_entry, FileReaderHandler handler)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(file_entry.type == ENTRYTYPE_FILE)
            
            m_handler = handler;
            m_reader.init(c, file_entry.cluster_index, APRINTER_CB_OBJFUNC_T(&FileReader::reader_handler, this));
            m_rem_file_size = file_entry.file_size;
            m_state = FILEREADER_STATE_WAITREQ;
        }
        
        // WARNING: Only allowed together with deiniting the whole FatFs and underlying storage!
        void deinit (Context c)
        {
            TheDebugObject::access(c);
            
            m_reader.deinit(c);
        }
        
        void requestBlock (Context c, WrapBuffer buf)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == FILEREADER_STATE_WAITREQ)
            
            m_reader.requestBlock(c, buf);
            m_state = FILEREADER_STATE_READING;
        }
        
    private:
        void reader_handler (Context c, uint8_t status)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == FILEREADER_STATE_READING)
            
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
            
            m_state = FILEREADER_STATE_WAITREQ;
            return m_handler(c, is_error, read_length);
        }
        
        FileReaderHandler m_handler;
        BaseReader m_reader;
        uint32_t m_rem_file_size;
        uint8_t m_state;
    };
    
private:
    static void init_block_read_handler (Context c, bool read_error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == FS_STATE_INIT)
        
        uint8_t error_code = 99;
        
        do {
            if (read_error) {
                error_code = 20;
                goto error;
            }
            
            char *buffer = o->init_buffer->buffer;
            uint16_t sector_size =     ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0xB);
            o->sectors_per_cluster =   ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0xD);
            o->num_reserved_sectors =  ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0xE);
            o->num_fats =              ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0x10);
            uint16_t max_root =        ReadBinaryInt<uint16_t, BinaryLittleEndian>(buffer + 0x11);
            uint32_t sectors_per_fat = ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + 0x24);
            o->root_cluster =          ReadBinaryInt<uint32_t, BinaryLittleEndian>(buffer + 0x2C);
            uint8_t sig =              ReadBinaryInt<uint8_t,  BinaryLittleEndian>(buffer + 0x42);
            
            if (sector_size == 0 || sector_size % BlockSize != 0) {
                error_code = 22;
                goto error;
            }
            o->blocks_per_sector = sector_size / BlockSize;
            
            if (o->sectors_per_cluster > UINT16_MAX / o->blocks_per_sector) {
                error_code = 23;
                goto error;
            }
            o->blocks_per_cluster = o->blocks_per_sector * o->sectors_per_cluster;
            
            if ((uint32_t)o->num_reserved_sectors * sector_size < 0x47) {
                error_code = 24;
                goto error;
            }
            
            if (o->num_fats != 1 && o->num_fats != 2) {
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
            
            o->fat_end_sectors = (SectorIndexType)o->num_reserved_sectors + (SectorIndexType)o->num_fats * sectors_per_fat;
            
            if (o->fat_end_sectors > get_capacity_sectors(c)) {
                error_code = 29;
                goto error;
            }
            
            error_code = 0;
        } while (0);
        
    error:
        o->state = error_code ? FS_STATE_FAILED : FS_STATE_READY;
        return InitHandler::call(c, error_code);
    }
    
    static SectorIndexType get_capacity_sectors (Context c)
    {
        auto *o = Object::self(c);
        return TheBlockAccess::getCapacityBlocks(c) / o->blocks_per_sector;
    }
    
    static bool is_cluster_idx_valid (ClusterIndexType cluster_idx)
    {
        return (cluster_idx >= 2 && cluster_idx < UINT32_C(0xFFFFFF8));
    }
    
    static bool get_cluster_block_idx (Context c, ClusterIndexType cluster_idx, ClusterBlockIndexType cluster_block_idx, BlockIndexType *out_block_idx)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(is_cluster_idx_valid(cluster_idx))
        AMBRO_ASSERT(cluster_block_idx < o->blocks_per_cluster)
        
        SectorIndexType sectors_after_fat_end = (SectorIndexType)(cluster_idx - 2) * o->sectors_per_cluster;
        if (sectors_after_fat_end >= get_capacity_sectors(c) - o->fat_end_sectors) {
            return false;
        }
        SectorIndexType sector_idx = o->fat_end_sectors + sectors_after_fat_end;
        *out_block_idx = sector_idx * o->blocks_per_sector + cluster_block_idx;
        return true;
    }
    
    static bool get_fat_entry_block_idx (Context c, ClusterIndexType cluster_idx, BlockIndexType *out_block_idx)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(is_cluster_idx_valid(cluster_idx))
        
        if (cluster_idx >= o->num_fat_entries) {
            return false;
        }
        *out_block_idx = ((BlockIndexType)o->num_reserved_sectors * o->blocks_per_sector) + (cluster_idx / FatEntriesPerBlock);
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
    private:
        enum {BASEREAD_STATE_WAITREQ, BASEREAD_STATE_REQEVENT, BASEREAD_STATE_READING_DATA, BASEREAD_STATE_READING_FAT};
        
    public:
        enum {BASEREAD_STATUS_ERR, BASEREAD_STATUS_EOF, BASEREAD_STATUS_OK};
        using BaseReadHandler = Callback<void(Context c, uint8_t status)>;
        
        void init (Context c, ClusterIndexType first_cluster, BaseReadHandler handler)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(o->state == FS_STATE_READY)
            
            m_handler = handler;
            m_state = BASEREAD_STATE_WAITREQ;
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&BaseReader::event_handler, this));
            m_block_user.init(c, APRINTER_CB_OBJFUNC_T(&BaseReader::read_handler, this));
            m_current_cluster = first_cluster;
            m_block_in_cluster = 0;
        }
        
        // WARNING: Only allowed together with deiniting the whole FatFs and underlying storage!
        void deinit (Context c)
        {
            m_block_user.deinit(c);
            m_event.deinit(c);
        }
        
        void requestBlock (Context c, WrapBuffer buf)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == BASEREAD_STATE_WAITREQ)
            
            m_state = BASEREAD_STATE_REQEVENT;
            m_req_buf = buf;
            m_event.appendNowNotAlready(c);
        }
        
    private:
        void event_handler (Context c)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == BASEREAD_STATE_REQEVENT)
            
            uint8_t status = BASEREAD_STATUS_ERR;
            
            if (!is_cluster_idx_valid(m_current_cluster)) {
                status = BASEREAD_STATUS_EOF;
                goto report;
            }
            
            if (m_block_in_cluster == o->blocks_per_cluster) {
                BlockIndexType entry_block_idx;
                if (!get_fat_entry_block_idx(c, m_current_cluster, &entry_block_idx)) {
                    goto report;
                }
                m_block_user.startRead(c, entry_block_idx, m_req_buf);
                m_state = BASEREAD_STATE_READING_FAT;
                return;
            }
            
            BlockIndexType block_idx;
            if (!get_cluster_block_idx(c, m_current_cluster, m_block_in_cluster, &block_idx)) {
                goto report;
            }
            
            m_block_user.startRead(c, block_idx, m_req_buf);
            m_state = BASEREAD_STATE_READING_DATA;
            return;
            
        report:
            m_state = BASEREAD_STATE_WAITREQ;
            return m_handler(c, status);
        }
        
        void read_handler (Context c, bool is_read_error)
        {
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == BASEREAD_STATE_READING_DATA || m_state == BASEREAD_STATE_READING_FAT)
            
            uint8_t status = BASEREAD_STATUS_ERR;
            
            if (is_read_error) {
                goto report;
            }
            
            if (m_state == BASEREAD_STATE_READING_FAT) {
                size_t block_offset = (size_t)4 * (m_current_cluster % FatEntriesPerBlock);
                char cluster_number_buf[4];
                m_req_buf.copyOut(block_offset, 4, cluster_number_buf);
                m_current_cluster = ReadBinaryInt<uint32_t, BinaryLittleEndian>(cluster_number_buf);
                m_block_in_cluster = 0;
                m_state = BASEREAD_STATE_REQEVENT;
                m_event.appendNowNotAlready(c);
                return;
            }
            
            m_block_in_cluster++;
            status = BASEREAD_STATUS_OK;
            
        report:
            m_state = BASEREAD_STATE_WAITREQ;
            return m_handler(c, status);
        }
        
        BaseReadHandler m_handler;
        uint8_t m_state;
        typename Context::EventLoop::QueuedEvent m_event;
        BlockAccessUser m_block_user;
        ClusterIndexType m_current_cluster;
        ClusterBlockIndexType m_block_in_cluster;
        WrapBuffer m_req_buf;
    };
    
public:
    struct Object : public ObjBase<FatFs, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        SharedBuffer *init_buffer;
        BlockAccessUser init_block_user;
        uint8_t state;
        uint8_t sectors_per_cluster;
        uint16_t num_reserved_sectors;
        uint8_t num_fats;
        ClusterIndexType root_cluster;
        uint16_t blocks_per_sector;
        ClusterBlockIndexType blocks_per_cluster;
        ClusterIndexType num_fat_entries;
        SectorIndexType fat_end_sectors;
    };
};

template <
    int TMaxFileNameSize
>
struct FatFsService {
    static int const MaxFileNameSize = TMaxFileNameSize;
    
    template <typename Context, typename ParentObject, typename TheBlockAccess, typename InitHandler>
    using Fs = FatFs<Context, ParentObject, TheBlockAccess, InitHandler, FatFsService>;
};

#include <aprinter/EndNamespace.h>

#endif
