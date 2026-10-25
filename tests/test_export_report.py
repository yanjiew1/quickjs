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

"""Keep public/private symbol checks working across Binutils PE report formats."""
import unittest
from test_shared_exports import pe_exports


class PEExportReportTest(unittest.TestCase):
    def test_old_and_new_name_tables(self):
        reports = [
            '[Ordinal/Name Pointer] Table\n'
            '\t[   0] JS_NewRuntime\n\t[ 1] private_helper\n\n',
            '[Ordinal/Name Pointer] Table -- Ordinal Base 1\n'
            '\t          Ordinal   Hint Name\n'
            '\t[   0] +base[   1]  0000 JS_NewRuntime\n'
            '\t[   1] +base[   2]  0001 private_helper\n\n',
        ]
        for report in reports:
            self.assertEqual(pe_exports(report), {'JS_NewRuntime', 'private_helper'})

    def test_unknown_row_is_not_silently_dropped(self):
        with self.assertRaises(ValueError):
            pe_exports('[Ordinal/Name Pointer] Table\n[ 0] unexpected format private_helper\n')


if __name__ == '__main__':
    unittest.main()
