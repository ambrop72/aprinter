import os
import tty
import termios
import littlevent.close
import littlevent.loop
import littlevent.fd_io

class Serial (littlevent.close.Obj):
    def __init__ (self, loop, device_path, baud_rate, error_handler):
        littlevent.close.Obj.__init__(self)
        try:
            try:
                fd_num = os.open(device_path, os.O_RDWR)
            except OSError as e:
                raise littlevent.error.Error(e)
            self.fd_io = self.add(littlevent.fd_io.FileDescriptor(loop, fd_num, True, error_handler))
            try:
                tty.setraw(fd_num)
            except termios.error as e:
                raise littlevent.error.Error(e)
        
        except littlevent.error.Error:
            self.close()
            raise
    
    def read_io (self):
        return self.fd_io
    
    def write_io (self):
        return self.fd_io
