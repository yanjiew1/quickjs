"""Pure current/historical premise derivation tests; no engine execution."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import premise_source_compatibility as compat

class CompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.here = self.root / 'tools/ci/native-prefix'
        self.here.mkdir(parents=True)
        actual = Path(__file__).resolve().parent
        self.packet = self.here / 'original-assertions'
        shutil.copytree(actual / 'original-assertions', self.packet)
        for name in ('premise-runner-compatibility.json', 'premise_source_compatibility.py',
                     'launch_current_premise_witness.py', 'premise-source-guard-review.json', 'execution_threads.py'):
            shutil.copyfile(actual / name, self.here / name)
        approval = json.loads((self.here / 'premise-runner-compatibility.json').read_bytes())
        approved = dict(approval['subjects'], **approval['additional_dependencies'])
        for name in approved:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(actual.parents[2] / name, target)
        self.patch = mock.patch.object(compat, 'HERE', self.here)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        self.files = json.loads((self.packet / 'PREMISE-SOURCE-PINS.json').read_bytes())
        for name in approved:
            self.files[name] = compat.digest(self.root / name)
        for name in (compat.APPROVAL, compat.ADAPTER, compat.LAUNCHER, compat.THREADS):
            self.files[name] = compat.digest(self.root / name)
        source = compat.source_map_digest(self.files)
        profile = {'defaults': {'DateTimeFormat': {'timeZone': 'UTC', 'locale': 'en-US',
                                                   'numberingSystem': 'latn'}}}
        review = json.loads((self.here / 'premise-source-guard-review.json').read_bytes())
        review.update(engine_sha256='a' * 64, source_tree_sha256=source,
                      profile_sha256='b' * 64, profile_canonical_sha256=compat.canonical(profile),
                      approved_source_scope_sha256=compat.digest(self.here / 'premise-source-guard-review.json'))
        receipt = compat.authenticated_receipt(self.files, self.packet,
                    compat.digest(self.here / 'premise-source-guard-review.json'))
        self.bindings = dict(engine_sha256='a' * 64, source_tree_sha256=source,
            profile_sha256='b' * 64, current_source_files=self.files, profile=profile,
            premise_source_guard_review=review,
            premise_source_guard_review_sha256=compat.canonical(review),
            premise_source_compatibility=receipt)
        self.plan = json.loads((self.packet / 'HOSTED-RENDER-PLAN.json').read_bytes())

    def configuration(self):
        import sys
        previous = sys.modules.pop('render_practical_witness', None)
        self.addCleanup(lambda: sys.modules.__setitem__('render_practical_witness', previous)
                        if previous is not None else sys.modules.pop('render_practical_witness', None))
        sys.path.insert(0, str(self.packet))
        import guard_binding
        original = guard_binding.prepare_premise_binding
        self.addCleanup(setattr, guard_binding, 'prepare_premise_binding', original)
        compat.install(self.packet)
        installed = guard_binding.prepare_premise_binding
        compat.install(self.packet)
        self.assertIs(guard_binding.prepare_premise_binding, installed)
        before = copy.deepcopy(self.bindings)
        binding, data = installed(self.packet, self.bindings, self.plan)
        self.assertEqual(self.bindings, before)
        config = dict(sites={sid: {'premise_guard': guard} for sid, guard in
                           self.plan['premise_guard_sites'].items()},
            premise_bindings=binding, premise_data=data,
            engine_sha256=self.bindings['engine_sha256'],
            source_tree_sha256=self.bindings['source_tree_sha256'],
            profile_sha256=self.bindings['profile_sha256'], profile=self.bindings['profile'])
        guard_binding.validate_premise_config(config)
        return config

    def test_actual_map_effective_data_and_historical_comparison_are_all_preserved(self):
        config = self.configuration()
        historical = json.loads((self.packet / 'PREMISE-DATA.json').read_bytes())
        self.assertEqual(config['premise_bindings']['current_source_files'], self.files)
        changed = copy.deepcopy(historical)
        for name in self.bindings['premise_source_compatibility']['subjects']:
            changed['source_files'][name] = self.files[name]
        self.assertEqual(config['premise_data'], changed)
        original = copy.deepcopy(config)
        stable = compat.stable_effective_config(config, self.bindings)
        self.assertEqual(config, original)
        self.assertEqual(stable['premise_data'], historical)
        self.assertEqual(stable['premise_bindings']['source_pins_canonical_sha256'],
                         compat.canonical(historical['source_files']))

    def test_unapproved_runner_and_other_source_changes_are_rejected(self):
        for path in (compat.SUBJECT, next(name for name in self.files if name != compat.SUBJECT)):
            files = dict(self.files, **{path: 'f' * 64})
            with self.assertRaises(ValueError):
                compat.authenticated_receipt(files, self.packet,
                    compat.digest(self.here / 'premise-source-guard-review.json'))

    def test_effective_data_and_canonical_substitutions_are_rejected(self):
        for field in ('data', 'pins-canonical', 'data-canonical', 'other-source'):
            config = self.configuration()
            if field == 'data':
                config['premise_data']['unknown-injected-value'] = True
            elif field == 'other-source':
                other = next(name for name in config['premise_data']['source_files'] if name != compat.SUBJECT)
                config['premise_data']['source_files'][other] = 'f' * 64
            else:
                name = 'source_pins_canonical_sha256' if field == 'pins-canonical' else 'premise_data_canonical_sha256'
                config['premise_bindings'][name] = 'f' * 64
            with self.assertRaises(ValueError):
                compat.stable_effective_config(config, self.bindings)

    def test_forged_receipt_and_adapter_sources_are_rejected(self):
        bindings = copy.deepcopy(self.bindings)
        bindings['premise_source_compatibility']['current_sha256'] = 'f' * 64
        with self.assertRaises(ValueError):
            compat.validate_bindings(bindings, self.packet)
        for name in (compat.APPROVAL, compat.ADAPTER, compat.LAUNCHER, compat.THREADS):
            files = dict(self.files, **{name: 'f' * 64})
            with self.assertRaises(ValueError):
                compat.authenticated_receipt(files, self.packet,
                    compat.digest(self.here / 'premise-source-guard-review.json'))

    def test_unknown_config_and_binding_fields_remain_significant(self):
        config = self.configuration()
        config['unknown-new-field'] = {'preserved': True}
        config['premise_bindings']['unknown-new-field'] = {'preserved': True}
        stable = compat.stable_effective_config(config, self.bindings)
        self.assertEqual(stable['unknown-new-field'], config['unknown-new-field'])
        self.assertEqual(stable['premise_bindings']['unknown-new-field'],
                         config['premise_bindings']['unknown-new-field'])

    def test_isolated_launcher_imports_before_help_without_engine_execution(self):
        result = subprocess.run([sys.executable, '-I', '-B',
            str(self.here / 'launch_current_premise_witness.py'), '--help'],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('--bindings', result.stdout)

    def test_packet_member_tampering_is_rejected_before_import(self):
        member = self.packet / 'guard_binding.py'
        member.write_text('raise AssertionError("untrusted import executed")\n')
        with self.assertRaisesRegex(ValueError, 'sealed premise source before import'):
            compat.install(self.packet)

    def test_diagnostic_engine_inherits_receipt_and_actual_source_map(self):
        receipt = copy.deepcopy(self.bindings['premise_source_compatibility'])
        self.bindings['engine_sha256'] = 'd' * 64
        review = self.bindings['premise_source_guard_review']
        review['engine_sha256'] = self.bindings['engine_sha256']
        self.bindings['premise_source_guard_review_sha256'] = compat.canonical(review)
        config = self.configuration()
        self.assertEqual(self.bindings['premise_source_compatibility'], receipt)
        self.assertEqual(config['premise_bindings']['current_source_files'], self.files)
        stable = compat.stable_effective_config(config, self.bindings)
        self.assertEqual(stable['engine_sha256'], 'd' * 64)

if __name__ == '__main__':
    unittest.main()
