#!/usr/bin/env python3
"""Dictionary wire extents, graph semantics and exact typed-array domains."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import bundle
import segment_dictionary as dictionary
import segment_dictionary_tables as wire
import segment_dictionary_trie as trie
import tables


def prepared(text, valued=False):
    return trie.prepare(dictionary.parse(text.encode('utf-8'), valued))


class DictionaryTablesTests(unittest.TestCase):
    def setUp(self):
        self.data = {'cjk': prepared('a\t0\nab\t4294967295\ncb\t4294967295\n', True),
                     'thai': prepared('ก\nกา\n𠀀\n')}
        self.section = wire.encode(self.data)

    def rejected_word(self, offset, value):
        raw = bytearray(self.section[1])
        struct.pack_into('<I', raw, offset, value)
        with self.assertRaises(DataError):
            wire.decode((self.section[0], bytes(raw)))

    def test_roundtrip_and_canonical_family_order(self):
        self.assertEqual(wire.decode(self.section), self.data)
        self.assertEqual(self.section, wire.encode(dict(reversed(tuple(self.data.items())))))
        self.assertEqual(wire.decode(None), {})
        # A self-contained dictionary section needs no shared string indexes.
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(('', 'root')),
                              bundle.LOCALES: (1, struct.pack('<II', 1, 0xffffffff)),
                              bundle.SEGMENT_DICTIONARIES: self.section}, bytes(32), 'minimal')
        self.assertEqual(tables.validate(binary)['segment_dictionaries'], self.data)

    def test_truncation_header_descriptor_and_trailing_bytes(self):
        raw = self.section[1]
        for length in [0, 15, 16, 63, len(raw) - 1]:
            with self.subTest(length=length), self.assertRaises(DataError):
                wire.decode((2, raw[:length]))
        with self.assertRaises(DataError):
            wire.decode((1, raw))
        with self.assertRaises(DataError):
            wire.decode((2, raw + b'\0'))
        for offset, value in [(0, 0), (8, 2), (12, 0),
                              (16, 5), (20, 0), (24, 0), (28, 99), (32, 99), (36, 99),
                              (40, 65537), (44, 65536), (48, 0xffffffff), (52, 0xffffffff),
                              (56, 0), (60, 1), (64, 0)]:
            with self.subTest(offset=offset, value=value):
                self.rejected_word(offset, value)

    def test_graph_cost_labels_and_scalar_corruption(self):
        raw = self.section[1]
        descriptor = wire.RECORD.unpack_from(raw, wire.HEADER.size)
        family, valued, root, words, scalar_max, unit_max, a, v, s, e, start, size = descriptor
        offsets, _ = wire.layout(a, v, s, e)
        alphabet, values, starts, terminals, labels, targets = (start + offset for offset in offsets)
        # Acyclic targets alone are insufficient: language counts and maxima
        # are also checked, even when several prefixes share a terminal state.
        for offset, value in [(alphabet, 0xd800), (alphabet, 0x110000),
                              (starts, 1), (starts + 4 * s, e + 1),
                              (targets, s), (targets, 0xffffffff),
                              (values, 0xffffffff)]:
            with self.subTest(offset=offset, value=value):
                self.rejected_word(offset, value)
        for offset, value in [(terminals, v + 1), (terminals, 0), (labels, a)]:
            bad = bytearray(raw)
            struct.pack_into('<H', bad, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(DataError):
                wire.decode((2, bytes(bad)))

    def test_alignment_padding_is_zero_and_length_is_exact(self):
        data = prepared('a\nb\nc\n')
        section = wire.encode({'thai': data})
        descriptor = wire.RECORD.unpack_from(section[1], wire.HEADER.size)
        a, v, s, e, start, size = descriptor[6:]
        offsets, _ = wire.layout(a, v, s, e)
        self.assertEqual(e, 3)
        padding = start + offsets[4] + 2 * e
        self.assertLess(padding, start + offsets[5])
        bad = bytearray(section[1])
        bad[padding] = 1
        with self.assertRaises(DataError):
            wire.decode((1, bytes(bad)))

    def test_reachability_productivity_and_distinct_length_maxima(self):
        unreachable = trie.Trie((ord('a'),), (0,), (0, 0, 0, 1), (1, 1, 0),
                                (0,), (0,), 2, False, 1, 1, 1)
        with self.assertRaisesRegex(DataError, 'unreachable'):
            wire.validate(unreachable)
        dead = trie.Trie((ord('a'), ord('b')), (0,), (0, 0, 0, 2), (1, 0, 0),
                         (0, 1), (0, 1), 2, False, 1, 1, 1)
        with self.assertRaisesRegex(DataError, 'unproductive'):
            wire.validate(dead)
        # The longest scalar path and longest UTF-16 path are different words.
        data = prepared('abcd\n𠀀𠀁𠀂\n')
        self.assertEqual((data.max_scalars, data.max_utf16), (4, 6))
        self.assertEqual(wire.decode(wire.encode({'thai': data}))['thai'], data)

    def test_exact_integer_types_and_graph_budgets(self):
        base = self.data['cjk']
        for value in [base._replace(root=float(base.root)), base._replace(words=True),
                      base._replace(alphabet=(True,) + base.alphabet[1:]),
                      base._replace(labels=(0.0,) + base.labels[1:]),
                      base._replace(terminals=list(base.terminals))]:
            with self.subTest(value=value), self.assertRaises(DataError):
                wire.encode({'cjk': value})
        for count in [True, 2.0, '2']:
            with self.subTest(count=count), self.assertRaises(DataError):
                wire.decode((count, self.section[1]))
        for counts in [(1, 1, wire.MAX_STATES + 1, 1),
                       (1, 1, 2, wire.MAX_EDGES + 1), (True, 1, 2, 1)]:
            with self.subTest(counts=counts), self.assertRaises(DataError):
                wire.layout(*counts)

    def test_semantic_metadata_and_family_modes(self):
        for data in [self.data['cjk']._replace(words=4),
                     self.data['cjk']._replace(max_scalars=3),
                     self.data['thai']._replace(max_utf16=3)]:
            with self.subTest(data=data), self.assertRaises(DataError):
                wire.validate(data)
        for value in [{}, {'unknown': self.data['thai']}, {'cjk': self.data['thai']},
                      {'thai': self.data['cjk']}]:
            with self.subTest(value=value), self.assertRaises(DataError):
                wire.encode(value)


if __name__ == '__main__':
    unittest.main()
