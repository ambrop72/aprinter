from __future__ import print_function
import argparse
import os
import errno
import shutil
import string
import json
import configema

def simple_list(elem_title, value_title, **kwargs):
    return configema.Array(table=True, elem=configema.Compound('elem', title=elem_title, attrs=[
        configema.String(key='value', title=value_title)
    ]), **kwargs)

def interrupt_timer_choice(**kwargs):
    return configema.Compound('interrupt_timer', ident='id_interrupt_timer_choice', attrs=[
        configema.Reference(key='oc_unit', ref_array='id_configuration.board_data', ref_array_descend=['clock', 'avail_oc_units'], ref_id_key='value', ref_name_key='value', title='OC unit')
    ], **kwargs)

def editor():
    return configema.Compound('editor', title='Configuration editor', disable_collapse=True, no_header=True, attrs=[
        configema.Array(key='configurations', title='Configurations', elem=configema.Compound('config', key='config', ident='id_configuration', title='Configuration', title_key='name', collapsed=True, attrs=[
            configema.String(key='name', title='Name'),
            configema.Reference(key='board_id', ref_array='boards', ref_id_key='identifier', ref_name_key='name', deref_key='board_data', title='Board'),
            interrupt_timer_choice(key='event_channel_timer', title='Event channel timer'),
            configema.Array(key='fans', title='Fans', elem=configema.Compound('fan', title='Fan', attrs=[
                configema.Integer(key='x', title='X'),
                configema.Integer(key='y', title='Y'),
            ]))
        ])),
        configema.Array(key='boards', title='Boards', elem=configema.Compound('board', title='Board', title_key='name', collapsed=True, ident='id_board', attrs=[
            configema.String(key='identifier', title='Identifier'),
            configema.String(key='name', title='Name'),
            configema.OneOf(key='clock', title='Clock', collapsed=True, choices=[
                configema.Compound('At91Sam3xClock', attrs=[
                    configema.Integer(key='prescaler', title='Prescaler'),
                    configema.String(key='primary_timer', title='Primary timer'),
                    simple_list(key='avail_oc_units', title='Available output compare units', elem_title='OC unit', value_title='OC unit (e.g. TC0A)')
                ])
            ]),
            configema.OneOf(key='adc', title='ADC', collapsed=True, choices=[
                configema.Compound('At91SamAdc', attrs=[
                    configema.Float(key='freq', title='Frequency'),
                    configema.Float(key='avg_interval', title='Averaging interval'),
                    configema.Float(key='smoothing', title='Smoothing factor'),
                    configema.Integer(key='startup', title='Startup time'),
                    configema.Integer(key='settling', title='Settling time'),
                    configema.Integer(key='tracking', title='Tracking time'),
                    configema.Integer(key='transfer', title='Transfer time')
                ])
            ])
        ]))
    ])

class MyStringTemplate(string.Template):
    delimiter = '$$'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--json-editor-dist-dir', default='../../json-editor/dist')
    parser.add_argument('--bootstrap-dist-dir', default='../../bootstrap-3.2.0-dist')
    parser.add_argument('--rm', action='store_true')
    args = parser.parse_args()
    
    # Build schema.
    the_editor = editor()
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
    with open(os.path.join(src_dir, 'init.js.template'), 'rb') as f:
        init_js_template = f.read()
    init_js = MyStringTemplate(init_js_template).substitute({'SCHEMA': schema_json})
    with open(os.path.join(dist_dir, 'init.js'), 'wb') as f:
        f.write(init_js)

main()
