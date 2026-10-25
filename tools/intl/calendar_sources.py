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

"""Shared LDML calendar containers; typed leaves and alias mappings have owners."""
from pathlib import Path
import re

from cldr_xml import DataError, read_xml
import parents

WIDTHS = ('full', 'long', 'medium', 'short')
CALENDAR = re.compile(r'[a-z][a-z0-9]*(?:-[a-z0-9]+)*')
CALENDAR_ALIAS = re.compile(r"\.\./calendar\[@type=(['\"])([a-z0-9-]+)\1\]")
CALENDAR_CHILDREN = frozenset((
    'months', 'monthNames', 'monthAbbr', 'monthPatterns', 'dayOfMonths',
    'days', 'dayNames', 'dayAbbr', 'quarters', 'week', 'am', 'pm',
    'dayPeriods', 'eras', 'cyclicNameSets', 'dateFormats', 'timeFormats',
    'dateTimeFormats', 'fields',
))


def children(schema, node, selectors=()):
    attributes = schema.attributes(node)
    if any(name not in selectors and schema.fields[node.tag][name].role != 'metadata'
           for name in attributes):
        raise DataError('unsupported calendar container selector')
    if (node.text and node.text.strip(' \t\r\n') or
            any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
        raise DataError('unexpected calendar container text')
    return attributes, [child for child in node if child.tag != 'special']


def alias(schema, nodes):
    targets = [node for node in nodes if node.tag == 'alias']
    if not targets:
        return None
    node = targets[0]
    attributes = schema.attributes(node)
    if (len(nodes) != 1 or len(node) or set(attributes) != {'source', 'path'} or
            node.text and node.text.strip(' \t\r\n')):
        raise DataError('mixed/unsupported calendar alias')
    source = attributes['source']
    return attributes['path'], None if source == 'locale' else parents.locale_id(source)


def calendars(sources, schema):
    """Yield (locale, calendar, children) without interpreting typed subtrees.

    Callers consume each yielded list before advancing; these XML nodes are
    transient host inputs, never retained in the generated data model.
    """
    for filename in sorted(sources.paths('common/main/')):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        dates = root.findall('dates')
        if len(dates) > 1:
            raise DataError('repeated calendar dates container')
        if not dates:
            continue
        nodes = children(schema, dates[0])[1]
        if any(node.tag == 'alias' for node in nodes):
            raise DataError('dates alias requires reviewed calendar mapping')
        groups = [node for node in nodes if node.tag == 'calendars']
        if len(groups) > 1:
            raise DataError('repeated calendars container')
        seen = set()
        for group in groups:
            for node in children(schema, group)[1]:
                if node.tag != 'calendar':
                    raise DataError('unknown calendars child/alias')
                attributes, nodes = children(schema, node, ('type',))
                calendar = attributes.get('type', '')
                if not CALENDAR.fullmatch(calendar) or calendar in seen:
                    raise DataError('invalid/repeated calendar')
                seen.add(calendar)
                yield locale, calendar, nodes
