#! /usr/bin/env nix-shell
#! nix-shell -i "python2.7 -B" -p python27

from __future__ import print_function
import os
import argparse
import pipes

def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-c', '--config', required=True,
        help='JSON configuration file to use (from the configuration web app).')
    parser.add_argument('-n', '--cfg-name', default='',
        help='Build this configuration instead of the one specified in the configuration file.')
    parser.add_argument('-o', '--output', required=True,
        help='Path to where the output will be linked (as a directory symlink).')
    parser.add_argument('nix_build_options', nargs='*', metavar='NIX_BUILD_OPTION',
        help='Additional options passed to nix-build. Always add -- just before these.')
    args = parser.parse_args()

    aprinter_src_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    command = [
        'nix-build', aprinter_src_dir, '-A', 'aprinterBuild', '-o', args.output,
        '--argstr', 'aprinterConfigFile', args.config,
        '--argstr', 'aprinterConfigName', args.cfg_name
    ] + args.nix_build_options
    
    print('Running: {}'.format(command_to_string(command)))

    os.execvp(command[0], command[1:])

def command_to_string(command):
    return ' '.join(pipes.quote(elem) for elem in command)

if __name__ == '__main__':
    main()
