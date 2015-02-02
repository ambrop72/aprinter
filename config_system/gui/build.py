from __future__ import print_function
import sys
import os
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '../utils'))
import argparse
import shutil
import json
import file_utils
import rich_template
import aprinter_config_editor

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--json-editor-dist-dir', default='../../../json-editor/dist')
    parser.add_argument('--bootstrap-dist-dir', default='../../../bootstrap-3.2.0-dist')
    parser.add_argument('--filesaver-dir', default='../../../FileSaver.js')
    parser.add_argument('--rm', action='store_true')
    args = parser.parse_args()
    
    # Build editor schema.
    the_editor = aprinter_config_editor.editor()
    editor_schema = the_editor._json_schema()
    
    # Determine directories.
    src_dir = file_utils.file_dir(__file__)
    dist_dir = os.path.join(src_dir, 'dist')
    
    # Remove dist dir.
    if args.rm and os.path.isdir(dist_dir):
        shutil.rmtree(dist_dir)
    
    # Create dist dir.
    os.mkdir(dist_dir)
    
    # Copy json-editor.
    shutil.copytree(args.json_editor_dist_dir, os.path.join(dist_dir, 'json-editor'))
    
    # Copy Bootstrap.
    shutil.copytree(args.bootstrap_dist_dir, os.path.join(dist_dir, 'bootstrap'))
    
    # Copy FileSaver.
    shutil.copyfile(os.path.join(args.filesaver_dir, 'FileSaver.min.js'), os.path.join(dist_dir, 'FileSaver.min.js'))
    
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
