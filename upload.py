from __future__ import print_function
import argparse
import sys
import signal

def main():
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--length', type=int, default=1000000)
    parser.add_argument('-c', '--chunked', action='store_true')
    parser.add_argument('-s', '--chunk-size', type=int, default=512)
    parser.add_argument('-p', '--request-path', default='/uploadTest')
    args = parser.parse_args()
    
    assert args.chunk_size > 0
    
    request = ''
    request += 'POST {} HTTP/1.1\r\n'.format(args.request_path)
    request += 'Connection: close\r\n'
    if args.chunked:
        request += 'Transfer-Encoding: Chunked\r\n'
    else:
        request += 'Content-Length: {}\r\n'.format(args.length)
    request += '\r\n'
    
    sys.stdout.write(request)
    
    rem_length = args.length
    
    while rem_length > 0:
        chunk_size = min(rem_length, args.chunk_size)
        rem_length -= chunk_size
        
        chunk_data = 'X' * chunk_size
        
        if args.chunked:
            chunk = '{:X}\r\n{}\r\n'.format(chunk_size, chunk_data)
        else:
            chunk = chunk_data
        
        sys.stdout.write(chunk)
    
    if args.chunked:
        sys.stdout.write('0\r\n\r\n')

if __name__ == '__main__':
    main()
