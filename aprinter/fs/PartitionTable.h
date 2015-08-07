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

#ifndef APRINTER_PARTITION_TABLE_H
#define APRINTER_PARTITION_TABLE_H

#include <stdint.h>

#include <aprinter/base/BinaryTools.h>
#include <aprinter/fs/BlockRange.h>

#include <aprinter/BeginNamespace.h>

template <typename TheFs, typename BlockIndexType>
static bool FindMbrPartition (char const *mbr, BlockIndexType capacity_blocks, BlockRange<BlockIndexType> *out_block_range)
{
    uint16_t signature = ReadBinaryInt<uint16_t, BinaryLittleEndian>(mbr + 510);
    if (signature != UINT16_C(0xAA55)) {
        return false;
    }
    
    for (int partNum = 0; partNum < 4; partNum++) {
        char const *part_entry_buf = mbr + (446 + partNum * 16);
        uint8_t part_type =           ReadBinaryInt<uint8_t,  BinaryLittleEndian>(part_entry_buf + 0x4);
        uint32_t part_start_blocks =  ReadBinaryInt<uint32_t, BinaryLittleEndian>(part_entry_buf + 0x8);
        uint32_t part_length_blocks = ReadBinaryInt<uint32_t, BinaryLittleEndian>(part_entry_buf + 0xC);
        
        if (!(part_start_blocks <= capacity_blocks && part_length_blocks <= capacity_blocks - part_start_blocks && part_length_blocks > 0)) {
            continue;
        }
        
        if (TheFs::isPartitionTypeSupported(part_type)) {
            *out_block_range = BlockRange<BlockIndexType>{part_start_blocks, part_start_blocks + part_length_blocks};
            return true;
        }
    }
    
    return false;
}

#include <aprinter/EndNamespace.h>

#endif
