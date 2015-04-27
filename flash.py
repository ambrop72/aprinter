from __future__ import print_function
import sys
import os
import argparse
import subprocess

def report_error(error):
    print('Error: {}'.format(error), file=sys.stderr)
    sys.exit(1)

class ArduinoMegaType(object):
    SUPPORTED_EXTENSIONS = ['.hex']
    
    def flash_cmds(self, opts):
        port = '/dev/ttyACM0' if opts['port'] is None else opts['port']
        return [['avrdude', '-p', 'atmega2560', '-P', port, '-b', '115200', '-c', 'stk500v2', '-D', '-U', 'flash:w:{}:i'.format(opts['image_file'])]]

class MelziType(object):
    SUPPORTED_EXTENSIONS = ['.hex']
    
    def flash_cmds(self, opts):
        port = '/dev/ttyUSB0' if opts['port'] is None else opts['port']
        return [['avrdude', '-p', 'atmega1284p', '-P', port, '-b', '57600', '-c', 'stk500v1', '-D', '-U', 'flash:w:{}:i'.format(opts['image_file'])]]

class ArduinoDueType(object):
    SUPPORTED_EXTENSIONS = ['.bin']
    
    def flash_cmds(self, opts):
        port = '/dev/ttyACM0' if opts['port'] is None else opts['port']
        port_prefix = '/dev/'
        if not port.startswith(port_prefix):
            report_error('port must start with {}'.format(port_prefix))
        bare_port = port[len(port_prefix):]
        return [
            ['stty', '-F', port, '1200'],
            ['sleep', '0.5'],
            ['bossac', '-p', bare_port, '-U', 'false', '-i', '-e', '-w', '-v', '-b', opts['image_file'], '-R'],
        ]

class Teensy3Type(object):
    SUPPORTED_EXTENSIONS = ['.hex']
    
    def flash_cmds(self, opts):
        if opts['port'] is not None:
            report_error('port specification is not supported by teensy_loader_cli')
        return [['teensy_loader_cli', '-mmcu=mk20dx128', opts['image_file']]]

class Stm32f4Type(object):
    SUPPORTED_EXTENSIONS = ['.elf', '.bin']
    
    def flash_cmds(self, opts):
        program_file_arg = opts['image_file']
        address_arg = ' 0x08000000' if opts['extension'] == '.bin' else ''
        return [['openocd', '-f', 'interface/stlink-v2.cfg', '-f', 'target/stm32f4x_stlink.cfg', '-c', 'program "{}" verify{}'.format(program_file_arg, address_arg), '-c', 'reset']]

TYPES = {
    'arduino_mega': ArduinoMegaType,
    'melzi': MelziType,
    'arduino_due': ArduinoDueType,
    'teensy3': Teensy3Type,
    'stm32f4': Stm32f4Type,
}

def available_types():
    return 'Available types:\n{}'.format(''.join('  {}\n'.format(type) for type in TYPES))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-f', '--image-file', help='Image to flash to chip.')
    parser.add_argument('-t', '--type', help='Type of microcontroller or board.')
    parser.add_argument('-p', '--port', help='Port to use (meaning depends on the type).')
    parser.add_argument('-n', '--dry-run', action='store_true', help='Only print the commands to be run, not execute.')
    parser.add_argument('-l', '--list-types', action='store_true', help='List available types.')
    args = parser.parse_args()
    
    if args.list_types:
        print(available_types())
        return 0
    
    if not args.image_file:
        report_error('--image-file is required!')

    if not args.type:
        report_error('--type is required!')
    
    if args.type not in TYPES:
        report_error('invalid type specified!\n{}'.format(available_types()))
    type_class = TYPES[args.type]()
    
    _, extension = os.path.splitext(args.image_file)
    print(extension)
    if extension not in type_class.SUPPORTED_EXTENSIONS:
        report_error('unsupported image file extension!')
    
    opts = {
        'image_file': args.image_file,
        'port': args.port,
        'extension': extension,
    }
    
    flash_cmds = type_class.flash_cmds(opts)
    
    for cmd in flash_cmds:
        if args.dry_run:
            print(cmd)
            continue
        
        print('Running: {}'.format(cmd), file=sys.stderr)
        
        exit_status = subprocess.call(cmd)
        if exit_status != 0:
            report_error('command failed!')

if __name__ == '__main__':
    main()
