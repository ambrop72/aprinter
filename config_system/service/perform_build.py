from __future__ import print_function
import sys
import os
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '../utils'))
import argparse
import json
import collections
import base64
import subprocess
import file_utils

def run_process_inout(cmd, input_str, description):
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        output, _ = proc.communicate(input_str)
    except:
        proc.wait()
        raise
    status = proc.wait()
    if status != 0:
        raise Exception('Execution of {} failed.'.format(description))
    return output

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--aprinter-src-dir')
    parser.add_argument('--request-file')
    parser.add_argument('--response-file')
    parser.add_argument('--temp-dir')
    parser.add_argument('--python')
    parser.add_argument('--nix-build')
    parser.add_argument('--mkdir')
    parser.add_argument('--rsync')
    parser.add_argument('--p7za')
    args = parser.parse_args()
    
    # Read the request.
    with file_utils.use_input_file(args.request_file) as input_stream:
        request = input_stream.read()
    
    # The response will be built from these variables.
    response_success = False
    response_message = ''
    response_filename = None
    response_data = None

    try:
        # Run the generate script.
        generate_path = os.path.join(args.aprinter_src_dir, 'config_system/generator/generate.py')
        aprinter_nix_dir = os.path.join(args.aprinter_src_dir, 'nix')
        cmd = [args.python, '-B', generate_path, '--config', '-', '--output', '-', '--nix', '--nix-dir', aprinter_nix_dir]
        nix_expr = run_process_inout(cmd, request, 'the generate script')
        
        # Do the build...
        result_path = os.path.join(args.temp_dir, 'result')
        nixbuild_cmd = [args.nix_build, '-', '-o', result_path]
        run_process_inout(nixbuild_cmd, nix_expr, 'nix-build')
        
        # Create a subfolder which we will archive.
        build_path = os.path.join(args.temp_dir, 'aprinter-build')
        run_process_inout([args.mkdir, build_path], '', 'mkdir')
        
        # Copy the build to the build_path.
        run_process_inout([args.rsync, '-rL', '--chmod=ugo=rwX', '{}/'.format(result_path), '{}/'.format(build_path)], '', 'rsync')
        
        # Add the configuration to the build folder.
        with open(os.path.join(build_path, 'config.json'), 'wb') as output_stream:
            output_stream.write(request)
        
        # Produce the archive.
        archive_filename = 'aprinter-build.zip'
        archive_path = os.path.join(args.temp_dir, archive_filename)
        archive_cmd = [args.p7za, 'a', archive_path, build_path]
        run_process_inout(archive_cmd, '', 'p7za')
        
        # Read the archive contents.
        with open(archive_path, 'rb') as input_stream:
            archive_contents = input_stream.read()
        
        response_success = True
        response_message = 'Compilation successful.'
        response_filename = archive_filename
        response_data = archive_contents
        
    except Exception as e:
        response_message = str(e)
    
    # Build the response.
    response = collections.OrderedDict({})
    response['success'] = response_success
    response['message'] = response_message
    if response_filename is not None:
        response['filename'] = response_filename
        response['data'] = base64.b64encode(response_data)
    
    # Write the response.
    with file_utils.use_output_file(args.response_file) as output_stream:
        json.dump(response, output_stream)
    
main()
