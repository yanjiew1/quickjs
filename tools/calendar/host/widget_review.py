#!/usr/bin/env python3
"""Root-owned offline factual review; this source has not been executed.

Decode only numeric literals from the PMO-hosted lunarInfo array. Compare its
facts to independently parsed PMO PDF days and all usable captured HKO days.
No retrieved JS is executed or copied into an implementation. No missing HKO
row is inferred. A report survives ordinary factual disagreements, which make
the command fail. This review does not itself activate any calendar provider.

MIT license, Copyright (c) 2026 Yan-Jie Wang.
"""
import argparse
import calendar
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import sys

B = Path('/srv/data/work/quickjs-tmp')
PARSERS = B / 'quickjs-calendar-hko-parser-source-preparation-20261009-v4'
PINS = {
    PARSERS / 'prepare_hko.py': '818da295e07864ebdc24166ecd486d0ec4dbe86ced2f8ed8c09403309b7054da',
    PARSERS / 'prepare_pmo_months.py': '3f845f3cabc2a5ce7ed394678d797bb3965f5e171edce7c4e22ede569f4f523f',
    B / 'quickjs-Chinese-primary-calendar-root-fetch-20261008/pmo-calendar.js': 'd9bda47311deb984d1c307406561ee5eb133e035c5c83645bd47534c70c303cd',
    B / 'quickjs-Chinese-primary-calendar-root-fetch-20261008/pmo-calendar.js.receipt.json': '6d923404d78097e36ed82d1c3c79399720b2053ca1f4db8a5530d6938c4070db',
    B / 'quickjs-calendar-primary-source-fetch-root-20261009-v1/pmo-index.html': '6ce51486cb689a7fb2095e4b9d290d27aa4914efb0a691265bc6fb4c532c69fa',
    B / 'quickjs-current-stage4-living-feature-gap-inventory-20261008/era-spec.emu': 'e68f6d071f9df155d0117d913ef6e14bc3535b80e31425fb8b92054fd89c8031',
}
EPOCH = dt.date(1970, 1, 1)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def pinned_inputs():
    for path, expected in PINS.items():
        require(digest(path.read_bytes()) == expected, 'source pin mismatch: ' + str(path))
    receipt = json.loads((B / 'quickjs-Chinese-primary-calendar-root-fetch-20261008/pmo-calendar.js.receipt.json').read_text())
    require(receipt['url'] == 'https://pmo.cas.cn/images/calendar.js' and
            receipt['returncode'] == 0 and receipt['commands_drained'] is True and
            receipt['sha256'] == PINS[B / 'quickjs-Chinese-primary-calendar-root-fetch-20261008/pmo-calendar.js'],
            'widget acquisition receipt')
    page = (B / 'quickjs-calendar-primary-source-fetch-root-20261009-v1/pmo-index.html').read_text()
    require('<script src="//pmo.cas.cn/images/calendar.js"></script>' in page and
            'var lunar = calendar.solar2lunar();' in page,
            'spec-linked PMO page must actively consume this widget')


def decode_numeric_array(source):
    matches = re.findall(r'\blunarInfo\s*:\s*\[(.*?)\]', source, re.S)
    require(len(matches) == 1, 'one literal lunarInfo array required')
    body = re.sub(r'/\*.*?\*/|//[^\r\n]*', '', matches[0], flags=re.S)
    require(re.fullmatch(r'\s*0x[0-9a-fA-F]+(?:\s*,\s*0x[0-9a-fA-F]+)*\s*,?\s*', body) is not None,
            'numeric literals only; expressions and unknown text rejected')
    words = [int(value, 16) for value in re.findall(r'0x[0-9a-fA-F]+', body)]
    require(len(words) == 201, 'complete 1900..2100 arithmetic record count')
    day = dt.date(1900, 1, 31)
    daily, months = {}, []
    for year, word in enumerate(words, 1900):
        require(0 <= word < 0x20000 and word & 15 <= 12, 'invalid factual bit record')
        leap = word & 15
        require(leap or not word & 0x10000, 'unused leap-length bit')
        first = day
        for nominal in range(1, 13):
            for is_leap in range(1 + int(nominal == leap)):
                length = 29 + int(bool(word & (0x10000 if is_leap else (0x10000 >> nominal))))
                months.append({'epoch_day': (day - EPOCH).days, 'lunar_year': year,
                               'month': nominal, 'is_leap': is_leap, 'length': length})
                for number in range(1, length + 1):
                    require(day not in daily, 'duplicate decoded Gregorian day')
                    daily[day] = (year, nominal, is_leap, number)
                    day += dt.timedelta(days=1)
        require((day - first).days in (353, 354, 355, 383, 384, 385), 'invalid lunar year length')
    require(day == dt.date(2101, 1, 29) and len(months) == 2486,
            'pinned widget terminal boundary or month count changed')
    return daily, months, day


def strict_pmo_days(pmo):
    pdf = (B / 'quickjs-calendar-primary-source-fetch-root-20261009-v1/pmo-calendar-1900-2025.pdf').read_bytes()
    text = (B / 'quickjs-calendar-primary-pdf-extraction-root-20261009-v2/pmo-calendar-1900-2025.txt').read_bytes()
    require(pdf.startswith(b'%PDF-') and digest(pdf) == pmo.PDF_SHA256 and
            digest(text) == pmo.TEXT_SHA256, 'PMO PDF/extraction pins')
    rows = pmo.parse_document(text.decode('utf-8', errors='strict'))
    pmo.validate_days(rows)
    output, year = [], rows[0].date.year - 1
    for index, row in enumerate(rows):
        if index and (row.month, row.leap) != (rows[index - 1].month, rows[index - 1].leap):
            year = pmo.next_month(rows[index - 1], row, year)
        output.append((row.date, year, row.month, int(row.leap), row.day))
    return output


def retained_2069_observations(raw, hko):
    """Compare printed rows while preserving rejection of the incomplete year.

    This separate observational grammar requires the exact pinned missing date;
    it never emits an annual table, repaired row, or a continuous full dataset.
    """
    try:
        hko.parse_annual(raw, 2069)
    except hko.InvalidSource as error:
        require(str(error) == '2069: annual line count', 'unexpected 2069 source defect')
    else:
        raise ValueError('known incomplete 2069 source unexpectedly accepted')
    lines = [line for line in raw.splitlines() if line.strip(b' \t')]
    require(len(lines) == 366, '2069 printed title/header/364-row count')
    require(re.fullmatch(rb'Gregorian-Lunar Calendar Conversion Table of 2069 \([^\r\n()]+\)[ \t]*', lines[0]) is not None,
            '2069 printed title')
    require(re.fullmatch(rb'Gregorian date[ \t]+Lunar date[ \t]+Day-of-week[ \t]+Solar terms[ \t]*', lines[1]) is not None,
            '2069 printed header')
    missing = dt.date(2069, 12, 30)
    expected = [dt.date(2069, 1, 1) + dt.timedelta(days=i) for i in range(365)]
    expected.remove(missing)
    observations, terms = [], []
    for line, date in zip(lines[2:], expected):
        match = hko.ROW.fullmatch(line.decode('ascii', errors='strict'))
        require(match is not None, '2069 malformed retained row')
        year, month, day, cell, weekday, term = match.groups()
        require(dt.date(int(year), int(month), int(day)) == date and
                weekday == hko.WEEKDAYS[date.weekday()], '2069 retained Gregorian sequence/weekday')
        if 'Lunar' in cell:
            ordinal = cell.split()[0]
            require(ordinal in hko.ORDINALS, '2069 lunar ordinal')
            label, number = hko.ORDINALS.index(ordinal) + 1, 1
        else:
            label, number = 0, int(cell)
            require(2 <= number <= 30, '2069 numeric lunar day')
        term = (term or '').strip(' \t')
        if term:
            require(term in hko.TERMS and date.month == hko.TERMS.index(term) // 2 + 1,
                    '2069 retained solar term')
            terms.append(term)
        observations.append(hko.Observation(date, label, number))
    require(tuple(terms) == hko.TERMS, '2069 retained solar term sequence')
    return observations


def compare_rows(label, facts, rows, unknown_leap=255):
    differences, count, uncovered, unknown_month, unknown_flag = [], 0, [], 0, 0
    for date, year, month, leap, day in rows:
        if date not in facts:
            uncovered.append(date.isoformat())
            continue
        actual = facts[date]
        expected = (year, month, leap, day)
        known = (year is not None, bool(month), leap != unknown_leap, True)
        unknown_month += not known[1]
        unknown_flag += not known[2]
        fields = [name for index, name in enumerate(('year', 'month', 'is_leap', 'day'))
                  if known[index] and actual[index] != expected[index]]
        if fields:
            differences.append({'date': date.isoformat(), 'fields': fields,
                                'widget': actual, label: expected})
        count += 1
    return {'compared_days': count, 'matched_days': count - len(differences),
            'unknown_nominal_days': unknown_month, 'unknown_leap_days': unknown_flag,
            'uncovered_dates': uncovered, 'differences': differences}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'fresh report path required')
    pinned_inputs()
    sys.path.insert(0, str(PARSERS))
    import prepare_hko as hko
    import prepare_pmo_months as pmo
    source = (B / 'quickjs-Chinese-primary-calendar-root-fetch-20261008/pmo-calendar.js').read_text('utf-8')
    facts, months, end = decode_numeric_array(source)
    primary = compare_rows('pmo_pdf', facts, strict_pmo_days(pmo))
    raw_files, captures = hko.read_capture_set(
        B / 'quickjs-calendar-hko-all-years-source-fetch-root-20261009-v1',
        B / 'quickjs-calendar-hko-all-years-source-fetch-root-20261009-v1/state.json')
    prefix, suffix = [], []
    for year in range(1901, 2101):
        if year == 2069:
            sparse = retained_2069_observations(raw_files[year], hko)
            prefix.extend(row for row in sparse if row.date < dt.date(2069, 12, 30))
        elif year <= 2068:
            prefix.extend(hko.parse_annual(raw_files[year], year))
        else:
            suffix.extend(hko.parse_annual(raw_files[year], year))
    rows = []
    bounds = []
    for observations in (prefix, suffix):
        days, unused_months, bound = hko.validate_sequence(observations)
        bounds.append(bound)
        rows.extend((EPOCH + dt.timedelta(days=r.epoch_day), r.lunar_year,
                     r.month, r.is_leap, r.day) for r in days)
    # December31 is printed, but its missing predecessor cannot certify a
    # continuous month. Compare its directly observed day/month only.
    final_2069 = sparse[-1]
    require(final_2069.date == dt.date(2069, 12, 31), 'retained 2069 last date')
    rows.append((final_2069.date, None, final_2069.label, hko.UNKNOWN_LEAP, final_2069.day))
    rows.sort(key=lambda row: row[0])
    auxiliary = compare_rows('hko', facts, rows)
    require(len(rows) == (dt.date(2101, 1, 1) - dt.date(1901, 1, 1)).days - 1,
            'every captured HKO day except exact missing2069-12-30 must be compared')
    report = {
        'status': 'completed factual comparison; differences preserved',
        'source_pins': {str(path): pin for path, pin in PINS.items()},
        'widget_provenance': 'PMO hosts and actively uses array on exact spec-linked page; third-party author Jea Yang remains attributed',
        'widget_months': len(months), 'widget_days': len(facts),
        'widget_first_iso': min(facts).isoformat(), 'widget_end_iso_exclusive': end.isoformat(),
        'pmo_pdf': primary, 'hko': auxiliary, 'hko_contiguous_observation_bounds': bounds,
        'hko_captures': captures, 'hko_missing_date': '2069-12-30',
        'continuous_hko_1901_2100_certified': False,
        'normative_policy': 'No annual byte-receipt requirement appears in era-spec line60; source and factual results require independent root acceptance',
        'provider_integration': 'not attempted; January1900 still requires acquired PDF plus outside-window1899 prefix; complete2100 year has decoded next-M01 end sentinel',
        'decoded_widget_months': months,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as output:
        output.write(json.dumps(report, indent=2) + '\n')
    failures = len(primary['differences']) + len(auxiliary['differences'])
    print(json.dumps({'report': str(args.output), 'pmo_compared_days': primary['compared_days'],
                      'hko_compared_days': auxiliary['compared_days'], 'different_days': failures}))
    return int(bool(failures))


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, UnicodeError, ValueError, KeyError, TypeError) as error:
        print('FAIL CLOSED: ' + str(error), file=sys.stderr)
        sys.exit(1)
