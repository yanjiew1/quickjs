#!/usr/bin/env python3
"""Pure pinned metadata tests; no zic, compiler, engine or host tzdata."""
import argparse
import importlib.util
from pathlib import Path
import re
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('qjs_timezone_primary', ROOT / 'tools/timezone/primary.py')
primary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(primary)
sys.path.insert(0, str(ROOT / 'tools/timezone'))
import bundle
import embed


class BundleTests(unittest.TestCase):
    @staticmethod
    def fixed(offset=0):
        header = b'TZif2' + bytes(15) + struct.pack('>6I', 0, 0, 0, 0, 1, 4)
        block = struct.pack('>iBB', offset, 0, 0) + b'FIX\0'
        return header + block + header + block + b'\n\n'

    def sample(self):
        return bundle.pack({'UTC': ('UTC', self.fixed()),
                            'Etc/UTC': ('UTC', self.fixed()),
                            'Area/Seconds': ('Area/Seconds', self.fixed(1172))})

    def test_one_self_contained_binary_and_exact_embedding(self):
        binary = self.sample()
        records = bundle.unpack(binary)
        self.assertEqual(binary[:4], b'QJTZ')
        self.assertEqual([r['identifier'] for r in records], ['Area/Seconds', 'Etc/UTC', 'UTC'])
        self.assertEqual((records[1]['offset'], records[1]['length']),
                         (records[2]['offset'], records[2]['length']))
        self.assertEqual(binary[records[0]['offset']:records[0]['offset'] + records[0]['length']],
                         self.fixed(1172))
        source = embed.render(binary)
        self.assertEqual(bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', source)), binary)
        self.assertIn('qjs_tz_embedded_size', source)
        self.assertNotIn('"Area/Seconds"', source)  # Name bytes live in the binary.

    def test_generation_determinism_and_alias_integrity(self):
        items = {'UTC': ('UTC', self.fixed()), 'Zulu': ('UTC', self.fixed())}
        self.assertEqual(bundle.pack(items), bundle.pack(dict(reversed(list(items.items())))))
        with self.assertRaisesRegex(ValueError, 'alias payload'):
            bundle.pack({'UTC': ('UTC', self.fixed()), 'Zulu': ('UTC', self.fixed(1))})
        with self.assertRaisesRegex(ValueError, 'primary'):
            bundle.pack({'UTC': ('Missing', self.fixed())})
        with self.assertRaisesRegex(ValueError, 'collision'):
            bundle.pack({'UTC': ('UTC', self.fixed()), 'utc': ('UTC', self.fixed())})
        for name in ('../UTC', '/UTC', 'Area//UTC', 'Area/./UTC', 'Area/../UTC', 'UTC\0'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                bundle.pack({name: (name, self.fixed())})

    def test_corrupt_binary_bounds_and_index_relationships(self):
        binary = self.sample()
        corruptions = []
        for position, value in ((4, 2), (8, len(binary) + 1), (12, 65536), (16, 0),
                                (20, 0), (24, len(binary) + 1), (28, 1),
                                (32, len(binary)), (36, len(binary)),
                                (40, len(binary) - 1), (44, 65536)):
            changed = bytearray(binary)
            struct.pack_into('>I', changed, position, value)
            corruptions.append(bytes(changed))
        corruptions.extend((binary[:31], binary[:-1], binary + b'X'))
        for changed in corruptions:
            with self.subTest(binary=changed[:48]), self.assertRaises(ValueError):
                bundle.unpack(changed)
        for blob in (b'TZif2', b'TZif1' + bytes(60), b'TZif4' + bytes(65536)):
            with self.assertRaises(ValueError):
                bundle.pack({'UTC': ('UTC', blob)})


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
        xml = '<ldmlBCP47><keyword><key name="tz"><type name="aafirst" alias="Old/A Area/B" region="BB"/></key></keyword></ldmlBCP47>'
        countries = primary.cldr_countries(xml, {'Area/A': 'AA', 'Area/B': 'BB'})
        self.assertEqual(countries['Old/A'], 'BB')
        self.assertEqual(countries['Area/A'], 'AA')
        xml = '<ldmlBCP47><keyword><key name="tz"><type name="mixed" alias="Area/A Area/B"/></key></keyword></ldmlBCP47>'
        with self.assertRaisesRegex(ValueError, 'ambiguous CLDR alias territory'):
            primary.cldr_countries(xml, {'Area/A': 'AA', 'Area/B': 'BB'})

    def test_invalid_graphs_and_duplicate_history(self):
        with self.assertRaisesRegex(ValueError, 'link cycle'):
            primary.resolve_zone('A', set(), {'A': 'B', 'B': 'A'})
        with self.assertRaisesRegex(ValueError, 'missing link target'):
            primary.resolve_zone('A', set(), {'A': 'B'})
        with self.assertRaisesRegex(ValueError, 'duplicate backzone'):
            primary.backzone_links('Link A B\n#PACKRATLIST zone.tab Link C B\n')

    def test_pinned_cross_country_and_iso_metadata(self):
        table, rows = primary.zone_table((TZ_SOURCE / 'zone.tab').read_text())
        countries = primary.cldr_countries((ROOT / 'tools/timezone/cldr/timezone.xml').read_bytes(), table)
        historical = primary.backzone_links((TZ_SOURCE / 'backzone').read_text())
        self.assertEqual(countries['Pacific/Truk'], 'FM')
        self.assertEqual(countries['Pacific/Ponape'], 'FM')
        self.assertEqual(countries['Atlantic/Jan_Mayen'], 'SJ')
        self.assertEqual(countries['Europe/Mariehamn'], 'AX')
        self.assertEqual(countries['America/Curacao'], 'CW')
        self.assertEqual(historical['Pacific/Truk'], 'Pacific/Chuuk')
        zones, links = set(), {}
        for name in ('africa', 'antarctica', 'asia', 'australasia', 'europe',
                     'northamerica', 'southamerica', 'etcetera', 'factory', 'backward'):
            for line in (TZ_SOURCE / name).read_text().splitlines():
                fields = line.split('#', 1)[0].split()
                if fields and fields[0] == 'Zone':
                    zones.add(fields[1])
                elif fields and fields[0] == 'Link':
                    links[fields[2]] = fields[1]
        result = primary.available_named_primaries(zones, links, table, rows, countries, historical)
        expected = {'Pacific/Truk': 'Pacific/Chuuk', 'Pacific/Yap': 'Pacific/Chuuk',
                    'Pacific/Ponape': 'Pacific/Pohnpei', 'Africa/Asmera': 'Africa/Asmara',
                    'Atlantic/Jan_Mayen': 'Arctic/Longyearbyen', 'US/Eastern': 'America/New_York',
                    'Europe/Bratislava': 'Europe/Bratislava', 'Etc/UTC': 'UTC', 'GMT': 'UTC'}
        for identifier, target in expected.items():
            self.assertEqual(result[identifier], target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tz-source', type=Path, default=ROOT / 'third_party/tz')
    args, rest = parser.parse_known_args()
    TZ_SOURCE = args.tz_source
    unittest.main(argv=[__file__] + rest)
