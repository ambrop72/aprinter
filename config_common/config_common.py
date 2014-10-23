import string
import os

def read_file(path):
    with open(path, 'rb') as f:
        return f.read()

def write_file(path, data):
    with open(path, 'wb') as f:
        f.write(data)

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
    
    def run (self, choice, *args, **kwargs):
        if choice not in self._options:
            raise SelectionError()
        return self._options[choice](*args, **kwargs)
