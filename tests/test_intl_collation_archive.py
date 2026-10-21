#!/usr/bin/env python3
"""Compiler byte determinism, canonical framing and atomic publication failures."""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_intl_collation_ir import encode, program, FINGERPRINT
from test_intl_collation_root_input import root, GROUPS, wire
import collation_archive as archive
from cldr_xml import DataError

compiler = None


class ArchiveTests(unittest.TestCase):
    def inputs(self, directory):
        root_path, ir_path = directory / 'root.qjcr', directory / 'rules.qjci'
        root_path.write_bytes(wire.encode(root(), GROUPS, FINGERPRINT))
        ir_path.write_bytes(encode([program('&a<b'), program('[numericOrdering on]', 'fr')]))
        return root_path, ir_path

    def compile(self, directory):
        if compiler is None:
            self.skipTest('pass --compiler to exercise runtime archive output')
        root_path, ir_path = self.inputs(directory)
        result = subprocess.run([compiler, '--compile-pair', str(root_path), str(ir_path)],
                                timeout=10, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        return result.stdout

    def test_determinism_and_canonical_layout(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            first, second = self.compile(directory), self.compile(directory)
        self.assertEqual(first, second)
        view = archive.unpack(first, FINGERPRINT)
        self.assertEqual(view['program_count'], 2)
        self.assertEqual(view['counts'][8], 2)
        self.assertEqual(view['counts'][11], 0)
        self.assertEqual(len(view['sections'][0]), 80)
        self.assertEqual(view['fingerprint'], FINGERPRINT)

    def test_reject_frames_without_reinterpreting_semantic_pools(self):
        with tempfile.TemporaryDirectory() as temporary:
            data = self.compile(Path(temporary))
        def changed(at, word):
            return data[:at] + struct.pack('<I', word) + data[at + 4:]
        cases = [b'bad!' + data[4:], changed(4, 2), changed(8, 17 << 16), changed(12, 17 << 16),
                 changed(16, len(data) + 1), changed(20, 11), changed(24, 3), changed(28, 1),
                 changed(64, 2), changed(68, 255), changed(72, 79), changed(76, 2),
                 data + b'\0\0\0\0', data[:-1]]
        # Block cardinalities and label spans have their own checked domains.
        _, start, _, _ = archive.DIRECTORY.unpack_from(data, 64 + 8 * 16)
        _, blocks, _, _ = archive.DIRECTORY.unpack_from(data, 64 + 10 * 16)
        cases += [changed(start, 1), changed(start + 4, 0), changed(start + 24, 1),
                  changed(start + 28, 63), changed(blocks + 16, 0xffffffff)]
        for case in cases:
            with self.subTest(prefix=case[:80]), self.assertRaises(DataError):
                archive.unpack(case, FINGERPRINT)
        with self.assertRaises(DataError):
            archive.unpack(data, b'x' * 32)

    def test_atomic_publish_and_rejected_partial_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            accepted = self.compile(directory)
            root_path, ir_path = self.inputs(directory)
            output = directory / 'accepted.qjcl'
            archive.compile_archive(Path(compiler), root_path, ir_path, output)
            self.assertEqual(output.read_bytes(), accepted)
            failures = [subprocess.CompletedProcess([], 1, b'partial', b'error'),
                        subprocess.CompletedProcess([], 0, accepted[:-1], b''),
                        subprocess.CompletedProcess([], 0, accepted[:32] + b'x' * 32 + accepted[64:], b'')]
            for result in failures:
                with patch.object(archive.subprocess, 'run', return_value=result), self.assertRaises(DataError):
                    archive.compile_archive(Path(compiler), root_path, ir_path, output)
                self.assertEqual(output.read_bytes(), accepted)
            for error in (OSError('cannot start compiler'), subprocess.TimeoutExpired(str(compiler), 180)):
                with patch.object(archive.subprocess, 'run', side_effect=error), self.assertRaises(DataError):
                    archive.compile_archive(Path(compiler), root_path, ir_path, output)
                self.assertEqual(output.read_bytes(), accepted)
            # The real paired C path must emit no binary when identity checks fail.
            raw = ir_path.read_bytes()
            ir_path.write_bytes(raw[:20] + b'x' * 32 + raw[52:])
            result = subprocess.run([compiler, '--compile-pair', str(root_path), str(ir_path)],
                                    timeout=10, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, b'')
            with self.assertRaises(DataError):
                archive.compile_archive(Path(compiler), root_path, ir_path, output)
            self.assertEqual(output.read_bytes(), accepted)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--compiler')
    args, remaining = parser.parse_known_args()
    compiler = args.compiler
    unittest.main(argv=[sys.argv[0]] + remaining)
