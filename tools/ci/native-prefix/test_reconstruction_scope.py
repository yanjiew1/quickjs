"""Pure approved rename/retirement derivation checks."""
import copy
import json
from pathlib import Path
import unittest
from reconstruction_scope import required_paths


class ReconstructionScopeTests(unittest.TestCase):
    def setUp(self):
        here = Path(__file__).resolve().parent
        self.historical = json.loads((here / 'production-source-sha256.json').read_text())
        self.derivation = json.loads((here / 'source-path-derivation.json').read_text())

    def test_approved_replacements_and_coverage_are_required(self):
        paths = required_paths(self.historical, self.derivation)
        for old, row in self.derivation['replacements'].items():
            self.assertNotIn(old, paths)
            self.assertIn(row['path'], paths)
        for old, row in self.derivation['omissions'].items():
            self.assertNotIn(old, paths)
            self.assertTrue(set(row['covered_by']) <= paths)
        self.assertTrue(set(self.derivation['additional_sources']) <= paths)

    def test_historical_bytes_remain_immutable(self):
        changed = dict(self.historical, Makefile='0' * 64)
        with self.assertRaises(RuntimeError):
            required_paths(changed, self.derivation)

    def test_unknown_retirement_is_rejected(self):
        for field in ('replacements', 'omissions'):
            changed = copy.deepcopy(self.derivation)
            changed[field]['unreviewed-path.c'] = next(iter(changed[field].values()))
            with self.assertRaises(RuntimeError):
                required_paths(self.historical, changed)

    def test_retirement_cannot_drop_replacement_coverage(self):
        changed = copy.deepcopy(self.derivation)
        next(iter(changed['omissions'].values()))['covered_by'] = []
        with self.assertRaises(RuntimeError):
            required_paths(self.historical, changed)

    def test_retired_source_cannot_be_reintroduced(self):
        changed = copy.deepcopy(self.derivation)
        changed['additional_sources'].append(next(iter(changed['omissions'])))
        with self.assertRaises(RuntimeError):
            required_paths(self.historical, changed)

    def test_duplicate_or_unsafe_source_is_rejected(self):
        for value in ('../escape', '/absolute', 'a\\b', self.derivation['additional_sources'][0]):
            changed = copy.deepcopy(self.derivation)
            changed['additional_sources'].append(value)
            with self.assertRaises(RuntimeError):
                required_paths(self.historical, changed)

    def test_unknown_derivation_field_is_rejected(self):
        changed = dict(self.derivation, ignore_missing_sources=True)
        with self.assertRaises(RuntimeError):
            required_paths(self.historical, changed)
