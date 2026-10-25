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

"""Bounded source acquisition and offline timezone generation contracts."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import struct
import subprocess
import unittest
from unittest import mock

from test_data_io import Response

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools' / 'timezone'
spec = importlib.util.spec_from_file_location('timezone_acquire',
                                             str(TOOLS / 'acquire.py'))
acquire = importlib.util.module_from_spec(spec)
spec.loader.exec_module(acquire)


def load_tool(name):
    spec = importlib.util.spec_from_file_location('timezone_' + name,
                                                 str(TOOLS / (name + '.py')))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


primary = load_tool('primary')
bundle = load_tool('bundle')
with mock.patch.dict('sys.modules', {'acquire': acquire, 'bundle': bundle, 'primary': primary}):
    embed = load_tool('embed')
    windows = load_tool('windows')
    generate = load_tool('generate')


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class AcquisitionTests(unittest.TestCase):
    def test_verified_cache_is_offline(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            path.write_bytes(b'pinned')
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen') as network:
                self.assertEqual(acquire.fetch(path, source), b'pinned')
                network.assert_not_called()
                self.assertEqual(acquire.fetch(path, source, True), b'pinned')

    def test_bad_download_preserves_existing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            path.write_bytes(b'existing')
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen',
                                   return_value=Response(b'wrong')):
                with self.assertRaises(ValueError):
                    acquire.fetch(path, source)
            self.assertEqual(path.read_bytes(), b'existing')

    def test_valid_download_is_atomic_and_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'nested' / 'source'
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen',
                                   return_value=Response(b'pinned')) as network:
                self.assertEqual(acquire.fetch(path, source), b'pinned')
                self.assertEqual(network.call_args[1]['timeout'], 30)
            self.assertEqual(path.read_bytes(), b'pinned')
            self.assertEqual([entry.name for entry in path.parent.iterdir()], ['source'])

    def test_offline_rejects_missing_or_modified(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            for exists in [False, True]:
                if exists:
                    path.write_bytes(b'modified')
                with mock.patch.object(acquire.data_io.urllib.request, 'urlopen') as network:
                    with self.assertRaises(ValueError):
                        acquire.fetch(path, source, True)
                    network.assert_not_called()

    @staticmethod
    def archive(rows):
        output = io.BytesIO()
        with tarfile.open(fileobj=output, mode='w:gz') as tar:
            for name, data, link in rows:
                member = tarfile.TarInfo(name)
                if link:
                    member.type = tarfile.SYMTYPE
                    member.linkname = data
                    tar.addfile(member)
                else:
                    member.size = len(data)
                    tar.addfile(member, io.BytesIO(data))
        return output.getvalue()

    def test_only_verified_regular_files_are_extracted(self):
        raw = self.archive([('tz-test/africa', b'data', False),
                            ('../escape', b'not extracted', False)])
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'source'
            acquire.extract_sources(raw, target, 'test', ('africa',))
            self.assertEqual((target / 'africa').read_bytes(), b'data')
            self.assertEqual([entry.name for entry in Path(directory).iterdir()], ['source'])

    def test_rejects_links_duplicates_and_missing_sources_before_writes(self):
        archives = [
            (self.archive([('tz-test/africa', b'data', False)]), ('africa', 'asia')),
            (self.archive([('tz-test/africa', b'data', False),
                           ('tz-test/africa', b'duplicate', False)]), ('africa',)),
            (self.archive([('tz-test/africa', '/tmp/invalid', True)]), ('africa',)),
        ]
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            for raw, names in archives:
                with self.assertRaises(ValueError):
                    acquire.extract_sources(raw, target, 'test', names)
                self.assertEqual(list(target.iterdir()), [])

    def test_generation_accepts_supplied_archive_not_extracted_cache(self):
        pin = json.loads((ROOT / 'data/timezone/pin.json').read_text())
        rows = [('tz-test/' + name, ('pinned:' + name).encode(), False)
                for name in acquire.SOURCE_NAMES]
        raw = self.archive(rows)
        pin['tz'] = {'release': 'test', 'archive': {
            'url': 'https://invalid.test/archive', 'sha256': sha256(raw)}}
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / 'cache'
            cache.mkdir()
            archive = cache / 'tz-test.tar.gz'
            archive.write_bytes(raw)
            stale = cache / 'tz-test'
            stale.mkdir()
            (stale / 'zic.c').write_bytes(b'poisoned extracted source')
            work = Path(directory) / 'private'
            with mock.patch.object(acquire.data_io.urllib.request, 'urlopen') as network:
                source, cldr, identity = generate.read_sources(cache, pin, work)
                network.assert_not_called()
            self.assertEqual(source, work / 'sources')
            self.assertEqual((source / 'zic.c').read_bytes(), b'pinned:zic.c')
            self.assertEqual((stale / 'zic.c').read_bytes(), b'poisoned extracted source')
            self.assertEqual(set(p.name for p in source.iterdir()), set(acquire.SOURCE_NAMES))
            self.assertIn(b'<', cldr)
            self.assertEqual(identity, sha256(raw))

            # Hashes select official acquisition, not a generator whitelist.
            custom = self.archive([(name, data + b'custom', link)
                                   for name, data, link in rows])
            archive.write_bytes(custom)
            metadata = Path(directory) / 'cldr'
            metadata.mkdir()
            (metadata / 'timezone.xml').write_bytes(b'custom CLDR')
            source, cldr, identity = generate.read_sources(
                cache, pin, Path(directory) / 'custom', cldr_dir=metadata)
            self.assertEqual((source / 'zic.c').read_bytes(), b'pinned:zic.ccustom')
            self.assertEqual(cldr, b'custom CLDR')
            self.assertEqual(identity, sha256(custom))
            self.assertNotEqual(identity, pin['tz']['archive']['sha256'])

            # Malformed archives still fail before any new source is materialized.
            archive.write_bytes(b'changed archive')
            work = Path(directory) / 'failed'
            with self.assertRaises((ValueError, tarfile.TarError)):
                generate.read_sources(cache, pin, work)
            self.assertFalse(work.exists())

    def test_pinned_metadata_is_present_and_unchanged(self):
        pin = json.loads((ROOT / 'data' / 'timezone' / 'pin.json').read_text())
        self.assertEqual(pin['tz']['tag'], pin['tz']['release'])
        for name, source in pin['cldr']['files'].items():
            data = (ROOT / 'data' / 'timezone' / 'sources' / 'cldr' / name).read_bytes()
            self.assertEqual(sha256(data), source['sha256'], name)


class EmbeddingTests(unittest.TestCase):
    def test_requires_canonical_core_utc(self):
        blob = b'TZif2' + bytes(39)
        for zones in ({'utc': ('utc', blob)}, {'Zulu': ('Zulu', blob)},
                      {'UTC': ('Zulu', blob), 'Zulu': ('Zulu', blob)}):
            binary = bundle.pack(zones, 'test', 'test')
            rows = bundle.unpack(binary)['records']
            spans = sorted({(row['offset'], row['length']) for row in rows})
            with self.assertRaisesRegex(ValueError, 'canonical UTC'):
                embed.render(binary, rows, spans, [{'future': 1}] * len(spans))

    def test_rejects_incomplete_production_history(self):
        binary = bundle.pack({'UTC': ('UTC', b'TZif2' + bytes(39))}, 'test', 'test')
        row = bundle.unpack(binary)['records'][0]
        with self.assertRaisesRegex(ValueError, 'complete footer'):
            embed.render(binary, [row], [(row['offset'], row['length'])], [{'future': 0}])

    def test_rejects_nonzero_utc_rules(self):
        blob = b'TZif2' + bytes(39) + struct.pack('>iBB', 3600, 0, 0)
        binary = bundle.pack({'UTC': ('UTC', blob)}, 'test', 'test')
        row = bundle.unpack(binary)['records'][0]
        state = {'future': 1, 'standard_offset': 0, 'type_count': 1,
                 'types': row['offset'] + 44}
        with self.assertRaisesRegex(ValueError, 'zero offsets and no DST'):
            embed.render(binary, [row], [(row['offset'], row['length'])], [state])

    def test_failed_reader_preserves_previous_output(self):
        binary = bundle.pack({'UTC': ('UTC', b'TZif2' + bytes(39))}, 'test', 'test')
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input.bin'
            output = Path(directory) / 'output.c'
            source.write_bytes(binary)
            output.write_bytes(b'previous complete output')
            failures = [subprocess.CalledProcessError(1, 'reader'),
                        subprocess.TimeoutExpired('reader', 60)]
            for failure in failures:
                with mock.patch.object(embed.subprocess, 'run', side_effect=failure):
                    with self.assertRaises(type(failure)):
                        embed.generate(source, output, Path(directory) / 'reader')
                self.assertEqual(output.read_bytes(), b'previous complete output')


class WindowsMappingTests(unittest.TestCase):

    def test_release_xml_matches_authored_json(self):
        rows = [{'mapZone': {'_other': 'Example Standard Time', '_territory': '001', '_type': 'Area/A'}}]
        xml = b'<supplementalData><windowsZones><mapTimezones><mapZone other="Example Standard Time" territory="001" type="Area/A"/></mapTimezones></windowsZones></supplementalData>'
        self.assertEqual(windows.pack(xml, {'Area/A'}), windows.pack(json.dumps({'supplemental': {
            'windowsZones': {'mapTimezones': rows}}}).encode(), {'Area/A'}))
        with self.assertRaisesRegex(ValueError, 'mapping record'):
            windows.pack(xml.replace(b'territory="001"', b'unknown="001"'))


    def source(self, rows):
        return json.dumps({'supplemental': {'windowsZones': {'mapTimezones': [
            {'mapZone': {'_other': key, '_territory': country, '_type': targets}}
            for key, country, targets in rows]}}})

    def test_deterministic_mapping_and_available_targets(self):
        rows = [('Example Time', '001', 'Area/A'), ('Example Time', 'AA', 'Area/A Area/B')]
        first = windows.pack(self.source(rows), {'Area/A', 'Area/B'})
        self.assertEqual(first, windows.pack(self.source(rows[::-1]), {'Area/A', 'Area/B'}))
        spaced = [('Example Time', '001', 'Area/A '),
                  ('Example Time', 'AA', 'Area/A  Area/B\n')]
        self.assertEqual(first, windows.pack(self.source(spaced), {'Area/A', 'Area/B'}))
        with self.assertRaisesRegex(ValueError, 'absent from IANA'):
            windows.pack(self.source(rows), {'Area/A'})

    def test_rejects_missing_fallback_duplicate_and_bad_names(self):
        for rows in ([('Example Time', 'AA', 'Area/A')],
                     [('Example Time', '001', 'Area/A')] * 2,
                     [('Example Time', '001', 'Area/A ?')]):
            with self.assertRaises(ValueError):
                windows.pack(self.source(rows))


class PrimaryTests(unittest.TestCase):
    def compute(self, zones, links, table, countries, historical=''):
        # Every synthetic graph includes the required UTC record.
        zones = set(zones) | {'Etc/UTC'}
        links = dict(links, UTC='Etc/UTC')
        table_countries, rows = primary.zone_table(table)
        countries = dict(countries, **table_countries)
        return primary.available_named_primaries(zones, links, table_countries,
            rows, countries, primary.backzone_links(historical))

    def test_cross_country_multiple_rows_uses_historical_link(self):
        result = self.compute({'Area/B'}, {'Old/A': 'Area/B', 'Area/A1': 'Area/B', 'Area/A2': 'Area/B'},
            'AA +0000+00000 Area/A1\nAA +0100+00000 Area/A2\nBB +0000+00100 Area/B\n',
            {'Old/A': 'AA'}, 'Link\tArea/A1\tOld/A\n')
        self.assertEqual(result['Old/A'], 'Area/A1')
        self.assertEqual(result['Area/A1'], 'Area/A1')

    def test_cross_country_single_row_precedes_historical_link(self):
        result = self.compute({'Area/B'}, {'Old/A': 'Area/B', 'Area/A': 'Area/B'},
            'AA +0000+00000 Area/A\nBB +0000+00100 Area/B\n',
            {'Old/A': 'AA'}, '#PACKRATLIST zone.tab Link Area/B Old/A\n')
        self.assertEqual(result['Old/A'], 'Area/A')

    def test_same_country_uses_resolved_zone(self):
        result = self.compute({'Area/A1', 'Area/A2'}, {'Old/A': 'Area/A2'},
            'AA +0000+00000 Area/A1\nAA +0100+00000 Area/A2\n',
            {'Old/A': 'AA'}, 'Link Area/A1 Old/A\n')
        self.assertEqual(result['Old/A'], 'Area/A2')

    def test_all_utc_aliases_and_etc_offset(self):
        result = self.compute({'Etc/GMT', 'Etc/GMT+5'},
            {'GMT': 'Etc/GMT', 'Zulu': 'Etc/UTC', 'Universal': 'Zulu'}, '', {})
        for name in ('UTC', 'Etc/UTC', 'GMT', 'Etc/GMT', 'Zulu', 'Universal'):
            self.assertEqual(result[name], 'UTC')
        self.assertEqual(result['Etc/GMT+5'], 'Etc/GMT+5')

    def test_missing_territory_does_not_guess(self):
        with self.assertRaisesRegex(ValueError, 'missing geographic territory'):
            self.compute({'Area/A'}, {'Old/A': 'Area/A'},
                'AA +0000+00000 Area/A\n', {})

    def test_missing_cross_country_history_does_not_guess(self):
        with self.assertRaisesRegex(ValueError, 'missing cross-country backzone'):
            self.compute({'Area/B'}, {'Old/A': 'Area/B', 'Area/A1': 'Area/B', 'Area/A2': 'Area/B'},
                'AA +0000+00000 Area/A1\nAA +0100+00000 Area/A2\nBB +0000+00100 Area/B\n', {'Old/A': 'AA'})

    def test_cldr_country_join_and_explicit_region(self):
        data = json.dumps({'keyword': {'u': {'tz': {'aafirst': {
            '_alias': 'Old/A Area/B', '_region': 'BB'}}}}})
        countries = primary.cldr_countries(data, {'Area/A': 'AA', 'Area/B': 'BB'})
        self.assertEqual(countries['Old/A'], 'BB')
        self.assertEqual(countries['Area/A'], 'AA')
        data = json.dumps({'keyword': {'u': {'tz': {'mixed': {
            '_alias': 'Area/A Area/B'}}}}})
        with self.assertRaisesRegex(ValueError, 'ambiguous CLDR alias territory'):
            primary.cldr_countries(data, {'Area/A': 'AA', 'Area/B': 'BB'})

    def test_release_xml_aliases(self):
        raw = b'<ldmlBCP47><keyword><key name="tz"><type name="aax" alias="Old/A Area/B" region="BB"/></key></keyword></ldmlBCP47>'
        self.assertEqual(primary.cldr_countries(raw, {'Area/B': 'BB'})['Old/A'], 'BB')
        with self.assertRaisesRegex(ValueError, 'keyword record'):
            primary.cldr_countries(raw.replace(b'</key>', b'<type name="aax"/></key>'), {})

    def test_invalid_graphs_and_duplicate_history(self):
        with self.assertRaisesRegex(ValueError, 'link cycle'):
            primary.resolve_zone('A', set(), {'A': 'B', 'B': 'A'})
        with self.assertRaisesRegex(ValueError, 'missing link target'):
            primary.resolve_zone('A', set(), {'A': 'B'})
        with self.assertRaisesRegex(ValueError, 'duplicate backzone'):
            primary.backzone_links('Link A B\n#PACKRATLIST zone.tab Link C B\n')

    def test_released_country_aliases(self):
        source = ROOT / 'data' / 'timezone' / 'sources' / 'cldr' / 'timezone.xml'
        countries = primary.cldr_countries(source.read_bytes(), {
            'Pacific/Chuuk': 'FM', 'Pacific/Pohnpei': 'FM',
            'Arctic/Longyearbyen': 'SJ', 'Europe/Mariehamn': 'AX',
            'America/Curacao': 'CW'})
        for name, territory in [('Pacific/Truk', 'FM'), ('Pacific/Ponape', 'FM'),
                                ('Atlantic/Jan_Mayen', 'SJ'),
                                ('Europe/Mariehamn', 'AX'), ('America/Curacao', 'CW')]:
            self.assertEqual(countries[name], territory)


class BundleTests(unittest.TestCase):
    @staticmethod
    def fixed(offset=0):
        header = b'TZif2' + bytes(15) + struct.pack('>6I', 0, 0, 0, 0, 1, 4)
        block = struct.pack('>iBB', offset, 0, 0) + b'FIX\0'
        return header + block + header + block + b'\n\n'

    def sample(self):
        return bundle.pack({'UTC': ('UTC', self.fixed()),
                            'Etc/UTC': ('UTC', self.fixed()),
                            'Area/Seconds': ('Area/Seconds', self.fixed(1172))},
                           '2026e', '48.2.3')

    def test_single_pack_has_versions_and_shared_alias_payloads(self):
        binary = self.sample()
        contents = bundle.unpack(binary)
        records = contents['records']
        self.assertEqual(contents['tz_version'], '2026e')
        self.assertEqual(contents['cldr_version'], '48.2.3')
        self.assertEqual(contents['algorithm_version'], 1)
        self.assertEqual([r['identifier'] for r in records],
                         ['Area/Seconds', 'Etc/UTC', 'UTC'])
        self.assertEqual((records[1]['offset'], records[1]['length']),
                         (records[2]['offset'], records[2]['length']))
        self.assertEqual(binary[records[0]['offset']:
                                records[0]['offset'] + records[0]['length']],
                         self.fixed(1172))

    def test_published_pack_matches_source_pin_and_manifest(self):
        data = ROOT / 'data' / 'timezone'
        binary = (data / 'timezone-data.bin').read_bytes()
        manifest = json.loads((data / 'timezone-data-manifest.json').read_text())
        pin = json.loads((ROOT / 'data' / 'timezone' / 'pin.json').read_text())
        decoded = bundle.unpack(binary)
        self.assertEqual(sha256(binary), manifest['data_sha256'])
        self.assertEqual(len(binary), manifest['binary_size'])
        self.assertEqual(decoded['tz_version'], pin['tz']['release'])
        self.assertEqual(decoded['cldr_version'], pin['cldr']['release'])
        self.assertEqual(decoded['records'], manifest['records'])
        self.assertEqual(manifest['tz_commit'], pin['tz']['commit'])
        self.assertEqual(manifest['cldr_commit'], pin['cldr']['commit'])
        names = {r['identifier']: r for r in decoded['records']}
        for name, expected in [('US/Eastern', 'America/New_York'),
                               ('Pacific/Truk', 'Pacific/Chuuk'),
                               ('Atlantic/Jan_Mayen', 'Arctic/Longyearbyen'),
                               ('Etc/UTC', 'UTC')]:
            self.assertEqual(names[name]['primary'], expected)

    def test_determinism_and_identifier_integrity(self):
        items = {'UTC': ('UTC', self.fixed()), 'Zulu': ('UTC', self.fixed())}
        self.assertEqual(bundle.pack(items, '2026e', '48.2.3'),
            bundle.pack(dict(reversed(list(items.items()))), '2026e', '48.2.3'))
        invalid = [
            {'UTC': ('UTC', self.fixed()), 'Zulu': ('UTC', self.fixed(1))},
            {'UTC': ('Missing', self.fixed())},
            {'UTC': ('UTC', self.fixed()), 'utc': ('UTC', self.fixed())},
        ]
        invalid.extend({name: (name, self.fixed())} for name in
            ('../UTC', '/UTC', 'Area//UTC', 'Area/./UTC', 'Area/../UTC', 'UTC\0'))
        for zones in invalid:
            with self.subTest(zones=list(zones)), self.assertRaises(ValueError):
                bundle.pack(zones, '2026e', '48.2.3')

    def test_corrupt_bounds_metadata_and_index(self):
        binary = self.sample()
        for position, value in [(4, 99), (8, len(binary) + 1), (12, 65536),
                                (16, 0), (20, 0), (24, len(binary) + 1),
                                (28, 1), (32, 1), (36, 2), (40, 1), (44, 1),
                                (48, len(binary)), (52, len(binary)),
                                (56, len(binary) - 1), (60, 65536)]:
            changed = bytearray(binary)
            struct.pack_into('>I', changed, position, value)
            with self.subTest(position=position), self.assertRaises(ValueError):
                bundle.unpack(bytes(changed))
        for changed in (binary[:47], binary[:-1], binary + b'X'):
            with self.assertRaises(ValueError):
                bundle.unpack(changed)
        for blob in (b'TZif2', b'TZif1' + bytes(60), b'TZif4' + bytes(65536)):
            with self.assertRaises(ValueError):
                bundle.pack({'UTC': ('UTC', blob)}, '2026e', '48.2.3')


if __name__ == '__main__':
    unittest.main()
