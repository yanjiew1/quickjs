"""Compile compact-number divisors and typed sign/affix forms from LDML."""
import argparse
from functools import lru_cache
import json
from pathlib import Path

import acquire
from cldr_xml import DataError
import compact_patterns
import number_pattern_syntax as syntax
from resolve import BLOCK

SCALED, LITERAL, FALLBACK = range(3)


@lru_cache(maxsize=8192)
def parse(value):
    if (not isinstance(value, str) or not value or
            any(not ord(char) or 0xd800 <= ord(char) <= 0xdfff for char in value)):
        raise DataError('compact pattern must be nonempty scalar text')
    pieces = syntax.characters(value)
    numeric = any(not quoted and char in '#0123456789@.,*' for char, quoted in pieces[0])
    if numeric:
        settings, positive, negative, plus = syntax.compile_pattern(value)
        primary, secondary, minimum, places, low, high, point, exponent, sign = settings
        if (primary or secondary or minimum != places or low or high or point or exponent or sign):
            raise DataError('compact pattern requires consecutive zero placeholders')
        if any(kind in (syntax.PERCENT, syntax.PERMILLE)
               for form in (positive, negative, plus) for kind, text in form):
            raise DataError('compact patterns cannot introduce percent scaling')
        return places, positive, negative, plus
    positive = syntax.affix(pieces[0])
    negative = syntax.affix(pieces[1]) if len(pieces) == 2 else ((syntax.MINUS, ''),) + positive
    if any(kind in (syntax.PERCENT, syntax.PERMILLE)
           for form in (positive, negative) for kind, text in form):
        raise DataError('compact patterns cannot introduce percent scaling')
    if any(kind == syntax.MINUS for kind, text in negative):
        plus = tuple((syntax.PLUS if kind == syntax.MINUS else kind, text) for kind, text in negative)
    else:
        plus = positive
    return 0, positive, negative, plus


def compile_pattern(value, magnitude, default_exponent=None):
    if type(magnitude) is not int or not 0 <= magnitude <= 0xffffffff:
        raise DataError('compact magnitude outside uint32')
    zeros, positive, negative, plus = parse(value)
    if value == '0':
        return FALLBACK, 0, zeros, positive, negative, plus
    if zeros:
        exponent = magnitude - zeros + 1
        if not 0 <= exponent <= magnitude:
            raise DataError('compact divisor outside magnitude')
        return SCALED, exponent, zeros, positive, negative, plus
    if type(default_exponent) is not int or not 0 <= default_exponent <= magnitude:
        raise DataError('literal compact pattern lacks a baseline divisor')
    return LITERAL, default_exponent, 0, positive, negative, plus


def compile_map(entries):
    fields = {(magnitude, count, variant): value for magnitude, count, variant, value in entries}
    if len(fields) != len(entries):
        raise DataError('repeated compact syntax field')
    baselines = {}
    for magnitude in sorted({magnitude for magnitude, count, variant in fields}):
        value = fields.get((magnitude, 'other', 'standard'))
        if value is None or value in ('', BLOCK):
            raise DataError('compact magnitude lacks a usable other pattern')
        baselines[(magnitude, 'standard')] = compile_pattern(value, magnitude)
    for (magnitude, count, variant), value in fields.items():
        if count == 'other' and variant != 'standard' and value not in ('', BLOCK):
            baselines[(magnitude, variant)] = compile_pattern(value, magnitude)
    result = []
    for magnitude, count, variant, value in entries:
        if count not in compact_patterns.COUNTS or variant not in compact_patterns.VARIANTS:
            raise DataError('unknown compact syntax selector')
        if value in ('', BLOCK):
            compiled = value
        else:
            baseline = baselines.get((magnitude, variant))
            compiled = compile_pattern(value, magnitude, None if baseline is None else baseline[1])
            # Preserve each field's divisor. In the pinned data, inherited
            # currency alternates can differ from the baseline, and a plural
            # form can explicitly request ordinary formatting with "0".
            # Choosing a coherent notation policy is the consumer's job.
        result.append((magnitude, count, variant, compiled))
    return tuple(result)


def compile_inventory(data):
    return tuple(compile_map(entries) for entries in data['maps'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = compact_patterns.collect(sources)
    compiled = compile_inventory(data)
    print(json.dumps({'release': sources.manifest['release'], 'maps': len(compiled),
                      'fields': sum(len(entries) for entries in compiled),
                      'literal_fields': sum(value[0] == LITERAL for entries in compiled
                                            for magnitude, count, variant, value in entries
                                            if value not in ('', BLOCK))}, sort_keys=True, indent=2))
