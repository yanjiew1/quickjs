"""Pure MOCK filesystem tests of extracted input_identity; never import run.py.

Root executes these. AST extraction excludes all runner top-level side effects,
Git, processes, runtime commands and engine execution.
"""
import ast
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


class CurrentCheckoutIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source = Path(__file__).with_name('run.py').read_text()
        tree = ast.parse(source)
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef)
                     and node.name in ('sha', 'canonical', 'require', 'input_identity')]
        self.assertEqual({node.name for node in functions}, {'sha', 'canonical', 'require', 'input_identity'})
        self.namespace = dict(hashlib=hashlib, json=json, Path=Path, os=os,
            HERE=self.root / 'helpers', OUT=self.root / 'out', FIXTURE_MAP=None,
            HISTORICAL_BASELINE_PRODUCTION_MAP='c71443f857120b4ece57b2d4c2ce635e9b54bacf97f33f5eb4b6177c839a3b70')
        # Root-owned MOCK execution of only the exact selected pure filesystem helpers.
        exec(compile(ast.Module(body=functions, type_ignores=[]), 'MOCK-input-identity', 'exec'), self.namespace)
        self.write('helpers/mock-helper.py', b'current helper')
        self.write('production.c', b'current production')
        self.write('test262/mock.js', b'unchanged mock fixture')
        self.write('test262-intl.conf', b'unchanged mock configuration')
        self.write('test262_errors.txt', b'unchanged mock baseline14')
        self.write('qjs', b'MOCK binary bytes; never executed')
        self.write('run-test262', b'MOCK runner bytes; never executed')
        self.namespace['FIXTURE_MAP'] = self.namespace['canonical']({'mock.js':self.hash('test262/mock.js')})
        self.production = {name:self.hash(name) for name in ('production.c', 'helpers/mock-helper.py')}
        self.current_head = '1' * 40
        self.extra_sources = {}
        self.mode_overrides = {}
        self.links = {}
        self.mode_capabilities = {'root': dict(platform='MOCK', core_filemode=None, stat_execute_bits_enforced=True)}
        self.namespace['inspect_checkout'] = lambda: dict(
            head=self.current_head, sources=dict(self.production, **self.extra_sources),
            modes=dict({name: '100644' for name in self.production}, **self.mode_overrides),
            gitlinks=self.links, mode_capabilities=self.mode_capabilities)
        self.binding = dict(source_sha256=self.production,
                            source_map_sha256=self.namespace['canonical'](self.production), adapter_sha256={},
                            git_modes={name: '100644' for name in self.production},
                            adapter_git_modes={}, gitlinks={},
                            mode_capabilities=copy.deepcopy(self.mode_capabilities))
        self.document('helpers/binding.json', self.binding)
        self.document('out/binding.json', self.binding)
        self.document('out/actual-production-source-sha256.json', self.production)
        self.initialized = dict(candidate_sha='1'*40, gitlinks={},
            mode_capabilities=copy.deepcopy(self.mode_capabilities),
            binding_sha256=self.hash('helpers/binding.json'),
            actual_production_source_map_sha256=self.namespace['canonical'](self.production))
        self.document('out/execution-identity.json', self.initialized)

    def write(self, name, raw):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)

    def document(self, name, value):
        self.write(name, (json.dumps(value, indent=2)+'\n').encode())

    def hash(self, name):
        return hashlib.sha256((self.root / name).read_bytes()).hexdigest()

    def identity(self):
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            with patch.dict(os.environ, GITHUB_SHA='1'*40):
                return self.namespace['input_identity']()
        finally:
            os.chdir(previous)

    def test_refreshed_top_level_census_modes_and_head_cannot_drift(self):
        for kind in ('added-source', 'executable-mode', 'head', 'gitlink'):
            with self.subTest(kind=kind):
                if kind == 'added-source': self.extra_sources['unexpected.c'] = 'a' * 64
                elif kind == 'executable-mode': self.mode_overrides['production.c'] = '100755'
                elif kind == 'head': self.current_head = '2' * 40
                else: self.links['third_party/tz'] = {'commit': '3' * 40}
                with self.assertRaises(RuntimeError):
                    self.identity()
                self.extra_sources.clear()
                self.mode_overrides.clear()
                self.links.clear()
                self.current_head = '1' * 40

    def test_filesystem_mode_capability_drift_is_rejected(self):
        self.mode_capabilities['root']['stat_execute_bits_enforced'] = False
        with self.assertRaisesRegex(RuntimeError, 'filesystem mode capabilities'):
            self.identity()

    def test_actual_current_checkout_map_can_differ_from_historical_baseline(self):
        result = self.identity()
        self.assertEqual(result['production_source_map_sha256'], self.namespace['canonical'](self.production))
        self.assertNotEqual(result['production_source_map_sha256'], result['historical_baseline_production_source_map_sha256'])
        self.assertEqual(result['actual_initialized_checkout_commit'], '1'*40)

    def test_current_map_binding_commit_and_checked_out_byte_drift_blocks(self):
        for mutation in ('source', 'helper', 'binding', 'map', 'commit'):
            with self.subTest(mutation=mutation):
                if mutation == 'source': self.write('production.c', b'changed production')
                elif mutation == 'helper': self.write('helpers/mock-helper.py', b'changed helper')
                elif mutation == 'binding': self.write('helpers/binding.json', b'{}')
                elif mutation == 'map': self.document('out/actual-production-source-sha256.json', {})
                else:
                    self.initialized['candidate_sha'] = '2'*40
                    self.document('out/execution-identity.json', self.initialized)
                with self.assertRaises((RuntimeError, KeyError)):
                    self.identity()
                self.write('production.c', b'current production')
                self.write('helpers/mock-helper.py', b'current helper')
                self.document('helpers/binding.json', self.binding)
                self.document('out/actual-production-source-sha256.json', self.production)

    def test_derivative_adapter_is_in_full_current_identity_and_drift_blocks(self):
        self.write('adapter.py', b'authenticated derivative adapter')
        self.binding['adapter_sha256'] = {'adapter.py': self.hash('adapter.py')}
        self.binding['adapter_git_modes'] = {'adapter.py': '100644'}
        self.production = dict(self.production, **self.binding['adapter_sha256'])
        self.assertNotEqual(self.binding['source_map_sha256'], self.namespace['canonical'](self.production))
        self.document('helpers/binding.json', self.binding)
        self.document('out/binding.json', self.binding)
        self.document('out/actual-production-source-sha256.json', self.production)
        self.initialized['binding_sha256'] = self.hash('helpers/binding.json')
        self.initialized['actual_production_source_map_sha256'] = self.namespace['canonical'](self.production)
        self.document('out/execution-identity.json', self.initialized)
        identity = self.identity()
        self.assertEqual(identity['actual_full_tracked_source_map_sha256'], self.namespace['canonical'](self.production))
        self.assertEqual(identity['actual_full_tracked_source_map_sha256'], identity['production_source_map_sha256'])
        self.write('adapter.py', b'changed derivative adapter')
        with self.assertRaises(RuntimeError):
            self.identity()

    def test_new_current_source_is_included_and_cannot_be_omitted_from_actual_map(self):
        self.write('added.c', b'new current committed source')
        self.production['added.c'] = self.hash('added.c')
        self.binding['git_modes']['added.c'] = '100644'
        self.binding['source_map_sha256'] = self.namespace['canonical'](self.production)
        self.document('helpers/binding.json', self.binding)
        self.document('out/binding.json', self.binding)
        self.initialized['binding_sha256'] = self.hash('helpers/binding.json')
        self.initialized['actual_production_source_map_sha256'] = self.namespace['canonical'](self.production)
        self.document('out/execution-identity.json', self.initialized)
        with self.assertRaises(RuntimeError): self.identity()
        self.document('out/actual-production-source-sha256.json', self.production)
        self.assertEqual(self.identity()['production_source_map_sha256'], self.namespace['canonical'](self.production))


if __name__ == '__main__':
    unittest.main()
