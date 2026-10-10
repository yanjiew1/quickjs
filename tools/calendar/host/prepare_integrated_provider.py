#!/usr/bin/env python3
"""Root-owned offline integration; exact acquired authority scopes only.

MIT license, Copyright (c) 2026 Yan-Jie Wang. No runtime ingestion.
This source is not execution evidence. Output is still a review candidate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

from prepare_boundary_provider import (InvalidSource, Month, identity, load_kasi,
                                      load_pmo, prove_sequence, require, window)
from prepare_pmo_months import complete_years

B = Path('/srv/data/work/quickjs-tmp')
K = B / 'quickjs-calendar-primary-month-parser-root-gates-20261009-v4'
P = B / 'quickjs-calendar-lunisolar-boundary-provider-root-gates-20261009-v1'
PMO = B / 'quickjs-calendar-primary-month-parser-root-gates-20261009-v2/pmo-generated/pmo-months.bin'
PINS = Path(__file__).resolve().parent.parent / 'inputs/pinned-inputs.json'


def pinned_inputs():
    pins = json.loads(PINS.read_text())
    require(len(pins) == 9, 'exact pin inventory missing')
    for name, expected in pins.items():
        require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == expected,
                'source pin mismatch: ' + name)
    return pins


def binary_records(raw):
    require(len(raw) >= 32, 'short generated KASI binary')
    magic, version, count, first, end, proven_first, proven_end = struct.unpack_from('<8sIIiiii', raw)
    require(magic == b'QJSCALM1' and version == 1 and len(raw) == 32 + count * 12,
            'generated KASI schema')
    records = []
    for index in range(count):
        day, year, month, leap, length, reserved = struct.unpack_from('<iiBBBB', raw, 32 + 12 * index)
        require(reserved == 0 and leap in (0, 1), 'generated KASI reserved/leap')
        records.append(Month(day, year, month, bool(leap), length))
    prove_sequence(records)
    require(records[0].epoch_day == first == proven_first and
            records[-1].epoch_day + records[-1].length == end == proven_end,
            'generated KASI bounds')
    require(complete_years(records) == list(range(1899, 2049)),
            'binary stores complete 1899..2049 records, without its next-M01 point')
    # complete_years needs a following M01; explicit end is audited separately.
    require(records[-1].lunar_year == 2049 and records[-1].month == 12,
            'generated KASI final 2049 month')
    return records, end


def read_provider(calendar):
    source = (P / (calendar + '-provider/provider.c')).read_text('ascii')
    audit = json.loads((P / (calendar + '-provider/provenance.json')).read_text())
    require(audit['calendar'] == calendar and audit['c_source_sha256'] == hashlib.sha256(source.encode('ascii')).hexdigest(),
            'provider source receipt mismatch')
    body = source.split('static const QJSPublishedCalendarMonth composite_months[] = {\n', 1)[1].split('};', 1)[0]
    rows = []
    for line in body.splitlines():
        match = re.fullmatch(r'    \{ (-?\d+), (-?\d+), (\d+), ([01]), (\d+) \},', line)
        require(match is not None, 'provider row schema')
        d, y, m, l, n = map(int, match.groups())
        rows.append(Month(d, y, m, bool(l), n))
    prove_sequence(rows)
    require(len(rows) == audit['month_count'], 'provider row count')
    spans = audit['primary_spans']
    require(spans and all(a < b for a, b in spans) and
            all(b <= c for (_, b), (c, _) in zip(spans, spans[1:])), 'provider span order')
    bounds = audit['bounds']
    require(rows[0].epoch_day == bounds['first_day'] and
            rows[-1].epoch_day + rows[-1].length == bounds['end_day'], 'provider bounds')
    return rows, spans, bounds


def primary_rows(rows, spans):
    return [r for r in rows if r.length and any(a <= r.epoch_day and r.epoch_day + r.length <= b for a, b in spans)]


def prove_integration():
    pins = pinned_inputs()
    dangi, ds, db = read_provider('dangi')
    chinese, cs, cb = read_provider('chinese')
    kasi, end = binary_records((K / 'kasi-generated/kasi-months.bin').read_bytes())
    require([identity(r) for r in dangi[:len(kasi)]] == [identity(r) for r in kasi] and
            dangi[len(kasi)].epoch_day == end and dangi[len(kasi)].lunar_year == 2050 and
            dangi[len(kasi)].month == 1 and not dangi[len(kasi)].is_leap,
            'generated KASI years/2050 exact join')
    captured = load_kasi(K / 'kasi-captures.json')
    require([identity(r) for r in primary_rows(dangi, ds)] == [identity(r) for r in captured],
            'retained KASI primary captures mismatch')
    require([identity(r) for r in primary_rows(chinese, cs)] == [identity(r) for r in load_pmo(PMO)],
            'retained PMO binary mismatch')
    first, end = window('dangi')
    require(any(a <= first and b >= end for a, b in ds) and db['entire_mandatory_window_primary_covered'] is True,
            'every mandatory Dangi day requires KASI')
    require(cs == [[-25567, 20442]] and not cb['entire_mandatory_window_primary_covered'],
            'Chinese acquired scope must stay partial')
    require(complete_years(chinese) == list(range(1899, 2025)),
            'no completed primary Chinese 2025/2026 year')
    require(complete_years(dangi) == list(range(1899, 2051)),
            'Dangi composite year proof')
    for calendar, rows, spans in [('chinese', chinese, cs), ('dangi', dangi, ds)]:
        first, end = window(calendar)
        acquired = {identity(r) for r in primary_rows(rows, spans)}
        for r in rows:
            require(identity(r) in acquired or r.length == 0 or
                    r.epoch_day + r.length <= first or r.epoch_day >= end,
                    'approximation inside required Gregorian window')
    return {'chinese': (chinese, cs, cb), 'dangi': (dangi, ds, db)}, pins


def encode(groups):
    out = ['/* Root-generated integration of pinned primary observations and explicit',
           ' * outside-window arithmetic. MIT license, Copyright (c) 2026 Yan-Jie Wang.',
           ' * Publication inputs and license provenance are retained in provenance.json. */',
           '#include "published-lunisolar.h"']
    for calendar, (rows, spans, bounds) in groups.items():
        out += [f'static const QJSPublishedCalendarMonth {calendar}_months[] = {{']
        out += [f'    {{ {r.epoch_day}, {r.lunar_year}, {r.month}, {int(r.is_leap)}, {r.length} }},' for r in rows]
        out += ['};', f'static const QJSPublishedCalendarTable {calendar}_arithmetic = {{',
                f'    {calendar}_months, sizeof({calendar}_months) / sizeof({calendar}_months[0]),',
                f'    {bounds["first_day"]}, {bounds["end_day"]}, {bounds["first_day"]}, {bounds["end_day"]}', '};',
                f'static const QJSLunisolarPrimarySpan {calendar}_spans[] = {{']
        authority = 'PMO' if calendar == 'chinese' else 'KASI'
        out += [f'    {{ {a}, {b}, QJS_LUNISOLAR_PRIMARY_{authority} }},' for a, b in spans]
        out += ['};', f'static const QJSLunisolarProvider {calendar}_provider = {{',
                f'    &{calendar}_arithmetic, {calendar}_spans, sizeof({calendar}_spans) / sizeof({calendar}_spans[0])', '};']
    out += ['const QJSLunisolarProvider *qjs_calendar_integrated_source_provider(QJSCalendarId calendar)', '{',
            '    if (calendar == QJS_CAL_CHINESE) return &chinese_provider;',
            '    if (calendar == QJS_CAL_DANGI) return &dangi_provider;', '    return NULL;', '}',
            '#ifndef QJS_CAL_PROVIDER_TEST_OVERRIDE',
            'const QJSLunisolarProvider *qjs_calendar_lunisolar_provider(QJSCalendarId calendar)', '{',
            '    return qjs_calendar_integrated_source_provider(calendar);', '}', '#endif', '']
    return '\n'.join(out).encode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    groups, pins = prove_integration()
    source = encode(groups)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    (args.output_dir / 'provider.c').write_bytes(source)
    audit = {'status': 'root-generated source; integrated runtime gates pending',
             'input_sha256': pins, 'provider_sha256': hashlib.sha256(source).hexdigest(),
             'calendars': {c: {'primary_spans': s, 'bounds': b, 'complete_arithmetic_years': complete_years(r)}
                           for c, (r, s, b) in groups.items()},
             'dangi_mandatory_gregorian_coverage': '1900-01-01..2050-12-31 inclusive',
             'chinese_future_pmo_2027_2100': 'unacquired; no substitution',
             'chinese_2025_2026': 'partial primary records retained; complete metadata unavailable',
             'discovery_authorization': False}
    (args.output_dir / 'provenance.json').write_text(json.dumps(audit, indent=2) + '\n')


if __name__ == '__main__':
    try:
        main()
    except (InvalidSource, OSError, ValueError, KeyError, TypeError, IndexError) as error:
        print('FAIL CLOSED: ' + str(error), file=sys.stderr)
        sys.exit(1)
