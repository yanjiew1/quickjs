"""Test selection and instrumentation contracts without building an engine."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('runner', Path(__file__).with_name('history_runner.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class ConfigurationTests(unittest.TestCase):
    def source(self, root, meson=False, options=''):
        root.mkdir()
        subprocess.run(['git', 'init', '-q', str(root)], check=True)
        (root / 'run-test262.c').write_text('old runner')
        if meson:
            (root / 'meson.build').write_text("project('historical', 'c')\n")
            (root / 'meson_options.txt').write_text(options)
        subprocess.run(['git', '-C', str(root), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(root), '-c', 'user.name=Fixture',
                        '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'fixture'], check=True)
        return root

    def test_old_make_keeps_only_available_features(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = self.source(root / 'source')
            state = runner.settings(source, root / 'reports', 'asan-ubsan')
            self.assertEqual(state['features'], {})
            self.assertEqual(state['build_system'], 'make')
            self.assertEqual(state['compiler'], 'gcc')
            self.assertEqual(state['sanitizer'], 'address,undefined')
            self.assertFalse(state['timing'])
            self.assertFalse(state['progress'])
            flags = runner.make_args(state, root / 'cc')
            self.assertIn('CONFIG_ASAN=y', flags)
            self.assertIn('CONFIG_UBSAN=y', flags)
            self.assertIn('CONFIG_LTO=', flags)

    def test_selects_only_declared_full_profile_options(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = self.source(root / 'source', True, '\n'.join(
                "option('%s', type: 'string')" % name for name in
                ['temporal', 'intl', 'calendars', 'timezone_backend', 'system_timezone_policy']))
            (source / 'tests').mkdir()
            (source / 'tests/intl-locale-test262.conf').write_text('[config]\n')
            state = runner.settings(source, root / 'reports', 'msan')
            self.assertEqual(state['features'], dict(temporal='true', intl='native', calendars='native',
                                                    timezone_backend='native', system_timezone_policy='utc'))
            self.assertTrue(state['intl_config'])
            self.assertEqual(state['build_system'], 'meson')
            self.assertEqual(state['compiler'], 'clang')
            self.assertEqual(state['sanitizer'], 'memory')

    def test_wrapper_preserves_original_flags_and_instruments_generated_links(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            state = dict(compiler='clang', sanitizer='memory')
            wrapper = runner.compiler_wrapper(root, state)
            fake = root / 'bin'
            fake.mkdir()
            compiler = fake / 'clang'
            compiler.write_text('#!/usr/bin/env python3\nimport json, sys\nprint(json.dumps(sys.argv[1:]))\n')
            compiler.chmod(0o755)
            env = dict(os.environ, PATH=str(fake) + os.pathsep + os.environ['PATH'])
            def invoke(*args):
                return json.loads(subprocess.check_output([str(wrapper), *args], env=env))
            compile_args = invoke('input.c', '-c', '-O2')
            self.assertEqual(compile_args[:3], ['input.c', '-c', '-O2'])
            self.assertIn('-fsanitize=memory', compile_args)
            self.assertIn('-fPIE', compile_args)
            self.assertNotIn('-pie', compile_args)
            self.assertNotIn('-fPIE', invoke('module.c', '-fPIC', '-c'))
            self.assertIn('-pie', invoke('input.o', '-o', 'program'))
            shared = invoke('input.o', '-shared', '-o', 'library.so')
            self.assertIn('-fPIC', shared)
            self.assertNotIn('-pie', shared)

    def test_new_logging_options_are_capability_checked(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = self.source(root / 'source')
            (source / 'run-test262.c').write_text('--timing-log --progress-log')
            state = runner.settings(source, root / 'reports', 'ubsan')
            self.assertTrue(state['timing'])
            self.assertTrue(state['progress'])
            self.assertNotIn('CONFIG_ASAN=y', runner.make_args(state, root / 'cc'))
            self.assertIn('CONFIG_UBSAN=y', runner.make_args(state, root / 'cc'))


if __name__ == '__main__':
    unittest.main()
