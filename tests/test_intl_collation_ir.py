#!/usr/bin/env python3
"""Checked host-compiler records and pinned-Unicode normalization constraints."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import collation_ir as ir
import collation_rules as rules
from cldr_xml import DataError

compiler = None


def program(text, locale='en'):
    return rules.Program(locale, 'standard', '', '', '', rules.parse(text, 'x'))


def node_layout(raw):
    # Locate counted records without interpreting their instruction semantics.
    size, locale, kind, alternate, count = ir.PROGRAM.unpack_from(raw, ir.HEADER.size)
    at = ir.HEADER.size + ir.PROGRAM.size + locale + kind + alternate
    result = []
    for _ in range(count):
        fields = ir.NODE.unpack_from(raw, at)
        cursor, vectors = at + ir.NODE.size + fields[-1], []
        for _ in range(3):
            n, = struct.unpack_from('<I', raw, cursor)
            vectors.append((cursor, n))
            cursor += 4 + 4 * n
        result.append((at, fields, vectors))
        at += fields[0]
    return result


def changed(raw, at, value):
    return raw[:at] + struct.pack('<I', value) + raw[at + 4:]


class IRTests(unittest.TestCase):
    def test_counted_order_and_source_provenance(self):
        raw = ir.encode([program('&a<*zAB-Ca [reorder Latn Grek others]')])
        self.assertEqual(ir.HEADER.unpack_from(raw), (b'QJCI', 1, 18 << 16, len(raw), 1))
        nodes = node_layout(raw)
        self.assertEqual([fields[1] for _, fields, _ in nodes], [1, 3, 4])
        at, count = nodes[1][2][0]
        values = struct.unpack_from('<' + 'I' * count, raw, at + 4)
        self.assertEqual(values, tuple(map(ord, 'zzAAB Caa'.replace(' ', ''))))
        self.assertEqual([fields[3:5] for _, fields, _ in nodes], [(1, 1), (1, 3), (1, 12)])
        self.assertEqual(raw[nodes[0][0] + ir.NODE.size], ord('x'))

    def test_unexpanded_imports_and_bad_manual_nodes_reject(self):
        p = program('&a<b')
        cases = [p._replace(nodes=(rules.Import(rules.Position('x', 1, 1), 'en', 'standard'),)),
                 p._replace(nodes=(p.nodes[0]._replace(before=4),)),
                 p._replace(nodes=(p.nodes[1]._replace(strength=5),)),
                 p._replace(nodes=(p.nodes[1]._replace(text=(0xd800,)),)),
                 p._replace(nodes=(p.nodes[0]._replace(position=rules.Position('x\ny', 1, 1)),)),
                 p._replace(alternate='bad\0label')]
        for case in cases:
            with self.subTest(case=case), self.assertRaises(DataError):
                ir.encode([case])
        for programs in ([p, p], [program('', 'fr'), p]):
            with self.assertRaises(DataError):
                ir.encode(programs)


class CompilerTests(unittest.TestCase):
    def audit(self, raw, success=True):
        if compiler is None:
            self.skipTest('pass --compiler to exercise the C reader')
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'input.qjci'
            path.write_bytes(raw)
            result = subprocess.run([compiler, '--audit-ir', str(path)], timeout=10,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr.decode())
        if success:
            return json.loads(result.stdout.decode())
        self.assertNotIn(b'Sanitizer', result.stderr)
        self.assertNotIn(b'runtime error:', result.stderr)

    def test_all_instruction_shapes_and_unaligned_vectors(self):
        raw = ir.encode([program('[normalization on] &é <a|b/c <*zAB-Ca '
                                 '[suppressContractions []] [optimize [a-z]]')])
        result = self.audit(raw)
        self.assertEqual(result, {'programs': 1, 'instructions': 6,
                                  'normalized_texts': 4, 'starred_scalars': 5})
        self.assertEqual(self.audit(ir.encode([program('')]))['instructions'], 0)
        p = program('&a<b')
        p = p._replace(nodes=tuple(n._replace(position=n.position._replace(source='範例'))
                                  for n in p.nodes))
        self.assertEqual(self.audit(ir.encode([p]))['instructions'], 2)
        nul = program("&'\0'<'a\0'|'b\0'/'c\0'")
        self.assertEqual(self.audit(ir.encode([nul]))['normalized_texts'], 4)

    def test_starred_validation_is_before_normalization_and_checks_interiors(self):
        for text in ('&a<*é', '&a<*\u0301', '&a<*가', '&a<*\u00b6-\u00d7',
                     '&a<*\u02ff-\u0370'):
            with self.subTest(text=text):
                self.audit(ir.encode([program(text)]), False)
        # Compatibility decomposition is irrelevant for NFD-inertness.
        self.assertEqual(self.audit(ir.encode([program('&a<*ﬀ')]))['starred_scalars'], 1)

    def test_record_extents_and_unknown_instructions(self):
        raw = ir.encode([program('&a<b')])
        first, fields, vectors = node_layout(raw)[0]
        corruptions = [b'bad!' + raw[4:], changed(raw, 4, 2), changed(raw, 8, 17 << 16),
                       changed(raw, 12, len(raw) + 1), changed(raw, 16, 0xffffffff),
                       changed(raw, 20, 0xffffffff), changed(raw, 24, 0xffffffff),
                       changed(raw, 36, 0xffffffff), changed(raw, first, 0),
                       changed(raw, first, 0xffffffff), changed(raw, first + 4, 99),
                       changed(raw, first + 8, 4), changed(raw, first + 12, 0),
                       changed(raw, first + 16, 0), changed(raw, first + 20, 0xffffffff),
                       raw[:first + ir.NODE.size] + b'\xff' + raw[first + ir.NODE.size + 1:],
                       changed(raw, vectors[0][0], 0xffffffff),
                       changed(raw, vectors[0][0] + 4, 0xd800),
                       changed(raw, vectors[0][0] + 4, 0x110000), raw + b'\0']
        for corrupt in corruptions:
            with self.subTest(raw=corrupt[:50]):
                self.audit(corrupt, False)
        for length in list(range(21)) + [first, first + ir.NODE.size, len(raw) - 1]:
            self.audit(raw[:length], False)

    def test_prefix_boundaries_use_original_recursive_canonical_leads(self):
        for code in ('\u0301', '\u09be', '\u1161', '\u11a8', '\U000113c5',
                     '\U00016121', '\U00016d68'):
            for text in ('&a<' + code + '|b', '&a<a|' + code):
                with self.subTest(text=text):
                    self.audit(ir.encode([program(text)]), False)
        # No-prefix relations and independent extension lookups do not acquire
        # the prefix split restriction. A canonical singleton may be safe.
        for text in ('&a<\u0301', '&a<a|b/\u0301', '&a<\u212b|b'):
            self.audit(ir.encode([program(text)]))

    def test_artifact_expansion_budget_precedes_normalization(self):
        # Each small record encodes a million otherwise-inert scalars. Repeating
        # it across programs must be rejected without millions of allocations.
        text = '&a<*\U00040000-\U000ba11f\U00040000-\U000ba11f'
        self.audit(ir.encode([program(text, 'en-x-p' + str(i)) for i in range(5)]), False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--compiler')
    args, rest = parser.parse_known_args()
    compiler = args.compiler
    unittest.main(argv=[sys.argv[0]] + rest)
