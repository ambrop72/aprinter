from __future__ import print_function
import argparse
import os
import errno
import shutil
import string
import json
import aprinter_config_editor

class MyStringTemplate(string.Template):
    delimiter = '$$'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--json-editor-dist-dir', default='../../json-editor/dist')
    parser.add_argument('--bootstrap-dist-dir', default='../../bootstrap-3.2.0-dist')
    parser.add_argument('--rm', action='store_true')
    args = parser.parse_args()
    
    # Build editor schema.
    the_editor = aprinter_config_editor.editor()
    schema_json = json.dumps(the_editor._json_schema(), indent=2)
    
    # Determine directories.
    src_dir = os.path.dirname(os.path.realpath(__file__))
    dist_dir = os.path.join(src_dir, 'dist')
    
    # Remove dist dir.
    if args.rm and os.path.isdir(dist_dir):
        shutil.rmtree(dist_dir)
    
    # Create dist dir.
    os.mkdir(dist_dir)
    
    # Copy json-editor.
    shutil.copytree(args.json_editor_dist_dir, os.path.join(dist_dir, 'json-editor'))
    
    # Coppy Bootstrap.
    shutil.copytree(args.bootstrap_dist_dir, os.path.join(dist_dir, 'bootstrap'))
    
    # Copy index.html.
    shutil.copyfile(os.path.join(src_dir, 'index.html'), os.path.join(dist_dir, 'index.html'))
    
    # Build and write init.js.
    with open(os.path.join(src_dir, 'init.js'), 'rb') as f:
        init_js_template = f.read()
    init_js = MyStringTemplate(init_js_template).substitute({'SCHEMA': schema_json})
    with open(os.path.join(dist_dir, 'init.js'), 'wb') as f:
        f.write(init_js)

main()
