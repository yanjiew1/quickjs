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

"""Authored wire envelope checks; semantic section checks belong to collectors."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle


class BundleTests(unittest.TestCase):
    def example(self):
        return bundle.pack({2: (1, b'abc'), 10: (3, b'xyz12')}, bytes(range(32)), 'native')

    def test_canonical_order_padding_and_identity(self):
        binary = self.example()
        self.assertEqual(binary, bundle.pack({10: (3, b'xyz12'), 2: (1, b'abc')},
                                            bytes(range(32)), 'native'))
        result = bundle.unpack(binary)
        self.assertEqual(result['generation'], bytes(range(32)))
        self.assertEqual(result['profile'], 1)
        self.assertEqual(result['sections'], {2: (1, b'abc'), 10: (3, b'xyz12')})
        self.assertEqual(len(binary), 105)
        self.assertEqual(binary[99], 0)
        self.assertEqual(bundle.HEADER.size, 64)

    def test_empty_and_unknown_optional_sections(self):
        binary = bundle.pack({123: (0, b'')}, bytes(32), 'minimal')
        self.assertEqual(bundle.unpack(binary)['sections'], {123: (0, b'')})

    def test_bad_headers_directories_and_truncation(self):
        original = self.example()
        for end in range(len(original)):
            with self.assertRaises(ValueError):
                bundle.unpack(original[:end])
        # Magic, version, size, count, profile, flags, reserved words; section
        # order, overflow, gaps, overlap, bad alignment, nonzero padding.
        changes = ((0, b'BAD!'), (4, b'\x02\0'), (6, b'\x01\0'),
                   (8, struct.pack('<I', 106)), (12, bytes(4)),
                   (12, struct.pack('<I', 129)), (48, struct.pack('<I', 2)),
                   (52, b'\x01'), (56, b'\x01'), (60, b'\x01'),
                   (64, bytes(4)), (80, struct.pack('<I', 2)),
                   (68, struct.pack('<I', 0xfffffffc)),
                   (72, struct.pack('<I', 0xffffffff)),
                   (84, struct.pack('<I', 104)),
                   (84, struct.pack('<I', 96)),
                   (84, struct.pack('<I', 99)), (99, b'\x01'))
        for offset, data in changes:
            binary = original[:offset] + data + original[offset + len(data):]
            with self.assertRaises(ValueError, msg=repr((offset, data))):
                bundle.unpack(binary)
        with self.assertRaises(ValueError):
            bundle.unpack(original + b'\0')

    def test_encoder_rejects_invalid_fields(self):
        cases = (({}, bytes(32), 'native'), ({0: (1, b'a')}, bytes(32), 'native'),
                 ({1: (-1, b'a')}, bytes(32), 'native'),
                 ({1: (2**32, b'a')}, bytes(32), 'native'),
                 ({1: (0, bytearray())}, bytes(32), 'native'),
                 ({1: (0, b'')}, bytes(31), 'native'),
                 ({1: (0, b'')}, bytes(32), 'icu4c'))
        for sections, generation, profile in cases:
            with self.assertRaises(ValueError):
                bundle.pack(sections, generation, profile)


if __name__ == '__main__':
    unittest.main()
