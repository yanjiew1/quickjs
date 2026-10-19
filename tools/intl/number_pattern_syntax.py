"""Compile standard LDML number patterns into typed affixes and numeric metadata.

This is a data compiler, not a user-facing DecimalFormat pattern interpreter.
Digit defaults remain available as metadata; ECMA-402 options govern rounding.
Unsupported padding, rounding-increment and significant-digit skeletons fail
explicitly rather than producing a formatter capability gap.
"""
import argparse
from functools import lru_cache
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError
import number_patterns
from resolve import BLOCK

LITERAL, NUMBER, MINUS, PLUS, PERCENT, PERMILLE = range(6)
CURRENCY, CURRENCY_ISO, CURRENCY_NAME, CURRENCY_NARROW = range(6, 10)
TOKENS = ('literal', 'number', 'minus', 'plus', 'percent', 'permille',
          'currency', 'currency-iso', 'currency-name', 'currency-narrow')
SPECIAL = {'-': MINUS, '+': PLUS, '%': PERCENT, '‰': PERMILLE}
CURRENCIES = {1: CURRENCY, 2: CURRENCY_ISO, 3: CURRENCY_NAME, 5: CURRENCY_NARROW}
SKELETON = re.compile(r'([#0,]+)(?:\.([#0]*))?(?:E(\+?)(0+))?')


def characters(value):
    """Split unquoted semicolons, retaining literal status after apostrophe rules."""
    pieces, current, quoted, index = [], [], False, 0
    while index < len(value):
        char = value[index]
        if char == "'":
            if index + 1 < len(value) and value[index + 1] == "'":
                current.append(("'", True))
                index += 2
                continue
            quoted = not quoted
        elif char == ';' and not quoted:
            pieces.append(current)
            current = []
        else:
            current.append((char, quoted))
        index += 1
    if quoted:
        raise DataError('unclosed number pattern quote')
    pieces.append(current)
    # LDML ignores trailing semicolons, but not whitespace after a semicolon.
    while len(pieces) > 1 and not pieces[-1]:
        pieces.pop()
    if not 1 <= len(pieces) <= 2 or not all(pieces):
        raise DataError('missing or excess number subpattern')
    return pieces


def token_append(output, kind, text=''):
    if kind == LITERAL and output and output[-1][0] == LITERAL:
        output[-1] = (LITERAL, output[-1][1] + text)
    else:
        output.append((kind, text))


def affix(chars):
    output, index = [], 0
    while index < len(chars):
        char, quoted = chars[index]
        if quoted:
            token_append(output, LITERAL, char)
        elif char in SPECIAL:
            token_append(output, SPECIAL[char])
        elif char == '¤':
            end = index + 1
            while end < len(chars) and chars[end] == ('¤', False):
                end += 1
            if end - index not in CURRENCIES:
                raise DataError('unsupported currency placeholder width')
            token_append(output, CURRENCIES[end - index])
            index = end
            continue
        elif char in '#0123456789@.,*':
            raise DataError('numeric syntax outside number skeleton')
        else:
            token_append(output, LITERAL, char)
        index += 1
    return tuple(output)


def subpattern(chars):
    first = next((i for i, (char, quoted) in enumerate(chars)
                  if not quoted and char in '#0@.,123456789*'), None)
    if first is None:
        raise DataError('missing number skeleton')
    text = ''.join(char for char, quoted in chars[first:] if not quoted)
    match = SKELETON.match(text)
    if match is None:
        raise DataError('unsupported standard number skeleton')
    end = first + match.end()
    # Quoted characters cannot disappear into the contiguous numeric part.
    if any(quoted for char, quoted in chars[first:end]):
        raise DataError('quoted text interrupts number skeleton')
    integer, fraction, exponent_plus, exponent = match.groups()
    groups = integer.split(',')
    digits = ''.join(groups)
    if (not all(groups) or re.fullmatch('#*0*', digits) is None or
            fraction is not None and re.fullmatch('0*#*', fraction) is None or
            exponent is not None and len(groups) > 1):
        raise DataError('invalid number digit/grouping sequence')
    primary = len(groups[-1]) if len(groups) > 1 else 0
    secondary = len(groups[-2]) if len(groups) > 2 else primary
    numeric = (primary, secondary, digits.count('0'), len(digits),
               (fraction or '').count('0'), len(fraction or ''),
               int(fraction is not None), len(exponent or ''), int(bool(exponent_plus)))
    tokens = affix(chars[:first]) + ((NUMBER, ''),) + affix(chars[end:])
    return numeric, tokens


@lru_cache(maxsize=4096)
def compile_pattern(value):
    if (not isinstance(value, str) or not value or
            any(not ord(char) or 0xd800 <= ord(char) <= 0xdfff for char in value)):
        raise DataError('number pattern must be nonempty scalar text')
    pieces = characters(value)
    numeric, positive = subpattern(pieces[0])
    negative = subpattern(pieces[1])[1] if len(pieces) == 2 else ((MINUS, ''),) + positive
    # Only negative affixes apply; its numeric skeleton never overrides positive
    # grouping or digit settings. Explicit-plus formatting substitutes all minus
    # tokens, including suffix signs, without touching quoted literal hyphens.
    if any(kind == MINUS for kind, text in negative):
        plus = tuple((PLUS if kind == MINUS else kind, text) for kind, text in negative)
    else:
        # LDML leaves positive unchanged when negative has no minus token.
        # A NumberFormat signDisplay policy may still need to insert a sign;
        # that is a service operation, not a source pattern transformation.
        plus = positive
    return numeric, positive, negative, plus


def compile_inventory(data):
    values = {value for locale, system, row in data['rows'] for value in row
              if value is not None and value != BLOCK and value != ''}
    return {value: compile_pattern(value) for value in sorted(values)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    patterns = compile_inventory(number_patterns.collect(sources))
    print(json.dumps({'release': sources.manifest['release'],
                      'compiled_patterns': len(patterns),
                      'affix_tokens': sum(len(row) for item in patterns.values() for row in item[1:])},
                     sort_keys=True, indent=2))
