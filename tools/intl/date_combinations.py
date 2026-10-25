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

"""Resolve date/time style connectors without choosing formatter fallbacks."""
import argparse
import json
from pathlib import Path
import re

import acquire
import calendar_sources
from calendar_sources import CALENDAR_ALIAS, CALENDAR_CHILDREN, WIDTHS
from cldr_xml import DataError, Schema
import date_pattern_syntax
import parents
from resolve import BLOCK, INHERIT, Records

FORMS = ('standard', 'atTime', 'relative')
GROUP_ALIAS = re.compile(r"\.\./\.\./calendar\[@type=(['\"])([a-z0-9-]+)\1\]/dateTimeFormats")
FORM_ALIAS = re.compile(r"\.\./dateTimeFormat\[@type=(['\"])(standard|atTime|relative)\1\]")
OTHER_CALENDAR_CHILDREN = CALENDAR_CHILDREN - {'dateTimeFormats'}
OTHER_FORMAT_CHILDREN = frozenset(('availableFormats', 'appendItems', 'intervalFormats',
                                  'numericSeparators'))


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, calendars, alternates = [], [], set(), {''}

    def children(node, selectors=()):
        return calendar_sources.children(schema, node, selectors)

    def alias(nodes):
        return calendar_sources.alias(schema, nodes)

    # None width/form means an alias over that whole typed dimension. Expand
    # only after discovering the complete alternate inventory.
    for locale, calendar, nodes in calendar_sources.calendars(sources, schema):
        calendars.add(calendar)
        target = alias(nodes)
        if target:
            match = CALENDAR_ALIAS.fullmatch(target[0])
            if not match:
                raise DataError('unsupported combination calendar alias')
            aliases.append((locale, calendar, None, None, match.group(2), None, target[1]))
            continue
        seen = False
        for group in nodes:
            if group.tag in OTHER_CALENDAR_CHILDREN:
                continue
            if group.tag != 'dateTimeFormats' or seen:
                raise DataError('unknown/repeated combination calendar child')
            seen = True
            lengths = children(group)[1]
            target = alias(lengths)
            if target:
                match = GROUP_ALIAS.fullmatch(target[0])
                if target[0] == '../dateTimeFormats':
                    target_calendar = calendar
                elif match:
                    target_calendar = match.group(2)
                else:
                    raise DataError('unsupported date/time combination group alias')
                aliases.append((locale, calendar, None, None, target_calendar, None, target[1]))
                continue
            seen_widths = set()
            for length in lengths:
                if length.tag in OTHER_FORMAT_CHILDREN:
                    continue
                if length.tag != 'dateTimeFormatLength':
                    raise DataError('unknown combination length child/alias')
                attributes, formats = children(length, ('type',))
                width = attributes.get('type')
                if width not in WIDTHS or width in seen_widths:
                    raise DataError('unknown/repeated combination length')
                seen_widths.add(width)
                seen_forms = set()
                for form in formats:
                    if form.tag != 'dateTimeFormat':
                        raise DataError('unknown combination format child/alias')
                    attributes, leaves = children(form, ('type',))
                    kind = attributes.get('type', 'standard')
                    if kind not in FORMS or kind in seen_forms:
                        raise DataError('unknown/repeated combination form')
                    seen_forms.add(kind)
                    target = alias(leaves)
                    if target:
                        match = FORM_ALIAS.fullmatch(target[0])
                        if not match:
                            raise DataError('unsupported date/time combination form alias')
                        aliases.append((locale, calendar, width, kind, calendar, match.group(2), target[1]))
                        continue
                    for leaf in leaves:
                        if leaf.tag != 'pattern' or len(leaf):
                            raise DataError('unknown/structured combination leaf')
                        attributes = schema.attributes(leaf)
                        if (any(name not in ('alt', 'numbers', 'type') and
                                schema.fields[leaf.tag][name].role != 'metadata' for name in attributes) or
                                attributes.get('type', 'standard') != 'standard'):
                            raise DataError('unsupported combination leaf selector')
                        alt = attributes.get('alt', '')
                        alternates.add(alt)
                        text = leaf.text or ''
                        if text and text not in (BLOCK, INHERIT):
                            date_pattern_syntax.compile_combination(text)
                        value = text if text in (BLOCK, INHERIT) else (text, attributes.get('numbers', ''))
                        raw.append((locale, (calendar, width, kind, alt), value))

    calendars, alternates = tuple(sorted(calendars)), tuple(sorted(alternates))
    keys = tuple((calendar, width, kind, alt) for calendar in calendars
                 for width in WIDTHS for kind in FORMS for alt in alternates)
    records = Records(graph, keys)
    for record in raw:
        records.add(*record)
    for locale, calendar, width, kind, target_calendar, target_form, source in aliases:
        for selected_width in WIDTHS if width is None else (width,):
            for selected_form in FORMS if kind is None else (kind,):
                for alt in alternates:
                    records.alias(locale, (calendar, selected_width, selected_form, alt),
                                  (target_calendar, selected_width,
                                   selected_form if target_form is None else target_form, alt), source)
    rows = []
    for locale in graph.tags:
        for key in keys:
            result = records.resolve(locale, key)
            if result is None:
                value = ('missing', '', '', '', '', '', '', '')
            else:
                value = (('blocked', '', '') if result.blocked else ('value',) + result.value)
                value += (result.locale,) + result.key
            rows.append((locale,) + key + value)
    return {'calendars': calendars, 'widths': WIDTHS, 'forms': FORMS,
            'alternates': alternates, 'rows': tuple(rows),
            'source_records': len(raw), 'source_aliases': len(aliases)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'calendars': data['calendars'], 'alternates': data['alternates'],
                      'source_records': data['source_records'],
                      'source_aliases': data['source_aliases']}, sort_keys=True, indent=2))
