"""Pure reviewed-fixture selection tests; no engine or Test262 execution."""
import ast
from pathlib import Path
import unittest

class DataProfileTests(unittest.TestCase):
    def setUp(self):
        source = Path(__file__).with_name('run.py')
        tree = ast.parse(source.read_text())
        nodes = [node for node in tree.body if isinstance(node, ast.FunctionDef)
                 and node.name == 'test262_data_profile']
        self.assertEqual(len(nodes), 1)
        namespace = {}
        exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), namespace)
        self.profile = namespace['test262_data_profile']

    def test_native_keeps_exact_existing_overlay_map_and_census(self):
        profile = self.profile('native')
        self.assertEqual(profile['unicode_data_version'], [18, 0, 0, 0])
        self.assertEqual(profile['fixture_source_map_sha256'],
                         '808f46746cde3b35db59c83dc7ecb9b91df7970bcae65572ffd4ee126547896d')
        self.assertEqual(profile['runner_distinct_source_count'], 53617)
        self.assertEqual(set(profile['patches']),
                         {'tests/test262.patch', 'tests/test262-unicode18.patch'})

    def test_icu_uses_independently_derived_official_unicode17_map(self):
        profile = self.profile('icu')
        self.assertEqual(profile['unicode_data_version'], [17, 0, 0, 0])
        self.assertEqual(profile['fixture_source_map_sha256'],
                         '994760adcc776f52a7a7fe0c04a7d12307117789bad2e06d501defbbdf8adf40')
        self.assertEqual(profile['runner_distinct_source_count'], 53611)
        self.assertEqual(set(profile['patches']), {'tests/test262.patch'})

    def test_no_profile_grants_latest_unicode_conformance(self):
        for backend in ('native', 'icu'):
            profile = self.profile(backend)
            self.assertIs(profile['zero_error_conformance_claim'], False)
            self.assertIs(profile['latest_unicode_conformance_claim'], False)

    def test_invalid_backend_rejected(self):
        with self.assertRaises(ValueError):
            self.profile('unknown')

    def test_profile_mutation_does_not_change_future_selection(self):
        profile = self.profile('native')
        profile['patches'].clear()
        profile['unicode_data_version'][0] = 0
        self.assertEqual(len(self.profile('native')['patches']), 2)
        self.assertEqual(self.profile('icu')['unicode_data_version'], [17, 0, 0, 0])

if __name__ == '__main__':
    unittest.main()
