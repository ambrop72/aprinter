class Obj (object):
    def __init__ (self):
        self.close_actions = []
    
    def add (self, obj):
        if obj is not None:
            action = obj.close if hasattr(obj, 'close') else obj
            self.close_actions.append(action)
        return obj
    
    def close (self):
        for action in reversed(self.close_actions):
            action()
