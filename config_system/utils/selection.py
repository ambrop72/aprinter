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
