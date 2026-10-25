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

"""Exercise the real C query tool instead of the host Unicode database."""
import argparse
import json
import subprocess
import unittest

tool = None


class PropertyToolTests(unittest.TestCase):
    def query(self, argument):
        result = subprocess.run([tool, argument], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout.decode())

    def test_counted_ranges_and_version(self):
        data = self.query('General_Category=Cs')
        self.assertEqual(data['unicode'], '18.0.0')
        self.assertEqual(data['ranges'], [[0xd800, 0xe000]])
        self.assertEqual(self.query('ASCII=False')['ranges'], [[0x80, 0x110000]])
        self.assertEqual(self.query('Any')['ranges'], [[0, 0x110000]])

    def test_unsupported_property_has_no_partial_stdout(self):
        result = subprocess.run([tool, 'Word_Break=not-a-value'], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'')
        self.assertTrue(result.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', required=True)
    args, remaining = parser.parse_known_args()
    tool = args.tool
    unittest.main(argv=[__file__] + remaining)
