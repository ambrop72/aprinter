# Copyright (c) 2019 Ambroz Bizjak
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

import os
import re
import hashlib

import file_utils

_REF_REGEX = re.compile(r'\[REF:([^\[]*)\]')

def hashify(root_dir_path, ref_file_exts, root_files, out_dir_path):
    file_paths = _collect_files(root_dir_path)
    file_entries = _read_files_and_collect_refs(root_dir_path, file_paths, ref_file_exts)
    ordered_refs = _resolve_and_order_refs(file_entries, root_files)
    _calc_hashes_and_substitute_refs(ordered_refs, file_entries)
    _write_to_output(ordered_refs, file_entries, root_files, out_dir_path)

def _collect_files(root_dir_path):
    files = []
    for dir_path, subdir_names, file_names in os.walk(root_dir_path):
        rel_dir_path = os.path.relpath(dir_path, root_dir_path)
        if rel_dir_path == '.':
            rel_dir_path = ''
        for file_name in file_names:
            files.append(os.path.join(rel_dir_path, file_name))
    
    return files

def _read_files_and_collect_refs(root_dir_path, file_paths, ref_file_exts):
    file_entries = {}

    for file in file_paths:
        file_content = file_utils.read_file(os.path.join(root_dir_path, file))
        
        if any(file.endswith(ext) for ext in ref_file_exts):
            file_refs = [_normalize_ref(file, ref_val)
                for ref_val in _REF_REGEX.findall(file_content)]
        else:
            file_refs = []
        
        file_entries[file] = {'content': file_content, 'refs': file_refs}
    
    return file_entries

def _normalize_ref(file, ref_val):
    return os.path.normpath(os.path.join(os.path.dirname(file), ref_val))

def _resolve_and_order_refs(file_entries, root_files):
    resolved_list = []
    resolved_set = set()
    current_set = set()

    def resolve(file, from_file):
        if file in resolved_set:
            return
        
        if file in current_set:
            raise Exception('Cyclic reference to {} from {}.'.format(file, from_file))
        
        if file not in file_entries:
            raise Exception('Invalid reference to {} from {}.'.format(file, from_file))
        file_entry = file_entries[file]

        if len(file_entry['refs']) > 0:
            current_set.add(file)
            for ref_file in file_entry['refs']:
                resolve(ref_file, file)
            current_set.remove(file)
        
        resolved_list.append(file)
        resolved_set.add(file)
    
    for file in root_files:
        resolve(file, '(root)')
    
    return resolved_list

def _calc_hashes_and_substitute_refs(ordered_refs, file_entries):
    for file in ordered_refs:
        file_dir = os.path.dirname(file)
        file_entry = file_entries[file]

        if len(file_entry['refs']) > 0:
            def replace_func(matchobj):
                ref_val = matchobj.group(1)
                ref_file = _normalize_ref(file, ref_val)
                ref_new_name = file_entries[ref_file]['new_name']
                return os.path.join(os.path.dirname(ref_val), ref_new_name)

            file_entry['content'] = _REF_REGEX.sub(replace_func, file_entry['content'])
        
        file_hash = hashlib.md5(file_entry['content']).hexdigest()

        name_base, name_ext = os.path.splitext(os.path.basename(file))
        file_entry['new_name'] = '{}_{}{}'.format(name_base, file_hash, name_ext)

def _write_to_output(files, file_entries, root_files, out_dir_path):
    for file in files:
        file_entry = file_entries[file]

        rel_dst_path = file if file in root_files else \
            os.path.join(os.path.dirname(file), file_entry['new_name'])
        
        dst_path = os.path.join(out_dir_path, rel_dst_path)
        
        dir_path = os.path.dirname(dst_path)
        if not os.path.isdir(dir_path):
            os.makedirs(dir_path)
        
        file_utils.write_file(dst_path, file_entry['content'])
