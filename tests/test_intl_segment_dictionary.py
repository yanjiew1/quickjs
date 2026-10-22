#!/usr/bin/env python3
"""Bounded dictionary text parsing without network or host Unicode properties."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import segment_dictionary as dictionary


class DictionaryTests(unittest.TestCase):
    def test_membership_is_sorted_without_normalizing_keys(self):
        data = dictionary.parse('\ufeff # notice\r\n\t# second\r\n\r\né\né\n[ab]9\n𠀀a\n'.encode('utf-8'), False)
        self.assertEqual([row.word for row in data.entries], ['[ab]9', 'é', 'é', '𠀀a'])
        self.assertTrue(all(row.cost is None for row in data.entries))
        self.assertFalse(data.valued)
        self.assertEqual(data.max_scalars, 5)
        self.assertEqual(data.max_utf16, 5)
        self.assertEqual(data.comments, (' # notice', '\t# second'))
        astral = dictionary.parse('𠀀𠀁\n'.encode('utf-8'), False)
        self.assertEqual((astral.max_scalars, astral.max_utf16), (2, 4))

    def test_cost_zero_maximum_and_terminal_prefix(self):
        data = dictionary.parse('東京\t4294967295\n東\t0\na\t00042'.encode('utf-8'), True)
        self.assertEqual(data.entries, (('a', 42), ('東', 0), ('東京', 0xffffffff)))
        self.assertTrue(data.valued)
        # None and zero must remain distinct for a later terminal/value pool.
        self.assertNotEqual(data.entries[1].cost, dictionary.parse('東'.encode('utf-8'), False).entries[0].cost)

    def test_unicode_line_separators_are_not_record_delimiters(self):
        data = dictionary.parse('a\u2028b\u0085c\n'.encode('utf-8'), False)
        self.assertEqual([row.word for row in data.entries], ['a\u2028b\u0085c'])

    def test_malformed_encoding_and_record_syntax(self):
        for raw in [b'\xff', b'a\0b', b'a\rb\n', b'a\r', b'a\t0\n',
                    b' a\n', b'a \n', b'a\x7f\n', b'a\x01b\n',
                    '\ufeff\ufeffa'.encode('utf-8'), 'a\ufeff'.encode('utf-8')]:
            with self.subTest(raw=raw), self.assertRaises(DataError):
                dictionary.parse(raw, False, 'fixture')
        for raw in [b'a', b'a\t', b'\t1', b'a\t1\t2', b'a 1', b'a\t-1',
                    b'a\t+1', b'a\t0x10', b'a\t1e2', b'a\t4294967296',
                    b'a\t1 # comment', 'a\t١'.encode('utf-8')]:
            with self.subTest(raw=raw), self.assertRaises(DataError):
                dictionary.parse(raw, True, 'fixture')

    def test_duplicates_and_empty_input_are_errors(self):
        for raw, valued in [(b'a\na\n', False), (b'a\t1\na\t1\n', True),
                            (b'a\t1\na\t2\n', True), (b'', False), (b' # notice\n', True)]:
            with self.subTest(raw=raw), self.assertRaises(DataError):
                dictionary.parse(raw, valued)

    def test_each_budget_and_invalid_configuration(self):
        default = dictionary.DEFAULT_LIMITS
        for field, raw in [('bytes', b'ab'), ('records', b'a\nb'),
                           ('line_bytes', b'ab'), ('key_scalars', b'ab'),
                           ('total_scalars', b'a\nb')]:
            with self.subTest(field=field), self.assertRaises(DataError):
                dictionary.parse(raw, False, limits=default._replace(**{field: 1}))
        with self.assertRaises(DataError):
            dictionary.parse(b'a', False, limits=default._replace(records=True))
        with self.assertRaises(DataError):
            dictionary.parse(b'a', 1)
        # Encoding overhead, not just Python character count, bounds a line.
        with self.assertRaises(DataError):
            dictionary.parse('𠀀'.encode('utf-8'), False, limits=default._replace(line_bytes=3))


if __name__ == '__main__':
    unittest.main()
