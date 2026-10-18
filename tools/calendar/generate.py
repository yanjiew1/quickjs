#!/usr/bin/env python3
"""Generate one calendar binary from the checked normalized publications."""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import json
from pathlib import Path

from acquire import SOURCES, atomic_write, checked_sources, digest
from bundle import pack, unpack
from normalize import persian, ummalqura


def datasets():
    pin = checked_sources()
    normalized = [persian((SOURCES / 'kabise.txt').read_text(encoding='utf-8')),
                  ummalqura((SOURCES / 'ummalqura_data.rs').read_text(encoding='utf-8'))]
    result = []
    for data in normalized:
        saved = SOURCES.parent / (data['calendar'] + '.json')
        if json.loads(saved.read_text()) != data:
            raise ValueError('normalized calendar differs from source: ' + data['calendar'])
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
    return result, pin


def generate(output, manifest):
    records, pin = datasets()
    binary = pack(records)
    contents = unpack(binary)
    receipt = {'format': 'QJCA', 'format_version': 1, 'algorithm_version': 1,
               'spec_commit': pin['spec_commit'], 'sources': pin['sources'],
               'binary_sha256': digest(binary), 'binary_size': len(binary),
               'calendars': []}
    for data in contents:
        record = {k: v for k, v in data.items() if k != 'years'}
        record['year_count'] = len(data['years'])
        record['first_epoch_day'] = data['years'][0][0]
        receipt['calendars'].append(record)
    atomic_write(output, binary)
    atomic_write(manifest, (json.dumps(receipt, indent=2) + '\n').encode('ascii'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    generate(args.output, args.manifest)


if __name__ == '__main__':
    main()
