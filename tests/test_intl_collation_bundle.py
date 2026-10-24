#!/usr/bin/env python3
"""Nested compiled data must match the enclosing source/profile exactly."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_intl_collation_ir import encode, program, FINGERPRINT
from test_intl_collation_root_input import root, GROUPS, wire
import collation_archive as archive
import collation_bundle as compiled
from collation_bindings import Binding
from cldr_xml import DataError
import bundle
import generate
import tables
from test_intl_tables import VerifiedFixture

compiler = None


class BundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if compiler is None:
            raise unittest.SkipTest('pass --compiler to exercise compiled collation data')
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            root_path, ir = directory / 'root.qjcr', directory / 'rules.qjci'
            root_path.write_bytes(wire.encode(root(), GROUPS, FINGERPRINT))
            ir.write_bytes(encode([program('&a<b'), program('[numericOrdering on]', 'fr')]))
            result = subprocess.run([compiler, '--compile-pair', str(root_path), str(ir)],
                                    timeout=10, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode:
                raise RuntimeError(result.stderr.decode())

        cls.raw = result.stdout
        cls.en, cls.fr = ('en', 'standard', ''), ('fr', 'standard', '')
        cls.rows = (Binding('en', '', cls.en, cls.fr),)
        cls.data = archive.bind(cls.raw, cls.rows, FINGERPRINT)

    def test_frame_counts_optional_presence_and_borrowed_identities(self):
        self.assertIsNone(compiled.decode(None))
        decoded = compiled.decode((2, self.data))
        self.assertEqual(decoded['fingerprint'], FINGERPRINT)
        self.assertEqual(decoded['programs'], (self.en, self.fr))
        self.assertEqual(decoded['bindings'], self.rows)
        for section in ((0, self.data), (1, self.data), (3, self.data), (2, self.raw)):
            with self.assertRaises(DataError):
                compiled.decode(section)

    def test_pairing_requires_exact_source_profile_bindings_and_target_set(self):
        class Sources:
            def __init__(self):
                self.consumed = []

            def read(self, name):
                self.consumed.append(name)
                return b'authored root input'

        for profile in ('minimal', 'native'):
            source = Sources()
            with patch.object(compiled.collation_bindings, 'collect', return_value=self.rows) as collect:
                self.assertEqual(compiled.pair(source, profile, FINGERPRINT, self.data), (2, self.data))
            collect.assert_called_once_with(source, profile)
            self.assertEqual(source.consumed, [compiled.collation_root.SOURCE, compiled.collation_groups.SOURCE,
                                              compiled.collation_groups.SCRIPTS])

        for rows in ((self.rows[0]._replace(search=self.en),),
                     (self.rows[0]._replace(locale='fr'),),
                     self.rows + (Binding('en', 'emoji', self.en, None),)):
            with patch.object(compiled.collation_bindings, 'collect', return_value=rows):
                with self.assertRaises(DataError):
                    compiled.pair(Sources(), 'minimal', FINGERPRINT, self.data)
        with self.assertRaises(DataError):
            compiled.pair(Sources(), 'minimal', b'x' * 32, self.data)

        # Correct binding meanings alone are insufficient: reject an unused
        # standalone program left over from whole-catalog diagnostic compilation.
        rows = (Binding('en', '', self.en, self.en),)
        unpruned = archive.bind(self.raw, rows, FINGERPRINT)
        with patch.object(compiled.collation_bindings, 'collect', return_value=rows):
            with self.assertRaises(DataError):
                compiled.pair(Sources(), 'minimal', FINGERPRINT, unpruned)

    def test_outer_recipe_tracks_nested_bytes_and_separate_source_identity(self):
        source = VerifiedFixture()
        source.pin['manifest_sha256'] = FINGERPRINT.hex()
        section = 2, self.data

        # This authored partial source fixture has no collation XML. Isolate
        # pairing (tested above) while exercising actual QJIN generation/hash.
        with patch.object(generate.collation_bundle, 'pair', return_value=section) as pair:
            binary, manifest = generate.generate(source, 'minimal', self.data)
        self.assertEqual(pair.call_args[0][1:], ('minimal', FINGERPRINT, self.data))
        view = tables.validate(binary)
        self.assertEqual(view['compiled_collation']['bindings'], self.rows)
        self.assertEqual(view['envelope']['sections'][bundle.COMPILED_COLLATION], section)
        self.assertNotEqual(view['envelope']['generation'], FINGERPRINT)
        self.assertEqual(manifest['inputs']['sections']['38']['count'], 2)
        self.assertEqual(manifest['inputs']['sections']['38']['sha256'], generate.acquire.digest(self.data))

        without, old = generate.generate(source, 'minimal')
        self.assertIsNone(tables.validate(without)['compiled_collation'])
        self.assertNotEqual(old['generation'], manifest['generation'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--compiler')
    args, remaining = parser.parse_known_args()
    compiler = args.compiler
    unittest.main(argv=[sys.argv[0]] + remaining)
