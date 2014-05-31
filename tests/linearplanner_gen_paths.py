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
