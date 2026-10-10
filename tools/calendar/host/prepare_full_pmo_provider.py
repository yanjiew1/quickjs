#!/usr/bin/env python3
"""Root-owned offline extraction of PMO-published factual month records.

Original Python/C source: MIT, Copyright (c) 2026 Yan-Jie Wang.
Numerical lunarInfo facts: PMO-hosted calendar.js, credited to Jea Yang
(JJonline@JJonline.Cn), version 1.0.3, 2014..2017. No retrieved JavaScript
implementation is evaluated or incorporated. No authority patch is applied.
The prior failed PMO/HKO comparison remains a pinned diagnostic input.
"""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import sys

import prepare_integrated_provider as base
import widget_review as review
from prepare_boundary_provider import Month, identity, prove_sequence, require, window
from prepare_pmo_months import complete_years, InvalidSource

B = Path('/srv/data/work/quickjs-tmp')
SCRIPT = B / 'quickjs-Chinese-primary-calendar-root-fetch-20261008/pmo-calendar.js'
REPORT = B / 'quickjs-calendar-widget-domain-review-root-gates-20261009-v1/compare.json'
FAILED_STATE = REPORT.parent / 'state.json'
OLD_PROVIDER = B / 'quickjs-calendar-primary-integrated-provider-root-gates-20261009-v1/generated/provider.c'
EXTRA_PINS = {
    REPORT: '1a94a1d4a4d857ab777e564bafe0bde1cc3d5097b20f2d0a35f269b3bc7883dc',
    FAILED_STATE: '4324064f5c5caa87bcec39c0e1774c983df0dbb0c57d10268d87f69b08865da7',
    OLD_PROVIDER: 'b907bd5bffbb473c1af729a281ee7fb57779b69c498424baf1923dd9c33c91f1',
}
EPOCH = dt.date(1970, 1, 1)


def pin(path, expected):
    require(hashlib.sha256(path.read_bytes()).hexdigest() == expected,
            'pinned evidence changed: ' + str(path))


def bound_diagnostic(report, facts, months):
    """Require the known disagreement exactly; never reinterpret it as a pass."""
    require(report['pmo_pdf']['compared_days'] == 45991 and
            report['pmo_pdf']['matched_days'] == 45991 and
            report['pmo_pdf']['differences'] == [], 'all observed PMO PDF days must agree')
    require(report['decoded_widget_months'] == months, 'diagnostic decoded facts changed')
    hko = report['hko']
    require(hko['compared_days'] == 73048 and hko['matched_days'] == 73018 and
            hko['unknown_nominal_days'] == 31 and hko['unknown_leap_days'] == 91 and
            hko['uncovered_dates'] == [], 'HKO observation accounting changed')
    differences = hko['differences']
    expected = []
    for i in range(30):
        date = dt.date(2057, 9, 28) + dt.timedelta(days=i)
        expected.append({'date': date.isoformat(),
                         'fields': ['month', 'day'] if i == 0 else ['day'],
                         'widget': [2057, 8, 0, 30] if i == 0 else [2057, 9, 0, i],
                         'hko': [2057, 9, 0, i + 1]})
    require(differences == expected, 'exact 30 HKO future disagreements must remain explicit')
    for row in differences:
        require(tuple(row['widget']) == facts[dt.date.fromisoformat(row['date'])],
                'provider facts must retain published PMO side of disagreement')
    require(report['hko_missing_date'] == '2069-12-30' and
            report['continuous_hko_1901_2100_certified'] is False,
            'incomplete 2069 HKO source remains incomplete')
    return differences


def independent_day_oracle(source, prefix):
    """Separate direct numeric-word expansion for C field verification.

    The provider path consumes review.decode_numeric_array month records.
    This oracle computes ordinal and complete-year fields directly from each
    year's bit word. It does not inspect the generated provider C.
    """
    match = re.findall(r'\blunarInfo\s*:\s*\[(.*?)\]', source, re.S)
    require(len(match) == 1, 'oracle array inventory')
    body = re.sub(r'/\*.*?\*/|//[^\r\n]*', '', match[0], flags=re.S)
    require(re.fullmatch(r'\s*0x[0-9a-fA-F]+(?:\s*,\s*0x[0-9a-fA-F]+)*\s*,?\s*', body),
            'oracle numeric literal grammar')
    words = [int(s, 16) for s in re.findall(r'0x[0-9a-fA-F]+', body)]
    require(len(words) == 201, 'oracle full published year inventory')
    first, end = window('chinese')
    oracle = []
    # Only the independently proved private 1899 prefix supplies year fields;
    # its M12 is the acquired PDF's 1900-01-01..01-30, never widget 1899 data.
    require(len(prefix) == 12 and prefix[-1].epoch_day == first and
            prefix[-1].length == 30 and prefix[0].epoch_day == -25892 and
            prefix[-1].epoch_day + 30 == -25537, '1899/PDF exact boundary')
    m = prefix[-1]
    for day in range(m.epoch_day, m.epoch_day + m.length):
        oracle.append((day, 1899, 12, 12, 0, day - m.epoch_day + 1,
                       day - prefix[0].epoch_day + 1, 30, 355, 12))
    day = -25537
    for year, word in enumerate(words, 1900):
        leap = word % 16
        lengths = [29 + ((word >> (16 - number)) & 1) for number in range(1, 13)]
        leap_length = 29 + ((word >> 16) & 1) if leap else 0
        days_in_year = sum(lengths) + leap_length
        year_start = day
        require(leap <= 12 and days_in_year in (353, 354, 355, 383, 384, 385),
                'oracle complete-year facts')
        for nominal, length in enumerate(lengths, 1):
            for is_leap, size in [(0, length)] + ([(1, leap_length)] if nominal == leap else []):
                ordinal = nominal + int(bool(leap) and (nominal > leap or bool(is_leap)))
                for number in range(1, size + 1):
                    if first <= day < end:
                        oracle.append((day, year, ordinal, nominal, is_leap, number,
                                       day - year_start + 1, size, days_in_year, 12 + bool(leap)))
                    day += 1
    require(day == 47875 and len(oracle) == end - first and
            [r[0] for r in oracle] == list(range(first, end)),
            'oracle mandatory window and terminal boundary')
    return oracle


def dangi_block(source):
    match = re.search(r'(static const QJSPublishedCalendarMonth dangi_months\[\].*?\n};)\nconst QJSLunisolarProvider',
                      source, re.S)
    require(match is not None, 'Dangi emitted block schema')
    return match[1]


def prove_full_integration():
    groups, pins = base.prove_integration()
    for path, expected in EXTRA_PINS.items():
        pin(path, expected)
    review.pinned_inputs()
    old_state = json.loads(FAILED_STATE.read_text())
    require(old_state['status'] == 'failed' and old_state['commands_drained'] is True,
            'original comparison must remain failed and drained')
    source = SCRIPT.read_text('utf-8')
    facts, decoded, terminal = review.decode_numeric_array(source)
    report = json.loads(REPORT.read_text())
    differences = bound_diagnostic(report, facts, decoded)
    # Re-read the independent PMO PDF parser rather than trusting its report.
    sys.path.insert(0, str(review.PARSERS))
    import prepare_pmo_months as pmo
    observed = review.strict_pmo_days(pmo)
    comparison = review.compare_rows('pmo_pdf', facts, observed)
    require(comparison == report['pmo_pdf'], 'fresh PDF observations differ from pinned comparison')
    old_chinese, old_spans, old_bounds = groups['chinese']
    primary = base.load_pmo(base.PMO)
    published = [Month(r['epoch_day'], r['lunar_year'], r['month'], bool(r['is_leap']), r['length']) for r in decoded]
    published_ids = {identity(r) for r in published}
    require(all(identity(r) in published_ids for r in primary if r.lunar_year >= 1900),
            'every retained historical PMO complete month must match published table')
    prefix = [r for r in old_chinese if r.lunar_year == 1899]
    require(len(prefix) == 12 and [r.month for r in prefix] == list(range(1, 13)) and
            all(not r.is_leap for r in prefix), 'private complete 1899 arithmetic prefix')
    require(identity(prefix[-1]) == identity(primary[0]) and
            prefix[-1].epoch_day + prefix[-1].length == published[0].epoch_day,
            'PMO PDF January 1900 exact published-table join')
    january = [(date, year, month, leap, number) for date, year, month, leap, number in observed
               if date < dt.date(1900, 1, 31)]
    require(january == [(dt.date(1900, 1, 1) + dt.timedelta(days=i), 1899, 12, 0, i + 1)
                        for i in range(30)], 'all initial 30 primary PDF days required')
    sentinel = Month((terminal - EPOCH).days, 2101, 1, False, 0)
    rows = prefix + published + [sentinel]
    prove_sequence(rows)
    require(len(rows) == 2499 and complete_years(rows) == list(range(1899, 2101)),
            'every complete year and exact terminal M01 required')
    first, mandatory_end = window('chinese')
    require(all(r.epoch_day + r.length <= first for r in prefix[:-1]),
            'private approximation must remain outside mandatory window')
    spans = [[first, published[0].epoch_day], [published[0].epoch_day, sentinel.epoch_day]]
    bounds = {'first_day': rows[0].epoch_day, 'end_day': sentinel.epoch_day,
              'mandatory_first_day': first, 'mandatory_end_day': mandatory_end,
              'entire_mandatory_window_primary_covered': True,
              'private_1899_approximation_months': 11,
              'next_m01_role': 'derived zero-length 2101 M01 boundary, not acquired primary 2101 year'}
    groups['chinese'] = (rows, spans, bounds)
    oracle = independent_day_oracle(source, prefix)
    # Independent directly expanded fields must exactly locate every native
    # month and full-year boundary, including the initial private year.
    by_day = {r[0]: r for r in oracle}
    for row in rows[:-1]:
        if row.epoch_day in by_day:
            expected = by_day[row.epoch_day]
            require((expected[1], expected[3], expected[4], expected[5], expected[7]) ==
                    (row.lunar_year, row.month, int(row.is_leap), 1, row.length),
                    'independent day oracle/provider month mismatch')
    encoded = encode(groups)
    require(dangi_block(encoded.decode('ascii')) == dangi_block(OLD_PROVIDER.read_text('ascii')),
            'accepted Dangi emitted C must remain byte-identical')
    all_pins = dict(pins)
    all_pins.update({str(path): value for path, value in EXTRA_PINS.items()})
    all_pins.update({str(path): value for path, value in review.PINS.items()})
    return groups, all_pins, oracle, differences


def encode(groups):
    source = base.encode(groups)
    notice = ('/* PMO-hosted lunarInfo factual sequence: Jea Yang (JJonline@JJonline.Cn),\n'
              ' * calendar.js version 1.0.3, 2014..2017. Numeric facts only.\n'
              ' * No retrieved JavaScript implementation or license claim is incorporated.\n'
              ' * PMO PDF covers initial January 1900; private 1899 prefix is approximate.\n'
              ' * HKO differs on 30 dates in 2057; exact evidence retained in provenance. */\n')
    return notice.encode('ascii') + source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output_dir.exists(), 'fresh output directory required')
    groups, pins, oracle, differences = prove_full_integration()
    source = encode(groups)
    text = ''.join(' '.join(map(str, row)) + '\n' for row in oracle).encode('ascii')
    audit = {
        'status': 'root-generated candidate; C/frontend acceptance remains separate',
        'input_sha256': pins, 'provider_sha256': hashlib.sha256(source).hexdigest(),
        'day_oracle_sha256': hashlib.sha256(text).hexdigest(), 'day_oracle_rows': len(oracle),
        'authority_policy': 'Exact Era5833 Chinese row names PMO-published data; HKO is corroboration, not substitution',
        'pmo_publication': 'Exact spec-linked PMO page hosts and actively uses third-party-attributed factual sequence',
        'pmo_authorship_claimed': False, 'separate_gb_certification_claimed': False,
        'upstream_attribution': 'Jea Yang (JJonline@JJonline.Cn), calendar.js v1.0.3, 2014..2017',
        'upstream_js_license': 'No license grant declared in cached script; no JS implementation copied',
        'calendars': {c: {'primary_spans': s, 'bounds': b, 'complete_arithmetic_years': complete_years(r)}
                      for c, (r, s, b) in groups.items()},
        'chinese_primary_roles': ['PMO PDF January1900', 'PMO-hosted numerical table 1900M01..2100M12'],
        'chinese_mandatory_gregorian_coverage': '1900-01-01..2100-12-31 inclusive',
        'dangi_mandatory_gregorian_coverage': '1900-01-01..2050-12-31 inclusive; emitted C unchanged',
        'historical_pmo_pdf': {'compared': 45991, 'matched': 45991, 'additional_initial_january_days': 30},
        'hko_comparison_status': 'original all-source comparison remains FAILED',
        'hko_known_differences': differences, 'hko_compared': 73048, 'hko_matched': 73018,
        'hko_missing_date': '2069-12-30', 'continuous_hko_1901_2100_certified': False,
        'discovery_authorization': False,
    }
    args.output_dir.mkdir()
    (args.output_dir / 'provider.c').write_bytes(source)
    (args.output_dir / 'chinese-day-oracle.tsv').write_bytes(text)
    (args.output_dir / 'provenance.json').write_text(json.dumps(audit, indent=2) + '\n')


if __name__ == '__main__':
    try:
        main()
    except (InvalidSource, OSError, UnicodeError, ValueError, KeyError, TypeError, IndexError) as error:
        print('FAIL CLOSED: ' + str(error), file=sys.stderr)
        sys.exit(1)
