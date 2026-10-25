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

"""Shared byte I/O bounds, deadlines and unchanged-output behavior."""
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/data'))
from _support import io as data_io


class Response(io.BytesIO):
    def __init__(self, data, url='https://invalid.test/data'):
        super().__init__(data)
        self.url = url

    def geturl(self):
        return self.url


class DataIOTests(unittest.TestCase):
    def test_download_bounds_and_https_redirect(self):
        for response, limit, expected in [(Response(b'data'), 4, b'data'),
                                           (Response(b'data'), 3, None),
                                           (Response(b'data', 'http://invalid.test'), 4, None)]:
            with mock.patch.object(data_io.urllib.request, 'urlopen', return_value=response):
                if expected is None:
                    with self.assertRaises(ValueError):
                        data_io.download_bytes('https://invalid.test', limit)
                else:
                    self.assertEqual(data_io.download_bytes('https://invalid.test', limit), expected)
        with mock.patch.object(data_io.urllib.request, 'urlopen') as network:
            with self.assertRaises(ValueError):
                data_io.download_bytes('http://invalid.test', 4)
            network.assert_not_called()

    def test_deadline_bounds_progress_and_retries(self):
        # A peer can deliver bytes continually without hitting a socket
        # timeout. That progress must not reset the overall deadline.
        clock = [0]

        class SlowResponse(Response):
            calls = 0

            def read1(self, size=-1):
                self.calls += 1
                clock[0] += 61
                return b'x'

        response = SlowResponse(b'')
        with mock.patch.object(data_io.urllib.request, 'urlopen', return_value=response), \
                mock.patch.object(data_io.time, 'monotonic', side_effect=lambda: clock[0]):
            with self.assertRaises(TimeoutError):
                data_io.download_bytes('https://invalid.test', 4)
            self.assertEqual(response.calls, 2)
        with mock.patch.object(data_io.urllib.request, 'urlopen', side_effect=OSError) as network, \
                mock.patch.object(data_io.time, 'monotonic', side_effect=[0, 0, 121]):
            with self.assertRaises(OSError):
                data_io.download_bytes('https://invalid.test', 4)
            self.assertEqual(network.call_count, 1)

    def test_retry_keeps_the_request_bound(self):
        with mock.patch.object(data_io.urllib.request, 'urlopen',
                               side_effect=[OSError(), Response(b'ok')]) as network:
            self.assertEqual(data_io.download_bytes('https://invalid.test', 2), b'ok')
            self.assertEqual(network.call_count, 2)
            self.assertTrue(all(0 < item[1]['timeout'] <= 30 for item in network.call_args_list))

    def test_final_read_must_complete_before_deadline(self):
        clock = [0]

        class LateResponse(Response):
            def read1(self, size=-1):
                clock[0] = 121
                return b''

        with mock.patch.object(data_io.urllib.request, 'urlopen', return_value=LateResponse(b'')), \
                mock.patch.object(data_io.time, 'monotonic', side_effect=lambda: clock[0]):
            with self.assertRaises(TimeoutError):
                data_io.download_bytes('https://invalid.test', 4)

    def test_local_read_is_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input'
            path.write_bytes(b'four')
            self.assertEqual(data_io.read_bytes(path, 4), b'four')
            with self.assertRaises(ValueError):
                data_io.read_bytes(path, 3)

    def test_atomic_output_preserves_unchanged_mtime_and_replaces_changed_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'output'
            data_io.write_atomic(path, b'generated' * 20000)
            os.utime(str(path), ns=(1000000000000000000, 1000000000000000000))
            before = path.stat().st_mtime_ns
            data_io.write_atomic(path, path.read_bytes())
            self.assertEqual(path.stat().st_mtime_ns, before)
            for data in [b'prefix-extra', b'prefix', b'', b'value', b'x' * 65536 + b'extra', b'x' * 65536]:
                data_io.write_atomic(path, data)
                self.assertEqual(path.read_bytes(), data)
                self.assertEqual([item.name for item in path.parent.iterdir()], ['output'])


if __name__ == '__main__':
    unittest.main()
