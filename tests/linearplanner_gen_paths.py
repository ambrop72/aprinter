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
import random

count_top = 100
paths_per_count = 100
max_distance = 100.0
max_maxv2 = 100.0
max_twomaxa = 100.0

paths = []

for count in range(1, count_top + 1):
    for _ in range(paths_per_count):
        name = 'path{}'.format(len(paths))

        print('static Segment const {}_segs[] = {{'.format(name))

        for i in range(count):
            distance = max_distance * random.random()
            maxv2 = max_maxv2 * random.random()
            twomaxa = max_twomaxa * random.random()
            end = ',' if i < (count - 1) else ''
            print('    {{{}, {}, {}}}{}'.format(distance, maxv2, twomaxa, end))

        print('};')
        paths.append('{{{}_segs, {}}}'.format(name, count))

print('static Path const paths[] = {{\n    {}\n}};'.format(',\n    '.join(paths)))
print('static size_t const num_paths = {};'.format(len(paths)))
print('static size_t const max_path_len = {};'.format(count_top))
