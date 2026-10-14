import hashlib
import json
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest
from project_original_assertions import (validated_semantic_observation,
                                        EVENT_ENVELOPE_FIELDS,
                                        CONFIG_ENVELOPE_FIELDS)


class ProjectorStreamingTests(unittest.TestCase):
    def observation(self, source, config=None):
        return validated_semantic_observation(source, config or {'sites': {}}, {})

    def test_canonical_digest_matches_finite_array(self):
        events = [{'schema': 'x', 'nested': {'unknown': '\u00e9'},
                   'engine_sha256': 'x'}, {'schema': 'y', 'value': [1, 2],
                   'unknown': True, 'source_tree_sha256': 'z'}]
        text = ''.join(json.dumps(x) + '\n' for x in events)
        result = self.observation(text)
        stable = [{k: v for k, v in x.items() if k not in EVENT_ENVELOPE_FIELDS}
                  for x in events]
        want = hashlib.sha256(json.dumps(stable, ensure_ascii=True,
            separators=(',', ':'), allow_nan=False).encode()).hexdigest()
        self.assertEqual(result['events_sha256'], want)
        self.assertEqual(result['event_count'], 2)

    def test_path_never_read_text(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'trace'
            path.write_bytes(b'{"kind":"start"}\n{"kind":"complete"}\n')
            original = Path.read_text
            def forbidden(*args, **kwargs):
                raise AssertionError('Whole trace text must not be loaded')
            Path.read_text = forbidden
            try:
                self.assertEqual(self.observation(path)['event_count'], 2)
            finally:
                Path.read_text = original

    def test_lazy_iterable_preserves_order_and_unknown_fields(self):
        events = [{'first': 1}, {'second': 2, 'extra': [3]}]
        source = (json.dumps(event) for event in events)
        self.assertEqual(self.observation(source),
                         self.observation('\n'.join(map(json.dumps, events))))

    def test_empty_trace_rejected(self):
        with self.assertRaises(ValueError):
            self.observation('')

    def test_nonobject_rejected(self):
        with self.assertRaises(ValueError):
            self.observation('[]\n')

    def test_duplicate_key_rejected(self):
        with self.assertRaises(ValueError):
            self.observation('{"a":1,"a":2}\n')

    def test_event_budget_rejected(self):
        with self.assertRaises(ValueError):
            self.observation('{"a":1}\n{"b":2}\n',
                             {'sites': {}, 'max_events': 1})

    def test_unknown_nested_difference_retained(self):
        self.assertNotEqual(self.observation('{"a":{"x":1}}\n')['events_sha256'],
                            self.observation('{"a":{"x":2}}\n')['events_sha256'])

    def test_second_pass_original_bytes_authenticate(self):
        text = '{"kind":"complete"}\n'
        want = hashlib.sha256(text.encode()).hexdigest()
        result = validated_semantic_observation(text, {'sites': {}}, {}, want)
        self.assertEqual(result['event_count'], 1)

    def test_changed_path_after_validation_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'trace'
            original = b'{"kind":"complete","value":1}\n'
            path.write_bytes(original)
            validated_digest = hashlib.sha256(original).hexdigest()
            path.write_bytes(b'{"kind":"complete","value":2}\n')
            with self.assertRaises(ValueError):
                validated_semantic_observation(path, {'sites': {}}, {},
                                               validated_digest)

    def test_isolated_launcher_reaches_argument_parser(self):
        launcher = Path(__file__).resolve().parent / 'original-assertions' / 'launch_practical_witness.py'
        result = subprocess.run([sys.executable, '-I', '-B', str(launcher), '--help'],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('--packet', result.stdout)
        self.assertNotIn('ModuleNotFoundError', result.stderr)

    def test_isolated_launcher_authenticates_before_helper_import(self):
        launcher = Path(__file__).resolve().parent / 'original-assertions' / 'launch_practical_witness.py'
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            packet = root / 'packet'
            packet.mkdir()
            (packet / 'SEAL.json').write_text('{"files":{}}')
            bindings = root / 'bindings.json'
            bindings.write_text('{"packet_seal_sha256":"wrong"}')
            raw = root / 'raw.json'
            raw.write_text('{}')
            result = subprocess.run([sys.executable, '-I', '-B', str(launcher),
                '--packet', str(packet), '--bindings', str(bindings), '--qjs',
                str(root / 'absent'), '--raw-full-run', str(raw),
                '--output-root', str(root / 'out')], capture_output=True,
                text=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Packet seal differs', result.stderr)
            self.assertNotIn('ModuleNotFoundError', result.stderr)


if __name__ == '__main__':
    unittest.main()
