#!/usr/bin/env python3
"""Authored wire fixture, hostile envelopes and policy/graph validation."""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import bundle
from segment_lower import LoweredProgram, LoweredRule
from segment_profile import Profile
from segment_program import assemble
import segment_program_tables as wire
import tables as pack_tables
from intl_segment_language_fixture import char, node


def profile(kind='WordBreak', rows=None):
    if rows is None:
        rows = ((char('a'), char('b'), False),)
    lowered = LoweredProgram(tuple(LoweredRule(None, left, right, action, False)
                                   for left, right, action in rows), None)
    program = assemble(lowered)
    requirements = {'GraphemeClusterBreak': 0, 'WordBreak': 13, 'SentenceBreak': 10}[kind]
    return Profile(kind, program, (0,) * len(rows), (0,) * len(program.alphabet.masks), requirements)


def golden():
    """Independent small wire construction; offsets are not writer-derived.

    One word rule joins a|b. Two complete two-state machines restart from
    opposite directions. Class zero has two disjoint ranges and rejecting
    states are valid. Every reserved/alignment byte starts at zero.
    """
    raw = bytearray(224)
    struct.pack_into('<8s8I32s', raw, 0, b'QJSSEGM\0', 1, 18 << 16, 1,
                     1, 1, 224, 0, 0, bytes(range(32)))
    struct.pack_into('<10I', raw, 72, 1, 13, 3, 4, 1, 2, 124, 100, 0, 0)
    struct.pack_into('<3I', raw, 112, 0, 1, 0)
    struct.pack_into('<4I', raw, 124, 97, 98, 99, 0x110000)
    struct.pack_into('<4H', raw, 140, 0, 1, 2, 0)
    struct.pack_into('<HHBBH', raw, 152, 0, 1, 0, 0, 0)
    struct.pack_into('<4I', raw, 160, 2, 0, 68, 80)
    struct.pack_into('<4I', raw, 176, 2, 1, 84, 96)
    struct.pack_into('<6H', raw, 192, 0, 1, 0, 0, 1, 0)
    raw[204] = 2
    struct.pack_into('<6H', raw, 208, 0, 0, 1, 0, 0, 1)
    raw[220] = 2
    return bytes(raw)


class SegmentProgramTablesTests(unittest.TestCase):
    def setUp(self):
        self.profile = profile()
        self.section = wire.encode((self.profile,), ((0, 1, 0),), bytes(range(32)), 1)

    def corrupt(self, offset, value, code='I'):
        raw = bytearray(golden())
        struct.pack_into('<' + code, raw, offset, value)
        with self.assertRaises(DataError):
            wire.decode((1, bytes(raw)), 1)

    def test_independent_golden_and_deterministic_roundtrip(self):
        self.assertEqual(self.section, (1, golden()))
        tables = wire.decode((1, golden()), 1)
        self.assertEqual(tables.programs, (wire.project(self.profile),))
        self.assertEqual(tables.fingerprint, bytes(range(32)))
        self.assertEqual(tables.bindings, ((0, 1, 0),))
        self.assertEqual(self.section, wire.encode((self.profile,), tables.bindings, tables.fingerprint, 1))
        self.assertIsNone(wire.decode(None, 1))
        binary = bundle.pack({bundle.STRINGS: pack_tables.encode_strings(('', 'root')),
                              bundle.LOCALES: (1, struct.pack('<II', 1, 0xffffffff)),
                              bundle.SEGMENT_PROGRAMS: self.section}, bytes(32), 'native')
        self.assertEqual(pack_tables.validate(binary)['segment_programs'], tables)

    def test_default_break_nullable_and_bos_modes(self):
        empty = profile('GraphemeClusterBreak', ())
        nullable = profile('SentenceBreak', ((node('epsilon'), node('epsilon'), True),))
        anchored = profile('GraphemeClusterBreak', ((node('concat', node('start'), char('a')), char('b'), False),))
        for source in (empty, nullable, anchored):
            section = wire.encode((source,), ((0, wire.project(source).kind, 0),), bytes(32), 1)
            self.assertEqual(wire.decode(section, 1).programs, (wire.project(source),))
        self.assertEqual(wire.project(anchored).machines[0].mode, wire.LEFT_BOS_ONLY)
        self.assertTrue(all(row.accepted[0] for row in wire.project(nullable).machines))
        self.assertEqual(wire.project(empty).machines, ())

    def test_header_directory_truncation_and_reserved_fields(self):
        for length in (0, 71, 72, 111, 123, 159, 223):
            with self.subTest(length=length), self.assertRaises(DataError):
                wire.decode((1, golden()[:length]), 1)
        for offset, value in ((0, 0), (8, 2), (12, 17 << 16), (16, 2), (20, 2),
                              (24, 0), (24, 65537), (28, 223), (32, 1), (36, 1),
                              (72, 3), (76, 0), (80, 0), (84, 0xffffffff),
                              (88, 1025), (92, 2049), (96, 125), (100, 99),
                              (104, 1), (108, 1), (112, 1), (116, 2), (120, 1)):
            with self.subTest(offset=offset, value=value):
                self.corrupt(offset, value)
        with self.assertRaises(DataError):
            wire.decode((1, golden() + b'\0'), 1)

    def test_class_coverage_context_and_padding(self):
        for offset, value, code in ((124, 0, 'I'), (128, 96, 'I'), (136, 0x110001, 'I'),
                                    (140, 1, 'H'), (142, 0, 'H'), (144, 3, 'H'),
                                    (148, 1, 'B'), (151, 1, 'B'), (205, 1, 'B'),
                                    (204, 0x82, 'B'), (223, 1, 'B')):
            with self.subTest(offset=offset):
                self.corrupt(offset, value, code)

    def test_machine_targets_modes_actions_and_roles(self):
        for offset, value, code in ((152, 1, 'H'), (154, 0, 'H'), (156, 2, 'B'),
                                    (157, 2, 'B'), (158, 1, 'H'), (160, 0, 'I'),
                                    (164, 3, 'I'), (168, 69, 'I'), (172, 81, 'I'),
                                    (180, 0, 'I'), (194, 2, 'H'), (194, 0, 'H')):
            with self.subTest(offset=offset):
                self.corrupt(offset, value, code)
        # A lexical join is permitted only for a word non-break row.
        raw = bytearray(golden())
        raw[157] = 1
        self.assertEqual(wire.decode((1, bytes(raw)), 1).programs[0].rules[0].role, 1)
        raw[156] = 1
        with self.assertRaises(DataError):
            wire.decode((1, bytes(raw)), 1)

    def test_reachable_but_noncanonical_bfs_numbering_is_rejected(self):
        program = wire.project(self.profile)
        machine = wire.Machine(((2, 0, 1), (0, 0, 0), (0, 0, 0)), (False,) * 3, 0)
        with self.assertRaisesRegex(DataError, 'state order'):
            wire.validate(program._replace(machines=(machine,) + program.machines[1:]))
        sink = wire.Machine(((0, 1, 0), (1, 1, 1)), (False, False), 0)
        wire.validate(program._replace(machines=(sink,) + program.machines[1:]))

    def test_u16_targets_and_acceptance_across_byte_boundaries(self):
        # A 257-state reachable chain exercises target 256 and acceptance bit
        # 256 independently of the two-state golden fixture's single byte.
        source = self.profile
        machine = source.program.machines[0]
        rows = tuple((min(state + 1, 256), 0, 0) for state in range(257))
        accepted = tuple(state in (7, 8, 255, 256) for state in range(257))
        expanded = machine._replace(dfa=machine.dfa._replace(rows=rows, accepted=accepted))
        source = source._replace(program=source.program._replace(
            machines=(expanded,) + source.program.machines[1:]))
        section = wire.encode((source,), ((0, 1, 0),), bytes(32), 1)
        decoded = wire.decode(section, 1).programs[0].machines[0]
        self.assertEqual(decoded.rows, rows)
        self.assertEqual(decoded.accepted, accepted)

    def test_binding_domains_order_and_all_programs_used(self):
        second = profile('GraphemeClusterBreak')
        for rows in (((1, 1, 0),), ((0, 0, 0),), ((0, 1, 0), (0, 1, 0)),
                     ((1, 1, 0), (0, 1, 0)), ((True, 1, 0),), ((0, 1, 0.0),)):
            with self.subTest(rows=rows), self.assertRaises(DataError):
                wire.encode((self.profile,), rows, bytes(32), 1)
        with self.assertRaisesRegex(DataError, 'unbound'):
            wire.encode((self.profile, second), ((0, 1, 0),), bytes(32), 1)
        with self.assertRaisesRegex(DataError, 'program order'):
            wire.encode((self.profile, second), ((0, 0, 1), (0, 1, 0)), bytes(32), 1)
        section = wire.encode((second, self.profile), ((0, 0, 0), (0, 1, 1)), bytes(32), 1)
        self.assertEqual(len(wire.decode(section, 1).programs), 2)

    def test_exact_types_and_aggregate_budget_before_cell_materialization(self):
        for count in (True, 1.0, '1'):
            with self.subTest(count=count), self.assertRaises(DataError):
                wire.decode((count, golden()), 1)
        for altered in (self.profile._replace(requirements=True),
                        self.profile._replace(roles=(False,)),
                        self.profile._replace(class_flags=(False,) * 3)):
            with self.subTest(altered=altered), self.assertRaises(DataError):
                wire.encode((altered,), ((0, 1, 0),), bytes(32), 1)
        with patch.object(wire, 'MAX_TOTAL_CELLS', 1), patch.object(wire.struct, 'unpack_from', wraps=struct.unpack_from) as unpack:
            with self.assertRaisesRegex(DataError, 'aggregate wire budget'):
                wire.decode((1, golden()), 1)
            self.assertFalse(any(call[0][0] == '<3H' for call in unpack.call_args_list))

    def test_profile_dimension_and_aggregate_gates_precede_projection(self):
        oversized = self.profile._replace(program=self.profile.program._replace(
            machines=(None,) * (wire.MAX_MACHINES + 1)))
        with patch.object(wire, 'project', side_effect=AssertionError('projected before preflight')):
            with self.assertRaisesRegex(DataError, 'dimension budget'):
                wire.encode((oversized,), ((0, 1, 0),), bytes(32), 1)
            with patch.object(wire, 'MAX_TOTAL_CELLS', 1):
                with self.assertRaisesRegex(DataError, 'aggregate wire budget'):
                    wire.encode((self.profile,), ((0, 1, 0),), bytes(32), 1)


if __name__ == '__main__':
    unittest.main()
