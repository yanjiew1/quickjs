#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

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
