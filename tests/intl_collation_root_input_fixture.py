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

"""Share authored root data; unsafe contexts isolate the pinned C NFC check."""
import argparse
from pathlib import Path

from test_intl_collation_root_input import root, GROUPS, FINGERPRINT, SOURCE, wire


def emit(output, name, raw):
    output.write('const unsigned char ' + name + '[] = {\n')
    for start in range(0, len(raw), 16):
        output.write('    ' + ', '.join(str(value) for value in raw[start:start + 16]) + ',\n')
    output.write('};\nconst size_t ' + name + '_size = sizeof(' + name + ');\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with args.output.open('w') as output:
        output.write('#include <stddef.h>\n')
        emit(output, 'intl_root_fixture', wire.encode(root(), GROUPS, FINGERPRINT))
        # An authored shared group checks alias identity, independently of the
        # script represented by the tiny fixture's arbitrary source weights.
        aliases = tuple(group._replace(tokens=('Hira', 'Kana', 'Hrkt'))
                        if group.tokens == ('Latn',) else group for group in GROUPS)
        emit(output, 'intl_root_shared_group', wire.encode(root(), aliases, FINGERPRINT))
        # A second Han range checks rank-pool offsets and binary-search gaps.
        source = SOURCE.replace('4E00..4E02', '4E00..4E02 20000..20001')
        source = source.replace('丂一丁', '丂一丁𠀁𠀀')
        catalog = wire.ordinals.Catalog(wire.fractional.parse(source.encode('utf-8')))
        emit(output, 'intl_root_multirange', wire.encode(catalog, GROUPS, FINGERPRINT))
        # Python validates ordering and coordinate structure, while only pinned
        # C Unicode decides NFC boundaries. Sorting these additional contexts
        # through the writer avoids a rejection caused merely by unordered keys.
        for name, key in (('prefix', '0301 | 0078'), ('text', '0078 | 0301')):
            source = SOURCE + key + '; [2A,05,05]\n'
            catalog = wire.ordinals.Catalog(wire.fractional.parse(source.encode('utf-8')))
            emit(output, 'intl_root_unsafe_' + name, wire.encode(catalog, GROUPS, FINGERPRINT))


if __name__ == '__main__':
    main()
