#!/usr/bin/env python3
"""Offline source proposal for explicitly mixed boundary-year providers.

Primary inputs are pinned root-validated PMO binary or pinned KASI captures.
An approximation export is a separate root-owned input. Its accepted records
must be wholly outside the mandatory Gregorian window. Exact joins are required;
the program never moves a published start, fills a gap or labels approximation
as primary. The output is a root-review provider, not discovery authorization.
"""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import struct
import sys

from prepare_pmo_months import EPOCH, InvalidSource, Month, require
from prepare_kasi_months import load_json, read_capture

PMO_BINARY_SHA256 = "c9c93b75fe6dc9c25a8627051ed9fdf666eec09f3f996bf42826b5661a9f0536"
KASI_MANIFEST_SHA256 = "5ffb8a5e66b40a7bb477e147db6b4ab1422afba9efabec1fc14f02a64e44e907"


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def window(calendar):
    require(calendar in ("chinese", "dangi"), "unknown calendar")
    return ((dt.date(1900, 1, 1) - EPOCH).days,
            (dt.date(2101 if calendar == "chinese" else 2051, 1, 1) - EPOCH).days)


def identity(record):
    return (record.epoch_day, record.lunar_year, record.month, int(record.is_leap), record.length)


def load_pmo(path):
    raw = path.read_bytes()
    require(digest(raw) == PMO_BINARY_SHA256, "pinned root PMO binary mismatch")
    require(len(raw) >= 32, "short PMO binary")
    magic, version, count, first, end, proven_first, proven_end = struct.unpack_from("<8sIIiiii", raw)
    require(magic == b"QJSCALM1" and version == 1 and len(raw) == 32 + 12 * count, "PMO binary schema mismatch")
    output = []
    for index in range(count):
        epoch_day, year, month, leap, length, reserved = struct.unpack_from("<iiBBBB", raw, 32 + 12 * index)
        require(reserved == 0 and leap in (0, 1) and length in (29, 30), "PMO binary record mismatch")
        output.append(Month(epoch_day, year, month, bool(leap), length))
    require(output and output[0].epoch_day == proven_first and output[-1].epoch_day + output[-1].length == proven_end and first <= proven_first < proven_end <= end, "PMO binary bounds mismatch")
    return output


def load_kasi(path):
    raw = path.read_bytes()
    require(digest(raw) == KASI_MANIFEST_SHA256, "pinned root KASI manifest mismatch")
    manifest = load_json(raw)
    require(manifest["calendar"] == "dangi" and len(manifest["captures"]) == 24, "KASI manifest mismatch")
    output, seen = [], set()
    for capture in manifest["captures"]:
        key, first, last, rows = read_capture(capture, path.parent)
        require(key not in seen and (first, last) == (1899, 2051), "KASI request coverage mismatch")
        seen.add(key)
        # Retain published following-year rows. The earlier complete-year
        # binary deliberately excluded them; these rows retain primary status.
        output.extend(rows)
    require(len(seen) == 24 and output, "KASI capture set missing")
    return sorted(output, key=lambda row: row.epoch_day)


def load_approximation(path, calendar, year):
    raw = path.read_bytes()
    value = load_json(raw)
    require(set(value) == {"calendar", "year", "role", "months", "next_m01"} and
            value["calendar"] == calendar and value["year"] == year and
            value["role"] == "approximation", "approximation export header mismatch")
    def record(obj, sentinel=False):
        require(set(obj) == {"epoch_day", "lunar_year", "month", "is_leap", "length"} and
                all(type(item) is int for item in obj.values()), "approximation record schema mismatch")
        require(-(1 << 31) <= obj["epoch_day"] < (1 << 31) and
                obj["lunar_year"] == year + int(sentinel) and
                1 <= obj["month"] <= 12 and obj["is_leap"] in (0, 1) and
                obj["length"] in ((0,) if sentinel else (29, 30)), "approximation record value mismatch")
        return Month(obj["epoch_day"], obj["lunar_year"], obj["month"], bool(obj["is_leap"]), obj["length"])
    require(type(value["months"]) is list and len(value["months"]) in (12, 13), "approximation month count")
    months = [record(obj) for obj in value["months"]]
    sentinel = record(value["next_m01"], True)
    require(sentinel.month == 1 and not sentinel.is_leap, "approximation next M01 malformed")
    prove_sequence(months + [sentinel])
    return months, sentinel, digest(raw)


def prove_sequence(records):
    require(bool(records), "empty arithmetic sequence")
    require(len({(row.lunar_year, row.month, int(row.is_leap)) for row in records}) == len(records), "duplicate month identity")
    leap_years = set()
    for index, row in enumerate(records):
        require(1 <= row.month <= 12 and row.is_leap in (False, True) and
                (row.length in (29, 30) or (index == len(records) - 1 and row.length == 0 and row.month == 1 and not row.is_leap)), "invalid arithmetic record")
        date = EPOCH + dt.timedelta(days=row.epoch_day)
        require(date.year in (row.lunar_year, row.lunar_year + 1), "arithmetic/Gregorian year mismatch")
        if row.month == 1 and not row.is_leap:
            require(date.year == row.lunar_year and date.month in (1, 2), "normal M01 Gregorian year/month mismatch")
        if row.is_leap:
            require(row.lunar_year not in leap_years, "two leap months in one arithmetic year")
            leap_years.add(row.lunar_year)
        if not index:
            continue
        previous = records[index - 1]
        require(previous.epoch_day + previous.length == row.epoch_day, "primary/approximation discontinuity: no boundary adjustment permitted")
        if row.is_leap:
            require(not previous.is_leap and row.month == previous.month and row.lunar_year == previous.lunar_year, "illegal leap identity at join")
        elif previous.month == 12:
            require(row.month == 1 and row.lunar_year == previous.lunar_year + 1, "illegal lunar-year join")
        else:
            require(row.month == previous.month + 1 and row.lunar_year == previous.lunar_year, "skipped/reversed normal month at join")


def merge(primary, approximate, sentinel, calendar, year):
    first, end = window(calendar)
    prove_sequence(primary)
    primary_ids = {identity(row) for row in primary}
    result, accepted = list(primary), []
    for row in approximate:
        if any(row.epoch_day < published.epoch_day + published.length and
               published.epoch_day < row.epoch_day + row.length for published in primary):
            continue
        # A wholly unpublished month inside the mandatory window is a missing
        # primary month. It is deliberately omitted, so continuity/year proof
        # fails; accepting it would counterfeit required evidence.
        if row.epoch_day < end and row.epoch_day + row.length > first:
            continue
        result.append(row)
        accepted.append(row)
    same_next = [row for row in primary if row.lunar_year == year + 1 and row.month == 1 and not row.is_leap]
    if same_next:
        next_m01 = same_next[0]
    else:
        require(not first <= sentinel.epoch_day < end, "next M01 requires missing primary evidence inside mandatory window")
        result.append(sentinel)
        next_m01 = sentinel
    result.sort(key=lambda row: row.epoch_day)
    prove_sequence(result)
    group = [row for row in result if row.lunar_year == year]
    require([row.month for row in group if not row.is_leap] == list(range(1, 13)), "boundary year lacks normal M01..M12")
    require(len(group) in (12, 13) and group[-1].epoch_day + group[-1].length == next_m01.epoch_day and
            353 <= next_m01.epoch_day - group[0].epoch_day <= 385, "boundary year/sentinel continuity not proven")
    spans = []
    for row in result:
        if identity(row) not in primary_ids:
            require(row.length == 0 or row.epoch_day + row.length <= first or row.epoch_day >= end, "approximation entered mandatory window")
            continue
        span = [row.epoch_day, row.epoch_day + row.length]
        if spans and spans[-1][1] == span[0]:
            spans[-1][1] = span[1]
        else:
            spans.append(span)
    required_start, required_end = max(first, group[0].epoch_day), min(end, next_m01.epoch_day)
    if required_start < required_end:
        require(any(a <= required_start and b >= required_end for a, b in spans), "boundary year's mandatory days lack primary coverage")
    proven_end = result[-1].epoch_day + result[-1].length
    return result, spans, {"first_day": result[0].epoch_day, "end_day": proven_end,
                           "boundary_year_first_day": group[0].epoch_day,
                           "boundary_year_end_day": next_m01.epoch_day,
                           "mandatory_first_day": first, "mandatory_end_day": end,
                           "entire_mandatory_window_primary_covered": any(a <= first and b >= end for a, b in spans),
                           "accepted_approximate_months": [dict(epoch_day=row.epoch_day, lunar_year=row.lunar_year, month=row.month, is_leap=int(row.is_leap), length=row.length) for row in accepted]}


def encode_provider(records, spans, bounds, calendar):
    authority = "QJS_LUNISOLAR_PRIMARY_PMO" if calendar == "chinese" else "QJS_LUNISOLAR_PRIMARY_KASI"
    identifier = "QJS_CAL_CHINESE" if calendar == "chinese" else "QJS_CAL_DANGI"
    rows = ['/* Explicit composite arithmetic. Primary authority is only in spans. */', '#include "published-lunisolar.h"', 'static const QJSPublishedCalendarMonth composite_months[] = {']
    rows.extend(f"    {{ {r.epoch_day}, {r.lunar_year}, {r.month}, {int(r.is_leap)}, {r.length} }}," for r in records)
    rows += ['};', 'static const QJSPublishedCalendarTable composite_arithmetic = {',
             '    composite_months, sizeof(composite_months) / sizeof(composite_months[0]),',
             f'    {bounds["first_day"]}, {bounds["end_day"]},', f'    {bounds["first_day"]}, {bounds["end_day"]}', '};',
             'static const QJSLunisolarPrimarySpan primary_spans[] = {']
    rows.extend(f'    {{ {first}, {end}, {authority} }},' for first, end in spans)
    rows += ['};', 'static const QJSLunisolarProvider provider = {',
             '    &composite_arithmetic, primary_spans, sizeof(primary_spans) / sizeof(primary_spans[0])', '};',
             'const QJSLunisolarProvider *qjs_calendar_lunisolar_provider(QJSCalendarId calendar)', '{',
             f'    return calendar == {identifier} ? &provider : NULL;', '}', '']
    return '\n'.join(rows).encode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--calendar', choices=('chinese', 'dangi'), required=True)
    parser.add_argument('--year', type=int, required=True)
    parser.add_argument('--primary', type=Path, required=True)
    parser.add_argument('--approximation-json', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    require(args.year in (1899, 2100 if args.calendar == 'chinese' else 2050), 'only mandatory-window boundary years may be completed')
    primary = load_pmo(args.primary) if args.calendar == 'chinese' else load_kasi(args.primary)
    approximate, sentinel, approximation_hash = load_approximation(args.approximation_json, args.calendar, args.year)
    records, spans, bounds = merge(primary, approximate, sentinel, args.calendar, args.year)
    c_source = encode_provider(records, spans, bounds, args.calendar)
    audit = {'status': 'boundary-year source continuity proven; discovery remains gated',
             'calendar': args.calendar, 'boundary_year': args.year,
             'primary_input_sha256': digest(args.primary.read_bytes()), 'approximation_input_sha256': approximation_hash,
             'primary_authority': 'PMO' if args.calendar == 'chinese' else 'KASI',
             'primary_spans': spans, 'bounds': bounds, 'month_count': len(records),
             'c_source_sha256': digest(c_source),
             'interpretation': 'Era5833 Gregorian mandatory window; only outside-window arithmetic can be approximate',
             'continuity_policy': 'exact dated joins, nominal order, one leap, 29/30-day lengths, full boundary-year next M01; no boundary shifts',
             'future_pmo_2027_2100': 'unacquired, never inferred from HKO', 'calendar_discovery_authorized': False}
    args.output_dir.mkdir(parents=True, exist_ok=False)
    (args.output_dir / 'provider.c').write_bytes(c_source)
    (args.output_dir / 'provenance.json').write_text(json.dumps(audit, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    try:
        main()
    except (InvalidSource, OSError, UnicodeError, ValueError, KeyError, TypeError) as error:
        print(f'FAIL CLOSED: {error}', file=sys.stderr)
        sys.exit(1)
