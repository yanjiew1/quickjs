#!/usr/bin/env python3
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
