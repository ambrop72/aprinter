# Copyright (c) 2015 Ambroz Bizjak
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import print_function
import argparse
import struct
import sys
import hashlib

class FatFs(object):
    def __init__(self, fh):
        self._fh = fh
        
        fh.seek(0, 2)
        self._size = fh.tell()
        
        if self._size < 0x18:
            raise ValueError('BPB missing')
        
        self._sector_size = self._read_unpack(0xB, '<H')[0]
        self._sectors_per_cluster = self._read_unpack(0xD, '<B')[0]
        self._num_reserved_sectors = self._read_unpack(0xE, '<H')[0]
        self._num_fats = self._read_unpack(0x10, '<B')[0]
        self._max_root = self._read_unpack(0x11, '<H')[0]
        
        self._cluster_size = self._sectors_per_cluster * self._sector_size
        self._reserved_size = self._num_reserved_sectors * self._sector_size
        
        if self._size < self._reserved_size:
            raise ValueError('EOF before end of reserved')
        
        if self._reserved_size < 0x47:
            raise ValueError('EBPB missing')
        
        self._sectors_per_fat = self._read_unpack(0x24, '<I')[0]
        self._root_cluster = self._read_unpack(0x2C, '<I')[0]
        self._sig = self._read_unpack(0x42, '<B')[0]
        
        if self._num_fats not in (1, 2):
            raise ValueError('Unsupported number of FATs')
        
        if self._sig not in (0x28, 0x29):
            raise ValueError('EBPB missing (bad signature at 0x42)')
        
        if self._max_root != 0:
            raise ValueError('max_root is not 0 (not a FAT32?)')
        
        self._fat_size = self._sectors_per_fat * self._sector_size
        self._fat_clusters = self._fat_size / 4
        
        print('size={} sector_size={} sectors_per_cluster={} reserved_size={} num_fats={} sig={} cluster_size={} fat_clusters={} root_cluster={} sectors_per_fat={}'.format(
            self._size, self._sector_size, self._sectors_per_cluster, self._reserved_size,
            self._num_fats, self._sig, self._cluster_size, self._fat_clusters, self._root_cluster, self._sectors_per_fat), file=sys.stderr)
        
    def open_root_dir(self):
        return FatFsDir(self, self._root_cluster)
    
    def open_entry_dir(self, entry):
        assert entry._type == 'dir'
        
        return FatFsDir(self, entry._cluster)
    
    def open_entry_file(self, entry):
        assert entry._type == 'file'
        
        return FatFsFile(self, entry._cluster, entry._file_size)
    
    def _read(self, offset, size):
        if offset <= 0:
            raise ValueError('Read before start!?')
        if offset + size > self._size:
            raise ValueError('Read after end!?')
        self._fh.seek(offset)
        data = self._fh.read(size)
        if len(data) != size:
            raise ValueError('EOF reached!?')
        return data
    
    def _read_unpack(self, offset, fmt):
        size = struct.calcsize(fmt)
        data = self._read(offset, size)
        return struct.unpack(fmt, data)
    
    def _read_fat_entry(self, cluster_number):
        if not (0 <= cluster_number < self._fat_clusters):
            raise ValueError('Out of range cluster number!?')
        val = self._read_unpack(self._reserved_size + cluster_number * 4, '<I')[0]
        return (val & 0xFFFFFFF)
    
    def _read_cluster(self, cluster_number):
        assert cluster_number >= 2
        
        offset = self._reserved_size + self._num_fats * self._fat_size + (cluster_number - 2) * self._cluster_size
        return self._read(offset, self._cluster_size)

class FatFsFileBase(object):
    def __init__(self, fat, first_cluster, max_size):
        self._fat = fat
        self._first_cluster = first_cluster
        self._max_size = max_size
        self._rewind()
    
    def _rewind(self):
        self._current_cluster = self._first_cluster
        self._cluster_data = None
        self._cluster_offset = 0
        self._pos = 0
    
    def _read(self, size):
        size = min(size, self._max_size - self._pos)
        data = ''
        
        while len(data) < size:
            if self._cluster_data is None or self._cluster_offset >= len(self._cluster_data):
                current_cluster = self._current_cluster
                if self._cluster_data is not None:
                    current_cluster = self._fat._read_fat_entry(current_cluster)
                
                if current_cluster < 2 or current_cluster >= 0xFFFFFF8:
                    break
                
                cluster_data = self._fat._read_cluster(current_cluster)
                
                self._current_cluster = current_cluster
                self._cluster_data = cluster_data
                self._cluster_offset = 0
            
            to_take = min(size - len(data), len(self._cluster_data) - self._cluster_offset)
            data += self._cluster_data[self._cluster_offset : self._cluster_offset + to_take]
            self._cluster_offset += to_take
            self._pos += to_take
        
        return data

class FatFsDir(FatFsFileBase):
    def __init__(self, fat, first_cluster):
        FatFsFileBase.__init__(self, fat, first_cluster, 0xFFFFFFFF)
    
    def rewind(self):
        self._rewind()
    
    def next_entry(self):
        vfat_seq = -1
        vfat_csum = 0
        vfat_data = []
        
        while True:
            data = self._read(32)
            if len(data) < 32:
                return None
            
            first_byte = struct.unpack('<B', data[0])[0]
            attrs = struct.unpack('<B', data[0xB])[0]
            type_byte = struct.unpack('<B', data[0xC])[0]
            checksum_byte = struct.unpack('<B', data[0xD])[0]
            
            # End marker.
            if first_byte == 0:
                return None
            
            # VFAT entry.
            if first_byte != 0xE5 and attrs == 0xF and type_byte == 0:
                entry_vfat_seq = first_byte & 0x1F
                
                if (first_byte & 0x60) == 0x40:
                    vfat_seq = entry_vfat_seq
                    vfat_csum = checksum_byte
                    vfat_data = []
                
                if entry_vfat_seq > 0 and vfat_seq != -1 and entry_vfat_seq == vfat_seq and checksum_byte == vfat_csum:
                    this_vfat_data = data[0x1 : 0x1 + 10] + data[0xE : 0xE + 12] + data[0x1C : 0x1C + 4]
                    vfat_data.append(this_vfat_data)
                    vfat_seq = entry_vfat_seq - 1
                else:
                    vfat_seq = -1
                
                continue
            
            # Forget VFAT state but remember for use in this entry.
            cur_vfat_seq = vfat_seq
            vfat_seq = -1
            
            # Free marker.
            if first_byte == 0xE5:
                continue
            
            # Ignore: volume label or device
            if (attrs & 0x8):
                continue
            if (attrs & 0x40):
                continue
            
            # TBD: 0x05
            
            is_dir = bool(attrs & 0x10)
            file_type = 'dir' if is_dir else 'file'
            
            file_size = struct.unpack('<I', data[0x1C : 0x1C + 4])[0]
            
            dot_entry = (first_byte == ord('.'))
            
            first_cluster = struct.unpack('<H', data[0x1A : 0x1A + 2])[0] | (struct.unpack('<H', data[0x14 : 0x14 + 2])[0] << 16)
            
            if not dot_entry and cur_vfat_seq == 0 and _vfat_checksum(data[0 : 11]) == vfat_csum:
                vfat_chars = ''.join(reversed(vfat_data)).decode('utf16')
                vfat_length = 0
                while vfat_length < len(vfat_chars) and vfat_chars[vfat_length] != u'\u0000':
                    vfat_length += 1
                name = vfat_chars[:vfat_length].encode('utf8')
            else:
                if first_byte == 0x05:
                    base_data = '\xE5{}'.format(data[1 : 8])
                else:
                    base_data = data[0 : 8]
                name_base = _fixup_83_name(base_data, bool(type_byte & 0x8))
                name_ext = _fixup_83_name(data[8 : 8 + 3], bool(type_byte & 0x10))
                name = '{}.{}'.format(name_base, name_ext) if name_ext != '' else name_base
            
            return FatFsDirEntry(file_type, name, first_cluster, file_size, dot_entry)

class FatFsDirEntry(object):
    def __init__(self, the_type, name, cluster, file_size, dot_entry):
        self._type = the_type
        self._name = name
        self._cluster = cluster
        self._file_size = file_size
        self._dot_entry = dot_entry
    
    def get_type(self):
        return self._type
    
    def get_name(self):
        return self._name
    
    def get_file_size(self):
        return self._file_size
    
    def is_dot_entry(self):
        return self._dot_entry

class FatFsFile(FatFsFileBase):
    def __init__(self, fat, first_cluster, file_size):
        FatFsFileBase.__init__(self, fat, first_cluster, file_size)
    
    def rewind(self):
        self._rewind()
    
    def read(self, size=0xFFFFFFFF):
        return self._read(size)

def _fixup_83_name(data, lowercase):
    length = len(data)
    while length > 0 and data[length - 1] == ' ':
        length -= 1
    trimmed_data = data[0 : length]
    if lowercase:
        trimmed_data = ''.join(chr(ord(c) + 32) if ord('A') <= ord(c) <= ord('Z') else c for c in trimmed_data)
    return trimmed_data

def _vfat_checksum(short_name_data):
    csum = 0
    for i in range(11):
        csum = (((csum & 1) << 7) + (csum >> 1) + ord(short_name_data[i])) & 0xFF
    return csum

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--file', required=True)
    args = parser.parse_args()
    
    with open(args.file, 'rb') as fh:
        ffs = FatFs(fh)
        
        def explore_dir(ffs_dir, indent):
            while True:
                entry = ffs_dir.next_entry()
                if entry is None:
                    break
                
                maybe_size = ' size={}'.format(entry.get_file_size()) if entry.get_type() == 'file' else ''
                print('{}type={} name={} dot={}{}'.format(indent, entry.get_type(), entry.get_name(), entry.is_dot_entry(), maybe_size))
                
                if entry.get_type() == 'dir' and not entry.is_dot_entry():
                    explore_dir(ffs.open_entry_dir(entry), '{}  '.format(indent))
                    
                elif entry.get_type() == 'file':
                    the_file = ffs.open_entry_file(entry)
                    hasher = hashlib.sha256()
                    data = the_file.read()
                    hasher.update(data)
                    
                    print('{}  read_size={} read_sha256={}'.format(indent, len(data), hasher.hexdigest()))
                    
                    if len(data) != entry.get_file_size():
                        print('WARNING: Read different size than expected.', file=sys.stderr)
        
        explore_dir(ffs.open_root_dir(), '')

main()
