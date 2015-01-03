import string
import os
import sys
import contextlib

def read_file(path):
    with open(path, 'rb') as f:
        return f.read()

def write_file(path, data):
    with open(path, 'wb') as f:
        f.write(data)

@contextlib.contextmanager
def use_input_file(path):
    if path == '-':
        yield sys.stdin
    else:
        with open(path, 'rb') as f:
            yield f

@contextlib.contextmanager
def use_output_file(path):
    if path == '-':
        yield sys.stdout
        sys.stdout.flush()
    else:
        with open(path, 'wb') as f:
            yield f
            f.close()

class RichTemplate(string.Template):
    delimiter = '$$'

def file_dir(file_path):
    return os.path.dirname(os.path.realpath(file_path))

class SelectionError (Exception):
    pass

class Selection (object):
    def __init__ (self):
        self._options = {}
    
    def option (self, name):
        def wrap (func):
            self._options[name] = func
            return func
        return wrap
    
    def options (self, names):
        def wrap (func):
            for name in names:
                def wrapped_func (*args, **kwargs):
                    return func(name, *args, **kwargs)
                self._options[name] = wrapped_func
            return func
        return wrap
    
    def run (self, choice, *args, **kwargs):
        if choice not in self._options:
            raise SelectionError()
        return self._options[choice](*args, **kwargs)

def escape_string_for_nix(data):
    return '"{}"'.format(''.join(('\\{}'.format(c) if c in ('"', '\\', '$') else c) for c in data))

class FunctionDefinedClass(object):
    def __init__(self, function):
        function(self)
