#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Compiler byte determinism, canonical framing and atomic publication failures."""
import argparse
import json
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
from collation_bindings import Binding

compiler = None


class ArchiveTests(unittest.TestCase):
    def bindings(self):
        en, fr = ('en', 'standard', ''), ('fr', 'standard', '')
        return (Binding('en', '', en, fr), Binding('en', 'emoji', en, None),
                Binding('fr', '', fr, en))

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
        cases = [b'bad!' + data[4:], changed(4, archive.FORMAT_VERSION + 1), changed(8, 17 << 16), changed(12, 17 << 16),
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
            audit = subprocess.run([compiler, '--audit-compiled', str(output)], timeout=10,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(audit.returncode, 0, audit.stderr.decode())

            # Framing alone is not a semantic acceptance check. The native
            # reader rejects out-of-domain policy flags in an otherwise framed image.
            invalid = bytearray(accepted)
            _, blocks_at, _, _ = archive.DIRECTORY.unpack_from(invalid, 64 + 10 * 16)
            struct.pack_into('<I', invalid, blocks_at + 12, 32)
            output.write_bytes(invalid)
            audit = subprocess.run([compiler, '--audit-compiled', str(output)], timeout=10,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(audit.returncode, 1)
            self.assertNotIn(b'Sanitizer', audit.stderr)
            output.write_bytes(accepted)
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

    def test_bound_identity_targets_and_readback(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            data = self.compile(directory)
            root_path, ir_path = self.inputs(directory)
            output = directory / 'bound.qjcl'
            rows = self.bindings()
            bound = archive.bind(data, rows, FINGERPRINT)
            original, view = archive.unpack(data), archive.unpack(bound)

            # Program/root payloads do not change when service identity labels
            # and target IDs are attached. The labels use their own exact suffix.
            self.assertEqual(view['counts'][11], 3)
            for i in tuple(range(9)) + (10,):
                self.assertEqual(view['sections'][i], original['sections'][i])
            words = list(archive.BINDING.iter_unpack(view['sections'][11]))
            self.assertEqual([row[4:] for row in words], [(0, 1), (0, 0xffffffff), (1, 0)])

            archive.compile_archive(Path(compiler), root_path, ir_path, output, (FINGERPRINT, rows))
            self.assertEqual(output.read_bytes(), bound)
            audit = subprocess.run([compiler, '--audit-compiled', str(output)], timeout=10,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(audit.returncode, 0, audit.stderr.decode())
            self.assertEqual(json.loads(audit.stdout)['bindings'], 3)

            # An audit failure or a mismatched source generation cannot replace
            # the previously accepted destination.
            accepted = output.read_bytes()
            compiled = subprocess.CompletedProcess([], 0, data, b'')
            failed = subprocess.CompletedProcess([], 1, b'', b'invalid metadata')
            with patch.object(archive.subprocess, 'run', side_effect=[compiled, failed]), self.assertRaises(DataError):
                archive.compile_archive(Path(compiler), root_path, ir_path, output, (FINGERPRINT, rows))
            self.assertEqual(output.read_bytes(), accepted)
            for error in (OSError('cannot start audit'), subprocess.TimeoutExpired(str(compiler), 60)):
                with patch.object(archive.subprocess, 'run', side_effect=[compiled, error]), self.assertRaises(DataError):
                    archive.compile_archive(Path(compiler), root_path, ir_path, output, (FINGERPRINT, rows))
                self.assertEqual(output.read_bytes(), accepted)
            with self.assertRaises(DataError):
                archive.bind(data, rows, b'x' * 32)
            with self.assertRaises(DataError):
                archive.bind(bound, rows, FINGERPRINT)

    def test_invalid_bindings_reject_before_publication(self):
        with tempfile.TemporaryDirectory() as temporary:
            data = self.compile(Path(temporary))
        default, named, last = self.bindings()
        cases = [(named,), (default, default), (last, default),
                 (default._replace(locale='root'),),
                 (default._replace(sort=None),),
                 (default._replace(search=('en', 'missing', '')),),
                 (default, named._replace(sort=None)),
                 (default, named._replace(collation='standard')),
                 (default, named._replace(collation='private-helper')),
                 (default, named._replace(collation='Emoji'))]
        for rows in cases:
            with self.subTest(rows=rows), self.assertRaises(DataError):
                archive.bind(data, rows, FINGERPRINT)

    def test_runtime_binding_mutations_reject(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            data = archive.bind(self.compile(directory), self.bindings(), FINGERPRINT)
            _, at, _, _ = archive.DIRECTORY.unpack_from(data, 64 + 11 * 16)
            cases = [(at, 0), (at + 4, 0), (at + 16, 2), (at + 20, 0xffffffff),
                     (at + 24 + 12, 0), (at + 24 + 16, 0xffffffff),
                     (at + 48, 0), (at + 48 + 20, 2)]
            for position, value in cases:
                candidate = bytearray(data)
                struct.pack_into('<I', candidate, position, value)
                path = directory / 'invalid.qjcl'
                path.write_bytes(candidate)
                result = subprocess.run([compiler, '--audit-compiled', str(path)], timeout=10,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                self.assertEqual(result.returncode, 1, (position, value, result.stderr))
                self.assertNotIn(b'Sanitizer', result.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--compiler')
    args, remaining = parser.parse_known_args()
    compiler = args.compiler
    unittest.main(argv=[sys.argv[0]] + remaining)
