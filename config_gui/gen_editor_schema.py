from __future__ import print_function
import json
import configema

def main():
    editor = configema.Compound('editor', title='Configuration editor', disable_collapse=True, attrs=[
        configema.Array(key='configurations', title='Configurations', elem=configema.Compound('config', key='config', ident='id_configuration', title='Configuration', title_key='name', collapsed=True, attrs=[
            configema.String(key='name', title='Name'),
            configema.Reference(key='board_id', ref_array='boards', ref_id_key='identifier', ref_name_key='name', deref_key='board_data', title='Board'),
            configema.Array(key='fans', title='Fans', elem=configema.Compound('fan', title='Fan', attrs=[
                configema.Integer(key='x', title='X'),
                configema.Integer(key='y', title='Y'),
            ]))
        ])),
        configema.Array(key='boards', title='Boards', collapsed=True, elem=configema.Compound('board', title='Board', title_key='name', collapsed=True, attrs=[
            configema.String(key='identifier', title='Identifier'),
            configema.String(key='name', title='Name'),
            configema.String(key='clockImpl', title='Clock implementation', enum=['At91Sam3uClock', 'At91Sam3xClock', 'AvrClock', 'Mk20Clock'])
        ]))
    ])
    
    print(json.dumps(editor._json_schema(), indent=2))

main()
