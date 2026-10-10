"""Root-only actual input replay and malformed arithmetic rejection fixtures."""
from dataclasses import replace
from pathlib import Path
import unittest
from unittest.mock import patch
from prepare_integrated_provider import (K, InvalidSource, binary_records,
                                         encode, prove_integration, read_provider)
from prepare_boundary_provider import prove_sequence


class ActualIntegration(unittest.TestCase):
    def test_actual_acquired_scopes(self):
        groups, pins = prove_integration()
        self.assertEqual(len(pins), 9)
        self.assertEqual(groups['dangi'][1], [[-25892, 29597]])
        self.assertEqual(groups['chinese'][1], [[-25567, 20442]])
        self.assertTrue(groups['dangi'][2]['entire_mandatory_window_primary_covered'])
        self.assertFalse(groups['chinese'][2]['entire_mandatory_window_primary_covered'])
        source = encode(groups).decode('ascii')
        self.assertEqual(source.count('const QJSLunisolarProvider *qjs_calendar_lunisolar_provider('), 1)
        self.assertIn('QJS_LUNISOLAR_PRIMARY_PMO', source)
        self.assertIn('QJS_LUNISOLAR_PRIMARY_KASI', source)

    def test_actual_dangi_2050_exact_join_mutations(self):
        rows, spans, bounds = read_provider('dangi')
        final = next(i for i, r in enumerate(rows) if r.lunar_year == 2050 and r.month == 12)
        self.assertGreaterEqual(rows[final].epoch_day, bounds['mandatory_end_day'])
        for bad in (rows[:final] + [replace(rows[final], epoch_day=rows[final].epoch_day + 1)] + rows[final+1:],
                    rows[:final] + [replace(rows[final], is_leap=True)] + rows[final+1:],
                    rows[:final] + [replace(rows[final], lunar_year=2051)] + rows[final+1:]):
            with self.assertRaises(InvalidSource):
                prove_sequence(bad)

    def test_actual_missing_required_primary_and_day_gap_rejected(self):
        original = read_provider
        for remove_required, gap in ((True, False), (False, True)):
            def corrupted(calendar):
                rows, spans, bounds = original(calendar)
                if calendar == 'dangi':
                    if remove_required:
                        i = next(i for i, r in enumerate(rows) if r.lunar_year == 2050 and r.month == 11)
                        rows = rows[:i] + rows[i+1:]
                    if gap:
                        spans = [[-25892, 29242], [29243, 29597]]
                return rows, spans, bounds
            with patch('prepare_integrated_provider.read_provider', side_effect=corrupted):
                with self.assertRaises(InvalidSource):
                    prove_integration()

    def test_generated_binary_reserved_byte_and_length_corruption(self):
        raw = (K / 'kasi-generated/kasi-months.bin').read_bytes()
        for offset, value in ((43, 1), (42, 28)):
            damaged = bytearray(raw); damaged[offset] = value
            with self.assertRaises(InvalidSource):
                binary_records(damaged)


if __name__ == '__main__':
    unittest.main()
