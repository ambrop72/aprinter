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

def file_dir(file_path):
    return os.path.dirname(os.path.realpath(file_path))
