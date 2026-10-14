"""Exercise actual repository coverage receipts without starting an engine."""
import ast
import json
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


class RepositoryCensusTests(unittest.TestCase):
    def exercise(self, special, logged=True, extra=(), mutate_extra=False):
        tree = ast.parse((HERE / 'run.py').read_text())
        body = next(node for node in tree.body
                    if isinstance(node, ast.FunctionDef) and node.name == 'repository_tests')
        module = ast.Module(body=[body], type_ignores=[])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            row = dict(C_targets=['tests/unit'], JS_files=['tests/basic.js'],
                       special_C_units=[], extra_C_invocations=list(extra),
                       special_JS_invocations=special)
            scope = dict(source_map=None, make_source_sha256='digest', native=row)
            (root / 'activation-scope.json').write_text(json.dumps(scope))
            (root / 'binding.json').write_text(json.dumps(
                dict(shared_timezone_snapshot_extension_adopted=True)))
            for entry in special:
                source = root / entry['source']
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_text('// fixture')
            saved, called, changed = {}, [], set()

            def require(value, message):
                if not value:
                    raise RuntimeError(message)

            def run(label, argv, **kwargs):
                called.append((label, argv))
                if mutate_extra and label.startswith('unicode-provider-oracle-'):
                    changed.add(extra[0]['target'])
                (root / (label + '.command.json')).write_text(json.dumps({'returncode': 0}))
                if label == 'repository-tests':
                    lines = ['./tests/unit', './qjs tests/basic.js']
                    if logged:
                        lines += [' '.join(entry['argv']) for entry in special]
                    return '\n'.join(lines)
                return ''

            namespace = dict(HERE=root, OUT=root, BACKEND='native', Path=lambda value: root / value,
                             json=json, require=require,
                             sha=lambda path: 'changed' if path in changed else 'digest',
                             special_c_units=lambda row: [], run=run,
                             save=lambda name, value: saved.update({name: value}))
            exec(compile(module, str(HERE / 'run.py'), 'exec'), namespace)
            namespace['repository_tests'](['make'])
            return saved['repository-invocation-coverage.json'], called

    def test_current_make_and_wait_async_census(self):
        import hashlib
        scope = json.loads((HERE / 'activation-scope.json').read_text())
        root = HERE.parents[2]
        self.assertEqual(scope['make_source_sha256'],
                         hashlib.sha256((root / 'Makefile').read_bytes()).hexdigest())
        for backend in ('native', 'icu'):
            self.assertIn('tests/test_wait_async', scope[backend]['C_targets'])

    def test_basic_c_and_js_census(self):
        receipt, calls = self.exercise([])
        self.assertTrue(receipt['completed'])
        self.assertEqual(len(receipt['records']), 2)
        self.assertEqual(len(calls), 1)

    def test_extra_c_modes_snapshot_binary_outside_basic_census(self):
        entries = [dict(target='tests/test_unicode_provider', argv=[
            './tests/test_unicode_provider', mode])
            for mode in ('--exhaustive', '--properties')]
        receipt, calls = self.exercise([], extra=entries)
        self.assertTrue(receipt['completed'])
        self.assertEqual(len(receipt['records']), 4)
        for index, entry in enumerate(entries):
            record = receipt['records']['unicode-provider-oracle-' + str(index)]
            self.assertEqual(record['binary_sha256_before'], 'digest')
            self.assertEqual(record['binary_sha256_after'], 'digest')
            self.assertEqual(calls[index + 1][1], entry['argv'])

    def test_extra_c_binary_change_is_rejected(self):
        entry = dict(target='tests/test_unicode_provider', argv=[
            './tests/test_unicode_provider', '--exhaustive'])
        with self.assertRaisesRegex(RuntimeError, 'Unicode oracle binary changed'):
            self.exercise([], extra=[entry], mutate_extra=True)

    def test_logged_zone_launcher_counts_as_covered(self):
        entry = dict(source='tests/zone.js', argv=[
            './tests/run_temporal_zone_test', 'UTC', './qjs', 'tests/zone.js'])
        receipt, calls = self.exercise([entry])
        self.assertTrue(receipt['completed'])
        self.assertEqual(len(receipt['records']), 3)
        self.assertEqual(receipt['records']['special-js-0']['repository_invocation_count'], 1)
        self.assertEqual(len(calls), 1)

    def test_missing_logged_mode_receives_independent_receipt(self):
        entry = dict(source='tests/zone.js', argv=['./qjs', 'tests/zone.js', '--mode'])
        receipt, calls = self.exercise([entry], logged=False)
        self.assertTrue(receipt['completed'])
        self.assertEqual(calls[-1], ('special-js-0', entry['argv']))
        self.assertEqual(receipt['records']['special-js-0']['returncode'], 0)

    def test_current_locale_fixture_binding_rejects_changed_bytes(self):
        tree = ast.parse((HERE / 'run.py').read_text())
        body = next(node for node in tree.body if isinstance(node, ast.FunctionDef)
                    and node.name == 'verify_host_default_locale_fixture')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'activation-scope.json').write_text(json.dumps(
                {'host_default_locale_fixture_sha256': 'reviewed-current-digest'}))
            def require(value, message):
                if not value:
                    raise RuntimeError(message)
            namespace = dict(HERE=root, json=json, require=require,
                             sha=lambda path: 'reviewed-current-digest')
            exec(compile(ast.Module(body=[body], type_ignores=[]), '', 'exec'), namespace)
            namespace['verify_host_default_locale_fixture']()
            namespace['sha'] = lambda path: 'changed-digest'
            with self.assertRaises(RuntimeError):
                namespace['verify_host_default_locale_fixture']()

    def test_direct_calendar_fixture_names_and_flags(self):
        inventory = json.loads((HERE / 'library-inventory.json').read_text())['enabled-suffix']
        names = {unit['name'] for unit in inventory['units']}
        self.assertIn('test_intl_calendar_domain', names)
        self.assertNotIn('test_calendar_domain', names)
        self.assertTrue({'test_calendar_portable', 'test_calendar_persian_authority',
                         'test_calendar_integrated_authority'} <= names)
        tree = ast.parse((HERE / 'run.py').read_text())
        owner = next(node for node in tree.body
                     if isinstance(node, ast.FunctionDef) and node.name == 'native_units')
        assignment = next(node for node in ast.walk(owner)
                          if isinstance(node, ast.Assign) and isinstance(node.value, ast.IfExp)
                          and isinstance(node.value.body, ast.Name)
                          and node.value.body.id == 'calendar' and
                          'unit' in ast.unparse(node.value.test))
        for name in names & {'test_intl_calendar_domain', 'test_calendar_portable',
                             'test_calendar_persian_authority',
                             'test_calendar_integrated_authority'}:
            self.assertEqual(eval(compile(ast.Expression(assignment.value), '', 'eval'),
                                  dict(unit={'name': name}, calendar=['-Dauthority'])),
                             ['-Dauthority'])


if __name__ == '__main__':
    unittest.main()
