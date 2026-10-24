#!/usr/bin/env python3
"""Generate one calendar binary from supplied normalized publications."""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import copy
import datetime
import json
from pathlib import Path

from acquire import SOURCES, atomic_write, load_pin, digest, read_normalized
from bundle import pack, unpack
from normalize import EPOCH, japanese_data, persian_data, ummalqura
from lunisolar import korea_precomputed


def datasets(data_dir=None, pin=None):
    root = SOURCES.parent if data_dir is None else Path(data_dir)
    pin = copy.deepcopy(load_pin() if pin is None else pin)
    # These receipts describe supplied bytes, not an authentication claim.
    # The pin retains declared revisions; acquire validates official inputs.
    for name, item in pin['normalized'].items():
        item['sha256'] = digest((root / name).read_bytes())
    normalized = [persian_data(read_normalized('persian.json', root)),
                  ummalqura(read_normalized('islamic-umalqura.json', root))]
    result = []
    for data in normalized:
        years = []
        if data['calendar'] == 'persian':
            version = 'tehran-1206-1498'
            for year, day, stars in data['rows']:
                years.append((day, [31] * 6 + [30] * 5 + [29 + bool(stars)], 0))
        else:
            version = 'icu4x-' + pin['sources']['ummalqura_data.rs']['revision']
            for year, day, mask in data['rows']:
                years.append((day, [29 + ((mask >> (11 - m)) & 1) for m in range(12)], 0))
        result.append({'calendar': data['calendar'], 'first_year': data['first_year'],
                       'end_epoch_day': data['end_epoch_day'], 'version': version, 'years': years})
    for name in ('chinese', 'dangi'):
        path = root / (name + '.json')
        data = read_normalized(path.name, root)
        first, end = data['primary_span']
        required_first, required_end = data['required_span']
        if data['schema'] != 1 or data['calendar'] != name or not first <= required_first < required_end <= end:
            raise ValueError('incomplete mandatory lunisolar publication coverage')
        for year, nominal, leap, day, length in data['approximate_months']:
            if not (day + length <= required_first or day >= required_end):
                raise ValueError('approximation enters mandatory calendar dates')
        if [r[0] for r in data['rows']] != list(range(1899, 2101)):
            raise ValueError('incomplete normalized calendar years')
        version = name + '-publications-20261019-model1'
        if name == 'dangi':
            source = 'icu4x-korea-data.json'
            expected = korea_precomputed(read_normalized(source, root))
            saved = {r[0]: r for r in data['rows']}
            if (data.get('precomputed_source') != source or
                    data.get('precomputed_span') != [end, data['end_epoch_day']] or
                    data['approximate_months'] or
                    any(saved[row[0]] != row for row in expected if row[0] <= 2100)):
                raise ValueError('normalized Dangi differs from precomputed source')
            version = 'dangi-kasi-icu4x-' + pin['sources']['icu4x-korea-data.rs']['revision']
        years = [(day, months, leap) for year, day, months, leap in data['rows']]
        result.append({'calendar': name, 'first_year': data['first_year'],
                       'end_epoch_day': data['end_epoch_day'], 'years': years,
                       'version': version})
    eras = japanese_data(read_normalized('japanese.json', root))
    rows = []
    for name, date, offset in eras['eras']:
        day = (datetime.date(*map(int, date.split('-'))) - EPOCH).days
        rows.append((day, offset, name))
    result.append({'calendar': 'japanese', 'version':
                   'cldr-' + pin['sources']['cldr-calendarData.xml']['revision'],
                   'eras': rows})
    return result, pin


def generate(output, manifest, data_dir=None, pin=None):
    records, pin = datasets(data_dir, pin)
    binary = pack(records)
    contents = unpack(binary)
    receipt = {'format': 'QJCA', 'format_version': 2, 'algorithm_version': 1,
               'spec_commit': pin['spec_commit'], 'sources': pin['sources'],
               'normalized': pin['normalized'], 'boundary_model': pin['boundary_model'],
               'boundary_months': pin['boundary_months'],
               'binary_sha256': digest(binary), 'binary_size': len(binary),
               'calendars': []}
    for data in contents:
        if data['calendar'] == 'japanese':
            receipt['calendars'].append({'calendar': 'japanese',
                'version': data['version'], 'era_count': len(data['eras']),
                'eras': [{'first_epoch_day': day, 'year_offset': offset, 'name': name}
                         for day, offset, name in data['eras']]})
            continue
        record = {k: v for k, v in data.items() if k != 'years'}
        record['year_count'] = len(data['years'])
        record['first_epoch_day'] = data['years'][0][0]
        receipt['calendars'].append(record)
    atomic_write(output, binary)
    atomic_write(manifest, (json.dumps(receipt, indent=2) + '\n').encode('ascii'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, help='calendar facts and sources directory')
    parser.add_argument('--pin', type=Path, help='declared source metadata, not an input whitelist')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    generate(args.output, args.manifest, args.data_dir,
             load_pin(args.pin) if args.pin is not None else None)


if __name__ == '__main__':
    main()
