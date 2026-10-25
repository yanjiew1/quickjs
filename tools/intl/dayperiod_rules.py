#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Collect supplemental local-clock rules, independently of translated names."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
from dayperiod_codes import CODES
from parents import locale_id

KINDS = ('format', 'selection')
FIXED = {'midnight': ('at', 0, 0), 'noon': ('at', 43200, 43200),
         'am': ('range', 0, 43200), 'pm': ('range', 43200, 86400)}


def boundary(text, before=False):
    """Reviewed HH:mm grammar; 24:00 is only an exclusive end."""
    if not re.fullmatch('[0-9]{2}:[0-9]{2}', text):
        raise DataError('invalid day-period clock boundary')
    hour, minute = (int(value) for value in text.split(':'))
    if minute > 59 or hour > 23 and not (before and hour == 24 and minute == 0):
        raise DataError('out-of-range day-period clock boundary')
    return hour * 3600 + minute * 60


def validate_rules(kind, rules):
    """Check typed facts; fixed points can overlap the variable partition."""
    if kind not in KINDS:
        raise DataError('unknown day-period rule kind')
    seen, segments = set(), []
    for code, form, start, end in rules:
        if code not in CODES or code in seen:
            raise DataError('unknown/repeated day-period code')
        seen.add(code)
        if (type(start) is not int or type(end) is not int or not 0 <= start < 86400 or
                not 0 <= end <= 86400 or start % 60 or end % 60):
            raise DataError('invalid day-period clock values')
        if code in FIXED:
            if (form, start, end) != FIXED[code]:
                raise DataError('invalid fixed day-period definition')
        elif form != 'range' or start == end:
            # Variable exact points use a deprecated, unreviewed source form.
            raise DataError('invalid/unreviewed variable day-period form')
        else:
            # hh without minutes must identify a unique 24-hour clock hour,
            # too. A twelve-hour interval with fractional-hour endpoints can
            # touch thirteen hour buckets even though hh:mm is unambiguous.
            unwrapped_end = end if end > start else end + 86400
            hours = (unwrapped_end - 1) // 3600 - start // 3600 + 1
            if kind == 'format' and hours > 12:
                raise DataError('ambiguous twelve-hour day-period interval')
            if start < end:
                segments.append((start, end))
            else:
                segments.append((start, 86400))
                if end:
                    segments.append((0, end))
    if segments:
        at = 0
        for start, end in sorted(segments):
            if start != at:
                raise DataError('overlapping/incomplete variable day-period partition')
            at = end
        if at != 86400:
            raise DataError('incomplete variable day-period partition')
    return tuple(sorted(rules))


def attributes(schema, node, allowed=()):
    fields = schema.attributes(node)
    if any(name not in allowed and schema.fields[node.tag][name].role != 'metadata' for name in fields):
        raise DataError('unhandled day-period attribute: ' + node.tag)
    return fields


def children(schema, node, allowed=()):
    fields = attributes(schema, node, allowed)
    if (node.text and node.text.strip(' \t\r\n') or
            any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
        raise DataError('unexpected day-period container text')
    return fields, tuple(node)


def collect(sources):
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/dayPeriods.xml', 'supplementalData')
    rows, kinds = {}, set()
    for group in children(schema, root)[1]:
        if group.tag in ('version', 'generation', 'cldrVersion'):
            continue
        if group.tag != 'dayPeriodRuleSet':
            raise DataError('unknown day-period source section')
        fields, groups = children(schema, group, ('type',))
        kind = fields.get('type', 'format')
        if kind not in KINDS or kind == 'format' and 'type' in fields or kind in kinds:
            raise DataError('unknown/repeated day-period ruleset')
        kinds.add(kind)
        for entry in groups:
            if entry.tag != 'dayPeriodRules':
                raise DataError('unknown day-period locale group')
            fields, items = children(schema, entry, ('locales',))
            tags = tuple(locale_id(value) for value in re.split('[ \t\r\n]+', fields['locales'].strip(' \t\r\n')))
            if len(set(tags)) != len(tags):
                raise DataError('repeated day-period locale')
            rules = []
            for item in items:
                if item.tag != 'dayPeriodRule':
                    raise DataError('unknown day-period rule')
                fields, nested = children(schema, item, ('type', 'at', 'from', 'before'))
                if nested:
                    raise DataError('structured day-period rule')
                if 'at' in fields and not ('from' in fields or 'before' in fields):
                    form, start = 'at', boundary(fields['at'])
                    end = start
                elif 'from' in fields and 'before' in fields and 'at' not in fields:
                    form, start, end = 'range', boundary(fields['from']), boundary(fields['before'], True)
                else:
                    raise DataError('invalid day-period endpoint combination')
                rules.append((fields['type'], form, start, end))
            rules = validate_rules(kind, rules)
            for tag in tags:
                if (kind, tag) in rows:
                    raise DataError('repeated day-period locale mapping')
                rows[kind, tag] = rules
    # Store explicit origins only. Runtime lookup must truncate the requested
    # locale structurally, never follow main-data exceptional parentLocales or
    # merge individual period codes from different source groups.
    return {'rows': tuple((kind, tag, rules) for (kind, tag), rules in sorted(rows.items()))}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    rows = collect(acquire.Sources(args.sources))['rows']
    print(json.dumps({kind: {'locales': sum(row[0] == kind for row in rows),
                            'rule_lists': len({row[2] for row in rows if row[0] == kind})}
                      for kind in KINDS}, sort_keys=True, indent=2))
