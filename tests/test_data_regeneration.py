#!/usr/bin/env python3
"""Offline regeneration planning, input preservation and process cleanup."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from types import SimpleNamespace
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/data'))
from _support import intl_pipeline as regenerate_intl


class RegenerationTests(unittest.TestCase):
    def test_regeneration_plan_keeps_source_selection_and_profile_dependencies(self):
        args = SimpleNamespace(sources=Path('/source'), source_pin=Path('/candidate.json'), dictionary_pin=None,
                               output_dir=Path('/output'), profiles=['minimal'],
                               collation_compiler=Path('/host/compiler'),
                               dictionaries=None, property_tool=None)
        steps = regenerate_intl.commands(args)
        self.assertEqual([label for label, _ in steps],
                         ['collation-minimal', 'generate-minimal', 'verify-minimal', 'collation-oracle'])
        for _, command in steps:
            self.assertEqual(command[command.index('--source-pin') + 1], str(args.source_pin))
        self.assertNotIn('--compiled-segmentation', steps[1][1])
        args.profiles = ['native']
        args.dictionaries, args.property_tool = Path('/dictionary'), Path('/host/properties')
        steps = regenerate_intl.commands(args)
        self.assertEqual(steps[0][0], 'segmentation')
        self.assertIn('--compiled-segmentation', steps[2][1])
        self.assertIn('--segmentation-manifest', steps[2][1])
        args.dictionary_pin = Path('/dictionary-candidate.json')
        command = regenerate_intl.commands(args)[0][1]
        self.assertEqual(command[command.index('--dictionary-pin') + 1], str(args.dictionary_pin))

    def test_stage_limits_keep_the_established_ci_bounds(self):
        for label, expected in [('segmentation', 180), ('collation-minimal', 240),
                                 ('collation-native', 240), ('generate-native', 600),
                                 ('verify-minimal', 600), ('collation-oracle', 600)]:
            self.assertEqual(regenerate_intl.stage_timeout(label), expected)
            self.assertEqual(regenerate_intl.stage_timeout(label, 12), 12)

    def test_public_commands_preserve_spaced_paths_and_working_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / 'cache with spaces'
            cache.mkdir()
            result = subprocess.run(
                [sys.executable, str(ROOT / 'tools/data/acquire.py'), 'cldr',
                 '--cache', str(cache), '--offline'],
                cwd=str(root), stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('cache with spaces', result.stderr.decode())
            # This family runs outside the dispatcher process. Relative output
            # names must stay relative to the caller, including their spaces.
            result = subprocess.run(
                [sys.executable, str(ROOT / 'tools/data/generate.py'), 'calendar',
                 '--output', 'output data.bin', '--manifest', 'manifest data.json'],
                cwd=str(root), stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual((root / 'output data.bin').read_bytes(),
                             (ROOT / 'data/calendar/calendar-data.bin').read_bytes())
            self.assertTrue((root / 'manifest data.json').is_file())

    def test_generator_timeout_is_bounded(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / 'stage.log'
            result = regenerate_intl.run([sys.executable, '-c', 'import time; time.sleep(60)'], log, 1)
            self.assertEqual(result, 124)

    @unittest.skipUnless(os.name == 'posix', 'requires POSIX process groups')
    def test_timeout_kills_term_resistant_descendant(self):
        # The parent exits on TERM; waiting for it alone used to leave this
        # child alive. A zombie is already stopped and may await init's reap.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            pid_file = root / 'child.pid'
            child = ('import os, signal, time; from pathlib import Path; '
                     'signal.signal(signal.SIGTERM, signal.SIG_IGN); '
                     'Path(' + repr(str(pid_file)) + ').write_text(str(os.getpid())); '
                     'time.sleep(30)')
            parent = ('import subprocess, sys; '
                      'subprocess.Popen([sys.executable, "-c", ' + repr(child) + ']).wait()')
            pid = None
            try:
                result = regenerate_intl.run([sys.executable, '-c', parent], root / 'log', 2)
                self.assertEqual(result, 124)
                pid = int(pid_file.read_text())
                end = time.monotonic() + 2
                while True:
                    state = subprocess.run(['ps', '-o', 'stat=', '-p', str(pid)],
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                           universal_newlines=True, timeout=2).stdout.strip()
                    if not state or state.startswith('Z'):
                        break
                    self.assertLess(time.monotonic(), end, 'generator child survived timeout')
                    time.sleep(0.02)
            finally:
                if pid is not None:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    @unittest.skipUnless(os.name == 'posix', 'requires POSIX process groups')
    def test_group_permission_failure_still_reaps_owned_child(self):
        with tempfile.TemporaryDirectory() as temp:
            with mock.patch.object(regenerate_intl.os, 'killpg', side_effect=PermissionError):
                self.assertEqual(regenerate_intl.run(
                    [sys.executable, '-c', 'import time; time.sleep(30)'],
                    Path(temp) / 'log', 1), 124)

    def test_regeneration_preflight_preserves_input_aliases(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sources, output = root / 'sources', root / 'out'
            sources.mkdir()
            output.mkdir()
            pin = output / 'collation-minimal.log'
            pin.write_bytes(b'input must survive')
            command = [sys.executable, str(ROOT / 'tools/data/generate.py'), 'intl',
                       '--sources', str(sources), '--source-pin', str(pin),
                       '--collation-compiler', sys.executable, '--profiles', 'minimal',
                       '--output-dir', str(output)]
            result = subprocess.run(command, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'output would replace an input', result.stderr)
            self.assertEqual(pin.read_bytes(), b'input must survive')
            command[command.index('--output-dir') + 1] = str(sources / 'generated')
            result = subprocess.run(command, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'outside source directories', result.stderr)
            self.assertFalse((sources / 'generated').exists())

    def test_regeneration_preflight_preserves_symlinked_source_members(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sources, output = root / 'sources', root / 'out'
            sources.mkdir()
            output.mkdir()
            member = sources / 'root.xml'
            member.write_bytes(b'source member must survive')
            alias = output / 'collation-minimal.log'
            try:
                alias.symlink_to(member)
            except (OSError, NotImplementedError):
                self.skipTest('symlink creation is unavailable')
            command = [sys.executable, str(ROOT / 'tools/data/generate.py'), 'intl',
                       '--sources', str(sources), '--collation-compiler', sys.executable,
                       '--profiles', 'minimal', '--output-dir', str(output)]
            result = subprocess.run(command, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'outside source directories', result.stderr)
            self.assertEqual(member.read_bytes(), b'source member must survive')


    def test_regeneration_preflight_preserves_hardlinked_source_members(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sources, output = root / 'sources', root / 'out'
            sources.mkdir()
            output.mkdir()
            member = sources / 'root.xml'
            member.write_bytes(b'source member must survive')
            alias = output / 'collation-minimal.log'
            try:
                os.link(str(member), str(alias))
            except (OSError, NotImplementedError):
                self.skipTest('hard-link creation is unavailable')
            args = SimpleNamespace(sources=sources, dictionaries=None,
                                   source_pin=None, dictionary_pin=None, collation_compiler=None,
                                   property_tool=None, output_dir=output)
            with self.assertRaisesRegex(ValueError, 'source member'):
                regenerate_intl.preflight(args, [('collation-minimal', [])])
            self.assertEqual(member.read_bytes(), b'source member must survive')

    @unittest.skipUnless(hasattr(os, 'mkfifo'), 'requires named pipes')
    def test_regeneration_preflight_rejects_special_output_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sources, output = root / 'sources', root / 'out'
            sources.mkdir()
            output.mkdir()
            os.mkfifo(str(output / 'collation-minimal.log'))
            args = SimpleNamespace(sources=sources, dictionaries=None,
                                   source_pin=None, dictionary_pin=None, collation_compiler=None,
                                   property_tool=None, output_dir=output)
            with self.assertRaisesRegex(ValueError, 'regular file'):
                regenerate_intl.preflight(args, [('collation-minimal', [])])


if __name__ == '__main__':
    unittest.main()
