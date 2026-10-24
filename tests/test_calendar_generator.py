#!/usr/bin/env python3
"""Calendar source identity, normalization and bounded acquisition contracts."""
import copy
import importlib.util
import io
import json
import shutil
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

from test_data_io import Response

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
with mock.patch.dict(sys.modules, {'acquire': acquire, 'normalize': normalize,
                                  'bundle': bundle, 'lunisolar': lunisolar}):
    generate = load_tool('generate')
    embed = load_tool('embed')
    icu4x = load_tool('import_icu4x')


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
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen') as network:
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
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen',
                                   return_value=Response(b'invalid')):
                with self.assertRaises(ValueError):
                    acquire.fetch(path, source)
            self.assertEqual(path.read_bytes(), b'old')
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen',
                                   return_value=Response(b'valid')) as network:
                acquire.fetch(path, source)
                self.assertEqual(network.call_args[1]['timeout'], 30)
            self.assertEqual(path.read_bytes(), b'valid')
            self.assertEqual([p.name for p in path.parent.iterdir()], ['source'])

    def test_selected_source_acquisition_is_offline_and_rejects_unknown_names(self):
        sources = {'chosen': {'vendored': False}, 'ignored': {'vendored': False}}
        with tempfile.TemporaryDirectory() as directory:
            argv = ['acquire', '--cache', directory, '--source', 'chosen', '--offline']
            with mock.patch.object(sys, 'argv', argv), \
                    mock.patch.object(acquire, 'checked_sources', return_value={'sources': sources}), \
                    mock.patch.object(acquire, 'fetch') as fetch:
                acquire.main()
                fetch.assert_called_once_with(Path(directory) / 'chosen', sources['chosen'], True)
            argv[4] = 'unknown'
            with mock.patch.object(sys, 'argv', argv), \
                    mock.patch.object(acquire, 'checked_sources', return_value={'sources': sources}), \
                    mock.patch.object(acquire, 'fetch') as fetch:
                with self.assertRaises(ValueError):
                    acquire.main()
                fetch.assert_not_called()

    def test_supplied_normalized_json_and_duplicate_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'facts.json'
            raw = b'{"schema":1}'
            path.write_bytes(raw)
            with mock.patch.object(acquire, 'SOURCES', root / 'sources'):
                self.assertEqual(acquire.read_normalized('facts.json'), {'schema': 1})
                path.write_bytes(b'{"schema":2}')
                self.assertEqual(acquire.read_normalized('facts.json'), {'schema': 2})
                raw = b'{"schema":1,"schema":2}'
                path.write_bytes(raw)
                with self.assertRaises(ValueError):
                    acquire.read_normalized('facts.json')

    def test_acquisition_rejects_modified_normalized_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'facts.json').write_bytes(b'{"schema":2}')
            pin = {'sources': {}, 'normalized': {
                'facts.json': {'sha256': acquire.digest(b'{"schema":1}')}}}
            with mock.patch.object(acquire, 'SOURCES', root / 'sources'), \
                    mock.patch.object(acquire, 'load_pin', return_value=pin):
                with self.assertRaises(ValueError):
                    acquire.checked_sources()

    def test_acquisition_authenticates_release_archive_and_member(self):
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, 'w') as output:
            output.writestr('common/facts.xml', b'facts')
        raw = archive.getvalue()
        source = {'url': 'https://invalid.test/archive.zip',
                  'archive_sha256': acquire.digest(raw), 'path': 'common/facts.xml',
                  'sha256': acquire.digest(b'facts')}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'facts.xml'
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen', side_effect=lambda *args, **kwargs: Response(raw)):
                acquire.fetch(path, source)
            self.assertEqual(path.read_bytes(), b'facts')
            path.unlink()
            for field in ('archive_sha256', 'sha256'):
                changed = dict(source, **{field: '0' * 64})
                with mock.patch.object(acquire.data_io.urllib.request, 'urlopen', side_effect=lambda *args, **kwargs: Response(raw)):
                    with self.assertRaises(ValueError):
                        acquire.fetch(path, changed)
                self.assertFalse(path.exists())

    def test_icu4x_import_accepts_supplied_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'ummalqura_data.rs').write_bytes(b'custom source')
            self.assertEqual(icu4x.read_rust(root, 'ummalqura_data.rs'), 'custom source')
        with self.assertRaises(ValueError):
            icu4x.parse_ummalqura_rust(
                'PackedHijriYearData::try_new(1300, [l, s], '
                'gregorian(1883, 11, 1)).unwrap(),')
        with self.assertRaises(ValueError):
            icu4x.parse_korean_rust('')


class NormalizationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.persian_facts = acquire.read_normalized('persian.json')
        # Authored grammar fixture, not an independent source oracle. CI
        # acquires the original transcription and compares its complete output.
        cls.persian_text = '\n'.join(str(year) + '*' * stars + ' ' +
            (normalize.EPOCH + normalize.datetime.timedelta(days=day)).isoformat()
            for year, day, stars in cls.persian_facts['rows'])
        cls.umm_facts = acquire.read_normalized('islamic-umalqura.json')

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
        data = normalize.ummalqura(self.umm_facts)
        self.assertEqual(len(data['rows']), 301)
        self.assertEqual(data['rows'][0], [1300, -31826, 0xAAA])
        self.assertEqual(data['end_epoch_day'], 74839)
        # The proposal requires civil fallback outside the publication range.
        for year, day in ((1300, data['rows'][0][1]), (1601, data['end_epoch_day'])):
            civil = 354 * (year - 1) + (11 * year + 3) // 30 + 1948440 - 2440588
            self.assertEqual(day, civil)
        self.assertEqual(data, json.loads((ROOT / 'data/calendar/islamic-umalqura.json').read_text()))

    def test_ummalqura_rejects_corrupt_records(self):
        mutations = [(0, 0, 1301), (0, 1, -31825), (0, 2, 4096),
                     (0, 2, True), (0, 1, -31826.0), (-1, 0, 1601)]
        for row, field, value in mutations:
            data = copy.deepcopy(self.umm_facts)
            data['rows'][row][field] = value
            with self.assertRaises(ValueError):
                normalize.ummalqura(data)
        for field, value in [('schema', True), ('end_epoch_day', 74840),
                             ('rows', self.umm_facts['rows'][:-1])]:
            data = dict(self.umm_facts, **{field: value})
            with self.assertRaises(ValueError):
                normalize.ummalqura(data)

    def test_pdf_normalization_preserves_star_meaning(self):
        rows = normalize.persian(self.persian_text)['rows']
        # The PDF places stars before the year and wraps numbers in bidi marks.
        text = '\n'.join('\u202b' + '*' * row[2] + str(row[0]) +
                         ' 2000 مارس 21\u202c' for row in rows)
        parsed = normalize.official_persian_rows(text)
        self.assertEqual([r[2] for r in parsed], [r[2] for r in rows])
        with self.assertRaises(ValueError):
            normalize.official_persian_rows(text.replace('1206', '1207'))

    def test_pdf_verification_checks_supplied_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source.pdf'
            path.write_bytes(b'custom PDF')
            result = mock.Mock(stdout=b'invalid publication')
            with mock.patch.object(normalize.subprocess, 'run', return_value=result) as converter:
                with self.assertRaises(ValueError):
                    normalize.verify_publication(path, {'rows': []})
                converter.assert_called_once()


class BinaryTests(unittest.TestCase):
    def test_future_japanese_era_needs_no_generator_code_change(self):
        raw = ET.fromstring(
            '<supplementalData><calendarData><calendar type="japanese"><eras>'
            '<era type="1" code="meiji" start="1868-09-08"/>'
            '<era type="2" code="reiwa" start="2019-05-01"/>'
            '</eras></calendar></calendarData></supplementalData>')
        eras = raw.find("./calendarData/calendar[@type='japanese']/eras")
        future = ET.SubElement(eras, 'era', {'type': '999', 'code': 'future', 'start': '2035-02-03'})
        data = normalize.japanese_eras(ET.tostring(raw, encoding='unicode'))
        self.assertEqual(len(data['eras']), 3)
        self.assertEqual(data['eras'][-1], ['future', '2035-02-03', 2034])
        day = (normalize.datetime.date(2035, 2, 3) - normalize.EPOCH).days
        records = [{'calendar': 'japanese', 'version': 'test',
                    'eras': [(day - 1, 2018, 'reiwa'), (day, 2034, 'future')]}]
        self.assertEqual(bundle.unpack(bundle.pack(records)), records)
        for bad in ({'_start': '2035-02-03'},
                    {'_code': 'reiwa', '_start': '2035-02-03'},
                    {'_code': 'future', '_start': '2035-02-30'}):
            future.attrib.clear()
            future.attrib.update({'type': '999', **{key[1:]: value for key, value in bad.items()}})
            with self.assertRaises(ValueError):
                normalize.japanese_eras(ET.tostring(raw, encoding='unicode'))
            authored = {item.get('type'): {'_' + key: value for key, value in item.attrib.items() if key != 'type'}
                        for item in eras}
            with self.assertRaises(ValueError):
                normalize.japanese_eras(json.dumps({'supplemental': {'calendarData': {
                    'japanese': {'eras': authored}}}}))

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

    def test_generator_accepts_custom_json_and_records_actual_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'facts'
            root.mkdir()
            for source in (ROOT / 'data/calendar').glob('*.json'):
                shutil.copyfile(source, root / source.name)
            path = root / 'japanese.json'
            facts = json.loads(path.read_text())
            facts['eras'].append(['future', '2035-02-03', 2034])
            path.write_text(json.dumps(facts))
            pin = acquire.load_pin()
            expected = pin['normalized']['japanese.json']['sha256']
            records, receipt = generate.datasets(root, pin)
            self.assertEqual(next(row for row in records if row['calendar'] == 'japanese')
                             ['eras'][-1][2], 'future')
            self.assertEqual(receipt['normalized']['japanese.json']['sha256'],
                             acquire.digest(path.read_bytes()))
            self.assertNotEqual(receipt['normalized']['japanese.json']['sha256'], expected)
            self.assertEqual(pin['normalized']['japanese.json']['sha256'], expected)
            self.assertTrue(bundle.unpack(bundle.pack(records)))

    def test_generation_parses_the_same_snapshot_it_hashes(self):
        path = ROOT / 'data/calendar/japanese.json'
        read_bytes = Path.read_bytes
        original = read_bytes(path)
        changed = json.loads(original)
        changed['eras'].append(['future', '2035-02-03', 2034])
        changed = json.dumps(changed).encode()
        reads = []

        def read(candidate):
            if candidate == path:
                reads.append(candidate)
                # A source edit after the receipt read must not change the
                # facts consumed by this generation. No file is modified.
                return original if len(reads) == 1 else changed
            return read_bytes(candidate)

        with mock.patch.object(Path, 'read_bytes', read):
            records, receipt = generate.datasets()
        self.assertEqual(len(reads), 1)
        eras = next(row['eras'] for row in records if row['calendar'] == 'japanese')
        self.assertNotEqual(eras[-1][2], 'future')
        self.assertEqual(receipt['normalized']['japanese.json']['sha256'],
                         acquire.digest(original))

    def test_factual_json_rejects_invalid_types_and_boundaries(self):
        for field, value in [('schema', True), ('end_epoch_day', 1), ('first_year', 1207)]:
            facts = acquire.read_normalized('persian.json')
            facts[field] = value
            with self.assertRaises(ValueError):
                normalize.persian_data(facts)
        for column, value in [(0, True), (1, -52149), (2, 3)]:
            facts = acquire.read_normalized('persian.json')
            facts['rows'][0][column] = value
            with self.assertRaises(ValueError):
                normalize.persian_data(facts)
        for column, value in [(0, 'ce'), (1, '1873-01-02'), (2, True)]:
            facts = acquire.read_normalized('japanese.json')
            facts['eras'][0][column] = value
            with self.assertRaises(ValueError):
                normalize.japanese_data(facts)

    def test_lunisolar_json_keeps_year_identity_and_integer_types(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for source in (ROOT / 'data/calendar').glob('*.json'):
                shutil.copyfile(source, root / source.name)
            for name in ('chinese', 'dangi'):
                path = root / (name + '.json')
                original = path.read_bytes()
                source = json.loads(original)
                fields = [('first_year', 1900), ('first_year', True),
                          ('schema', True), ('end_epoch_day', 47875.0),
                          ('primary_span', [False, 47875]),
                          ('approximate_months', [[1899, 1, False, -25892, 30]])]
                if name == 'dangi':
                    fields.append(('precomputed_span', [29597.0, 47875.0]))
                for field, value in fields:
                    facts = copy.deepcopy(source)
                    facts[field] = value
                    path.write_text(json.dumps(facts))
                    with self.subTest(calendar=name, field=field), self.assertRaises(ValueError):
                        generate.datasets(root)
                for column, value in [(0, 1899.0), (1, -25892.0),
                                      (2, [30.0] + source['rows'][0][2][1:]), (3, False)]:
                    facts = copy.deepcopy(source)
                    facts['rows'][0][column] = value
                    path.write_text(json.dumps(facts))
                    with self.subTest(calendar=name, column=column), self.assertRaises(ValueError):
                        generate.datasets(root)
                # A continuous table can still be attached to the wrong year.
                facts = copy.deepcopy(source)
                for row in facts['rows']:
                    row[1] += 365
                facts['end_epoch_day'] += 365
                path.write_text(json.dumps(facts))
                with self.subTest(calendar=name, change='shifted dates'), self.assertRaisesRegex(
                        ValueError, 'related Gregorian year'):
                    generate.datasets(root)
                facts = copy.deepcopy(source)
                facts['primary_span'][0] += 1
                path.write_text(json.dumps(facts))
                with self.subTest(calendar=name, change='partial month span'), self.assertRaises(ValueError):
                    generate.datasets(root)
                path.write_bytes(original)
            # The accepted facts and their binary remain unchanged.
            records, _ = generate.datasets(root)
            self.assertEqual(bundle.pack(records), (ROOT / 'data/calendar/calendar-data.bin').read_bytes())

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
            primary = [row for row in self.rows(data)
                       if data['primary_span'][0] <= row[3] < data['primary_span'][1]]
            boundary = approximate
            if name == 'dangi':
                boundary = [row for row in self.rows(data)
                            if row[0] == 2050 and row[3] >= data['primary_span'][1]]
            saved = dict(data)
            del saved['boundary_model']
            actual = lunisolar.merge(name, primary, boundary)
            if name == 'dangi':
                actual = lunisolar.extend_dangi(actual,
                    acquire.read_normalized('icu4x-korea-data.json'))
            self.assertEqual(actual, saved)
            self.assertEqual(len(approximate), 11 if name == 'chinese' else 0)
            with self.assertRaises(ValueError):
                lunisolar.merge(name, primary[:20] + primary[21:], boundary)
            changed = list(boundary)
            year, month, leap, day, length = changed[0]
            changed[0] = (year, month, leap, day + 1, length)
            with self.assertRaises(ValueError):
                lunisolar.merge(name, primary, changed)

    def test_korean_precomputed_coverage_and_conflict_rejection(self):
        source = acquire.read_normalized('icu4x-korea-data.json')
        rows = lunisolar.korea_precomputed(source)
        self.assertEqual([r[0] for r in rows], list(range(1912, 2103)))
        data = json.loads((ROOT / 'data/calendar/dangi.json').read_text())
        self.assertEqual(len(data['rows']), 202)
        self.assertEqual(data['end_epoch_day'], 47875)
        self.assertEqual(data['required_span'], [-25567, 29585])
        self.assertEqual(data['primary_span'], [-25892, 29597])
        self.assertEqual(data['precomputed_span'], [29597, 47875])
        saved = {r[0]: r for r in data['rows']}
        for row in rows:
            if row[0] <= 2100:
                self.assertEqual(saved[row[0]], row)
        bad = dict(data, rows=[list(r) for r in data['rows'] if r[0] <= 2050])
        bad['rows'][-1][1] += 1
        with self.assertRaises(ValueError):
            lunisolar.extend_dangi(bad, source)
        leap_row = next(i for i, row in enumerate(rows) if row[3])
        mutations = [(2051 - 1912, 0, 2052), (2051 - 1912, 1, 29627),
                     (leap_row, 3, 0), (-1, 0, 2103), (0, 3, True),
                     (0, 1, float(rows[0][1])), (-1, 1, rows[-1][1] + 1)]
        for row, field, value in mutations:
            changed = copy.deepcopy(source)
            changed['rows'][row][field] = value
            with self.assertRaises(ValueError):
                lunisolar.korea_precomputed(changed)
        changed = copy.deepcopy(source)
        changed['rows'][0][2][0] = 31
        with self.assertRaises(ValueError):
            lunisolar.korea_precomputed(changed)
        with self.assertRaises(ValueError):
            lunisolar.korea_precomputed(dict(source, schema=1.0))
        # A matching terminal boundary must not hide an impossible padding year.
        changed = copy.deepcopy(source)
        padding = changed['rows'][-1]
        padding[2][padding[2].index(29)] = 30
        changed['end_epoch_day'] += 1
        with self.assertRaises(ValueError):
            lunisolar.korea_precomputed(changed)

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
