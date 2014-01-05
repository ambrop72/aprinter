#!/usr/bin/env python2.7

from __future__ import print_function
from __future__ import with_statement
import struct

class GcodeSyntaxError(Exception):
    pass

EncodeLineErrors = GcodeSyntaxError

def encode_line(line):
    line = line.strip()
    if len(line) == 0:
        return ''
    parts = line.split()
    cmd_letter = parts[0][0]
    if cmd_letter == 'E':
        return chr(0xE0)
    if not _letter_ok(cmd_letter):
        raise GcodeSyntaxError('invalid command letter')
    try:
        cmd_number = int(parts[0][1:])
    except ValueError:
        raise GcodeSyntaxError('invalid command number')
    if not (cmd_number >= 0 and cmd_number < 2048):
        raise GcodeSyntaxError('invalid command number')
    packet_index = ''
    packet_payload = ''
    num_params = len(parts) - 1
    if num_params > 14:
        raise GcodeSyntaxError('too many parameters')
    for part in parts[1:]:
        param_letter = part[0]
        if not _letter_ok(param_letter):
            raise GcodeSyntaxError('invalid parameter letter')
        param_value = part[1:]
        if param_value == '':
            encode_as = 'void'
        else:
            encode_as = 'integer'
            try:
                integer_value = int(param_value)
                if not (integer_value >= 0 and integer_value < 2**64):
                    raise ValueError()
            except ValueError:
                encode_as = 'real'
                try:
                    real_value = float(param_value)
                except ValueError:
                    raise GcodeSyntaxError('invalid command argument')
        if encode_as == 'void':
            type_code = 5
            param_payload = ''
        elif encode_as == 'integer':
            if integer_value < 2**32:
                type_code = 3
                param_payload = struct.pack('<I', integer_value)
            else:
                type_code = 4
                param_payload = struct.pack('<Q', integer_value)
        elif encode_as == 'real':
            type_code = 1
            param_payload = struct.pack('<f', real_value)
        packet_index += chr((type_code << 5) + (ord(param_letter) - ord('A')))
        packet_payload += param_payload
    if (cmd_letter, cmd_number) in _SmallCommands:
        command_type_code = _SmallCommands[(cmd_letter, cmd_number)]
        packet_header_large = ''
    else:
        command_type_code = 15
        packet_header_large = struct.pack('BB', ((ord(cmd_letter) - ord('A')) << 3) + (cmd_number >> 8), (cmd_number & 0xFF))
    packet_header = struct.pack('B', (command_type_code << 4) + num_params) + packet_header_large
    packet = packet_header + packet_index + packet_payload
    return packet

EncodeFileErrors = (IOError, GcodeSyntaxError)

def encode_file(input_file_name, output_file_name):
    line_num = 0
    with open(input_file_name, "r") as input_file:
        with open(output_file_name, "w") as output_file:
            for line in input_file:
                line_num += 1
                try:
                    encoded_data = encode_line(line)
                except GcodeSyntaxError as e:
                    e.args = ('line {}: {}'.format(line_num, e.args[0]),)
                    raise
                output_file.write(encoded_data)
            output_file.write(chr(0xE0))

_SmallCommands = {
    ('G', 0) : 1,
    ('G', 1) : 2,
    ('G', 92) : 3
}

def _letter_ok(ch):
    return (ord(ch) >= ord('A') and ord(ch) <= ord('Z'))

def main():
    import argparse
    parser = argparse.ArgumentParser(description='G-code packet for APrinter firmware.')
    parser.add_argument('--input', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    encode_file(args.input, args.output)

if __name__ == '__main__':
    main()
