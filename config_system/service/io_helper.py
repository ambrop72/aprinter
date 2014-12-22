from __future__ import print_function
import sys
import os
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '../common'))
import argparse
import base64
import json
import collections
import config_common

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write-file', action='store_true')
    parser.add_argument('--write-file-src')
    parser.add_argument('--write-file-dst')
    parser.add_argument('--build-reply', action='store_true')
    parser.add_argument('--build-reply-success')
    parser.add_argument('--build-reply-message')
    parser.add_argument('--build-reply-data')
    parser.add_argument('--build-reply-dst')
    args = parser.parse_args()
    
    if args.write_file:
        with config_common.use_input_file(args.write_file_src) as input_stream:
            with config_common.use_output_file(args.write_file_dst) as output_stream:
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
            with config_common.use_input_file(args.build_reply_data) as input_stream:
                res['data'] = base64.b64encode(input_stream.read())
        
        with config_common.use_output_file(args.build_reply_dst) as output_stream:
            json.dump(res, output_stream)
    
    else:
        raise ValueError('no operation specified')

main()
