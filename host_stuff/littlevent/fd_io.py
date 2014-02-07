import os
import fcntl
import errno
import littlevent.error
import littlevent.close
import littlevent.loop

class FileDescriptor (littlevent.close.Obj):
    def __init__ (self, loop, fd_num, close_it, error_handler):
        self.fd_num = fd_num
        self.close_it = close_it
        self.error_handler = error_handler
        littlevent.close.Obj.__init__(self)
        self.loop_fd = self.add(littlevent.loop.FileDescriptor(loop, self.fd_num, self._fd_handler))
        self.read_handler = None
        self.read_max_length = 0
        self.read_lifo_event = self.add(littlevent.loop.LifoEvent(loop, self._read_lifo_event_handler))
        self.write_handler = None
        self.write_data = ''
        self.write_lifo_event = self.add(littlevent.loop.LifoEvent(loop, self._write_lifo_event_handler))
        try:
            fl = fcntl.fcntl(self.fd_num, fcntl.F_GETFL)
            fcntl.fcntl(self.fd_num, fcntl.F_SETFL, fl | os.O_NONBLOCK)
        except IOError as e:
            raise littlevent.error.Error(e)
        def on_close ():
            if self.close_it:
                try:
                    os.close(self.fd_num)
                except OSError as e:
                    pass
        self.add(on_close)
    
    def read_set_handler (self, read_handler):
        self.read_handler = read_handler
    
    def read_start (self, read_max_length):
        assert self.read_max_length == 0
        assert read_max_length > 0
        self.read_max_length = read_max_length
        self.read_lifo_event.push()
    
    def write_set_handler (self, write_handler):
        self.write_handler = write_handler
    
    def write_start (self, write_data):
        assert len(self.write_data) == 0
        assert len(write_data) > 0
        self.write_data = write_data
        self.write_lifo_event.push()
    
    def _read_lifo_event_handler (self):
        assert self.read_max_length > 0
        try:
            data = os.read(self.fd_num, self.read_max_length)
        except OSError as e:
            if e.args[0] == errno.EWOULDBLOCK or e.args[0] == errno.EAGAIN:
                self.loop_fd.add_events(littlevent.loop.FdEventRead)
                return
            data = None
            err = e
        else:
            if len(data) == 0:
                data = None
                err = IOError('EOF encountered.')
            else:
                err = None
        self.loop_fd.remove_events(littlevent.loop.FdEventRead)
        self.read_max_length = 0
        return self.read_handler(data, err)
    
    def _write_lifo_event_handler (self):
        assert len(self.write_data) > 0
        while True:
            try:
                written = os.write(self.fd_num, self.write_data)
            except OSError as e:
                if e.args[0] == errno.EWOULDBLOCK or e.args[0] == errno.EAGAIN:
                    self.loop_fd.add_events(littlevent.loop.FdEventWrite)
                    return
                err = e
            else:
                if written == 0:
                    err = IOError('Zero write.')
                else:
                    if written < len(self.write_data):
                        self.write_data = self.write_data[written:]
                        continue
                    err = None
            self.loop_fd.remove_events(littlevent.loop.FdEventWrite)
            self.write_data = ''
            return self.write_handler(err)
    
    def _fd_handler (self, returned_events):
        handled = False
        if self.read_max_length > 0 and (returned_events & littlevent.loop.FdEventRead):
            self.read_lifo_event.push()
            handled = True
        if len(self.write_data) > 0 and (returned_events & littlevent.loop.FdEventWrite):
            self.write_lifo_event.push()
            handled = True
        if not handled:
            return self.error_handler(returned_events)
