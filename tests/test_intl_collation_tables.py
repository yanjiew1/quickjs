#!/usr/bin/env python3
"""Root collation wire roundtrips, pool bounds and canonical representation."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import collation_root as source
import collation_tables as wire
import tables


def authored():
    return source.parse(b'''@version 18.0.0
0000 ; [.0000.0000.0000]
0020 ; [*0200.0020.0002]
0061 ; [.1000.0020.0002]
0061 0301 ; [.1000.0020.0002][.0000.0024.0002]
1F600 ; [.3000.0020.0002]
''')


class RootCollationTablesTests(unittest.TestCase):
    def test_roundtrip_and_independent_section(self):
        data = authored()
        section = wire.encode(data)
        self.assertEqual(wire.decode(section), data)
        reversed_rows = dict(data, rows=tuple(reversed(data['rows'])))
        self.assertEqual(wire.encode(reversed_rows), section)
        self.assertIsNone(wire.decode(None))
        # Scalar keys have their own pool: U+0000 does not enter UTF-8 strings.
        pool = ('', 'en', 'root')
        shared = {
            bundle.STRINGS: tables.encode_strings(pool),
            bundle.LOCALES: (2, struct.pack('<4I', 1, 1, 2, 0xffffffff)),
        }
        binary = bundle.pack(shared, bytes(32), 'minimal')
        self.assertIsNone(tables.validate(binary)['collation_root'])
        shared[bundle.COLLATION_ROOT] = section
        binary = bundle.pack(shared, bytes(32), 'minimal')
        self.assertEqual(tables.validate(binary)['collation_root'], data)

    def test_exact_extent_and_fixed_width_elements(self):
        count, raw = wire.encode(authored())
        version, unicode_version, key_count, elements = wire.HEADER.unpack_from(raw)
        self.assertEqual((version, unicode_version, count, key_count, elements),
                         (1, 18 << 16, 5, 6, 6))
        self.assertEqual(len(raw), 16 + count * 16 + key_count * 4 + elements * 8)
        self.assertEqual(raw[-8:], struct.pack('<4H', 0x3000, 0x20, 2, 0))

    def test_invalid_source_models(self):
        data = authored()
        for wrong in (None, {}, dict(data, version='17.0.0'), dict(data, rows=[]),
                      dict(data, rows=[((), ((False, 1, 2, 3),))]),
                      dict(data, rows=[((True,), ((False, 1, 2, 3),))]),
                      dict(data, rows=[((0xd800,), ((False, 1, 2, 3),))]),
                      dict(data, rows=[((0x110000,), ((False, 1, 2, 3),))]),
                      dict(data, rows=[((1,), ())]),
                      dict(data, rows=[((1,), ((1, 1, 2, 3),))]),
                      dict(data, rows=[((1,), ((False, 0x10000, 2, 3),))]),
                      dict(data, rows=[((1,), ((False, 1, True, 3),))]),
                      dict(data, rows=[((1,), ((True, 0, 2, 3),))]),
                      dict(data, rows=data['rows'] + (data['rows'][0],))):
            with self.subTest(wrong=wrong), self.assertRaises(ValueError):
                wire.encode(wrong)

    def test_corrupt_headers_offsets_keys_flags_and_order(self):
        count, original = wire.encode(authored())
        maps_at = wire.HEADER.size
        keys_at = maps_at + count * wire.MAPPING.size
        elements_at = keys_at + 6 * 4

        def changed(offset, value):
            raw = bytearray(original)
            struct.pack_into('<I', raw, offset, value)
            return count, bytes(raw)

        corrupt = [(0, original), (True, original), (count, b''),
                   (count, original[:-1]), (count, original + b'\0')]
        corrupt += [changed(offset, value) for offset, value in (
            (0, 2), (4, 17 << 16), (8, 0xffffffff), (12, 0xffffffff),
            (maps_at, 1), (maps_at + 4, 0), (maps_at + 8, 1), (maps_at + 12, 0),
            (maps_at + 16, 0), (maps_at + 16 + 8, 0),
            (keys_at, 0xd800), (keys_at, 0x110000),
            (keys_at + 4, 0), (keys_at + 4, 0xffff),
            # Upper halfword is the variable flag, lower is tertiary weight.
            (elements_at + 4, 2 << 16), (elements_at + 4, 1 << 16),
        )]
        for section in corrupt:
            with self.subTest(section=section), self.assertRaises(ValueError):
                wire.decode(section)


if __name__ == '__main__':
    unittest.main()
