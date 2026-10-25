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

"""Compile every byte value, chunk boundaries and trailing zero through C."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import acquire
import bundle
import embed
import tables


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    # Also cross C99's minimum guaranteed concatenated-string capacity, as the
    # supported compilers must handle much larger real prepared-data objects.
    payload = bytes(range(256)) * 17 + b'\0'
    sections = {bundle.STRINGS: tables.encode_strings(('', 'root')),
                bundle.LOCALES: (1, tables.LOCALE.pack(1, tables.ROOT)),
                123: (1, payload)}
    source = embed.render(bundle.pack(sections, bytes(range(32)), 'minimal'))
    source += '\nconst size_t qjs_test_embedding_storage_length = sizeof(qjs_intl_native_bytes);\n'
    acquire.atomic_write(args.output, source.encode('ascii'))


if __name__ == '__main__':
    main()
