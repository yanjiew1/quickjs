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

"""Reject missing public APIs and leaked private dynamic symbols."""
from pathlib import Path
import re
import subprocess
import sys

def pe_exports(output):
    table = output.split('[Ordinal/Name Pointer] Table', 1)[1].split('\n\n', 1)[0]
    names = set()
    for line in table.splitlines():
        if not re.match(r'^\s*\[\s*\d+\]', line):
            continue
        match = re.fullmatch(
            r'\s*\[\s*\d+\]\s+(?:\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+)?(\S+)\s*', line)
        if not match:
            raise ValueError('Unrecognized PE export row: ' + line)
        names.add(match[1])
    return names


def main():
    source, library, system = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
    expected = set(subprocess.check_output(
        [sys.executable, str(source / 'tools/build/public_exports.py'), 'list',
         str(source / 'include/quickjs.h'), str(source / 'include/quickjs-libc.h')],
        universal_newlines=True).splitlines())
    if system == 'windows':
        output = subprocess.check_output(['objdump', '-p', library], universal_newlines=True)
        actual = pe_exports(output)
    elif system == 'darwin':
        output = subprocess.check_output(['nm', '-gU', library], universal_newlines=True)
        actual = {line.split()[-1][1:] for line in output.splitlines() if line.split()}
    else:
        output = subprocess.check_output(['nm', '-D', '--defined-only', library],
                                         universal_newlines=True)
        actual = {line.split()[-1] for line in output.splitlines() if line.split()}
    assert actual == expected, ('missing', sorted(expected - actual),
                                'unexpected', sorted(actual - expected))


if __name__ == '__main__':
    main()
