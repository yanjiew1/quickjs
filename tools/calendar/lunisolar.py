#!/usr/bin/env python3
"""Normalize PMO/KASI facts and explicitly unpublished boundary approximations.

Raw publications remain in the selected source cache. No retrieved JavaScript
is executed, and approximate months may not enter mandatory Gregorian dates.
"""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import calendar
import datetime
import json
from pathlib import Path
import re
import subprocess

from acquire import atomic_write, checked_sources, digest

EPOCH = datetime.date(1970, 1, 1)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_source(cache, pin, name):
    raw = (cache / name).read_bytes()
    require(digest(raw) == pin['sources'][name]['sha256'], 'modified source: ' + name)
    return raw


def pmo(text):
    arrays = re.findall(r'\blunarInfo\s*:\s*\[(.*?)\]', text, re.S)
    require(len(arrays) == 1, 'expected one PMO numeric array')
    body = re.sub(r'/\*.*?\*/|//[^\r\n]*', '', arrays[0], flags=re.S)
    require(re.fullmatch(r'\s*0x[0-9a-fA-F]+(?:\s*,\s*0x[0-9a-fA-F]+)*\s*,?\s*', body),
            'PMO array must contain only numeric literals')
    words = [int(word, 16) for word in re.findall(r'0x[0-9a-fA-F]+', body)]
    require(len(words) == 201, 'incomplete PMO 1900..2100 array')
    result, day = [], (datetime.date(1900, 1, 31) - EPOCH).days
    for year, word in enumerate(words, 1900):
        leap = word & 15
        require(word < 0x20000 and leap <= 12 and (leap or not word & 0x10000),
                'invalid PMO month bits')
        for month in range(1, 13):
            for is_leap in range(1 + (month == leap)):
                length = 29 + bool(word & (0x10000 if is_leap else 0x10000 >> month))
                result.append((year, month, is_leap, day, length))
                day += length
    require(len(result) == 2486 and day == (datetime.date(2101, 1, 29) - EPOCH).days,
            'PMO terminal boundary mismatch')
    return result


def kasi(raw, month, leap):
    def unique(pairs):
        value = {}
        for key, item in pairs:
            require(key not in value, 'duplicate KASI JSON key')
            value[key] = item
        return value

    values = json.loads(raw.decode('utf-8'), object_pairs_hook=unique)
    require(isinstance(values, list), 'KASI response must be an array')
    output = []
    previous = None
    for row in values:
        def number(key, width):
            value = row[key]
            require(isinstance(value, str) and re.fullmatch('[0-9]{%d}' % width, value),
                    'invalid KASI numeric field: ' + key)
            return int(value)

        fields = {'LUNC_YYYY', 'LUNC_MM', 'LUNC_DD', 'LUNC_LEAP_MM',
                  'SOLC_YYYY', 'SOLC_MM', 'SOLC_DD', 'SOLC_JD', 'SOLC_WEEK',
                  'SOLC_LEAP_YYYY', 'LUNC_EN_DD'}
        require(isinstance(row, dict) and fields <= set(row), 'incomplete KASI record')
        year = number('LUNC_YYYY', 4)
        require(1899 <= year <= 2050 and number('LUNC_MM', 2) == month and
                row['LUNC_DD'] == '01' and row['LUNC_LEAP_MM'] == ('윤' if leap else '평'),
                'KASI record disagrees with request')
        gy, gm, gd = number('SOLC_YYYY', 4), number('SOLC_MM', 2), number('SOLC_DD', 2)
        date = datetime.date(gy, gm, gd)
        day = (date - EPOCH).days
        require(gy in (year, year + 1) and (month != 1 or leap or gy == year and gm in (1, 2)),
                'KASI arithmetic year mismatch')
        require(number('SOLC_JD', 7) == day + 2440588 and row['SOLC_WEEK'] == '월화수목금토일'[date.weekday()],
                'KASI Gregorian date, JDN or weekday mismatch')
        require(row['SOLC_LEAP_YYYY'] == ('윤' if calendar.isleap(gy) else '평'),
                'KASI Gregorian leap-year mismatch')
        length = number('LUNC_EN_DD', 2)
        require(length in (29, 30) and (previous is None or year > previous),
                'invalid KASI length or year order')
        output.append((year, month, leap, day, length))
        previous = year
    return output


def approximate_months(row):
    year, day, leap = row['year'], row['first_day'], row['leap_month']
    lengths = row['months']
    require(type(year) is int and type(day) is int and type(leap) is int and
            0 <= leap <= 12 and len(lengths) == 12 + bool(leap), 'invalid approximate year')
    result = []
    for ordinal, length in enumerate(lengths, 1):
        require(type(length) is int and length in (29, 30), 'invalid approximate month')
        is_leap = bool(leap and ordinal == leap + 1)
        nominal = ordinal - bool(leap and ordinal > leap)
        result.append((year, nominal, int(is_leap), day, length))
        day += length
    require(day == row['end_day'], 'inconsistent approximate end')
    return result


def merge(name, primary, approximate):
    required_first = (datetime.date(1900, 1, 1) - EPOCH).days
    required_end = (datetime.date(2101 if name == 'chinese' else 2051, 1, 1) - EPOCH).days
    primary = sorted(primary, key=lambda r: r[3])
    require(primary, 'empty primary calendar')
    for previous, current in zip(primary, primary[1:]):
        require(previous[3] + previous[4] == current[3], 'primary month gap or overlap')
    require(primary[0][3] <= required_first and primary[-1][3] + primary[-1][4] >= required_end,
            'primary data does not cover mandatory Gregorian dates')
    accepted = []
    identities = {(r[0], r[1], r[2]) for r in primary}
    for row in approximate:
        if row[:3] in identities:
            continue
        require(row[3] + row[4] <= required_first or row[3] >= required_end,
                'approximation enters mandatory Gregorian dates')
        accepted.append(row)
    combined = sorted(primary + accepted, key=lambda r: r[3])
    groups = {}
    previous = None
    for row in combined:
        year, month, leap, day, length = row
        require(1 <= month <= 12 and leap in (0, 1) and length in (29, 30), 'invalid month')
        if previous:
            require(previous[3] + previous[4] == day, 'primary/approximate boundary discontinuity')
        groups.setdefault(year, []).append(row)
        previous = row
    output = []
    for year in sorted(groups):
        rows = groups[year]
        require([r[1] for r in rows if not r[2]] == list(range(1, 13)), 'incomplete normal month sequence')
        require(len(rows) in (12, 13), 'invalid month count')
        leap_months = [r[1] for r in rows if r[2]]
        require(len(leap_months) == len(rows) - 12, 'multiple leap months')
        for index, row in enumerate(rows):
            if row[2]:
                require(index and rows[index - 1][1:3] == (row[1], 0), 'misplaced leap month')
        output.append([year, rows[0][3], [r[4] for r in rows], leap_months[0] if leap_months else 0])
    require([r[0] for r in output] == list(range(1899, (2100 if name == 'chinese' else 2050) + 1)),
            'incomplete composite year coverage')
    return {'schema': 1, 'calendar': name, 'first_year': output[0][0],
            'end_epoch_day': combined[-1][3] + combined[-1][4],
            'primary_span': [primary[0][3], primary[-1][3] + primary[-1][4]],
            'required_span': [required_first, required_end],
            'approximate_months': [list(r) for r in accepted], 'rows': output}


def normalize(cache, boundary_tool):
    pin = checked_sources()
    primary_pmo = pmo(read_source(cache, pin, 'pmo-calendar.js').decode('utf-8'))
    for item in pin['boundary_months']:
        read_source(cache, pin, item['source'])
        day = (datetime.datetime.strptime(item['start'], '%Y-%m-%d').date() - EPOCH).days
        primary_pmo.append((item['year'], item['month'], item['leap'], day, item['length']))
    primary_kasi = []
    for month in range(1, 13):
        for leap in range(2):
            raw = read_source(cache, pin, 'kasi-month-%02d-%d.json' % (month, leap))
            primary_kasi.extend(kasi(raw, month, leap))
    raw = subprocess.run([str(boundary_tool.resolve())], stdout=subprocess.PIPE,
                         check=True, timeout=30)
    boundary = json.loads(raw.stdout.decode('ascii'))
    require(boundary['schema'] == 1 and boundary['role'] == 'approximation' and
            len(boundary['years']) == 2, 'invalid boundary export')
    approx = {r['calendar']: approximate_months(r) for r in boundary['years']}
    result = [merge('chinese', primary_pmo, approx['chinese']),
              merge('dangi', primary_kasi, approx['dangi'])]
    for data in result:
        data['boundary_model'] = pin['boundary_model']
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--boundary-tool', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    datasets = normalize(args.cache, args.boundary_tool)
    for data in datasets:
        rows = data.pop('rows')
        text = json.dumps(data, indent=2)[:-2] + ',\n  "rows": [\n'
        text += ',\n'.join('    ' + json.dumps(row) for row in rows) + '\n  ]\n}\n'
        atomic_write(args.output_dir / (data['calendar'] + '.json'), text.encode('ascii'))


if __name__ == '__main__':
    main()
