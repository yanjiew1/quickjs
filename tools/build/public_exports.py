#!/usr/bin/env python3
"""Generate an exact export list from public callable declarations."""
from pathlib import Path
import re
import sys


def public_symbols(headers):
    names = set()
    for header in headers:
        text = Path(header).read_text()
        for declaration in re.findall(r'^JS_EXTERN\s+[^;]+;', text, re.MULTILINE):
            matches = re.findall(r'\b((?:JS_|js_|__JS_)\w+)\)?\s*\(', declaration)
            if len(matches) != 1:
                raise ValueError('Expected one callable public declaration: ' + declaration)
            names.add(matches[0])
    if not names:
        raise ValueError('No public callable declarations found')
    return sorted(names)


if __name__ == '__main__':
    style = sys.argv[1]
    names = public_symbols(sys.argv[2:])
    if style == 'elf':
        print('{\n  global:\n' + ''.join('    ' + n + ';\n' for n in names) +
              '  local: *;\n};')
    elif style == 'darwin':
        print('\n'.join('_' + n for n in names))
    elif style == 'windows':
        print('EXPORTS\n' + '\n'.join(names))
    elif style == 'list':
        print('\n'.join(names))
    else:
        raise ValueError('Unknown export-list format: ' + style)
