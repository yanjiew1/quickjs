#!/usr/bin/env python3
"""Independent suppression envelope bytes, literal policy and hostile graphs."""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
from cldr_xml import DataError
import scalar_trie
import scalar_trie_tables
import segment_suppression_tables as wire
import tables as pack_tables


def golden():
    """Empty policy plus keys a/ab, with independently fixed array offsets."""
    raw = bytearray(232)
    struct.pack_into('<8s8I32s', raw, 0, b'QJSSUPP\0', 1, 18 << 16, 1,
                     2, 2, 232, 0, 0, bytes(range(32)))
    struct.pack_into('<12I', raw, 72, 0, 0, 0, 0, 0, 0, 0, 0, 184, 0, 0, 0)
    struct.pack_into('<12I', raw, 120, 2, 2, 2, 2, 2, 1, 3, 2, 184, 48, 0, 0)
    struct.pack_into('<4I', raw, 168, 0, 0, 1, 1)
    struct.pack_into('<2I', raw, 184, 97, 98)
    struct.pack_into('<I', raw, 192, 0)
    struct.pack_into('<4I', raw, 196, 0, 0, 1, 2)
    struct.pack_into('<3H', raw, 212, 1, 1, 0)
    struct.pack_into('<2H', raw, 220, 1, 0)
    struct.pack_into('<2I', raw, 224, 0, 1)
    return bytes(raw)


class SuppressionTablesTests(unittest.TestCase):
    def setUp(self):
        self.graph = scalar_trie.prepare((('a', None), ('ab', None)))
        self.section = wire.encode((None, self.graph), ((0, 0), (1, 1)), bytes(range(32)), 2)

    def corrupt(self, offset, value, code='I'):
        raw = bytearray(golden())
        struct.pack_into('<' + code, raw, offset, value)
        with self.assertRaises(DataError):
            wire.decode((2, bytes(raw)), 2)

    def test_independent_golden_and_present_empty_policy(self):
        self.assertEqual(self.section, (2, golden()))
        result = wire.decode(self.section, 2)
        self.assertEqual(result, wire.Tables(bytes(range(32)), (None, self.graph), ((0, 0), (1, 1))))
        empty = wire.encode((None,), ((0, 0),), bytes(32), 1)
        self.assertEqual(wire.decode(empty, 1).lists, (None,))
        self.assertIsNone(wire.decode(None, 1))
        binary = bundle.pack({bundle.STRINGS: pack_tables.encode_strings(('', 'en', 'root')),
                              bundle.LOCALES: (2, struct.pack('<4I', 1, 1, 2, 0xffffffff)),
                              bundle.SEGMENT_SUPPRESSIONS: self.section}, bytes(32), 'native')
        self.assertEqual(pack_tables.validate(binary)['segment_suppressions'], result)

    def test_header_empty_descriptor_and_extents(self):
        for length in (0, 71, 72, 119, 167, 183, 231):
            with self.subTest(length=length), self.assertRaises(DataError):
                wire.decode((2, golden()[:length]), 2)
        for offset, value in ((0, 0), (8, 2), (12, 17 << 16), (16, 2), (20, 3),
                              (24, 0), (24, 65537), (28, 231), (32, 1), (36, 1),
                              (72, 1), (76, 1), (104, 183), (108, 4), (112, 1),
                              (120, 1), (124, 65537), (128, 257), (132, 1),
                              (136, 65537), (140, 2), (144, 262145), (148, 524289),
                              (152, 185), (156, 47), (160, 1), (164, 1)):
            with self.subTest(offset=offset):
                self.corrupt(offset, value)
        with self.assertRaises(DataError):
            wire.decode((2, golden() + b'\0'), 2)

    def test_graph_metadata_scalar_labels_payload_and_padding(self):
        for offset, value, code in ((124, 3, 'I'), (128, 1, 'I'), (184, 0xd800, 'I'),
                                    (188, 97, 'I'), (192, 7, 'I'), (196, 1, 'I'),
                                    (208, 3, 'I'), (212, 2, 'H'), (216, 1, 'H'),
                                    (218, 1, 'B'), (220, 2, 'H'), (224, 1, 'I'), (228, 2, 'I')):
            with self.subTest(offset=offset):
                self.corrupt(offset, value, code)

    def test_productive_but_unreachable_state_is_rejected(self):
        raw = bytearray(golden())
        struct.pack_into('<II', raw, 128, 1, 1)
        struct.pack_into('<4I', raw, 196, 0, 0, 0, 2)
        struct.pack_into('<2H', raw, 220, 0, 1)
        struct.pack_into('<2I', raw, 224, 1, 1)
        with self.assertRaisesRegex(DataError, 'unreachable'):
            wire.decode((2, bytes(raw)), 2)

    def test_route_sort_reference_and_first_use_domains(self):
        bindings = ((0, 0), (1, 0), (2, 1))
        section = wire.encode((None, self.graph), bindings, bytes(32), 3)
        self.assertEqual(wire.decode(section, 3).bindings, bindings)
        # Equal lists need not be deduplicated, including explicit empty lists.
        section = wire.encode((None, None), ((0, 0), (1, 1)), bytes(32), 2)
        self.assertEqual(wire.decode(section, 2).lists, (None, None))
        for offset, value in ((168, 2), (172, 1), (176, 0), (180, 0), (180, 2)):
            with self.subTest(offset=offset):
                self.corrupt(offset, value)
        for bindings in (((0, True), (1, 1)), ((0, 0), (0, 1)), ((1, 0), (0, 1)), ((0, 0),)):
            with self.subTest(bindings=bindings), self.assertRaises(DataError):
                wire.encode((None, self.graph), bindings, bytes(32), 2)

    def test_literal_space_and_supplementary_keys_remain_exact(self):
        graph = scalar_trie.prepare((('n. Chr.', None), ('𠀀.', None)))
        section = wire.encode((graph,), ((0, 0),), bytes(32), 1)
        self.assertEqual(wire.decode(section, 1).lists, (graph,))
        weighted = scalar_trie.prepare((('a', 7),))
        with self.assertRaisesRegex(DataError, 'nonmembership'):
            wire.encode((weighted,), ((0, 0),), bytes(32), 1)

    def test_aggregate_and_metadata_preflight_before_block_copies(self):
        for limit in ('MAX_TOTAL_STATES', 'MAX_TOTAL_EDGES', 'MAX_TOTAL_WORDS'):
            with self.subTest(limit=limit), patch.object(wire, limit, 1), \
                    patch.object(wire.graphs, 'decode_block') as block:
                with self.assertRaisesRegex(DataError, 'aggregate'):
                    wire.decode(self.section, 2)
                block.assert_not_called()
        with patch.object(scalar_trie_tables.struct, 'unpack_from') as unpack:
            with self.assertRaisesRegex(DataError, 'metadata'):
                scalar_trie_tables.decode_block(bytes(48), 0, 48, (2, 1, 3, 2), (2, 0, 2, 2))
            unpack.assert_not_called()


if __name__ == '__main__':
    unittest.main()
