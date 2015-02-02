def assign_func(where, key):
    def wrap(func):
        setattr(where, key, func)
        return func
    return wrap
