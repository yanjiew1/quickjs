# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
# Copyright (c) 2026 Yan-Jie Wang
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

"""Compile missing NumberFormat facts from matching CLDR data.

Public ICU4X decimal data supplies digits/grouping/separators where present.
A small complete CLDR numeric fallback covers mandatory systems it omits.
Its high-level currency APIs re-round, and pattern-only APIs discard semantic
sign/percent token distinctions. Retain typed outer affixes and missing
nonfinite/exponent/range symbols from matching CLDR, using existing collectors.
"""
import availability
import currencies
from cldr_xml import DataError
from embed import WRAPPER_LICENSE, c_string
import number_layout
import number_patterns
import number_pattern_syntax as syntax
import number_symbols
import numbering
import parents
from resolve import BLOCK

SYMBOLS = ('percentSign', 'plusSign', 'minusSign', 'approximatelySign',
           'exponential', 'perMille', 'infinity', 'nan', 'decimal', 'group')
LAYOUTS = ('range', 'approximately') + tuple(
    side + '-' + field for side in ('beforeCurrency', 'afterCurrency')
    for field in ('currencyMatch', 'surroundingMatch', 'insertBetween'))
FIELD_COUNT = len(SYMBOLS) + len(LAYOUTS)
PATTERN_COUNT = len(number_patterns.FIELDS)
ABSENT, BLOCKED, EMPTY = 0xffffffff, 0xfffffffe, 0xfffffffd


def collect(sources):
    graph = parents.load(sources)
    registry, aliases = availability.complete(graph)
    aliases = dict(aliases)
    symbols = number_symbols.collect(sources, graph)
    patterns = number_patterns.collect(sources, graph)
    layouts = number_layout.collect(sources, graph)
    systems = symbols['systems']
    if systems != patterns['systems'] or systems != layouts['systems']:
        raise DataError('number supplement system mismatch')
    symbol_rows = {(locale, system): tuple(row[symbols['fields'].index(key)] for key in SYMBOLS)
                   for locale, system, row in symbols['rows']}
    pattern_rows = {(locale, system): row for locale, system, row in patterns['rows']}
    layout_rows = {(locale, system): tuple(row[layouts['fields'].index(key)] for key in LAYOUTS)
                   for locale, system, row in layouts['rows']}
    definitions = numbering.definitions(sources)
    grouping = dict(patterns['grouping'])
    overrides = currencies.collect(sources, graph, selected_fields=('decimal', 'group', 'pattern'))
    override_maps = {}
    for locale, index in overrides['rows']:
        codes = {}
        for code, field, count, value in overrides['maps'][index]:
            codes.setdefault(code, {})[field] = value
        override_maps[locale] = tuple(
            (code, fields.get('decimal'), fields.get('group'), fields.get('pattern'))
            for code, fields in sorted(codes.items()))
    rows = []
    for locale in registry.tags:
        source = aliases.get(locale, locale)
        rows.append((locale, tuple((symbol_rows[source, system][:8] + layout_rows[source, system][:2] +
                                    symbol_rows[source, system][8:] + layout_rows[source, system][2:],
                                    pattern_rows[source, system],
                                    syntax.compile_pattern(pattern_rows[source, system][0])[0][:2] +
                                    (grouping[source],)) for system in systems)))
    return {'systems': systems, 'digits': tuple(tuple(map(ord, definitions[name][1]))
                                               for name in systems), 'locales': tuple(rows),
            'currencies': {locale: override_maps[aliases.get(locale, locale)]
                           for locale in registry.tags}}


def render(data):
    systems, locales = data['systems'], data['locales']
    if (not systems or tuple(sorted(set(systems))) != systems or not locales or
            tuple(sorted(row[0] for row in locales)) != tuple(row[0] for row in locales) or
            len({row[0] for row in locales}) != len(locales)):
        raise DataError('unordered/repeated numeric identities')
    digits = data['digits']
    if len(digits) != len(systems) or any(
            len(row) != 10 or len(set(row)) != 10 or
            any(type(value) is not int or not 0 < value <= 0x10ffff or
                0xd800 <= value <= 0xdfff for value in row) for row in digits):
        raise DataError('invalid numeric fallback digits')
    records = set()
    raw_patterns = set()
    strings = {'', data['version']} | set(systems) | {tag for tag, rows in locales}
    for locale, rows in locales:
        if len(rows) != len(systems):
            raise DataError('numeric locale/system extent mismatch')
        for fields, patterns, grouping in rows:
            if len(fields) != len(SYMBOLS) + len(LAYOUTS) or len(patterns) != len(number_patterns.FIELDS):
                raise DataError('numeric record width mismatch')
            if (len(grouping) != 3 or any(type(value) is not int or not 0 <= value <= 9
                                          for value in grouping) or not grouping[2] or
                    (bool(grouping[0]) != bool(grouping[1]))):
                raise DataError('invalid numeric fallback grouping')
            strings.update(text for text in fields if text not in (None, BLOCK))
            raw_patterns.update(text for text in patterns if text not in (None, BLOCK, ''))
    currency_maps = data['currencies']
    if set(currency_maps) != {tag for tag, rows in locales}:
        raise DataError('numeric currency source coverage mismatch')
    for rows in currency_maps.values():
        if (tuple(sorted(row[0] for row in rows)) != tuple(row[0] for row in rows) or
                len({row[0] for row in rows}) != len(rows)):
            raise DataError('unordered/repeated currency override')
        for code, decimal, group, pattern in rows:
            if currencies.CODE.fullmatch(code) is None:
                raise DataError('invalid currency override code')
            strings.add(code)
            strings.update(value for value in (decimal, group) if value not in (None, BLOCK))
            if pattern not in (None, BLOCK, ''):
                raw_patterns.add(pattern)
    compiled = {}
    for text in sorted(raw_patterns):
        _, positive, negative, plus = syntax.compile_pattern(text)
        # ECMA digit/grouping and exponent rules own the numeric body. LDML
        # skeleton defaults (including E+00) must not override those rules.
        compiled[text] = (positive, negative, plus)
    patterns = tuple(sorted(set(compiled.values())))
    if not patterns:
        raise DataError('numeric supplement has no executable outer patterns')
    pattern_ids = {pattern: index for index, pattern in enumerate(patterns)}
    affixes = tuple(sorted({affix for pattern in patterns for affix in pattern}))
    starts, tokens = {}, []
    for affix in affixes:
        starts[affix] = len(tokens)
        tokens.extend(affix)
        strings.update(text for kind, text in affix)
    pool, length = {}, 0
    for text in sorted(strings):
        c_string(text)
        pool[text] = length
        length += len(text.encode('utf-8')) + 1
    if length >= EMPTY:
        raise DataError('numeric string pool exceeds field bounds')

    def text_id(text):
        return ABSENT if text is None else BLOCKED if text == BLOCK else pool[text]

    def pattern_id(text):
        if text is None:
            return ABSENT
        if text == BLOCK:
            return BLOCKED
        return EMPTY if text == '' else pattern_ids[compiled[text]]

    mapped = {}
    for locale, rows in locales:
        mapped[locale] = tuple(tuple(text_id(text) for text in fields) +
                               tuple(pattern_id(text) for text in fields_patterns) + grouping
                               for fields, fields_patterns, grouping in rows)
        records.update(mapped[locale])
    records = tuple(sorted(records))
    record_ids = {record: index for index, record in enumerate(records)}
    vectors = {locale: tuple(record_ids[record] for record in rows) for locale, rows in mapped.items()}
    unique_vectors = tuple(sorted(set(vectors.values())))
    vector_ids = {vector: index for index, vector in enumerate(unique_vectors)}
    if max(len(records), len(tokens), len(unique_vectors) * len(systems)) >= EMPTY:
        raise DataError('numeric tables exceed field bounds')
    mapped_currencies = {
        locale: tuple((pool[code], text_id(decimal), text_id(group), pattern_id(pattern))
                      for code, decimal, group, pattern in rows)
        for locale, rows in currency_maps.items()}
    unique_currencies = tuple(sorted(set(mapped_currencies.values())))
    currency_ids = {row: index for index, row in enumerate(unique_currencies)}
    if sum(map(len, unique_currencies)) >= EMPTY:
        raise DataError('numeric currency maps exceed field bounds')
    lines = [WRAPPER_LICENSE, '/* CLDR '+data['version']+'; data license: data/intl/LICENSE-CLDR. */',
             '#include "src/intl/number/icu4x-data.h"', '',
             'const char qjs_icu4x_number_strings[] =']
    lines.extend('    '+c_string(text) for text in sorted(pool))
    lines += ['    ;', '']

    def array(kind, name, rows):
        lines.append('const '+kind+' '+name+'[] = {')
        lines.extend('    '+row+',' for row in rows)
        lines.extend(('};', ''))

    array('uint32_t', 'qjs_icu4x_number_systems', [str(pool[system]) for system in systems])
    lines.append('const uint32_t qjs_icu4x_number_digits[][10] = {')
    lines.extend('    {'+', '.join(map(str, row))+'},' for row in digits)
    lines.extend(('};', ''))
    array('QJSICU4XNumberToken', 'qjs_icu4x_number_tokens',
          ['{'+str(kind)+', '+str(pool[text])+'}' for kind, text in tokens])
    array('QJSICU4XNumberPattern', 'qjs_icu4x_number_patterns', [
        '{{'+', '.join('{'+str(starts[affix])+', '+str(len(affix))+'}'
                          for affix in pattern)+'}}'
        for pattern in patterns])
    array('QJSICU4XNumberRecord', 'qjs_icu4x_number_records', [
        '{{'+', '.join(map(str, row[:FIELD_COUNT]))+'}, {'+', '.join(map(str, row[FIELD_COUNT:FIELD_COUNT+PATTERN_COUNT]))+
        '}, {'+', '.join(map(str, row[FIELD_COUNT+PATTERN_COUNT:]))+'}}' for row in records])
    array('uint32_t', 'qjs_icu4x_number_rows', [
        ', '.join(map(str, row)) for row in unique_vectors])
    array('QJSICU4XNumberCurrency', 'qjs_icu4x_number_currencies', [
        '{'+', '.join(map(str, row))+'}' for group in unique_currencies for row in group] or ['{0, 0, 0, 0}'])
    currency_starts, start = [], 0
    for rows in unique_currencies:
        currency_starts.append('{'+str(start)+', '+str(len(rows))+'}')
        start += len(rows)
    array('QJSICU4XNumberCurrencyMap', 'qjs_icu4x_number_currency_maps', currency_starts)
    array('QJSICU4XNumberLocale', 'qjs_icu4x_number_locales', [
        '{'+str(pool[locale])+', '+str(vector_ids[vectors[locale]] * len(systems))+', '+
        str(currency_ids[mapped_currencies[locale]])+'}'
        for locale, rows in locales])
    lines += ['const uint32_t qjs_icu4x_number_version = '+str(pool[data['version']])+';',
              'const size_t qjs_icu4x_number_system_count = '+str(len(systems))+';',
              'const size_t qjs_icu4x_number_locale_count = '+str(len(locales))+';']
    return '\n'.join(lines)+'\n'
