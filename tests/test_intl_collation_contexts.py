#!/usr/bin/env python3
"""Authored prefix metadata contracts; no host UCD or downloaded fixtures."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import collation_contexts as contexts
import collation_root
from cldr_xml import DataError

ROOT = collation_root.parse(b'''@version 18.0.0
0061 ; [.1000.0020.0002]
0061 0062 ; [.1000.0020.0002][.0000.0021.0002]
0061 0062 0063 ; [.1000.0020.0002][.0000.0021.0002][.0000.0022.0002]
''')
VERSION = '[UCA version = 18.0.0]\n'


class CollationContextTests(unittest.TestCase):
    def test_metadata_preserves_complete_prefix(self):
        data = contexts.parse((VERSION + '''# ignored | comment
0061 0062 | 0063; [, FB B6, 05]
0061 | 0062; [, FB B4, 05] # encoding differs from allkeys
''').encode(), ROOT)
        self.assertEqual(data, ((1, 1, 1), (2, 2, 2)))
        section = contexts.encode(data, ROOT)
        self.assertEqual(contexts.decode(section, ROOT), data)
        self.assertEqual(len(section[1]), 8 + 2 * 12)

    def test_no_prefixes_differs_from_missing_section(self):
        self.assertEqual(contexts.parse(VERSION.encode(), ROOT), ())
        self.assertEqual(contexts.decode(contexts.encode((), ROOT), ROOT), ())
        self.assertIsNone(contexts.decode(None, ROOT))

    def test_source_and_weight_failures(self):
        for text in (
            '', '[UCA version = 17.0.0]', VERSION + VERSION,
            VERSION + '0061 | 0062;', VERSION + '0061 | 0062; [, garbage, 05]',
            VERSION + 'D800 | 0062; [, FB B6, 05]',
            VERSION + '110000 | 0062; [, FB B6, 05]',
            VERSION + '0062 | 0063; [, FB B6, 05]',
            VERSION + '0061 | 0062; [, FB B6, 05]\n0061 | 0062; [, FB B6, 05]',
            VERSION + '0061 | 0062; [, FBB6, 05]',
            VERSION + '0061 | 0062 | 0063; [, FB B6, 05]',
            VERSION + '0061 | 0062 0063; [, FB B6, 05]\n0061 0062 | 0063; [, FB B6, 05]',
        ):
            with self.subTest(text=text), self.assertRaises(DataError):
                contexts.parse(text.encode(), ROOT)
        with self.assertRaises(DataError):
            contexts.parse(b'\xff', ROOT)
        broken = dict(ROOT)
        broken['rows'] = ROOT['rows'][:1] + (((0x61, 0x62), ((False, 0x1001, 0x20, 2),)),)
        with self.assertRaises(DataError):
            contexts.parse((VERSION + '0061 | 0062; [, FB B6, 05]').encode(), broken)

    def test_wire_cross_references_and_canonical_rows(self):
        for rows in (((0, 1, 1),), ((1, 0, 1),), ((1, 2, 1),),
                     ((1, 1, 0),), ((1, 1, 2),), ((3, 1, 1),),
                     ((2, 2, 2), (1, 1, 1)), ((1, 1, 1), (1, 1, 1)),
                     ((True, 1, 1),), ((1, 1, 1, 1),)):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                contexts.encode(rows, ROOT)
        reversed_root = dict(ROOT, rows=tuple(reversed(ROOT['rows'])))
        with self.assertRaises(ValueError):
            contexts.encode(((1, 1, 1),), reversed_root)
        count, raw = contexts.encode(((1, 1, 1),), ROOT)
        for section in ((count + 1, raw), (count, raw + b'\0'), (count, raw[:-1]),
                        (count, struct.pack('<I', 2) + raw[4:]),
                        (count, raw[:4] + struct.pack('<I', 17 << 16) + raw[8:])):
            with self.subTest(section=section), self.assertRaises(ValueError):
                contexts.decode(section, ROOT)
        with self.assertRaises(ValueError):
            contexts.decode((count, raw), None)

    def test_authenticated_source_boundary(self):
        class Sources:
            def read(self, path):
                assert path == contexts.SOURCE
                return (VERSION + '0061 | 0062; [, FB B6, 05]').encode()
        self.assertEqual(contexts.collect(Sources(), ROOT), ((1, 1, 1),))


if __name__ == '__main__':
    unittest.main()
