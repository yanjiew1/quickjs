#!/usr/bin/env python3
"""Version/hash-bound Japanese source correction without raw-source mutation."""
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
from segment_inheritance import capture_variables
from segment_rules import prepare_variables
from segment_sources import Definition, Segmentation
import segment_compat as compat


class Sources:
    def __init__(self):
        self.pin = {'commit': compat.PIN}
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
        hashes = {name: hashlib.sha256(data).hexdigest() for name, data in self.sources.data.items()}
        # Keep authored tests offline while exercising the real hash check.
        # Production constants are separately checked against authenticated data.
        self.patches = mock.patch.object(compat, 'HASHES', hashes)
        self.patches.start()
        self.addCleanup(self.patches.stop)

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
        self.assertEqual(receipt['commit'], compat.PIN)
        self.assertEqual(receipt['historical_source'], '56a2de16366cd5bfe18628e941ea84849cfd4153')

    def test_reject_unreviewed_pin_or_modified_source_bytes(self):
        self.sources.pin['commit'] = 'unreviewed'
        with self.assertRaisesRegex(DataError, 'source pin'):
            compat.japanese_word_fe(self.sources, records())
        self.sources.pin['commit'] = compat.PIN
        for name in (compat.ROOT, compat.JAPANESE):
            original = self.sources.data[name]
            self.sources.data[name] += b'changed'
            with self.subTest(name=name), self.assertRaisesRegex(DataError, 'changed.*source'):
                compat.japanese_word_fe(self.sources, records())
            self.sources.data[name] = original

    def test_distribution_guards_use_separate_reviewed_byte_hashes(self):
        self.sources.pin['distribution'] = 'unknown'
        with self.assertRaisesRegex(DataError, 'source distribution'):
            compat.japanese_word_fe(self.sources, records())
        self.sources.pin['distribution'] = 'git'
        with self.assertRaisesRegex(DataError, 'changed.*source'):
            compat.japanese_word_fe(self.sources, records())
        hashes = {name: hashlib.sha256(data).hexdigest() for name, data in self.sources.data.items()}
        with mock.patch.object(compat, 'GIT_HASHES', hashes):
            result = compat.japanese_word_fe(self.sources, records())
        self.assertEqual(result.corrections[0].root_sha256, hashes[compat.ROOT])
        self.assertEqual(result.corrections[0].source_sha256, hashes[compat.JAPANESE])

    def test_reject_changed_declarations_missing_duplicate_or_reapplied_tailoring(self):
        root, japanese = records()
        cases = [(root,), (root, japanese, japanese),
                 (root, japanese._replace(source='other.xml')),
                 (root, japanese._replace(variables=japanese.variables[:1])),
                 (root._replace(variables=(Definition('$Extend', '[]'),) + root.variables[1:]), japanese),
                 (root._replace(variables=root.variables[:-1]), japanese),
                 (root._replace(variables=root.variables + (Definition('$FE', '[]'),)), japanese)]
        corrected = compat.japanese_word_fe(self.sources, (root, japanese))
        cases.append(corrected.records)
        for data in cases:
            with self.subTest(data=data), self.assertRaises(DataError):
                compat.japanese_word_fe(self.sources, data)


if __name__ == '__main__':
    unittest.main()
