#!/usr/bin/env python3
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
    if (not isinstance(value, str) or not value or
            any(not ord(char) or 0xd800 <= ord(char) <= 0xdfff for char in value)):
        raise DataError('date pattern must be nonempty scalar text')
    tokens, literal, quoted, index = [], [], False, 0

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
    flush()
    return tuple(tokens)


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
                    if value not in patterns:
                        try:
                            patterns[value] = compile_pattern(value)
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
    inputs.add_argument('--sources', type=Path)
    args = parser.parse_args()
    if args.sources is not None:
        sources = acquire.Sources(args.sources)
        result = dict(audit(sources), release=sources.manifest['release'])
    else:
        result = [token._asdict() for token in compile_pattern(args.pattern)]
    print(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2))
