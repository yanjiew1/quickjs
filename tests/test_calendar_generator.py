#!/usr/bin/env python3
"""Calendar source identity, normalization and bounded acquisition contracts."""
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools' / 'calendar'


def load_tool(name):
    spec = importlib.util.spec_from_file_location('calendar_' + name,
                                                 str(TOOLS / (name + '.py')))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


acquire = load_tool('acquire')
with mock.patch.dict(sys.modules, {'acquire': acquire}):
    normalize = load_tool('normalize')
    lunisolar = load_tool('lunisolar')
bundle = load_tool('bundle')
with mock.patch.dict(sys.modules, {'acquire': acquire, 'normalize': normalize, 'bundle': bundle}):
    generate = load_tool('generate')
    embed = load_tool('embed')


class AcquisitionTests(unittest.TestCase):
    def test_pinned_vendored_sources(self):
        pin = acquire.checked_sources()
        self.assertEqual(pin['spec_commit'], 'd026068ed653130a37d79a95c0122ecf5319f1a4')
        self.assertEqual(len(pin['sources']['persian-official.pdf']['sha256']), 64)

    def test_existing_verified_cache_needs_no_network(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            path.write_bytes(b'valid')
            source = {'url': 'https://invalid.test', 'sha256': acquire.digest(b'valid')}
            with mock.patch.object(acquire.urllib.request, 'urlopen') as network:
                acquire.fetch(path, source, True)
                network.assert_not_called()
            path.write_bytes(b'old')
            with self.assertRaises(ValueError):
                acquire.fetch(path, source, True)
            self.assertEqual(path.read_bytes(), b'old')

    def test_download_preserves_existing_data_on_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            path.write_bytes(b'old')
            source = {'url': 'https://invalid.test', 'sha256': acquire.digest(b'valid')}
            with mock.patch.object(acquire.urllib.request, 'urlopen',
                                   return_value=io.BytesIO(b'invalid')):
                with self.assertRaises(ValueError):
                    acquire.fetch(path, source)
            self.assertEqual(path.read_bytes(), b'old')
            with mock.patch.object(acquire.urllib.request, 'urlopen',
                                   return_value=io.BytesIO(b'valid')) as network:
                acquire.fetch(path, source)
                self.assertEqual(network.call_args[1]['timeout'], 30)
            self.assertEqual(path.read_bytes(), b'valid')
            self.assertEqual([p.name for p in path.parent.iterdir()], ['source'])


class NormalizationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.persian_text = (acquire.SOURCES / 'kabise.txt').read_text(encoding='utf-8')
        cls.umm_text = (acquire.SOURCES / 'ummalqura_data.rs').read_text(encoding='utf-8')

    def test_complete_persian_coverage_and_derived_boundary(self):
        data = normalize.persian(self.persian_text)
        self.assertEqual(len(data['rows']), 293)
        self.assertEqual(data['end_epoch_day'], 54866)
        by_year = {r[0]: r for r in data['rows']}
        self.assertEqual(by_year[1403][2], 1)
        self.assertEqual(by_year[1404][2], 0)
        self.assertNotIn(1499, by_year)
        self.assertEqual(data, json.loads((ROOT / 'data/calendar/persian.json').read_text()))

    def test_persian_rejects_missing_duplicate_and_inconsistent_rows(self):
        for text in (self.persian_text.replace('1206 1827-03-22', ''),
                     self.persian_text.replace('1207 1828-03-21', '1206 1828-03-21'),
                     self.persian_text.replace('1210**', '1210'),
                     self.persian_text.replace('1498*', '1498***')):
            with self.assertRaises(ValueError):
                normalize.persian(text)

    def test_complete_ummalqura_and_civil_join(self):
        data = normalize.ummalqura(self.umm_text)
        self.assertEqual(len(data['rows']), 301)
        self.assertEqual(data['rows'][0], [1300, -31826, 0xAAA])
        self.assertEqual(data['end_epoch_day'], 74839)
        # The proposal requires civil fallback outside the publication range.
        for year, day in ((1300, data['rows'][0][1]), (1601, data['end_epoch_day'])):
            civil = 354 * (year - 1) + (11 * year + 3) // 30 + 1948440 - 2440588
            self.assertEqual(day, civil)
        self.assertEqual(data, json.loads((ROOT / 'data/calendar/islamic-umalqura.json').read_text()))

    def test_ummalqura_rejects_corrupt_records(self):
        for text in (self.umm_text.replace('try_new(1300,', 'try_new(1301,'),
                     self.umm_text.replace('gregorian(1883, 11, 1)', 'gregorian(1883, 11, 2)'),
                     self.umm_text.replace('[l, s, l, s, l, s, l, s, l, s, l, s]', '[l, s]'),
                     self.umm_text.replace('try_new(1600,', 'try_new(1601,')):
            with self.assertRaises(ValueError):
                normalize.ummalqura(text)

    def test_pdf_normalization_preserves_star_meaning(self):
        rows = normalize.persian(self.persian_text)['rows']
        # The PDF places stars before the year and wraps numbers in bidi marks.
        text = '\n'.join('\u202b' + '*' * row[2] + str(row[0]) +
                         ' 2000 مارس 21\u202c' for row in rows)
        parsed = normalize.official_persian_rows(text)
        self.assertEqual([r[2] for r in parsed], [r[2] for r in rows])
        with self.assertRaises(ValueError):
            normalize.official_persian_rows(text.replace('1206', '1207'))

    def test_pdf_verification_checks_identity_before_invoking_converter(self):
        pin = acquire.checked_sources()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source.pdf'
            path.write_bytes(b'not the pinned PDF')
            with mock.patch.object(normalize.subprocess, 'run') as converter:
                with self.assertRaises(ValueError):
                    normalize.verify_publication(path, pin, {})
                converter.assert_not_called()


class BinaryTests(unittest.TestCase):
    def test_future_japanese_era_needs_no_generator_code_change(self):
        raw = json.loads((ROOT / 'data/calendar/sources/cldr-calendarData.json').read_text())
        eras = raw['supplemental']['calendarData']['japanese']['eras']
        eras['999'] = {'_code': 'future', '_start': '2035-02-03'}
        data = normalize.japanese_eras(json.dumps(raw))
        self.assertEqual(len(data['eras']), 6)
        self.assertEqual(data['eras'][-1], ['future', '2035-02-03', 2034])
        day = (normalize.datetime.date(2035, 2, 3) - normalize.EPOCH).days
        records = [{'calendar': 'japanese', 'version': 'test',
                    'eras': [(day - 1, 2018, 'reiwa'), (day, 2034, 'future')]}]
        self.assertEqual(bundle.unpack(bundle.pack(records)), records)
        for bad in ({'_start': '2035-02-03'},
                    {'_code': 'reiwa', '_start': '2035-02-03'},
                    {'_code': 'future', '_start': '2035-02-30'}):
            eras['999'] = bad
            with self.assertRaises(ValueError):
                normalize.japanese_eras(json.dumps(raw))

    def test_japanese_pack_rejects_invalid_era_records(self):
        valid = [(0, 1969, 'one'), (1, 1970, 'two')]
        bad_rows = [[], valid[::-1], valid + [valid[-1]],
                    [(0, 1000001, 'one')], [(0, 1969, 'ce')],
                    [(0, 1969, 'Name')], [(0, 1969, 'a' * 32)],
                    [(0, 1969, 'one'), (1, 1970, 'one')]]
        for rows in bad_rows:
            with self.assertRaises(ValueError):
                bundle.pack([{'calendar': 'japanese', 'version': 'test', 'eras': rows}])
        blob = bundle.pack([{'calendar': 'japanese', 'version': 'test', 'eras': valid}])
        offset = bundle.INDEX.unpack_from(blob, bundle.HEADER.size)[4]
        for position in (offset + 8, offset + 39, offset + bundle.ERA.size):
            corrupt = bytearray(blob)
            corrupt[position] = 255
            with self.assertRaises(ValueError):
                bundle.unpack(corrupt)

    def test_synthetic_leap_metadata_and_signed_years(self):
        rows = []
        for name, first, leap in (('chinese', -1, 2), ('dangi', 1000000, 12)):
            rows.append({'calendar': name, 'first_year': first, 'end_epoch_day': 384,
                         'version': 'synthetic', 'years': [(0, [30, 29] * 6 + [30], leap)]})
        self.assertEqual(bundle.unpack(bundle.pack(rows)), rows)
        rows[1]['first_year'] = 1000001
        with self.assertRaises(ValueError):
            bundle.pack(rows)
        rows[1]['first_year'] = 1000000
        for leap in (0, 13):
            rows[0]['years'][0] = (0, [30, 29] * 6 + [30], leap)
            with self.assertRaises(ValueError):
                bundle.pack(rows)

    def test_reproducible_pack_and_all_year_boundaries(self):
        datasets, pin = generate.datasets()
        binary = bundle.pack(datasets)
        self.assertEqual(binary, bundle.pack(list(reversed(datasets))))
        self.assertEqual(bundle.unpack(binary), datasets)
        self.assertEqual(binary, (ROOT / 'data/calendar/calendar-data.bin').read_bytes())
        with tempfile.TemporaryDirectory() as directory:
            out, manifest = Path(directory) / 'data.bin', Path(directory) / 'manifest.json'
            generate.generate(out, manifest)
            self.assertEqual(out.read_bytes(), binary)
            self.assertEqual(manifest.read_bytes(), (ROOT / 'data/calendar/calendar-data-manifest.json').read_bytes())
        self.assertIn('qjs_native_calendar_data', embed.render(binary))

    def test_pack_rejects_gap_duplicate_and_invalid_month(self):
        for change in ('gap', 'duplicate', 'month', 'leap', 'end'):
            datasets, pin = generate.datasets()
            if change == 'gap':
                first = datasets[0]['years'][0]
                datasets[0]['years'][0] = (first[0] + 1, first[1], first[2])
            elif change == 'duplicate':
                datasets.append(datasets[0])
            elif change == 'month':
                datasets[1]['years'][0][1][0] = 31
            elif change == 'leap':
                first = datasets[0]['years'][0]
                datasets[0]['years'][0] = (first[0], first[1], 1)
            else:
                datasets[1]['end_epoch_day'] += 1
            with self.assertRaises(ValueError):
                bundle.pack(datasets)

    def test_reader_rejects_malformed_header_index_and_year(self):
        binary = (ROOT / 'data/calendar/calendar-data.bin').read_bytes()
        for offset in list(range(0, 64, 4)) + [bundle.INDEX.unpack_from(binary, 32)[4] + i for i in range(12)]:
            corrupt = bytearray(binary)
            corrupt[offset] ^= 0x80
            with self.assertRaises(ValueError):
                bundle.unpack(corrupt)
        for length in (0, 31, len(binary) - 1):
            with self.assertRaises(ValueError):
                bundle.unpack(binary[:length])

    def test_embedding_failure_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / 'data.bin', Path(directory) / 'data.c'
            source.write_bytes(b'invalid')
            output.write_bytes(b'existing')
            with self.assertRaises(ValueError):
                embed.generate(source, output)
            self.assertEqual(output.read_bytes(), b'existing')


class LunisolarTests(unittest.TestCase):
    @staticmethod
    def rows(data):
        result = []
        for year, day, months, leap in data['rows']:
            for ordinal, length in enumerate(months, 1):
                is_leap = bool(leap and ordinal == leap + 1)
                nominal = ordinal - bool(leap and ordinal > leap)
                result.append((year, nominal, int(is_leap), day, length))
                day += length
        return result

    def test_complete_primary_coverage_and_boundary_isolation(self):
        for name in ('chinese', 'dangi'):
            data = json.loads((ROOT / ('data/calendar/' + name + '.json')).read_text())
            approximate = [tuple(r) for r in data['approximate_months']]
            primary = [row for row in self.rows(data) if row not in approximate]
            saved = dict(data)
            del saved['boundary_model']
            self.assertEqual(lunisolar.merge(name, primary, approximate), saved)
            self.assertEqual(len(approximate), 11 if name == 'chinese' else 1)
            with self.assertRaises(ValueError):
                lunisolar.merge(name, primary[:20] + primary[21:], approximate)
            changed = list(approximate)
            year, month, leap, day, length = changed[0]
            changed[0] = (year, month, leap, day + 1, length)
            with self.assertRaises(ValueError):
                lunisolar.merge(name, primary, changed)

    def test_pmo_literal_grammar_and_expected_record(self):
        data = json.loads((ROOT / 'data/calendar/chinese.json').read_text())
        words = []
        for year, first, lengths, leap in data['rows'][1:]:
            word = leap
            for ordinal, length in enumerate(lengths, 1):
                is_leap = leap and ordinal == leap + 1
                nominal = ordinal - bool(leap and ordinal > leap)
                if length == 30:
                    word |= 0x10000 if is_leap else 0x10000 >> nominal
            words.append('0x%x' % word)
        source = 'lunarInfo:[' + ','.join(words) + ']'
        rows = lunisolar.pmo(source)
        self.assertEqual(rows[0], (1900, 1, 0, -25537, 29))
        self.assertEqual(rows, self.rows(data)[12:])
        for invalid in (source + source, source.replace('0x4bd8', '0x4bd8+0'),
                        source.replace('0x4bd8', '0x4bdf')):
            with self.assertRaises(ValueError):
                lunisolar.pmo(invalid)

    def test_kasi_consumed_fields_and_duplicate_keys(self):
        row = {'LUNC_YYYY': '1899', 'LUNC_MM': '01', 'LUNC_DD': '01',
               'LUNC_LEAP_MM': '평', 'SOLC_YYYY': '1899', 'SOLC_MM': '02',
               'SOLC_DD': '10', 'SOLC_JD': '2414696', 'SOLC_WEEK': '금',
               'SOLC_LEAP_YYYY': '평', 'LUNC_EN_DD': '30'}
        def raw(value):
            return json.dumps(value).encode('ascii')
        self.assertEqual(lunisolar.kasi(raw([row]), 1, 0), [(1899, 1, 0, -25892, 30)])
        for field in ('SOLC_JD', 'SOLC_WEEK', 'LUNC_MM', 'LUNC_EN_DD'):
            bad = dict(row)
            bad[field] = 'invalid'
            with self.assertRaises(ValueError):
                lunisolar.kasi(raw([bad]), 1, 0)
        with self.assertRaises(ValueError):
            lunisolar.kasi(raw([row, row]), 1, 0)
        with self.assertRaises(ValueError):
            lunisolar.kasi(b'[{"LUNC_YYYY":"1899","LUNC_YYYY":"1900"}]', 1, 0)


if __name__ == '__main__':
    unittest.main()
