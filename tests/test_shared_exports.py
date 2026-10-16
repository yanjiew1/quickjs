#!/usr/bin/env python3
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
