# Copyright (c) 2015 Ambroz Bizjak
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

def escape_string_for_nix(data):
    return '"{}"'.format(''.join(('\\{}'.format(c) if c in ('"', '\\', '$') else c) for c in data))

def convert_bool_for_nix(value):
    return 'true' if value else 'false'

def convert_for_nix(value):
    if type(value) is str:
        return escape_string_for_nix(value)
    if type(value) is bool:
        return convert_bool_for_nix(value)
    if type(value) is list:
        if len(value) == 0:
            return '[]'
        else:
            return '[ {} ]'.format(' '.join(convert_for_nix(e) for e in value))
    if type(value) is dict:
        if len(value) == 0:
            return '{}'
        else:
            return '{{ {} }}'.format(' '.join('{} = {};'.format(convert_for_nix(k), convert_for_nix(v)) for k, v in sorted(value.iteritems())))
