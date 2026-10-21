#!/usr/bin/env python3
"""Profile targets remain complete after pruning standalone helper programs."""
import argparse
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_intl_collation_programs import catalog, program, keyword
from test_intl_collation_root_input import root, GROUPS, FINGERPRINT, wire
import collation_archive as archive
import collation_bindings
import collation_prepare as preparation
from cldr_xml import DataError
import parents

compiler = None


def fixture_catalog():
    c = catalog((program('en', text='[import fr-u-co-private-helper] &x<y'),
                 program('en-US', 'special', '&w<z'), program('fr', text='&d<e'),
                 program('fr', 'private-helper', '&a<b'), program('root', 'emoji', '&u<v')),
                keywords=(keyword('special'), keyword('emoji')))
    graph = parents.ParentGraph(('root', 'en', 'en-US', 'fr'), {}, {'collations': {}},
                                {'und': 'en-Latn-US'}, (), False)
    return type(c)(dict(c.programs), dict(c.defaults), graph, (keyword('special'), keyword('emoji')))


class PreparationTests(unittest.TestCase):
    def test_pruned_profiles_execute_and_bind_through_real_compiler(self):
        if compiler is None:
            self.skipTest('pass --compiler to exercise the prepared profile pipeline')
        class Sources:
            pin = {'manifest_sha256': FINGERPRINT.hex()}

        c = fixture_catalog()
        raw_root = wire.encode(root(), GROUPS, FINGERPRINT)
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for profile in ('minimal', 'native'):
                output = directory / (profile + '.qjcl')
                with patch.object(preparation.collation_programs, 'load', return_value=c), \
                     patch.object(preparation.collation_root_input, 'prepare', return_value=raw_root):
                    report = preparation.prepare(Sources(), profile, Path(compiler), output)

                view = archive.unpack(output.read_bytes(), FINGERPRINT)
                targets = collation_bindings.program_targets(collation_bindings.from_catalog(c, profile))
                self.assertEqual(view['program_count'], len(targets))
                self.assertEqual(report['programs'], len(targets))
                self.assertEqual(report['binding_rows'], view['counts'][11])
                self.assertEqual(report['bytes'], output.stat().st_size)
                # Staging is gone; accepted bytes retain all compiled state.
                self.assertEqual(sorted(p.name for p in directory.iterdir()),
                                 ['minimal.qjcl'] if profile == 'minimal' else ['minimal.qjcl', 'native.qjcl'])

            output = directory / 'minimal.qjcl'
            accepted = output.read_bytes()
            with patch.object(preparation.collation_programs, 'load', return_value=c), \
                 patch.object(preparation.collation_root_input, 'prepare', side_effect=DataError('source mismatch')):
                with self.assertRaises(DataError):
                    preparation.prepare(Sources(), 'minimal', Path(compiler), output)
            self.assertEqual(output.read_bytes(), accepted)
            self.assertEqual(len(list(directory.iterdir())), 2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--compiler')
    args, remaining = parser.parse_known_args()
    compiler = args.compiler
    unittest.main(argv=[sys.argv[0]] + remaining)
