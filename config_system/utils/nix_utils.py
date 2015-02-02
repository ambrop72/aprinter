def escape_string_for_nix(data):
    return '"{}"'.format(''.join(('\\{}'.format(c) if c in ('"', '\\', '$') else c) for c in data))
