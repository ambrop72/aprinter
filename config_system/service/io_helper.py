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
import sys
import os
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '../utils'))
import argparse
import base64
import json
import collections
import file_utils

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write-file', action='store_true')
    parser.add_argument('--write-file-src')
    parser.add_argument('--write-file-dst')
    parser.add_argument('--build-reply', action='store_true')
    parser.add_argument('--build-reply-success')
    parser.add_argument('--build-reply-message')
    parser.add_argument('--build-reply-data')
    parser.add_argument('--build-reply-filename')
    parser.add_argument('--build-reply-dst')
    args = parser.parse_args()
    
    if args.write_file:
        with file_utils.use_input_file(args.write_file_src) as input_stream:
            with file_utils.use_output_file(args.write_file_dst) as output_stream:
                while True:
                    data = input_stream.read(2**15)
                    if len(data) == 0:
                        break
                    output_stream.write(data)
    
    elif args.build_reply:
        res = collections.OrderedDict({})
        
        res['success'] = (args.build_reply_success == 'true')
        
        res['message'] = args.build_reply_message
        
        if args.build_reply_data is not None and len(args.build_reply_data) != 0:
            res['filename'] = args.build_reply_filename
            with file_utils.use_input_file(args.build_reply_data) as input_stream:
                res['data'] = base64.b64encode(input_stream.read())
        
        with file_utils.use_output_file(args.build_reply_dst) as output_stream:
            json.dump(res, output_stream)
    
    else:
        raise ValueError('no operation specified')

main()
