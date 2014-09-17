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

class ConfigBase (object):
    def __init__ (self, **kwargs):
        self.title = _kwarg_maybe('title', kwargs)
        self.collapsed = _kwarg_maybe('collapsed', kwargs, False)
        self.disable_collapse = _kwarg_maybe('disable_collapse', kwargs, False)
        self.ident = _kwarg_maybe('ident', kwargs)
        self.enum = _kwarg_maybe('enum', kwargs)
        self.kwargs = kwargs
    
    def _json_schema (self):
        return _merge_dicts(
            ({
                'title': self.title
            } if self.title is not None else {}),
            ({
                'id': self.ident
            } if self.ident is not None else {}),
            ({
                'options': {
                    'collapsed': True
                }
            } if self.collapsed else {}),
            ({
                'options': {
                    'disable_collapse': True
                }
            } if self.disable_collapse else {}),
            ({
                'enum': self.enum
            } if self.enum is not None else {}),
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

class Compound (ConfigBase):
    def __init__ (self, name, **kwargs):
        self.name = name
        self.attrs = _kwarg('attrs', kwargs)
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'type': 'object',
            'required': ['_compoundName'] + [param.kwargs['key'] for param in self.attrs],
            'additionalProperties': False,
            'properties': _merge_dicts(
                {
                    '_compoundName':  {
                        'type': 'string',
                        'enum': [self.name],
                        'options': {
                            'hidden': True
                        }
                    }
                },
                *(
                    [
                        {
                            param.kwargs['key']: _merge_dicts(
                                param._json_schema(),
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
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'type': 'array',
            'items': self.elem._json_schema()
        }

class Reference (ConfigBase):
    def __init__ (self, **kwargs):
        self.ref_array = _kwarg('ref_array', kwargs)
        self.ref_id_key = _kwarg('ref_id_key', kwargs)
        self.ref_name_key = _kwarg('ref_name_key', kwargs)
        self.deref_key = _kwarg('deref_key', kwargs)
        ConfigBase.__init__(self, **kwargs)
    
    def _json_extra (self):
        return {
            'type': 'string',
            'watch': {
                'watch_array': self.ref_array
            },
            'enumSource': [
                {
                    'source': 'watch_array',
                    'title': 'return vars.item[{}];'.format(json.dumps(self.ref_name_key)),
                    'value': 'return vars.item[{}];'.format(json.dumps(self.ref_id_key))
                }
            ]
        }
    
    def _json_new_properties (self, container_id):
        assert container_id is not None
        return {
            self.deref_key: {
                'watch': {
                    'watch_array': self.ref_array,
                    'watch_id': '{}.{}'.format(container_id, self.kwargs['key'])
                },
                'template': 'return aprinter_resolve_ref(vars.watch_array, {}, vars.watch_id);'.format(json.dumps(self.ref_id_key)),
                'options': {
                    'derived': True
                }
            }
        }
