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

"""Tokenize LDML date/time patterns without selecting fields or formatting dates.

TR35 Dates at the pinned CLDR revision defines apostrophe quoting and reserves
ASCII letters for fields. Keep source symbols and run lengths intact: calendar
selection, legal field widths, skeleton matching and ECMA-402 option projection
belong to the consuming compiler, not this lexical layer.
"""
import argparse
from collections import namedtuple
import json
from pathlib import Path

import acquire
from cldr_xml import DataError, Schema, read_xml
from resolve import BLOCK, INHERIT

# Includes the deprecated pattern-only leap marker l. Input-skeleton-only
# j/J/C are deliberately absent; they must never appear in stored patterns.
FIELD_SYMBOLS = frozenset('GyYuUrQqMLlwWdDFgEecabBhHKkmsSAzZOvVXx')
Token = namedtuple('Token', 'symbol width text')
IntervalPattern = namedtuple('IntervalPattern', 'order tokens')
PATTERN_PATHS = (
    'dateFormats/dateFormatLength/dateFormat/pattern',
    'timeFormats/timeFormatLength/timeFormat/pattern',
    'dateTimeFormats/availableFormats/dateFormatItem',
    'dateTimeFormats/intervalFormats/intervalFormatItem/greatestDifference',
)


def compile_pattern(value):
    """Return immutable field/literal tokens, retaining order and repeated fields.

    Fields have their source symbol, positive width and empty text. Literals
    have an empty symbol, zero width and decoded nonempty scalar text. Adjacent
    literals are coalesced. Literal braces have no placeholder meaning here;
    date-time combination templates use a different grammar.
    """
    return _compile(value, False)


def compile_combination(value):
    """Compile a date/time combination with exactly one time and date slot.

    Slot tokens have symbol '0' (time) or '1' (date), zero width and empty
    text. Other tokens follow compile_pattern, including fields outside the
    slots. Apostrophe quoting applies to both fields and braces, so quoted
    '{0}' is literal text, not a slot. No style selection policy is applied.
    """
    return _compile(value, True)


def compile_interval(value):
    """Keep an optional interval ordering directive and unsplit pattern tokens.

    Empty order means use intervalFormatFallback's ordering. Repeated fields
    retain their original symbols: logical equivalence and the split between
    start/end pattern parts belong to the consumer.
    """
    if not isinstance(value, str):
        raise DataError('interval pattern must be scalar text')
    order = ''
    for name in ('earliestFirst', 'latestFirst'):
        if value.startswith(name + ':'):
            order, value = name, value[len(name) + 1:]
            break
    return IntervalPattern(order, compile_pattern(value))


def _compile(value, combination):
    if (not isinstance(value, str) or not value or
            any(not ord(char) or 0xd800 <= ord(char) <= 0xdfff for char in value)):
        raise DataError('date pattern must be nonempty scalar text')
    tokens, literal, quoted, index = [], [], False, 0
    slots = [0, 0]

    def flush():
        if literal:
            tokens.append(Token('', 0, ''.join(literal)))
            literal.clear()

    while index < len(value):
        char = value[index]
        if char == "'":
            if index + 1 < len(value) and value[index + 1] == "'":
                literal.append("'")
                index += 2
                continue
            quoted = not quoted
        elif combination and not quoted and value[index:index + 3] in ('{0}', '{1}'):
            marker = value[index:index + 3]
            slot = int(marker[1])
            slots[slot] += 1
            if slots[slot] != 1:
                raise DataError('repeated date/time combination slot')
            flush()
            tokens.append(Token(marker[1], 0, ''))
            index += 3
            continue
        elif not quoted and ('A' <= char <= 'Z' or 'a' <= char <= 'z'):
            if char not in FIELD_SYMBOLS:
                raise DataError('unknown or input-skeleton-only date field: ' + char)
            flush()
            end = index + 1
            while end < len(value) and value[end] == char:
                end += 1
            tokens.append(Token(char, end - index, ''))
            index = end
            continue
        else:
            literal.append(char)
        index += 1
    if quoted:
        raise DataError('unclosed date pattern quote')
    if combination and slots != [1, 1]:
        raise DataError('date/time combination requires both slots')
    flush()
    return tuple(tokens)


def audit_combinations(sources):
    """Audit raw style combination leaves, not aliases or style fallback.

    Exact stored forms/alternates and marker counts are retained; this does
    not validate typed source containers or establish formatter coverage.
    """
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    patterns, records, markers = {}, 0, 0
    path = 'dates/calendars/calendar/dateTimeFormats/dateTimeFormatLength/dateTimeFormat/pattern'
    for name in sorted(sources.paths('common/main/')):
        if not name.endswith('.xml'):
            continue
        root = read_xml(sources, name, 'ldml')
        for node in root.findall(path):
            schema.attributes(node)
            if len(node):
                raise DataError('structured date/time combination in ' + name)
            value = node.text or ''
            if value in (BLOCK, INHERIT):
                markers += 1
                continue
            if value not in patterns:
                try:
                    patterns[value] = compile_combination(value)
                except DataError as exc:
                    raise DataError('{}: {}: {}'.format(name, path, exc))
            records += 1
    return {'records': records, 'markers': markers, 'patterns': len(patterns),
            'tokens': sum(len(tokens) for tokens in patterns.values())}


def audit(sources):
    """Check raw stored pattern syntax, not inheritance or formatter coverage.

    Every alternate and calendar in the named paths is checked. Aliases and
    inheritance/block markers are not patterns. Combination/append templates
    are intentionally outside these paths. A typed collector must subsequently
    validate container structure, resolve aliases and retain source provenance.
    """
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    patterns, records, markers = {}, 0, 0
    for name in sorted(sources.paths('common/main/')):
        if not name.endswith('.xml'):
            continue
        root = read_xml(sources, name, 'ldml')
        for calendar in root.findall('dates/calendars/calendar'):
            for path in PATTERN_PATHS:
                for node in calendar.findall(path):
                    schema.attributes(node)
                    if len(node):
                        raise DataError('structured date pattern in ' + name)
                    value = node.text or ''
                    if value in (BLOCK, INHERIT):
                        markers += 1
                        continue
                    # Prefix directives are legal only for interval patterns.
                    # Cache by grammar too so a valid interval cannot admit
                    # the same prefixed text as an ordinary date pattern.
                    interval = node.tag == 'greatestDifference'
                    key = interval, value
                    if key not in patterns:
                        try:
                            patterns[key] = compile_interval(value).tokens if interval else compile_pattern(value)
                        except DataError as exc:
                            raise DataError('{}: {}: {}'.format(name, path, exc))
                    records += 1
    return {'records': records, 'markers': markers, 'patterns': len(patterns),
            'tokens': sum(len(tokens) for tokens in patterns.values()),
            'fields': ''.join(sorted({token.symbol for tokens in patterns.values()
                                     for token in tokens if token.symbol}))}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--pattern')
    inputs.add_argument('--combination')
    inputs.add_argument('--sources', type=Path)
    parser.add_argument('--combinations', action='store_true',
                        help='audit style combinations instead of date patterns with --sources')
    args = parser.parse_args()
    if args.combinations and args.sources is None:
        parser.error('--combinations requires --sources')
    if args.sources is not None:
        sources = acquire.Sources(args.sources)
        result = dict((audit_combinations if args.combinations else audit)(sources),
                      release=sources.manifest['release'])
    else:
        tokens = (compile_combination(args.combination) if args.combination is not None
                  else compile_pattern(args.pattern))
        result = [token._asdict() for token in tokens]
    print(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2))
