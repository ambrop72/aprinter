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
import json

def _kwarg_maybe (name, kwargs, default=None):
    if name in kwargs:
        res = kwargs[name]
        del kwargs[name]
    else:
        res = default
    return res

def _kwarg (name, kwargs):
    assert name in kwargs
    res = kwargs[name]
    del kwargs[name]
    return res

def _merge_dicts (*dicts):
    res = dict()
    for d in dicts:
        assert type(d) is dict
        for (key, value) in d.items():
            if key not in res:
                res[key] = value
            else:
                assert type(value) is dict
                assert type(res[key]) is dict
                res[key] = _merge_dicts(res[key], value)
    return res

def _json_type_of (x):
    if type(x) is dict:
        return 'object'
    if type(x) is list:
        return 'array'
    if type(x) is str:
        return 'string'
    if type(x) is int or type(x) is long:
        return 'integer'
    if type(x) is float:
        return 'number'
    raise TypeError('wrong type')

class ConfigBase (object):
    def __init__ (self, **kwargs):
        self.title = _kwarg_maybe('title', kwargs)
        self.title_key = _kwarg_maybe('title_key', kwargs)
        self.collapsable = _kwarg_maybe('collapsable', kwargs, False)
        self.collapsed_initially = _kwarg_maybe('collapsed_initially', kwargs, True)
        self.ident = _kwarg_maybe('ident', kwargs)
        self.enum = _kwarg_maybe('enum', kwargs)
        self.no_header = _kwarg_maybe('no_header', kwargs, False)
        self.processing_order = _kwarg_maybe('processing_order', kwargs)
        self.default = _kwarg_maybe('default', kwargs)
        self.kwargs = kwargs
    
    def json_schema (self):
        return _merge_dicts(
            ({
                'title': self.title
            } if self.title is not None else {}),
            ({
                'headerTemplate': 'return vars.self[{}];'.format(json.dumps(self.title_key))
            } if self.title_key is not None else {}),
            ({
                'id': self.ident
            } if self.ident is not None else {}),
            ({
                'options': {
                    'disable_collapse': True
                }
            } if not self.collapsable else {}),
            ({
                'options': {
                    'collapsed': True
                }
            } if self.collapsable and self.collapsed_initially else {}),
            ({
                'options': {
                    'no_header': True
                }
            } if self.no_header else {}),
            ({
                'processingOrder': self.processing_order
            } if self.processing_order is not None else {}),
            ({
                'enum': self.enum
            } if self.enum is not None else {}),
            ({
                'default': self.default
            } if self.default is not None else {}),
            self._json_extra()
        )
    
    def _json_new_properties (self, container_id):
        return {}

class String (ConfigBase):
    def _json_extra (self):
        return {
            'type': 'string'
        }

class Integer (ConfigBase):
    def _json_extra (self):
        return {
            'type': 'integer'
        }

class Float (ConfigBase):
    def _json_extra (self):
        return {
            'type': 'number'
        }

class Boolean (ConfigBase):
    def __init__ (self, **kwargs):
        self.false_title = _kwarg_maybe('false_title', kwargs, 'No')
        self.true_title = _kwarg_maybe('true_title', kwargs, 'Yes')
        self.first_value = _kwarg_maybe('first_value', kwargs, False)
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'type': 'boolean',
            'enum': self._order([False, True]),
            'options': {
                'enum_titles': self._order([self.false_title, self.true_title])
            }
        }
    
    def _order (self, x):
        return list(reversed(x)) if self.first_value else x

class Compound (ConfigBase):
    def __init__ (self, name, **kwargs):
        self.name = name
        self.attrs = _kwarg('attrs', kwargs)
        if 'title' not in kwargs:
            kwargs['title'] = name
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'type': 'object',
            'additionalProperties': False,
            'properties': _merge_dicts(
                {
                    '_compoundName':  {
                        'constantValue': self.name,
                        'options': {
                            'hidden': True
                        },
                    }
                },
                *(
                    [
                        {
                            param.kwargs['key']: _merge_dicts(
                                param.json_schema(),
                                {
                                    'propertyOrder' : i
                                }
                            )
                        }
                        for (i, param) in enumerate(self.attrs)
                    ] + [
                        param._json_new_properties(self.ident)
                        for (i, param) in enumerate(self.attrs)
                    ]
                )
            )
        }

class Array (ConfigBase):
    def __init__ (self, **kwargs):
        self.elem = _kwarg('elem', kwargs)
        self.table = _kwarg_maybe('table', kwargs, False)
        self.copy_name_key = _kwarg_maybe('copy_name_key', kwargs)
        self.copy_name_suffix = _kwarg_maybe('copy_name_suffix', kwargs, ' (copy)')
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return _merge_dicts(
            {
                'type': 'array',
                'items': self.elem.json_schema()
            },
            ({
                'format': 'table'
            } if self.table else {}),
            ({
                'copyTemplate': 'return ce_copyhelper(vars.rows,vars.row,{},{});'.format(json.dumps(self.copy_name_key), json.dumps(self.copy_name_suffix))
            } if self.copy_name_key is not None else {}),
        )

class OneOf (ConfigBase):
    def __init__ (self, **kwargs):
        self.choices = _kwarg('choices', kwargs)
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'oneOf': [choice.json_schema() for choice in self.choices],
            'selectKey': '_compoundName',
        }

class Constant (ConfigBase):
    def __init__ (self, **kwargs):
        self.value = _kwarg('value', kwargs)
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'constantValue': self.value,
            'options': {
                'hidden': True
            },
        }

class Reference (ConfigBase):
    def __init__ (self, **kwargs):
        self.ref_array = _kwarg('ref_array', kwargs)
        self.ref_id_key = _kwarg('ref_id_key', kwargs)
        self.ref_name_key = _kwarg('ref_name_key', kwargs)
        self.deref_key = _kwarg_maybe('deref_key', kwargs)
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'type': 'string',
            'watch': {
                'watch_array': self.ref_array['base']
            },
            'enumSource': {
                'sourceTemplate': 'return {};'.format(self._array_expr()),
                'title': 'return vars.item[{}];'.format(json.dumps(self.ref_name_key)),
                'value': 'return vars.item[{}];'.format(json.dumps(self.ref_id_key)),
            },
        }
    
    def _json_new_properties (self, container_id):
        assert container_id is not None
        return ({
            self.deref_key: {
                'watch': {
                    'watch_array': self.ref_array['base'],
                    'watch_id': '{}.{}'.format(container_id, self.kwargs['key'])
                },
                'valueTemplate': 'return ce_deref({},{},vars.watch_id);'.format(self._array_expr(), json.dumps(self.ref_id_key)),
                'excludeFromFinalValue': True
            }
        } if self.deref_key is not None else {})
    
    def _array_expr (self):
        return 'ce_refarr(vars,["watch_array"{}])'.format(''.join(',{}'.format(json.dumps(attr)) for attr in self.ref_array['descend']))
