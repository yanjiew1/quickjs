#!/usr/bin/env python3
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
