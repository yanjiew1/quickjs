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

from acquire import SOURCES, atomic_write, checked_sources, digest

EPOCH = datetime.date(1970, 1, 1)


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


def ummalqura(text):
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


def verify_publication(pdf, pin, normalized):
    if digest(pdf.read_bytes()) != pin['sources']['persian-official.pdf']['sha256']:
        raise ValueError('official Persian PDF differs from pin')
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
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--verify-persian-pdf', type=Path)
    args = parser.parse_args()
    pin = checked_sources()
    datasets = [persian((SOURCES / 'kabise.txt').read_text(encoding='utf-8')),
                ummalqura((SOURCES / 'ummalqura_data.rs').read_text(encoding='utf-8'))]
    if args.verify_persian_pdf:
        verify_publication(args.verify_persian_pdf, pin, datasets[0])
    for data in datasets:
        # One year per line makes the factual transcription easy to review.
        rows = data.pop('rows')
        text = json.dumps(data, indent=2)[:-2] + ',\n  "rows": [\n'
        text += ',\n'.join('    ' + json.dumps(row) for row in rows) + '\n  ]\n}\n'
        atomic_write(args.output_dir / (data['calendar'] + '.json'), text.encode('ascii'))


if __name__ == '__main__':
    main()
