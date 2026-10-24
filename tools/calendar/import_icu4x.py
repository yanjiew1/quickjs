#!/usr/bin/env python3
"""Import supplied ICU4X Rust facts into candidate calendar JSON files.

This is an update helper, never an input to normal data generation. Original
source revisions, digests and the Unicode license remain in the data pin.
"""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import datetime
import json
from pathlib import Path
import re

from acquire import MAX_DOWNLOAD, atomic_write
from normalize import EPOCH, ummalqura
from lunisolar import korea_precomputed, require


def parse_ummalqura_rust(text):
    pattern = re.compile(r'PackedHijriYearData::try_new\((\d{4}), '
                         r'\[([ls, ]+)\], gregorian\((\d+), (\d+), (\d+)\)\)\.unwrap\(\),')
    rows = []
    previous_end = None
    for line in text.split('#[test]', 1)[0].splitlines():
        if not line.strip().startswith('PackedHijriYearData::try_new('):
            continue
        match = pattern.fullmatch(line.strip())
        if match is None:
            raise ValueError('malformed ICU4X Umm al-Qura row')
        year, months, gy, gm, gd = match.groups()
        lengths = months.split(', ')
        if len(lengths) != 12 or any(m not in ('l', 's') for m in lengths):
            raise ValueError('invalid Umm al-Qura month sequence')
        if int(year) != 1300 + len(rows):
            raise ValueError('Umm al-Qura years missing, duplicated or unordered')
        day = (datetime.date(int(gy), int(gm), int(gd)) - EPOCH).days
        days = 348 + lengths.count('l')
        if days not in (354, 355) or previous_end is not None and previous_end != day:
            raise ValueError('inconsistent Umm al-Qura year boundaries')
        mask = sum((m == 'l') << (11 - i) for i, m in enumerate(lengths))
        rows.append([int(year), day, mask])
        previous_end = day + days
    if len(rows) != 301 or rows[0][1] != -31826 or previous_end != 74839:
        raise ValueError('Umm al-Qura publication coverage or epoch mismatch')
    return {'schema': 1, 'calendar': 'islamic-umalqura', 'first_year': 1300,
            'end_epoch_day': previous_end, 'rows': rows}


def parse_korean_rust(text):
    """Decode pinned ICU4X records without executing Rust or its astronomy code."""
    pattern = re.compile(r'PackedEastAsianTraditionalYearData::new\((\d{4}), '
                         r'\[([ls, ]+)\], (None|Some\((\d+)\)), '
                         r'gregorian\((\d+), (\d+), (\d+)\)\),')
    rows = []
    previous_end = None
    for line in text.split('#[test]', 1)[0].splitlines():
        if not line.strip().startswith('PackedEastAsianTraditionalYearData::new('):
            continue
        match = pattern.fullmatch(line.strip())
        require(match is not None, 'malformed ICU4X Korean year')
        year, words, kind, leap, gy, gm, gd = match.groups()
        year = int(year)
        bits = words.split(', ')
        require(year == 1912 + len(rows) and len(bits) == 13 and
                all(bit in ('l', 's') for bit in bits),
                'incomplete ICU4X Korean years or months')
        # ICU4X stores the ordinal of the inserted month, not its nominal code.
        leap = int(leap) - 1 if leap is not None else 0
        require(kind == 'None' or 1 <= leap <= 12, 'invalid Korean leap month')
        lengths = [29 + (bit == 'l') for bit in bits[:12 + bool(leap)]]
        date = datetime.date(int(gy), int(gm), int(gd))
        require(date.year == year and date.month in (1, 2), 'invalid Korean new year')
        day = (date - EPOCH).days
        require(previous_end is None or day == previous_end,
                'discontinuous ICU4X Korean years')
        previous_end = day + sum(lengths)
        rows.append([year, day, lengths, leap])
    # The source includes two padding years beyond its advertised 2100 range.
    require([r[0] for r in rows] == list(range(1912, 2103)),
            'incomplete ICU4X Korean precomputed coverage')
    return rows


def read_rust(cache, name):
    with (cache / name).open('rb') as source:
        raw = source.read(MAX_DOWNLOAD + 1)
    if len(raw) > MAX_DOWNLOAD:
        raise ValueError('ICU4X source exceeds byte limit: ' + name)
    return raw.decode('utf-8')


def render(data):
    header = {key: value for key, value in data.items() if key != 'rows'}
    text = json.dumps(header, indent=2)[:-2] + ',\n  "rows": [\n'
    text += ',\n'.join('    ' + json.dumps(row) for row in data['rows'])
    return (text + '\n  ]\n}\n').encode('ascii')


def import_records(cache):
    umm = parse_ummalqura_rust(read_rust(cache, 'ummalqura_data.rs'))
    rows = parse_korean_rust(read_rust(cache, 'icu4x-korea-data.rs'))
    korean = {'schema': 1, 'calendar': 'dangi', 'first_year': 1912,
              'end_epoch_day': rows[-1][1] + sum(rows[-1][2]), 'rows': rows}
    # The maintained facts and imported facts obey the same semantic contract.
    ummalqura(umm)
    korea_precomputed(korean)
    return {'islamic-umalqura.json': umm, 'icu4x-korea-data.json': korean}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    records = import_records(args.cache)
    for name, data in records.items():
        atomic_write(args.output_dir / name, render(data))


if __name__ == '__main__':
    main()
