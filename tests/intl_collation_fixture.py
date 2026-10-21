#!/usr/bin/env python3
"""Emit a small root-only pack with overlapping keys and long expansions."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import collation_root
import collation_tables


def fixture():
    # Deliberately exceed the pinned source's observed key/expansion maxima.
    rows = (
        ((0,), ((False, 0, 0, 0),)),
        ((0x61,), ((False, 0x1000, 0x20, 2),)),
        ((0x61, 0x62, 0x79), ((False, 0x1001, 0x20, 2), (False, 0, 0x21, 0))),
        ((0x61, 0x62, 0x79, 0x7a), ((False, 0x1002, 0x20, 2),)),
        ((0x62,), tuple((i == 0, 0x1100 + i, 0x20, 2) for i in range(19))),
        ((0xfdd0,), ((False, 0x2000, 0, 0),)),
        ((0x1f600,), ((False, 0x3000, 0x20, 2),)),
    )
    section = collation_tables.encode({'version': collation_root.VERSION, 'rows': rows})
    # No locale or string section: root mappings have no such dependency.
    return bundle.pack({bundle.COLLATION_ROOT: section}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    binary = fixture()
    # The normal profile emitter requires locale/string sections. This fixture
    # intentionally omits those, so emit its validated root-only envelope here.
    lines = ['#include <stddef.h>', '#include <stdint.h>',
             'const uint8_t qjs_intl_collation_fixture_bytes[] = {']
    for at in range(0, len(binary), 12):
        lines.append('    ' + ', '.join('0x%02x' % b for b in binary[at:at + 12]) + ',')
    lines.extend(('};', 'const size_t qjs_intl_collation_fixture_length = %d;' % len(binary), ''))
    text = '\n'.join(lines)
    atomic_write(args.output, text.encode('ascii'))
