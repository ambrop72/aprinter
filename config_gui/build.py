from __future__ import print_function
import argparse
import os
import errno
import shutil
import string
import json
import configema as ce

def simple_list(elem_title, value_title, **kwargs):
    return ce.Array(table=True, elem=ce.Compound('elem', title=elem_title, attrs=[
        ce.String(key='value', title=value_title)
    ]), **kwargs)

def interrupt_timer_choice(**kwargs):
    return ce.Compound('interrupt_timer', ident='id_interrupt_timer_choice', collapsed=True, attrs=[
        ce.Reference(key='oc_unit', ref_array='id_configuration.board_data', ref_array_descend=['clock', 'avail_oc_units'], ref_id_key='value', ref_name_key='value', title='Output compare unit', deref_key='lalal')
    ], **kwargs)

def pin_choice(**kwargs):
    return ce.String(**kwargs)

def editor():
    return ce.Compound('editor', title='Configuration editor', disable_collapse=True, no_header=True, attrs=[
        ce.Array(key='configurations', title='Configurations', elem=ce.Compound('config', key='config', ident='id_configuration', title='Configuration', title_key='name', collapsed=True, attrs=[
            ce.String(key='name', title='Name'),
            ce.Reference(key='board_id', ref_array='boards', ref_id_key='identifier', ref_name_key='name', deref_key='board_data', title='Board'),
            pin_choice(key='LedPin', title='LED pin'),
            ce.Float(key='InactiveTime', title='Inactive time'),
            ce.Compound('serial', key='serial', title='Serial', collapsed=True, attrs=[
                ce.Integer(key='BaudRate', title='Baud rate'),
                ce.Integer(key='RecvBufferSizeExp', title='Receive buffer size (power of two exponent)'),
                ce.Integer(key='SendBufferSizeExp', title='Send buffer size (power of two exponent)'),
                ce.Integer(key='GcodeMaxParts', title='Max parts in GCode command')
            ]),
            ce.Compound('advanced', key='advanced', title='Advanced parameters', collapsed=True, attrs=[
                ce.Float(key='LedBlinkInterval', title='LED blink interval'),
                ce.Float(key='MaxStepsPerCycle', title='Max steps per cycle'),
                ce.Integer(key='StepperSegmentBufferSize', title='Stepper segment buffer size'),
                ce.Integer(key='EventChannelBufferSize', title='Event channel buffer size'),
                ce.Integer(key='LookaheadBufferSize', title='Lookahead buffer size'),
                ce.Integer(key='LookaheadCommitCount', title='Lookahead commit count'),
                ce.Float(key='ForceTimeout', title='Force timeout'),
                ce.String(key='FpType', enum=['float', 'double']),
                interrupt_timer_choice(key='EventChannelTimer', title='Event channel timer')
            ]),
            ce.Array(key='steppers', title='Steppers', elem=ce.Compound('stepper', title='Stepper', title_key='Name', attrs=[
                ce.String(key='Name', title='Name'),
                pin_choice(key='DirPin', title='Direction pin'),
                pin_choice(key='StepPin', title='Step pin'),
                pin_choice(key='EnablePin', title='Enable pin'),
                ce.Boolean(key='InvertDir', title='Invert direction'),
                ce.Float(key='StepsPerUnit', title='Steps per unit'),
                ce.Float(key='MinPos', title='Minimum position'),
                ce.Float(key='MaxPos', title='Maximum position'),
                ce.Float(key='MaxSpeed', title='Maximum speed'),
                ce.Float(key='MaxAccel', title='Maximum acceleration'),
                ce.Float(key='DistanceFactor', title='Distance factor'),
                ce.Float(key='CorneringDistance', title='Cornering distance'),
                ce.Boolean(key='EnableCartesianSpeedLimit', title='Is cartesian'),
                ce.Integer(key='StepBits', title='Step bits'),
                interrupt_timer_choice(key='StepperTimer', title='Stepper timer'),
                ce.OneOf(key='homing', title='Homing', collapsed=True, choices=[
                    ce.Compound('no_homing', attrs=[]),
                    ce.Compound('homing', attrs=[
                        ce.String(key='HomeDir', title='Homing direction', enum=['Negative', 'Positive']),
                        pin_choice(key='HomeEndPin', title='Endstop pin'),
                        # input mode
                        ce.Boolean(key='HomeEndInvert', title='Invert endstop'),
                        ce.Float(key='HomeFastMaxDist', title='Maximum fast (initial) travel'),
                        ce.Float(key='HomeRetractDist', title='Retraction travel'),
                        ce.Float(key='HomeSlowMaxDist', title='Maximum slow (after retraction) travel'),
                        ce.Float(key='HomeFastSpeed', title='Fast speed'),
                        ce.Float(key='HomeRetractSpeed', title='Retraction speed'),
                        ce.Float(key='HomeSlowSpeed', title='Slow speed')
                    ])
                ])
            ])),
            ce.Array(key='heaters', title='Heaters', elem=ce.Compound('heater', title='Heater', title_key='Name', collapsed=True, attrs=[
                ce.String(key='Name', title='Name'),
                ce.Integer(key='SetMCommand', title='Set command M-number'),
                ce.Integer(key='WaitMCommand', title='Wait command M-number'),
                pin_choice(key='AdcPin', title='Thermistor pin'),
                ce.Compound('temp_conversion', key='temp_conversion', title='Temperature conversion', collapsed=True, attrs=[
                    ce.Float(key='ResistorR', title='Resistor resistance'),
                    ce.Float(key='R0', title='Thermistor resistance'),
                    ce.Float(key='Beta', title='Thermistor beta value'),
                    ce.Float(key='MinTemp', title='Minimum temperature'),
                    ce.Float(key='MaxTemp', title='Maximum temperature')
                ])
            ])),
            ce.Array(key='fans', title='Fans', elem=ce.Compound('fan', title='Fan', attrs=[
                ce.Integer(key='x', title='X'),
                ce.Integer(key='y', title='Y'),
            ]))
        ])),
        ce.Array(key='boards', title='Boards', elem=ce.Compound('board', title='Board', title_key='name', collapsed=True, ident='id_board', attrs=[
            ce.String(key='identifier', title='Identifier'),
            ce.String(key='name', title='Name'),
            ce.OneOf(key='clock', title='Clock', collapsed=True, choices=[
                ce.Compound('At91Sam3xClock', attrs=[
                    ce.Integer(key='prescaler', title='Prescaler'),
                    ce.String(key='primary_timer', title='Primary timer'),
                    simple_list(key='avail_oc_units', title='Available output compare units', elem_title='OC unit', value_title='OC unit (e.g. TC0A)')
                ])
            ]),
            ce.OneOf(key='adc', title='ADC', collapsed=True, choices=[
                ce.Compound('At91SamAdc', attrs=[
                    ce.Float(key='freq', title='Frequency'),
                    ce.Float(key='avg_interval', title='Averaging interval'),
                    ce.Float(key='smoothing', title='Smoothing factor'),
                    ce.Integer(key='startup', title='Startup time'),
                    ce.Integer(key='settling', title='Settling time'),
                    ce.Integer(key='tracking', title='Tracking time'),
                    ce.Integer(key='transfer', title='Transfer time')
                ])
            ]),
            ce.OneOf(key='watchdog', title='Watchdog', collapsed=True, choices=[
                ce.Compound('At91SamWatchdog', attrs=[
                    ce.Integer(key='Wdv', title='Wdv')
                ])
            ]),
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
