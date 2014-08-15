#!/usr/bin/python2.7 -B

from __future__ import print_function
import sys
import argparse
import time
import littlevent.close
import littlevent.error
import littlevent.loop
import littlevent.fd_io
import littlevent.serial

class Program (littlevent.close.Obj):
    def __init__ (self):
        littlevent.close.Obj.__init__ (self)
        try:
            parser = argparse.ArgumentParser(description='Test 3D printer serial port latency.')
            parser.add_argument('--port', required=True, help='Serial port device.')
            parser.add_argument('--baud', type=int, required=True, help='Baud rate.')
            parser.add_argument('--count', type=int, default=5000, help='Number of commands.')
            args = parser.parse_args()
            print(args.port)
            print(args.baud)
            
            self.loop = self.add(littlevent.loop.Loop())
            self.serial = self.add(littlevent.serial.Serial(self.loop, args.port, args.baud, self._error_handler))
            self.serial.read_io().read_set_handler(self._read_handler)
            self.serial.write_io().write_set_handler(self._write_handler)
            
            self.want_count = args.count
            self.done_count = 0
            self.writing = False
            self.start_time = time.time()
            self.frame = ''
            
            self._read()
            self._write()
            
        except littlevent.error.Error as e:
            self.close()
            print('ERROR: {}'.format(e))
            sys.exit(1)
    
    def _error_handler (self, returned_events):
        print('ERROR: unexpected event.')
        self._quit()
    
    def _read_handler (self, data, err):
        if err is not None:
            print('ERROR: read error: {}'.format(err))
            return self._quit()
        self._read()
        while True:
            newline_pos = data.find('\n')
            if newline_pos < 0:
                self.frame += data
                return
            response = self.frame + data[:newline_pos]
            self.frame = ''
            data = data[(newline_pos + 1):]
            if len(response) > 0 and response[-1] == '\r':
                response = response[:-1]
            if not response.startswith('ok'):
                print('Unknown line received: >{}<'.format(response))
            else:
                if self.writing:
                    print('ERROR: early response')
                    return self._quit()
                self.done_count += 1
                if self.done_count >= self.want_count:
                    return self._finished()
                self._write()
        
    def _write_handler (self, err):
        assert self.writing
        if err is not None:
            print('ERROR: write error: {}'.format(err))
            return self._quit()
        self.writing = False
    
    def _read (self):
        self.serial.read_io().read_start(512)
    
    def _write (self):
        assert not self.writing
        msg = 'G1\n'
        self.serial.write_io().write_start(msg)
        self.writing = True
    
    def _quit (self):
        print('Quitting.')
        self.loop.quit(1)
    
    def _finished (self):
        total_time = time.time() - self.start_time
        print('Done {} requests in {} seconds.'.format(self.done_count, total_time))
        print('Average request time is {} seconds.'.format(total_time / self.done_count))
        self.loop.quit(0)
    
p = Program()
ret = p.loop.run()
p.close()
sys.exit(ret)
