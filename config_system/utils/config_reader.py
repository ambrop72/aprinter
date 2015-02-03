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

class ConfigError (Exception):
    pass

def _raise_config_error (path, msg):
    return

class ConfigPath (object):
    def __init__ (self, parent, name, key=None):
        self._parent = parent
        self._name = name
        self._key = key
    
    def get_text (self):
        return '.'.join(c._get_comp_text() for c in self._get_components())
    
    def error (self, msg):
        raise ConfigError('{}: {}'.format(self.get_text(), msg))
    
    def warning (self, msg):
        print('Warning: {}: {}'.format(self.get_text(), msg), file=sys.stderr)
    
    def _get_comp_text (self):
        return self._name if self._key is None else '{}[{}]'.format(self._name, self._key) 
    
    def _get_components (self):
        comps = []
        c = self
        while c is not None:
            comps.append(c)
            c = c._parent
        comps.reverse()
        return comps

class ConfigTypeBool (object):
    def read (self, path, val):
        if type(val) is not bool:
            path.error('Must be a boolean.')
        return val

class ConfigTypeInt (object):
    def read (self, path, val):
        if type(val) is not int and type(val) is not long:
            path.error('Must be an integer.')
        return val

class ConfigTypeFloat (object):
    def read (self, path, val):
        try:
            res = float(val)
        except (TypeError, ValueError, OverflowError):
            path.error('Must be convertible to float.')
        return res

class ConfigTypeString (object):
    def read (self, path, val):
        if type(val) is str:
            res = val
        elif type(val) is unicode:
            try:
                res = val.encode('utf-8')
            except UnicodeEncodeError:
                path.error('Cannot encode to UTF-8.')
        else:
            path.error('Must be str/unicode.')
        return res

class ConfigTypeList (object):
    def __init__ (self, elem_dtype, max_count=-1):
        self._elem_dtype = elem_dtype
        self._max_count = max_count
    
    def read (self, path, val):
        if type(val) is not list:
            path.error('Must be a list.')
        if self._max_count >= 0 and len(val) > self._max_count:
            path.error('Too many elements (maximum is {}).'.format(self._max_count))
        res = [self._elem_dtype.read(ConfigPath(path, str(i)), elem) for (i, elem) in enumerate(val)]
        return res

class ConfigTypeConfig (object):
    def __init__ (self, config_factory):
        self._config_factory = config_factory
    
    def read (self, path, val):
        if type(val) is not dict:
            path.error('Must be a dict.')
        return self._config_factory(val, path)

class ConfigReader (object):
    def __init__ (self, obj, path):
        assert type(obj) is dict
        self._obj = obj
        self._path = path
    
    def config_factory (self, obj, path):
        return type(self)(obj, path)
    
    def config_type (self):
        return ConfigTypeConfig(self.config_factory)
    
    def has (self, key):
        return key in self._obj
    
    def get (self, dtype, key):
        path = self.key_path(key)
        if key not in self._obj:
            path.error('Attribute missing.')
        val = dtype.read(path, self._obj[key])
        return val
    
    def enter (self):
        yield self
    
    def path (self):
        return self._path
    
    def key_path (self, key):
        return ConfigPath(self._path, key)
    
    def get_bool (self, key):
        return self.get(ConfigTypeBool(), key)
    
    def get_int (self, key):
        return self.get(ConfigTypeInt(), key)

    def get_float (self, key):
        return self.get(ConfigTypeFloat(), key)

    def get_string (self, key):
        return self.get(ConfigTypeString(), key)
    
    def get_list (self, elem_dtype, key, **list_kwargs):
        return self.get(ConfigTypeList(elem_dtype, **list_kwargs), key)
    
    def get_config (self, key):
        return self.get(self.config_type(), key)
    
    def enter_config (self, key):
        return self.get_config(key).enter()
    
    def iter_list_config (self, key, **list_kwargs):
        for config in self.get_list(self.config_type(), key, **list_kwargs):
            yield config
    
    def get_elem_by_id (self, key, id_key, id_val):
        path = self.key_path(key)
        found_config = None
        for config in self.get_list(self.config_type(), key):
            if config.get_string(id_key) == id_val:
                if found_config is not None:
                    path.error('Duplicate {} values detected.'.format(repr(id_key)))
                found_config = config
        if found_config is None:
            path.error('No element found with {} equal to {}.'.format(repr(id_key), repr(id_val)))
        return found_config
    
    def enter_elem_by_id (self, key, id_key, id_val):
        return self.get_elem_by_id(key, id_key, id_val).enter()

def start (obj, root_name='config', config_reader_class=ConfigReader, **kwargs):
    path = ConfigPath(None, root_name)
    if type(obj) is not dict:
        path.error('Must be a dict.')
    yield config_reader_class(obj, path, **kwargs) 
