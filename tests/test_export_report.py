#!/usr/bin/env python3
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
