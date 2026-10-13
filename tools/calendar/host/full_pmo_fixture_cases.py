"""Root-owned full PMO integration and preserved-disagreement fixtures.

Original source: MIT, Copyright (c) 2026 Yan-Jie Wang. Not execution evidence.
"""
from copy import deepcopy
from dataclasses import replace
import datetime as dt
import json
import unittest
from unittest.mock import patch

import prepare_full_pmo_provider as full
import prepare_integrated_provider as base
import widget_review as review
from prepare_boundary_provider import prove_sequence, window


class FullPublishedPMO(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.old_groups, unused = base.prove_integration()
        cls.groups, cls.pins, cls.oracle, cls.differences = full.prove_full_integration()
        cls.source = full.SCRIPT.read_text('utf-8')
        cls.facts, cls.months, cls.end = review.decode_numeric_array(cls.source)
        cls.report = json.loads(full.REPORT.read_text())

    def test_all_mandatory_days_and_complete_boundary_years(self):
        rows, spans, bounds = self.groups['chinese']
        self.assertEqual(len(rows), 2499)
        self.assertEqual(full.complete_years(rows), list(range(1899, 2101)))
        self.assertEqual(spans, [[-25567, -25537], [-25537, 47875]])
        self.assertTrue(bounds['entire_mandatory_window_primary_covered'])
        first, end = window('chinese')
        self.assertEqual(len(self.oracle), 73414)
        self.assertEqual([r[0] for r in self.oracle], list(range(first, end)))
        self.assertEqual(self.oracle[0], (-25567, 1899, 12, 12, 0, 1, 326, 30, 355, 12))
        self.assertEqual(self.oracle[29][5], 30)
        self.assertEqual(self.oracle[30][1:6], (1900, 1, 1, 0, 1))
        self.assertEqual(self.oracle[-1][1:6], (2100, 12, 12, 0, 1))
        self.assertEqual((rows[-1].epoch_day, rows[-1].lunar_year, rows[-1].month,
                          rows[-1].is_leap, rows[-1].length), (47875, 2101, 1, False, 0))

    def test_every_month_length_and_year_partition(self):
        rows, unused_spans, unused_bounds = self.groups['chinese']
        prove_sequence(rows)
        for year in range(1900, 2101):
            subset = [r for r in rows if r.lunar_year == year]
            next_year = next(r for r in rows if r.lunar_year == year + 1)
            self.assertEqual([r.month for r in subset if not r.is_leap], list(range(1, 13)))
            self.assertIn(len(subset), (12, 13))
            self.assertTrue(all(r.length in (29, 30) for r in subset))
            self.assertEqual(sum(r.length for r in subset), next_year.epoch_day - subset[0].epoch_day)

    def test_dangi_and_private_prefix_are_exactly_retained(self):
        self.assertEqual(self.groups['dangi'], self.old_groups['dangi'])
        self.assertEqual(self.groups['chinese'][0][:12], self.old_groups['chinese'][0][:12])
        self.assertEqual(full.dangi_block(full.encode(self.groups).decode('ascii')),
                         full.dangi_block(full.OLD_PROVIDER.read_text('ascii')))
        self.assertTrue(all(r.epoch_day + r.length <= -25567 for r in self.groups['chinese'][0][:11]))

    def test_hko_differences_and_original_failed_receipt_remain(self):
        self.assertEqual(len(self.differences), 30)
        self.assertEqual(self.differences[0]['widget'], [2057, 8, 0, 30])
        self.assertEqual(self.differences[0]['hko'], [2057, 9, 0, 1])
        self.assertEqual(self.differences[-1]['date'], '2057-10-27')
        self.assertEqual(json.loads(full.FAILED_STATE.read_text())['status'], 'failed')
        self.assertFalse(self.report['continuous_hko_1901_2100_certified'])

    def test_erased_extended_or_reversed_diagnostics_are_rejected(self):
        for action in ('erase', 'extra', 'reverse', 'historical', 'missing', 'repair'):
            report = deepcopy(self.report)
            if action == 'erase': report['hko']['differences'] = []
            if action == 'extra': report['hko']['differences'].append(report['hko']['differences'][-1])
            if action == 'reverse': report['hko']['differences'][0]['widget'] = [2057, 9, 0, 1]
            if action == 'historical': report['pmo_pdf']['matched_days'] -= 1
            if action == 'missing': report['hko_missing_date'] = None
            if action == 'repair': report['continuous_hko_1901_2100_certified'] = True
            with self.subTest(action=action), self.assertRaises(ValueError):
                full.bound_diagnostic(report, self.facts, self.months)

    def test_numeric_expressions_and_incomplete_inventory_are_rejected(self):
        for source in (self.source.replace('0x04bd8', '0x04bd8 + 0'),
                       self.source.replace('0x04bd8,', '', 1),
                       self.source.replace('0x04bd8', '0x24bd8', 1),
                       self.source.replace('0x0d520],//2100', '0x0d521],//2100')):
            with self.assertRaises(ValueError): review.decode_numeric_array(source)

    def test_private_prefix_and_january_pdf_boundary_mutations_rejected(self):
        for index, changes in ((0, {'month': 2}), (10, {'length': 30}),
                               (11, {'epoch_day': -25566})):
            groups = deepcopy(self.old_groups)
            rows, spans, bounds = groups['chinese']
            rows[index] = replace(rows[index], **changes)
            with patch.object(base, 'prove_integration', return_value=(groups, {})):
                with self.assertRaises(ValueError): full.prove_full_integration()

    def test_missing_initial_primary_pdf_month_rejected(self):
        primary = base.load_pmo(base.PMO)
        with patch.object(base, 'prove_integration', return_value=(deepcopy(self.old_groups), {})), \
             patch.object(base, 'load_pmo', return_value=primary[1:]):
            with self.assertRaises(ValueError): full.prove_full_integration()

    def test_terminal_m01_discontinuity_and_identity_rejected(self):
        rows = self.groups['chinese'][0]
        for changes in ({'epoch_day': 47876}, {'lunar_year': 2102},
                        {'month': 2}, {'is_leap': True}, {'length': 1}):
            with self.assertRaises(ValueError):
                prove_sequence(rows[:-1] + [replace(rows[-1], **changes)])


if __name__ == '__main__':
    unittest.main()
