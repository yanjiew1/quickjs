#!/usr/bin/env python3
"""Counted logical root records retain source identity and reject corrupt domains."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import collation_fractional as fractional
from collation_groups import Group
import collation_ordinals as ordinals
import collation_root_input as wire
from test_intl_collation_ordinals import ROOT

# The tiny root deliberately combines NUL, supplementary keys, expansions,
# equal concatenated keys with different prefix splits, and non-scalar-order Han.
SOURCE = ROOT + '''
FDD1 00A0; [03,05,05]
FDD1 201C; [04,05,05]
FDD1 263A; [06,05,05]
FDD1 20AC; [08,05,05]
FDD1 0034; [0E,05,05]
FDD1 004C; [2A,05,05]
FDD1 FDD0; [E4,05,05]
1F600; [2B,05,05]
00E6; [2A,05,05][2B,05,05]
0061 0062 0063; [2B,05,05]
0061 | 0062 0063; [2A,05,05]
0061 0062 | 0063; [2B,05,05]
[first tertiary ignorable [,,]]
[last tertiary ignorable [,,]]
[first variable [03,05,05]]
[last variable [0B FF FF FF,05,05]]
[first regular [0E,05,05]]
[first implicit [E0,05,05]]
[last implicit [E4,05,05]]
[first trailing [EF FD,05,05]]
[last trailing [EF FF,05,05]]
[first secondary in primary non-ignorable [X,05,X]]
[last secondary in primary non-ignorable [X,73,X]]
[last tertiary in secondary non-ignorable [X,X,20]]
'''
GROUPS = tuple(Group((weight,), (token,), (), None, 'reorderable', 0)
               for weight, token in ((3, 'space'), (4, 'punct'), (6, 'symbol'),
                                     (8, 'currency'), (14, 'digit'), (42, 'Latn')))
GROUPS += (Group((0x82, 2, 2), ('Hani',), (), None, 'reorderable', 0),
           Group((0xe4,), (), (), None, 'unassigned', 0))
FINGERPRINT = bytes(range(32))


def root():
    return ordinals.Catalog(fractional.parse(SOURCE.encode('utf-8')))


def section(raw, kind):
    return wire.DIRECTORY.unpack_from(raw, wire.HEADER.size + (kind - 1) * wire.DIRECTORY.size)


def replace_word(raw, kind, index, value):
    result = bytearray(raw)
    struct.pack_into('<I', result, section(raw, kind)[1] + 4 * index, value)
    return bytes(result)


class RootInputTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = root()
        cls.raw = wire.encode(cls.catalog, GROUPS, FINGERPRINT)

    def rejects(self, raw):
        with self.assertRaises(DataError):
            wire.decode(raw)

    def test_source_identity_and_shared_sequences(self):
        data = wire.decode(self.raw)
        self.assertEqual(data['fingerprint'], FINGERPRINT)
        expected = sorted(self.catalog.data['rows'], key=lambda row: (row.prefix + row.text, len(row.prefix)))
        self.assertEqual(len(data['mappings']), len(expected))
        for mapping, source in zip(data['mappings'], expected):
            self.assertEqual((mapping.prefix, mapping.text), (source.prefix, source.text))
            ces = data['ces'][mapping.ce_start:mapping.ce_start + mapping.ce_count]
            self.assertEqual(ces, tuple(wire._ce(self.catalog.coordinate(ce)) for ce in source.elements))
        self.assertTrue(any(text == (0,) for _, text, _, _ in data['mappings']))
        self.assertTrue(any(text == (0x1f600,) for _, text, _, _ in data['mappings']))
        split = [len(prefix) for prefix, text, _, _ in data['mappings']
                 if prefix + text == (0x61, 0x62, 0x63)]
        self.assertEqual(split, [0, 1, 2])
        # Identical immutable CE vectors may share storage, but keys never do.
        total = sum(row.ce_count for row in data['mappings'])
        self.assertLess(section(self.raw, wire.CES)[3], total)
        self.assertEqual(data['han_ranks'], (2, 3, 1))
        group = data['groups'][-1]
        self.assertEqual(group, (self.catalog.generic_base, wire.UNASSIGNED, ()))
        unassigned = next(row for row in data['mappings'] if not row.prefix and row.text == (0xfdd1, 0xfdd0))
        self.assertEqual(data['ces'][unassigned.ce_start][0], group[0])
        self.assertEqual(data['ces'][unassigned.ce_start][-1] & 8, 0)

    def test_literal_flags_and_nonexpanding_shared_spans(self):
        data = wire.decode(self.raw)
        values = {row.text: data['ces'][row.ce_start] for row in data['mappings'] if not row.prefix}
        self.assertEqual(values[(0,)], (0, 0, 0, 0, 0, 0))
        self.assertEqual(values[(0x61,)][-1], 0)   # Lower, nonvariable literal.
        self.assertEqual(values[(0x41,)][-1], 2)   # Independent upper-case bits.
        self.assertEqual(values[(0x27,)][-1], 4)   # Default variable, lower.
        self.assertEqual(values[(0xf900,)][-1], 8) # Virtual Han, lower.
        # Rows may legally share a long CE span. Decoding retains only spans,
        # so referenced payload size does not multiply diagnostic allocation.
        raw = self.raw
        count = section(raw, wire.CES)[3]
        for index in range(section(raw, wire.MAPPINGS)[3]):
            raw = replace_word(raw, wire.MAPPINGS, index * 4 + 2, 0)
            raw = replace_word(raw, wire.MAPPINGS, index * 4 + 3, count)
        shared = wire.decode(raw)
        self.assertEqual(len(shared['ces']), count)
        self.assertTrue(all((row.ce_start, row.ce_count) == (0, count) for row in shared['mappings']))

    def test_source_facts_are_not_current_extrema(self):
        data = wire.decode(self.raw)
        facts = {name: row for name, row in zip(wire.RESET_NAMES, data['resets'])}
        self.assertEqual(facts['first implicit'][1:3],
                         (wire.ARTIFICIAL_BOUNDARY, self.catalog.implicit_first_marker))
        self.assertEqual(facts['last implicit'][1:3],
                         (wire.ARTIFICIAL_BOUNDARY, self.catalog.implicit_last_marker))
        self.assertLess(facts['last implicit'][2], self.catalog.implicit(0).primary)
        self.assertEqual(facts['last regular'][1:3],
                         (wire.DERIVED_INITIAL, self.catalog.han_base))
        # Fixed E5 bounds the virtual range; the actual first-trailing CE is EF FD.
        self.assertGreater(facts['first trailing'][2], self.catalog.generic_limit)
        self.assertEqual([row[1] for row in data['weak']], [1, 1, 2, 2])

    def test_header_and_extent_rejection(self):
        self.rejects(self.raw[:wire.HEADER.size - 1])
        self.rejects(self.raw[:-1])
        self.rejects(self.raw + b'\0\0\0\0')
        for at in (0, 4, 8, 12, 16, 20, 24, 28):
            raw = bytearray(self.raw)
            raw[at] ^= 1
            with self.subTest(header=at):
                self.rejects(bytes(raw))
        for field in range(4):
            raw = bytearray(self.raw)
            at = wire.HEADER.size + field * 4
            struct.pack_into('<I', raw, at, struct.unpack_from('<I', raw, at)[0] + 1)
            with self.subTest(directory=field):
                self.rejects(bytes(raw))

    def test_mapping_context_and_scalar_rejection(self):
        for kind, index, value in ((wire.MAPPINGS, 0, 1), (wire.MAPPINGS, 1, 0),
                                   (wire.MAPPINGS, 2, 0xffffffff), (wire.MAPPINGS, 3, 0),
                                   (wire.KEYS, 0, 0xd800), (wire.KEYS, 0, 0x110000),
                                   (wire.KEYS, 0, 0xfdd0), (wire.CONTEXTS, 0, 0xffffffff),
                                   (wire.CONTEXTS, 1, 0), (wire.CONTEXTS, 1, 0xffffffff),
                                   (wire.CONTEXTS, 2, 1)):
            with self.subTest(kind=kind, index=index, value=value):
                self.rejects(replace_word(self.raw, kind, index, value))

    def test_ce_flags_bands_and_provenance_rejection(self):
        # The first row is the all-zero CE for NUL.
        for field, value in ((0, self.catalog.primary_max + 1),
                             (0, self.catalog.numeric), (1, self.catalog.secondary_max + 1),
                             (2, self.catalog.tertiary_max + 1), (3, 1), (4, 1),
                             (5, 3), (5, 4), (5, 8), (5, 16)):
            with self.subTest(field=field, value=value):
                self.rejects(replace_word(self.raw, wire.CES, field, value))

    def test_metadata_han_and_fact_rejection(self):
        for name in ('primary_max', 'secondary_common', 'tertiary_common', 'han_count',
                     'generic_count', 'generic_base', 'generic_limit', 'implicit_first_marker',
                     'implicit_last_marker', 'numeric', 'field_separator', 'last_regular'):
            with self.subTest(metadata=name):
                self.rejects(replace_word(self.raw, wire.META, wire.META_FIELDS.index(name), 0))
        for kind, index, value in ((wire.HAN_RANGES, 0, 0xd800),
                                   (wire.HAN_RANGES, 2, 1), (wire.HAN_RANKS, 0, 0),
                                   (wire.HAN_RANKS, 0, 3), (wire.RESETS, 0, 0),
                                   (wire.RESETS, 1, 4), (wire.WEAK_LIMITS, 1, 2),
                                   (wire.WEAK_LIMITS, 2, 0), (wire.WEAK_LIMITS, 3, 1)):
            with self.subTest(kind=kind, index=index, value=value):
                self.rejects(replace_word(self.raw, kind, index, value))

    def test_reset_role_anchor_and_order_rejection(self):
        for name, field, value in (('first tertiary ignorable', 1, wire.DERIVED_INITIAL),
                                   ('first implicit', 2, self.catalog.implicit_last_marker),
                                   ('first variable', 2, self.catalog.han_base)):
            index = wire.RESET_NAMES.index(name) * 8
            raw = replace_word(self.raw, wire.RESETS, index + field, value)
            if field == 2:
                raw = replace_word(raw, wire.RESETS, index + 6, value)
            with self.subTest(name=name):
                self.rejects(raw)
        # Copy a valid nonvariable regular CE, so only the derived last-regular
        # anchor check (not a flag/band check) can reject the changed fact.
        raw = self.raw
        data = wire.decode(raw)
        first = data['resets'][wire.RESET_NAMES.index('first regular')]
        index = wire.RESET_NAMES.index('last regular') * 8
        for field, value in enumerate(first[2:], 2):
            raw = replace_word(raw, wire.RESETS, index + field, value)
        self.rejects(raw)
        raw = replace_word(self.raw, wire.WEAK_LIMITS, 6, self.catalog.secondary_common - 1)
        self.rejects(raw)

    def test_group_token_rejection(self):
        for kind, index, value in ((wire.GROUPS, 0, 0), (wire.GROUPS, 1, 3),
                                   (wire.GROUPS, 2, 1), (wire.GROUPS, 3, 0),
                                   (wire.TOKENS, 0, 1), (wire.TOKENS, 1, 0)):
            with self.subTest(kind=kind, index=index, value=value):
                self.rejects(replace_word(self.raw, kind, index, value))
        # Ordered but semantically wrong Hani and digit boundaries must fail.
        self.rejects(replace_word(self.raw, wire.GROUPS, 6 * 4, self.catalog.han_base - 1))
        # Swap identities while retaining valid unique tokens and ordered rows.
        # Swapping digit/Latn isolates numeric ownership; space/digit checks the
        # core-group order independently of token spelling or length checks.
        for first, last in ((4, 5), (0, 4)):
            groups = list(GROUPS)
            groups[first] = GROUPS[first]._replace(tokens=GROUPS[last].tokens)
            groups[last] = GROUPS[last]._replace(tokens=GROUPS[first].tokens)
            with self.subTest(swapped=(first, last)), self.assertRaises(DataError):
                wire.encode(self.catalog, groups, FINGERPRINT)
        raw = bytearray(self.raw)
        raw[section(raw, wire.TOKEN_BYTES)[1]] = 0
        self.rejects(bytes(raw))

    def test_output_and_fingerprint_contract(self):
        for fingerprint in (b'', b'a' * 31, None):
            with self.subTest(fingerprint=fingerprint), self.assertRaises(DataError):
                wire.encode(self.catalog, GROUPS, fingerprint)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'nested' / 'root.qjcr'
            wire.write(path, self.raw)
            self.assertEqual(path.read_bytes(), self.raw)
            wire.write(path, self.raw)
            entries = list(path.parent.iterdir())
            self.assertEqual(len(entries), 1)
            # Windows may return another separator spelling for the same file.
            self.assertTrue(entries[0].samefile(path))


if __name__ == '__main__':
    unittest.main()
