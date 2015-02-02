class SelectionError (Exception):
    pass

class Selection (object):
    def __init__ (self):
        self._options = {}
        self._default = {}
    
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
    
    def default (self):
        def wrap (func):
            self._default[True] = func
            return func
        return wrap
    
    def run (self, choice, *args, **kwargs):
        if choice not in self._options:
            if True not in self._default:
                raise SelectionError()
            return self._default[True](choice, *args, **kwargs)
        return self._options[choice](*args, **kwargs)
