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
import shutil
import json
import subprocess
import file_utils
import rich_template
import aprinter_config_editor

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rm', action='store_true')
    parser.add_argument('--out-dir')
    args = parser.parse_args()
    
    # Build editor schema.
    the_editor = aprinter_config_editor.editor()
    editor_schema = the_editor._json_schema()
    
    # Determine directories.
    src_dir = file_utils.file_dir(__file__)
    libs_dir = os.path.join(src_dir, 'libs')
    if args.out_dir is not None:
        dist_dir = args.out_dir
    else:
        dist_dir = os.path.join(src_dir, 'dist')
    
    # Remove dist dir.
    if args.rm and os.path.isdir(dist_dir):
        shutil.rmtree(dist_dir)
    
    # Create dist dir.
    os.mkdir(dist_dir)
    
    # Copy json-editor.
    shutil.copyfile(os.path.join(libs_dir, 'jsoneditor.min.js'), os.path.join(dist_dir, 'jsoneditor.js'))
    
    # Copy FileSaver.
    shutil.copyfile(os.path.join(libs_dir, 'FileSaver.min.js'), os.path.join(dist_dir, 'FileSaver.js'))
    
    # Copy Bootstrap.
    subprocess.call(['unzip', '-q', os.path.join(libs_dir, 'bootstrap-3.3.2-dist.zip'), '-d', dist_dir])
    os.rename(os.path.join(dist_dir, 'bootstrap-3.3.2-dist'), os.path.join(dist_dir, 'bootstrap'))
    
    # Copy files.
    for filename in ['index.html', 'Ajax-loader.gif']:
        shutil.copyfile(os.path.join(src_dir, filename), os.path.join(dist_dir, filename))
    
    # Read default configuration.
    default_config = json.loads(file_utils.read_file(os.path.join(src_dir, 'default_config.json')))
    
    # Build and write init.js.
    init_js_template = file_utils.read_file(os.path.join(src_dir, 'init.js'))
    init_js = rich_template.RichTemplate(init_js_template).substitute({
        'SCHEMA': json.dumps(editor_schema, separators=(',',':'), sort_keys=True),
        'DEFAULT': json.dumps(default_config, separators=(',',':'), sort_keys=True)
    })
    file_utils.write_file(os.path.join(dist_dir, 'init.js'), init_js)

main()
