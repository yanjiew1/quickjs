#!/usr/bin/env python3
"""Shape-bound Japanese source correction without raw-source mutation."""
import hashlib
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
from segment_inheritance import capture_variables
from segment_rules import prepare_variables
from segment_sources import Definition, Segmentation
import segment_compat as compat


class Sources:
    def __init__(self):
        self.pin = {'commit': 'c' * 40}
        self.data = {compat.ROOT: b'authored root fixture', compat.JAPANESE: b'authored Japanese fixture'}
        self.consumed = []

    def read(self, name):
        self.consumed.append(name)
        return self.data[name]


def records():
    root = Segmentation('root', 'WordBreak', compat.ROOT_FE_VARIABLES, (), None, compat.ROOT)
    japanese = Segmentation('ja', 'WordBreak', compat.JAPANESE_VARIABLES, (), None, compat.JAPANESE)
    return (root, japanese)


def graph():
    return ParentGraph(('root', 'ja'), {}, {'segmentations': {}}, {'und': 'en-Latn-US'}, (), False)


class SegmentCompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.sources = Sources()

    def test_raw_strict_failure_then_derived_historical_bindings(self):
        raw = records()
        captured = capture_variables(raw, graph(), 'ja', 'WordBreak')
        with self.assertRaisesRegex(DataError, r'unknown variable \$FE'):
            prepare_variables(captured)
        corrected = compat.japanese_word_fe(self.sources, raw)
        prepared = prepare_variables(capture_variables(corrected.records, graph(), 'ja', 'WordBreak'))
        visible = dict(prepared.visible)
        wrapper = prepared.arena[visible['$Hiragana']]
        fe_slot = visible['$FE']
        self.assertLess(wrapper.children[0].value, fe_slot)
        self.assertEqual(wrapper.children[1].children[0].value, fe_slot)
        self.assertEqual(prepared.bindings[fe_slot].expression, '[$Format $Extend $ZWJ]')

    def test_raw_records_bytes_and_other_owners_are_unchanged(self):
        raw = records()
        before = repr(raw), dict(self.sources.data), dict(self.sources.pin)
        result = compat.japanese_word_fe(self.sources, raw)
        self.assertEqual((repr(raw), self.sources.data, self.sources.pin), before)
        self.assertIs(result.records[0], raw[0])
        self.assertEqual(len(raw[1].variables), 4)
        self.assertEqual(len(result.records[1].variables), 5)
        self.assertEqual(result.records[1].variables[1], Definition('$FE', '[$Format $Extend $ZWJ]'))
        self.assertEqual(set(self.sources.consumed), {compat.ROOT, compat.JAPANESE})

    def test_receipt_identifies_original_hashes_and_synthetic_declaration(self):
        result = compat.japanese_word_fe(self.sources, records())
        receipt = json.loads(json.dumps(result.corrections[0]._asdict()))
        self.assertEqual(receipt['source'], compat.JAPANESE)
        self.assertEqual(receipt['source_sha256'], hashlib.sha256(self.sources.data[compat.JAPANESE]).hexdigest())
        self.assertEqual(receipt['root_sha256'], hashlib.sha256(self.sources.data[compat.ROOT]).hexdigest())
        self.assertEqual(receipt['identifier'], '$FE')
        self.assertEqual(receipt['insert_before'], 1)
        self.assertEqual(receipt['commit'], self.sources.pin['commit'])
        self.assertEqual(receipt['historical_source'], '56a2de16366cd5bfe18628e941ea84849cfd4153')

    def test_compatible_bytes_and_metadata_need_no_repinning(self):
        self.sources.pin = {'commit': 'another declared revision', 'distribution': 'custom'}
        for name in (compat.ROOT, compat.JAPANESE):
            self.sources.data[name] += b' custom comments'
        result = compat.japanese_word_fe(self.sources, records())
        self.assertEqual(len(result.corrections), 1)
        self.assertEqual(result.corrections[0].commit, self.sources.pin['commit'])
        self.assertEqual(result.corrections[0].source_sha256,
                         hashlib.sha256(self.sources.data[compat.JAPANESE]).hexdigest())

    def test_already_correct_or_absent_tailoring_needs_no_correction(self):
        root, japanese = records()
        fixed_root = root._replace(variables=root.variables + (compat.HISTORICAL_FE,))
        corrected = compat.japanese_word_fe(self.sources, (root, japanese))
        for data in ((root,), (fixed_root, japanese), corrected.records):
            with self.subTest(data=data):
                result = compat.japanese_word_fe(self.sources, data)
                self.assertEqual(result.records, data)
                self.assertFalse(result.corrections)
        prepare_variables(capture_variables((fixed_root, japanese), graph(), 'ja', 'WordBreak'))
        prepare_variables(capture_variables(corrected.records, graph(), 'ja', 'WordBreak'))

    def test_unknown_defects_remain_strict_compiler_errors(self):
        root, japanese = records()
        cases = [
            (root, japanese._replace(source='other.xml')),
            (root._replace(variables=(Definition('$Extend', '[]'),) + root.variables[1:]), japanese),
            (root._replace(variables=root.variables[:-1]), japanese),
        ]
        for data in cases:
            result = compat.japanese_word_fe(self.sources, data)
            self.assertEqual(result.records, data)
            self.assertFalse(result.corrections)
            with self.subTest(data=data), self.assertRaises(DataError):
                prepare_variables(capture_variables(result.records, graph(), 'ja', 'WordBreak'))


if __name__ == '__main__':
    unittest.main()
