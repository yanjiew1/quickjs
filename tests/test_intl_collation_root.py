#!/usr/bin/env python3
"""Authored root collation source contracts; no downloads or host UCD data."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import collation_root as root


def parse(body):
    return root.parse(('@version 18.0.0\n' + body).encode('utf-8'))


class RootCollationTests(unittest.TestCase):
    def test_levels_variables_ignorables_and_canonical_order(self):
        data = parse('''
          0062 ; [.2000.0020.0002] # b
          0061 0301 ; [.1000.0020.0002][.0000.0024.0002]
          0000 ; [.0000.0000.0000] # retained ignorable
          0020 ; [*0200.0020.0002]
          0061 ; [.1000.0020.0002]
          1F600 ; [.3000.0020.0002] # supplementary scalar
        ''')
        rows = dict(data['rows'])
        self.assertEqual(data['version'], '18.0.0')
        self.assertEqual(tuple(key for key, elements in data['rows']), tuple(sorted(rows)))
        self.assertEqual(rows[(0,)], ((False, 0, 0, 0),))
        self.assertEqual(rows[(0x20,)], ((True, 0x200, 0x20, 2),))
        self.assertEqual(rows[(0x61, 0x301)],
                         ((False, 0x1000, 0x20, 2), (False, 0, 0x24, 2)))
        self.assertEqual(rows[(0x1f600,)], ((False, 0x3000, 0x20, 2),))
        self.assertEqual(root.summary(data), {
            'version': '18.0.0', 'mappings': 6, 'elements': 7,
            'contractions': 1, 'expansions': 1,
            'maximum_key_length': 2, 'maximum_expansion_length': 2})

    def test_not_limited_to_observed_source_maxima(self):
        key = '0061 0062 0063 0064'
        elements = '[.FFFF.0000.0000]' * 24
        row, = parse(key + ' ; ' + elements)['rows']
        self.assertEqual(row[0], (0x61, 0x62, 0x63, 0x64))
        self.assertEqual(len(row[1]), 24)
        self.assertEqual(row[1][0][1], 0xffff)

    def test_all_valid_scalar_extremes(self):
        rows = dict(parse('0000 ; [.0000.0000.0000]\n10FFFF ; [.FFFF.0020.0002]')['rows'])
        self.assertEqual(len(rows), 2)

    def test_malformed_records(self):
        for body in (
            '0061 ; ', '0061 ; [.1000.0020.0002] junk',
            '0061 ; [.1000.0020]', '0061 ; [x1000.0020.0002]',
            '0061 ; [.1000.0020.0002];', '0061 [.1000.0020.0002]',
            '061 ; [.1000.0020.0002]', '110000 ; [.1000.0020.0002]',
            'D800 ; [.1000.0020.0002]', '0061 DFFF ; [.1000.0020.0002]',
            '0061\u00a00062 ; [.1000.0020.0002]',
            '0061 ; [.100000000.0020.0002]',
            '0061 ; [.10000.0020.0002]', '0061 ; [*0000.0020.0002]',
            '0061 ; [.1000.0020.0002]\n0061 ; [.2000.0020.0002]',
            '@implicitweights 17000..18AFF; FB00',
            '@version 18.0.0\n0061 ; [.1000.0020.0002]',
        ):
            with self.subTest(body=body), self.assertRaises(DataError):
                parse(body)

    def test_version_and_encoding(self):
        for raw in (b'', b'# only comments', b'@version 17.0.0',
                    b'@version 18.0.0', b'\xff', b'0061 ; [.1000.0020.0002]'):
            with self.subTest(raw=raw), self.assertRaises(DataError):
                root.parse(raw)

    def test_authenticated_source_boundary(self):
        class Sources:
            def __init__(self):
                self.names = []

            def read(self, name):
                self.names.append(name)
                return b'@version 18.0.0\n0061 ; [.1000.0020.0002]'

        sources = Sources()
        self.assertEqual(root.collect(sources)['rows'][0][0], (0x61,))
        self.assertEqual(sources.names, [root.SOURCE])


if __name__ == '__main__':
    unittest.main()
