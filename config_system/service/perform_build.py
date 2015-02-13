from __future__ import print_function
import sys
import os
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '../utils'))
import argparse
import json
import collections
import base64
import subprocess
import pipes
import file_utils

class ProcessError(Exception):
    def __init__(self, msg, stderr_output):
        Exception.__init__(self, msg)
        self.stderr_output = stderr_output

def run_process(cmd, input_str, description):
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        output, error = proc.communicate(input_str)
    except:
        proc.wait()
        raise
    status = proc.wait()
    if status != 0:
        raise ProcessError(description, error)
    return output

def run_process_limited(args, cmd, input_str, description):
    shell_cmd = 'set -o pipefail && ( {} 3>&1 1>&2 2>&3 3>&- | ( {} -c {}; {} > /dev/null ) ) 3>&1 1>&2 2>&3 3>&-'.format(
        ' '.join(pipes.quote(x) for x in cmd), args.head, args.stderr_truncate_bytes, args.cat)
    wrap_cmd = [args.bash, '-c', shell_cmd]
    return run_process(wrap_cmd, input_str, description)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--aprinter-src-dir')
    parser.add_argument('--request-file')
    parser.add_argument('--response-file')
    parser.add_argument('--temp-dir')
    parser.add_argument('--stderr-truncate-bytes')
    parser.add_argument('--python')
    parser.add_argument('--nix-build')
    parser.add_argument('--mkdir')
    parser.add_argument('--rsync')
    parser.add_argument('--p7za')
    parser.add_argument('--bash')
    parser.add_argument('--head')
    parser.add_argument('--cat')
    args = parser.parse_args()
    
    # Read the request.
    with file_utils.use_input_file(args.request_file) as input_stream:
        request = input_stream.read()
    
    # The response will be built from these variables.
    response_success = False
    response_message = ''
    response_error = None
    response_filename = None
    response_data = None

    try:
        # Run the generate script.
        generate_path = os.path.join(args.aprinter_src_dir, 'config_system/generator/generate.py')
        aprinter_nix_dir = os.path.join(args.aprinter_src_dir, 'nix')
        cmd = [args.python, '-B', generate_path, '--config', '-', '--output', '-', '--nix', '--nix-dir', aprinter_nix_dir]
        nix_expr = run_process_limited(args, cmd, request, 'Failed to interpret the configuration.')
        
        # Do the build...
        result_path = os.path.join(args.temp_dir, 'result')
        nixbuild_cmd = [args.nix_build, '-', '-o', result_path]
        run_process_limited(args, nixbuild_cmd, nix_expr, 'Failed to compile the source code.')
        
        # Create a subfolder which we will archive.
        build_path = os.path.join(args.temp_dir, 'aprinter-build')
        run_process_limited(args, [args.mkdir, build_path], '', 'The mkdir failed!?')
        
        # Copy the build to the build_path.
        run_process_limited(args, [args.rsync, '-rL', '--chmod=ugo=rwX', '{}/'.format(result_path), '{}/'.format(build_path)], '', 'The rsync failed!?')
        
        # Add the configuration to the build folder.
        with open(os.path.join(build_path, 'config.json'), 'wb') as output_stream:
            output_stream.write(request)
        
        # Produce the archive.
        archive_filename = 'aprinter-build.zip'
        archive_path = os.path.join(args.temp_dir, archive_filename)
        archive_cmd = [args.p7za, 'a', archive_path, build_path]
        run_process_limited(args, archive_cmd, '', 'The p7za failed!?')
        
        # Read the archive contents.
        with open(archive_path, 'rb') as input_stream:
            archive_contents = input_stream.read()
        
        response_success = True
        response_message = 'Compilation successful.'
        response_filename = archive_filename
        response_data = archive_contents
        
    except ProcessError as e:
        response_message = str(e)
        response_error = e.stderr_output
    except Exception as e:
        response_message = str(e)
    
    # Build the response.
    response = collections.OrderedDict({})
    response['success'] = response_success
    response['message'] = response_message
    if response_error is not None:
        response['error'] = response_error
    if response_filename is not None:
        response['filename'] = response_filename
        response['data'] = base64.b64encode(response_data)
    
    # Write the response.
    with file_utils.use_output_file(args.response_file) as output_stream:
        json.dump(response, output_stream)
    
main()
