import select
import signal
import littlevent.close
import littlevent.error

FdEventRead = 2**0
FdEventWrite = 2**1
FdEventError = 2**2
FdEventHup = 2**3

class Loop (littlevent.close.Obj):
    def __init__ (self):
        littlevent.close.Obj.__init__(self)
        try:
            signal.signal(signal.SIGINT, signal.SIG_DFL)
            try:
                self.epoll = self.add(select.epoll())
            except socket.error as e:
                raise littlevent.error.Error(e)
            self.quitting = False
            self.fds = {}
            self.pending_fds = set()
            self.pending_lifo = []
            def check_close ():
                assert len(self.pending_lifo) == 0
                assert len(self.pending_fds) == 0
                assert len(self.fds) == 0
            self.add(check_close)
        
        except littlevent.error.Error:
            self.close()
            raise
    
    def run (self):
        self._process_lifo()
        while not self.quitting:
            results = self.epoll.poll()
            self.pending_fds = dict((self.fds[fd_num], epoll_returned_events) for (fd_num, epoll_returned_events) in results)
            while not self.quitting and len(self.pending_fds) != 0:
                fd_obj, epoll_returned_events = self.pending_fds.popitem()
                returned_events = 0
                if (fd_obj.events & FdEventRead) and (epoll_returned_events & select.EPOLLIN):
                    returned_events |= FdEventRead
                if (fd_obj.events & FdEventWrite) and (epoll_returned_events & select.EPOLLOUT):
                    returned_events |= FdEventWrite
                if (epoll_returned_events & select.EPOLLERR):
                    returned_events |= FdEventError
                if (epoll_returned_events & select.EPOLLHUP):
                    returned_events |= FdEventHup
                if returned_events != 0:
                    fd_obj.handler(returned_events)
                    self._process_lifo()
        return self.return_value
    
    def quit (self, return_value=0):
        self.quitting = True
        self.return_value = return_value
    
    def _process_lifo (self):
        while not self.quitting and len(self.pending_lifo) != 0:
            lifo_event = self.pending_lifo.pop()
            assert lifo_event.pending
            lifo_event.pending = False
            lifo_event.handler()

class LifoEvent (littlevent.close.Obj):
    def __init__ (self, loop, handler):
        self.loop = loop
        self.handler = handler
        littlevent.close.Obj.__init__(self)
        self.pending = False
        def on_close ():
            if self.pending:
                self.loop.pending_lifo.remove(self)
        self.add(on_close)
    
    def push (self):
        if self.pending:
            self.loop.pending_lifo.remove(self)
        self.loop.pending_lifo.append(self)
        self.pending = True
        
    def remove (self):
        if self.pending:
            self.loop.pending_lifo.remove(self)
            self.pending = False

class FileDescriptor (littlevent.close.Obj):
    def __init__ (self, loop, fd_num, handler):
        assert fd_num not in loop.fds
        self.loop = loop
        self.fd_num = fd_num
        self.handler = handler
        littlevent.close.Obj.__init__(self)
        try:
            self.events = 0
            self.loop.epoll.register(self.fd_num)
            self.loop.fds[self.fd_num] = self
            def close_it ():
                self.loop.pending_fds.pop(self, None)
                del self.loop.fds[self.fd_num]
                self.loop.epoll.unregister(self.fd_num)
            self.add(close_it)
        except select.error as e:
            self.close()
            raise littlevent.error.Error(e)
    
    def set_events (self, events):
        assert (events & ~(FdEventRead | FdEventWrite)) == 0
        if events == self.events:
            return
        epoll_events = 0
        if events & FdEventRead:
            epoll_events |= select.EPOLLIN
        if events & FdEventWrite:
            epoll_events |= select.EPOLLOUT
        try:
            self.loop.epoll.modify(self.fd_num, epoll_events)
        except select.error as e:
            raise littlevent.error.Error(e)
        self.events = events
    
    def add_events (self, events):
        self.set_events(self.events | events)
    
    def remove_events (self, events):
        self.set_events(self.events & ~events)
