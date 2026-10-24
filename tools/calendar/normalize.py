#!/usr/bin/env python3
"""Normalize reviewed factual calendar records without executing source code."""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import datetime
import json
from pathlib import Path
import re
import subprocess
import unicodedata
import xml.etree.ElementTree as ET

from acquire import atomic_write

EPOCH = datetime.date(1970, 1, 1)


def japanese_eras(text):
    """Keep the current and future modern eras; 1873 is an ECMA boundary."""
    if text.lstrip().startswith('<'):
        root = ET.fromstring(text)
        entries = root.find("./calendarData/calendar[@type='japanese']/eras")
        if root.tag != 'supplementalData' or entries is None:
            raise ValueError('missing CLDR Japanese era data')
        eras = {}
        for item in entries:
            identity = item.get('type')
            if (item.tag != 'era' or not identity or identity in eras or list(item) or
                    set(item.attrib) - {'type', 'start', 'code'} or not item.get('start')):
                raise ValueError('invalid CLDR Japanese era record')
            eras[identity] = {'_start': item.get('start'), '_code': item.get('code')}
    else:
        # Keep authored JSON inputs useful; shipped era data comes from release XML.
        eras = json.loads(text)['supplemental']['calendarData']['japanese']['eras']
    cutoff = datetime.date(1873, 1, 1)
    starts = []
    for era in eras.values():
        # Earlier Japanese starts can use the pre-Gregorian calendar, whose
        # day/month combinations are not necessarily valid Gregorian dates.
        parts = tuple(map(int, era['_start'].split('-')))
        if len(parts) != 3:
            raise ValueError('invalid Japanese era start')
        starts.append((parts, era.get('_code')))
    starts.sort(key=lambda row: row[0])
    prior = [i for i, (date, code) in enumerate(starts)
             if date <= (cutoff.year, cutoff.month, cutoff.day)]
    if not prior:
        raise ValueError('missing Japanese era at Gregorian adoption')
    rows, names = [], {'ce', 'bce', 'ad', 'bc'}
    previous = None
    for parts, name in starts[prior[-1]:]:
        start = datetime.date(*parts)
        if (not isinstance(name, str) or
                not re.fullmatch(r'[a-z][a-z0-9-]{0,30}', name) or name in names or
                previous is not None and start <= previous):
            raise ValueError('invalid or duplicate Japanese era code')
        names.add(name)
        previous = start
        rows.append([name, max(start, cutoff).isoformat(), start.year - 1])
    return {'schema': 1, 'calendar': 'japanese', 'eras': rows}


def persian(text):
    rows = []
    for line in text.splitlines():
        if not line.strip() or line.startswith('#'):
            continue
        match = re.fullmatch(r'(\d{4})(\*{0,2}) (\d{4}-\d{2}-\d{2})', line)
        if match is None:
            raise ValueError('malformed Persian transcription row')
        year, stars, date = match.groups()
        day = (datetime.datetime.strptime(date, '%Y-%m-%d').date() - EPOCH).days
        if int(year) != 1206 + len(rows):
            raise ValueError('Persian years missing, duplicated or unordered')
        if rows and day != rows[-1][1] + 365 + bool(rows[-1][2]):
            raise ValueError('Persian date conflicts with previous leap marker')
        rows.append([int(year), day, len(stars)])
    if len(rows) != 293 or rows[0][1] != -52150:
        raise ValueError('Persian publication coverage or epoch mismatch')
    # Derive the terminal boundary; the source does not publish a 1499 row.
    return {'schema': 1, 'calendar': 'persian', 'first_year': 1206,
            'end_epoch_day': rows[-1][1] + 365 + bool(rows[-1][2]),
            'rows': rows}


def persian_data(data):
    """Validate JSON facts without requiring their original transcription."""
    if (type(data) is not dict or type(data.get('schema')) is not int or
            data['schema'] != 1 or data.get('calendar') != 'persian' or
            type(data.get('first_year')) is not int or data['first_year'] != 1206 or
            type(data.get('end_epoch_day')) is not int or
            type(data.get('rows')) is not list or len(data['rows']) != 293):
        raise ValueError('invalid Persian factual data')
    previous_end = -52150
    for index, row in enumerate(data['rows']):
        if (type(row) is not list or len(row) != 3 or
                any(type(v) is not int for v in row)):
            raise ValueError('invalid Persian factual row')
        year, day, stars = row
        if year != 1206 + index or day != previous_end or not 0 <= stars <= 2:
            raise ValueError('invalid Persian year, leap marker or boundary')
        previous_end = day + 365 + bool(stars)
    if data['end_epoch_day'] != previous_end:
        raise ValueError('invalid Persian terminal boundary')
    return data


def japanese_data(data):
    """Validate modern era facts; the adoption boundary is part of the contract."""
    if (type(data) is not dict or type(data.get('schema')) is not int or
            data['schema'] != 1 or data.get('calendar') != 'japanese' or
            type(data.get('eras')) is not list or not data['eras']):
        raise ValueError('invalid Japanese factual data')
    previous = None
    names = {'ce', 'bce', 'ad', 'bc'}
    for index, row in enumerate(data['eras']):
        if type(row) is not list or len(row) != 3:
            raise ValueError('invalid Japanese factual row')
        name, text, offset = row
        if (not isinstance(name, str) or
                not re.fullmatch(r'[a-z][a-z0-9-]{0,30}', name) or name in names or
                not isinstance(text, str) or type(offset) is not int):
            raise ValueError('invalid Japanese era identity')
        start = datetime.datetime.strptime(text, '%Y-%m-%d').date()
        if (start.isoformat() != text or
                (index == 0 and (text != '1873-01-01' or not -1000000 <= offset <= 1872)) or
                (index != 0 and offset != start.year - 1) or
                (previous is not None and start <= previous)):
            raise ValueError('invalid Japanese era start or offset')
        names.add(name)
        previous = start
    return data


def ummalqura(data):
    """Validate the maintained ICU4X-derived facts, including both civil joins."""
    if (type(data) is not dict or type(data.get('schema')) is not int or
            data['schema'] != 1 or data.get('calendar') != 'islamic-umalqura' or
            type(data.get('first_year')) is not int or data['first_year'] != 1300 or
            type(data.get('end_epoch_day')) is not int or
            type(data.get('rows')) is not list or len(data['rows']) != 301):
        raise ValueError('invalid Umm al-Qura factual data')
    previous_end = None
    for index, row in enumerate(data['rows']):
        if type(row) is not list or len(row) != 3 or any(type(v) is not int for v in row):
            raise ValueError('invalid Umm al-Qura row')
        year, day, mask = row
        if year != 1300 + index or not 0 <= mask < 4096:
            raise ValueError('invalid Umm al-Qura year or month mask')
        days = 348 + sum((mask >> bit) & 1 for bit in range(12))
        if days not in (354, 355) or previous_end is not None and previous_end != day:
            raise ValueError('inconsistent Umm al-Qura year boundaries')
        previous_end = day + days
    if (data['rows'][0][1] != -31826 or previous_end != 74839 or
            data['end_epoch_day'] != previous_end):
        raise ValueError('Umm al-Qura publication coverage or epoch mismatch')
    return data


def official_persian_rows(text):
    # pdftotext preserves bidi controls around western digits. Normalize only
    # text presentation; never infer leap markers from Gregorian start dates.
    text = unicodedata.normalize('NFKC', text)
    text = ''.join(c for c in text if unicodedata.category(c) != 'Cf')
    rows = []
    for line in text.splitlines():
        match = re.fullmatch(r'\s*(\*{0,2})([0-9]{4})\s+([0-9]{4})\s+مارس\s+([0-9]{2})\s*', line)
        if match:
            stars, year, gy, gd = match.groups()
            if int(year) != 1206 + len(rows):
                raise ValueError('unordered official Persian PDF rows')
            rows.append([int(year), (datetime.date(int(gy), 3, int(gd)) - EPOCH).days,
                         len(stars)])
    if len(rows) != 293:
        raise ValueError('incomplete official Persian PDF rows')
    return rows


def verify_publication(pdf, normalized):
    result = subprocess.run(['pdftotext', '-layout', str(pdf.resolve()), '-'],
                            stdout=subprocess.PIPE, check=True, timeout=30)
    published = official_persian_rows(result.stdout.decode('utf-8'))
    if any(a[2] != b[2] for a, b in zip(published, normalized['rows'])):
        raise ValueError('transcribed leap flags differ from official publication')
    differences = sum(a[1] != b[1] for a, b in zip(published, normalized['rows']))
    if differences != 144:
        raise ValueError('unexpected official Gregorian column discrepancy')
    print('Verified all 293 official leap flags; 144 inconsistent printed starts')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True, help='supplied raw source directory')
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--verify-persian-pdf', type=Path)
    args = parser.parse_args()
    datasets = [persian((args.cache / 'kabise.txt').read_text(encoding='utf-8'))]
    if args.verify_persian_pdf:
        verify_publication(args.verify_persian_pdf, datasets[0])
    for data in datasets:
        # One year per line makes the factual transcription easy to review.
        rows = data.pop('rows')
        text = json.dumps(data, indent=2)[:-2] + ',\n  "rows": [\n'
        text += ',\n'.join('    ' + json.dumps(row) for row in rows) + '\n  ]\n}\n'
        atomic_write(args.output_dir / (data['calendar'] + '.json'), text.encode('ascii'))
    eras = japanese_eras((args.cache / 'cldr-calendarData.xml').read_text(encoding='utf-8'))
    atomic_write(args.output_dir / 'japanese.json',
                 (json.dumps(eras, indent=2) + '\n').encode('ascii'))


if __name__ == '__main__':
    main()
