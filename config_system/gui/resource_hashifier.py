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
import fnmatch
import itertools

import file_utils

_REF_REGEX = re.compile(r'\[REF:([^\[]*)\]')

def hashify(root_dir_path, config, out_dir_path):
    root_files = config['root_files']
    refmatcher_index = _RefMatcherIndex(config['ref_specs'])

    file_paths = _collect_files(root_dir_path)

    file_entries = _read_files_and_collect_refs(root_dir_path, file_paths, refmatcher_index)

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

def _read_files_and_collect_refs(root_dir_path, file_paths, refmatcher_index):
    file_entries = {}

    for file in file_paths:
        file_content = file_utils.read_file(os.path.join(root_dir_path, file))
        
        refmatcher = refmatcher_index.get_matcher_for_file(file)
        if refmatcher is not None:
            file_refs = [_normalize_ref(file, ref_val)
                for ref_val in refmatcher.find_refs(file_content)]
        else:
            file_refs = []
        
        file_entries[file] = {
            'content': file_content,
            'refmatcher': refmatcher,
            'refs': file_refs
        }
    
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
            def transform_ref_func(ref_val):
                ref_file = _normalize_ref(file, ref_val)
                ref_new_name = file_entries[ref_file]['new_name']
                return os.path.join(os.path.dirname(ref_val), ref_new_name)

            file_entry['content'] = file_entry['refmatcher'].replace_refs(
                file_entry['content'], transform_ref_func)
        
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

class _RefMatcherIndex(object):
    def __init__(self, ref_specs):
        self._ref_specs = ref_specs
        self._matchers = {}
    
    def get_matcher_for_file(self, file):
        regexps = frozenset(itertools.chain.from_iterable(
            ref_spec['ref_regexps'] for ref_spec in self._ref_specs
            if any(fnmatch.fnmatch(file, glob) for glob in ref_spec['file_globs'])))
        
        if len(regexps) == 0:
            return None
        
        if regexps in self._matchers:
            return self._matchers[regexps]
        else:
            matcher = _RefMatcher(regexps)
            self._matchers[regexps] = matcher
            return matcher

class _RefMatcher(object):
    def __init__(self, regexps):
        assert len(regexps) > 0

        regexps_list = sorted(regexps)

        for regexp in regexps_list:
            regexp_comp = re.compile(regexp)
            assert regexp_comp.groups == 2

        self._num_regexps = len(regexps_list)

        full_regexp = '|'.join('({})'.format(regexp) for regexp in regexps_list)
        
        self._full_regexp_comp = re.compile(full_regexp)
        assert self._full_regexp_comp.groups == self._num_regexps * 3
    
    def find_refs(self, content):
        for match in self._full_regexp_comp.finditer(content):
            ref = self._interpret_ref_match(match)
            yield ref['ref_content']
    
    def replace_refs(self, content, transform_ref_func):
        def replace_func(match):
            ref = self._interpret_ref_match(match)
            return '{}{}{}'.format(
                ref['match_content'][:ref['preserve_left_off']],
                transform_ref_func(ref['ref_content']),
                ref['match_content'][ref['preserve_right_off']:])
        
        return self._full_regexp_comp.sub(replace_func, content)

    def _interpret_ref_match(self, match):
        match_group_index = None
        for i in xrange(self._num_regexps):
            group_index = 1 + i * 3
            if match.group(group_index) is not None:
                assert match_group_index is None
                match_group_index = group_index
        assert match_group_index is not None

        replace_group_index = match_group_index + 1
        ref_group_index = match_group_index + 2

        match_content = match.group(match_group_index)
        assert match_content is not None
        ref_content = match.group(ref_group_index)
        assert ref_content is not None

        preserve_left_off = match.start(replace_group_index) - match.start(match_group_index)
        preserve_right_off = match.end(replace_group_index) - match.start(match_group_index)
        assert 0 <= preserve_left_off <= preserve_right_off <= len(match_content)

        return {
            'match_content': match_content,
            'ref_content': ref_content,
            'preserve_left_off': preserve_left_off,
            'preserve_right_off': preserve_right_off,
        }
