"""Pure extracted supplemental source-binding tests; no runner import."""
import ast
import hashlib
import json
from pathlib import Path
from types import SimpleNamespace
import unittest
from reconstruction_scope import required_paths


class SupplementalCurrentSourceTests(unittest.TestCase):
    def setUp(self):
        here = Path(__file__).resolve().parent
        qualification = here.parent / 'stop39b-qualification'
        tree = ast.parse((qualification / 'qualify.py').read_text())
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef)
                     and node.name == 'current_checkout_source_map']
        self.assertEqual(len(functions), 1)
        def canonical(value):
            return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()
        def require(value, reason):
            if not value:
                raise RuntimeError(reason)
        namespace = {'r': SimpleNamespace(canonical=canonical, require=require), 'required_paths': required_paths}
        exec(compile(ast.Module(body=functions, type_ignores=[]),
                     'MOCK-supplemental-source-binding', 'exec'), namespace)
        self.bind = namespace['current_checkout_source_map']
        self.canonical = canonical
        self.historical = json.loads((here / 'production-source-sha256.json').read_text())
        self.required = json.loads((qualification / 'current-required-source-sha256.json').read_text())
        self.derivation = json.loads((here / 'source-path-derivation.json').read_text())
        self.required = {path: self.required.get(path, 'e' * 64) for path in
                         required_paths(self.historical, self.derivation)}
        # MOCK source maps only; no Git, subprocess, project import or engine.
        self.committed = dict(self.required, **{'MOCK-added-current-source.c': 'f' * 64})

    def test_current_complete_map_differs_from_preserved_historical_baseline(self):
        current = self.bind(self.committed, self.historical, self.required, self.derivation)
        self.assertEqual(current, self.canonical(self.committed))
        self.assertNotEqual(current, self.canonical(self.historical))

    def test_changed_current_required_source_rejected(self):
        for path in self.required:
            with self.subTest(path=path):
                changed = dict(self.committed)
                changed[path] = '0' * 64 if changed[path] != '0' * 64 else '1' * 64
                with self.assertRaises(RuntimeError):
                    self.bind(changed, self.historical, self.required, self.derivation)

    def test_missing_current_required_source_rejected(self):
        for path in self.required:
            with self.subTest(path=path):
                changed = dict(self.committed)
                del changed[path]
                with self.assertRaises(RuntimeError):
                    self.bind(changed, self.historical, self.required, self.derivation)

    def test_missing_or_extra_required_manifest_path_rejected(self):
        changed = dict(self.required)
        del changed['tools/ci/native-prefix/test_run_current_identity.py']
        with self.assertRaises(RuntimeError):
            self.bind(self.committed, self.historical, changed, self.derivation)
        changed = dict(self.required, **{'MOCK-added-current-source.c': 'f' * 64})
        with self.assertRaises(RuntimeError):
            self.bind(self.committed, self.historical, changed, self.derivation)

    def test_changed_historical_manifest_rejected(self):
        changed = dict(self.historical)
        changed['Makefile'] = '0' * 64
        with self.assertRaises(RuntimeError):
            self.bind(self.committed, changed, self.required, self.derivation)

    def test_additional_current_tracked_files_remain_in_complete_identity(self):
        original = self.bind(self.committed, self.historical, self.required, self.derivation)
        changed = dict(self.committed, **{'MOCK-second-new-source.c': 'e' * 64})
        result = self.bind(changed, self.historical, self.required, self.derivation)
        self.assertEqual(result, self.canonical(changed))
        self.assertNotEqual(result, original)


class SupplementalMakeChoiceTests(unittest.TestCase):
    def arguments(self, row):
        source = Path(__file__).resolve().parent.parent / 'stop39b-qualification/qualify.py'
        tree = ast.parse(source.read_text())
        nodes = [node for node in tree.body
                 if isinstance(node, ast.FunctionDef) and node.name == 'make_args'
                 or isinstance(node, ast.Assign) and any(
                     isinstance(target, ast.Name) and target.id == 'MAKE'
                     for target in node.targets)]
        namespace = {'ROW': row, 'r': SimpleNamespace(BACKEND='native')}
        exec(compile(ast.Module(body=nodes, type_ignores=[]),
                     'MOCK-supplemental-Make-choice', 'exec'), namespace)
        return namespace['make_args']()

    def test_macos_uses_gnu_make_and_clang(self):
        args = self.arguments('macos')
        self.assertEqual(args[0], 'gmake')
        self.assertIn('CC=clang', args)
        self.assertIn('HOST_CC=clang', args)
        self.assertIn('CONFIG_CLANG=y', args)

    def test_other_native_rows_keep_existing_make_selection(self):
        for row in ('m32', 'mingw', 'msys2'):
            with self.subTest(row=row):
                args = self.arguments(row)
                self.assertEqual(args[0], 'make')
                self.assertIn('CONFIG_INTL_BACKEND=native', args)
        self.assertIn('CONFIG_M32=y', self.arguments('m32'))


if __name__ == '__main__':
    unittest.main()
