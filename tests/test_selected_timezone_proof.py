"""Finite provider revision and forged-artifact regression tests.

Root executes these tests; source preparation does not import the helper.
"""
import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path

path = Path(__file__).resolve().parents[1] / 'tools/ci/native-prefix/original-assertions/selected_timezone_proof.py'
spec = importlib.util.spec_from_file_location('selected_timezone_proof', path)
proof = importlib.util.module_from_spec(spec)
spec.loader.exec_module(proof)


def tzif(offset=-21600, footer='CST6CDT,M3.2.0,M11.1.0', transitions=()):
    def block(width):
        h = b'TZif3' + b'\0' * 15 + struct.pack('>6I', 0, 0, 0, len(transitions), 1, 4)
        times = b''.join(struct.pack('>q' if width == 8 else '>i', n) for n in transitions)
        return h + times + b'\0' * len(transitions) + struct.pack('>iBB', offset, 0, 0) + b'CST\0'
    return block(4) + block(8) + b'\n' + footer.encode() + b'\n'


class FiniteTimezoneProof(unittest.TestCase):
    def test_revision_does_not_become_engine_offset_bug(self):
        # Host CST footer and the pinned 2026e EST footer are distinct data.
        host = proof.parse_tzif(tzif())
        embedded = proof.parse_tzif(tzif(-18000, 'EST5'))
        at = '1893456000000000000'  # 2030-01-01 UTC, original fixture sample.
        self.assertEqual(proof.offset_ns(host, at), -21600000000000)
        self.assertEqual(proof.offset_ns(embedded, at), -18000000000000)
        self.assertNotEqual(proof.sha(tzif()), proof.sha(tzif(-18000, 'EST5')))

    def test_explicit_transition_boundary_and_type_zero(self):
        z = proof.parse_tzif(tzif(-18000, '', (0,)))
        self.assertEqual(proof.offset_ns(z, '-315619200000000000'), -18000000000000)
        self.assertEqual(proof.offset_ns(z, '0'), -18000000000000)

    def test_julian_and_day_rules(self):
        for footer in ('CST6CDT,J70,J310', 'CST6CDT,69,309',
                       'CST6CDT,M3.2.0/-2,M11.1.0/26'):
            z = proof.parse_tzif(tzif(footer=footer))
            self.assertEqual(proof.offset_ns(z, '1893456000000000000'), -21600000000000)

    def test_southern_season_and_quoted_name(self):
        z = proof.parse_tzif(tzif(36000, '<+10>-10<+11>-11,M10.1.0,M4.1.0'))
        self.assertEqual(proof.offset_ns(z, '1893456000000000000'), 39600000000000)

    def test_reject_timestamp_outside_fixed_fixture(self):
        with self.assertRaises(ValueError):
            proof.offset_ns(proof.parse_tzif(tzif()), '1')

    def test_reject_corrupt_and_oversized_blob(self):
        for raw in (b'', tzif()[:-1], b'\0' * 65537,
                    tzif().replace(b'TZif3', b'TZif1', 1),
                    tzif(footer='CST6CDT'), tzif(footer='CST6\nEST5')):
            with self.assertRaises(ValueError):
                proof.parse_tzif(raw)

    def test_reject_leap_aware_header(self):
        raw = bytearray(tzif())
        raw[28:32] = struct.pack('>I', 1)
        with self.assertRaises(ValueError):
            proof.parse_tzif(bytes(raw))

    def capture(self, directory, **changes):
        raw = tzif()
        row = dict(schema='qjs-selected-tzif-record-v1', pid=123,
                   sequence=1, identifier='America/Winnipeg', primary='America/Winnipeg',
                   origin='system', source_status=0, system_root='/usr/share/zoneinfo',
                   blob='snapshot-0001.tzif', size=len(raw))
        row.update(changes)
        (directory / 'snapshot-0001.tzif').write_bytes(raw)
        (directory / 'selected-timezones.ndjson').write_text(json.dumps(row) + '\n')
        command = dict(pid=123, lifecycle_closed=True, returncode=0,
                       timed_out=False, interrupted=False, output_limited=False)
        return command

    def read(self, directory, command):
        return proof.read_capture(directory, command,
            {'America/Winnipeg': 'America/Winnipeg'}, '/usr/share/zoneinfo')

    def test_authenticated_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            p = self.read(directory, self.capture(directory))
            self.assertEqual(p['selected_primaries']['America/Winnipeg']['offsets']
                             ['1893456000000000000'], -21600000000000)
            self.assertFalse(p['conformance_accepted'])

    def test_reject_forged_pid_origin_path_and_extent(self):
        for changes in (dict(pid=124), dict(origin='guessed-system'),
                        dict(blob='../host.tzif'), dict(size=44),
                        dict(source_status=2), dict(origin='embedded'),
                        dict(system_root='/tmp/elsewhere')):
            with tempfile.TemporaryDirectory() as temp:
                directory = Path(temp)
                command = self.capture(directory, **changes)
                with self.assertRaises(ValueError):
                    self.read(directory, command)

    def test_reject_incomplete_process_and_extra_member(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            command = self.capture(directory)
            command['lifecycle_closed'] = False
            with self.assertRaises(ValueError):
                self.read(directory, command)
            command['lifecycle_closed'] = True
            (directory / 'unexpected').write_text('unowned')
            with self.assertRaises(ValueError):
                self.read(directory, command)

    def test_reject_symlink_blob(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            command = self.capture(directory)
            blob = directory / 'snapshot-0001.tzif'
            data = blob.read_bytes()
            blob.unlink()
            target = directory / 'other.tzif'
            target.write_bytes(data)
            blob.symlink_to(target)
            with self.assertRaises(ValueError):
                self.read(directory, command)

    def test_reject_duplicate_json_key(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            command = self.capture(directory)
            manifest = directory / 'selected-timezones.ndjson'
            manifest.write_text(manifest.read_text().replace('"pid": 123', '"pid": 123, "pid": 123'))
            with self.assertRaises(ValueError):
                self.read(directory, command)

    def test_reject_changed_snapshot_inside_one_process(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            command = self.capture(directory)
            manifest = directory / 'selected-timezones.ndjson'
            row = json.loads(manifest.read_text())
            row.update(sequence=2, blob='snapshot-0002.tzif')
            raw = tzif(-18000, 'EST5')
            row['size'] = len(raw)
            (directory / row['blob']).write_bytes(raw)
            manifest.write_text(manifest.read_text() + json.dumps(row) + '\n')
            with self.assertRaises(ValueError):
                self.read(directory, command)

    def test_reject_provider_revision_between_probe_and_original(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            p = self.read(directory, self.capture(directory))
            a = json.loads(json.dumps(p))
            a['selected_primaries']['America/Winnipeg']['tzif_sha256'] = '0' * 64
            with self.assertRaises(ValueError):
                proof.compare_actual(p, a, {'America/Winnipeg'}, {})

    def test_reject_missing_snapshot_and_false_embedded_claim(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            p = self.read(directory, self.capture(directory, origin='embedded', source_status=1))
            with self.assertRaises(ValueError):
                proof.compare_actual(p, p, {'America/Rainy_River'}, {})
            with self.assertRaises(ValueError):
                proof.compare_actual(p, p, {'America/Winnipeg'},
                    {'America/Winnipeg': {'tzif_sha256': '0' * 64, 'offsets': {}}})


if __name__ == '__main__':
    unittest.main()
