#!/usr/bin/env python3
"""Independent suppression wire bytes and authored list/route variants."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import scalar_trie
import segment_suppression_tables as wire
import tables
from test_intl_segment_suppression_tables import golden


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    literal = scalar_trie.prepare((('n. Chr.', None), ('𠀀.', None)))
    sections = [('golden', (2, golden())), ('absent', None),
                ('empty', wire.encode((None, None), ((0, 0), (1, 1)), bytes(32), 3)),
                ('literal', wire.encode((literal,), ((0, 0), (1, 0)), bytes(32), 3)),
                ('two', wire.encode((literal, literal), ((0, 0), (1, 1)), bytes(32), 3))]
    # A productive but unreachable state keeps framing valid; only the C graph
    # proof can reject it. Do not send this through the validating host writer.
    unreachable = bytearray(golden())
    struct.pack_into('<II', unreachable, 128, 1, 1)
    struct.pack_into('<4I', unreachable, 196, 0, 0, 0, 2)
    struct.pack_into('<2H', unreachable, 220, 0, 1)
    struct.pack_into('<2I', unreachable, 224, 1, 1)
    sections.append(('unreachable', (2, bytes(unreachable))))
    # Aggregate metadata must fail before any array binding/allocation. All
    # individual dimensions stay in range, but their combined state count does
    # not. Small blocks deliberately cannot satisfy these huge dimensions.
    raw = bytearray(72 + 5 * 48 + 5 * 8)
    start = len(raw)
    for index in range(5):
        struct.pack_into('<12I', raw, 72 + index * 48,
                         262143, 2, 2, 2, 2, 1, 262144, 2, start, 48, 0, 0)
        struct.pack_into('<2I', raw, 72 + 5 * 48 + index * 8, index, index)
        raw.extend(golden()[184:])
        start += 48
    struct.pack_into('<8s8I32s', raw, 0, b'QJSSUPP\0', 1, 18 << 16, 1,
                     5, 5, len(raw), 0, 0, bytes(32))
    sections.append(('aggregate', (5, bytes(raw))))

    lines = ['/* Generated authored sentence suppression fixtures. */',
             '#include <stdint.h>', '#include <stddef.h>', '']
    for name, section in sections:
        tags = ('aa', 'ab', 'en', 'fr', 'root') if name == 'aggregate' else ('en', 'fr', 'root')
        strings = ('',) + tags
        locales = tuple(value for index in range(len(tags))
                        for value in (index + 1, len(tags) - 1 if index + 1 < len(tags) else 0xffffffff))
        parts = {bundle.STRINGS: tables.encode_strings(strings),
                 bundle.LOCALES: (len(tags), struct.pack('<{}I'.format(len(locales)), *locales))}
        if section is not None:
            parts[bundle.SEGMENT_SUPPRESSIONS] = section
        binary = bundle.pack(parts, bytes(32), 'native')
        symbol = 'qjs_intl_suppression_' + name
        lines.append('const uint8_t ' + symbol + '_bytes[] = {')
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % value for value in binary[at:at + 16]) + ',')
        lines += ['};', 'const size_t ' + symbol + '_length = sizeof(' + symbol + '_bytes);', '']
    atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
