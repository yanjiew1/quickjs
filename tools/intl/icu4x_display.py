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

"""Project only display facts absent from ICU4X public name data.

Resolve these small final label maps during generation. Runtime consumers need
no CLDR tree, general fallback engine, native pack or wire-format decoder.
Ordinary language/region/script and currency names remain owned by ICU4X.
"""
from pathlib import Path
import re

import availability
from cldr_xml import DataError, Schema, read_xml
import date_field_names
import display_names
from embed import WRAPPER_LICENSE
import locale_metadata
import parents
from resolve import BLOCK, Records

# Bit positions follow QJSIntlDisplayType. Language/region/script/calendar use
# standalone context; plain currency/date-field labels keep formatting context.
CONTEXTS = (('languages', 0), ('territory', 1), ('script', 2), ('keyValue', 4))
PUBLIC_TYPE = re.compile(r'[A-Za-z0-9]{3,8}(?:-[A-Za-z0-9]{3,8})*')


def context_flags(sources, graph):
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records = Records(graph, ((usage,) for usage, bit in CONTEXTS))
    for filename in sorted(sources.paths('common/main/')):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        groups = root.findall('contextTransforms')
        if len(groups) > 1:
            raise DataError('repeated contextTransforms')
        for group in groups:
            seen = set()
            if schema.attributes(group):
                raise DataError('unsupported contextTransforms attributes')
            for usage in group:
                fields = schema.attributes(usage)
                if usage.tag != 'contextTransformUsage' or set(fields) - {'type', 'draft', 'references'}:
                    raise DataError('unsupported contextTransformUsage')
                key = fields.get('type')
                if key not in dict(CONTEXTS):
                    continue
                if key in seen:
                    raise DataError('repeated contextTransformUsage')
                seen.add(key)
                for leaf in usage:
                    fields = schema.attributes(leaf)
                    if leaf.tag != 'contextTransform' or set(fields) - {'type', 'draft', 'references'}:
                        raise DataError('unsupported contextTransform')
                    if fields.get('type') != 'stand-alone':
                        continue
                    value = leaf.text or ''
                    if len(leaf) or value not in ('titlecase-firstword', 'no-change', BLOCK, '\u2191\u2191\u2191'):
                        raise DataError('invalid contextTransform value')
                    records.add(locale, (key,), value)
    output = {}
    for locale in graph.tags:
        mask = 0
        for usage, bit in CONTEXTS:
            record = records.resolve(locale, (usage,))
            if record is not None and not record.blocked and record.value == 'titlecase-firstword':
                mask |= 1 << bit
        output[locale] = mask
    return output


def normalized(text):
    return text.lower().replace('_', '-')


def calendar_sources(metadata, names):
    calendar_maps = {index for origin, group, index, parent in names['nodes'] if group == 'calendar'}
    codes = {normalized(row[0]) for index in calendar_maps for row in names['maps'][index]
             if not row[2] and not row[3]}
    types = {row[2]: row for row in metadata['types'] if row[:2] == ('u', 'ca')}
    result = {}
    for name, row in sorted(types.items()):
        # CanonicalCodeForDisplayNames only lowercases a calendar code. Keep
        # valid aliases/deprecated names as lookup keys, without changing the
        # original code the engine returns for API fallback.
        public = {value.lower() for value in (name,) + tuple(row[3])
                  if PUBLIC_TYPE.fullmatch(value)}
        candidates, seen = set(), set()
        while True:
            if name in seen or name not in types:
                raise DataError('invalid calendar display alias chain: '+name)
            seen.add(name)
            row = types[name]
            if row[6]:
                break  # A variable pattern is not an actual calendar identity.
            candidates.update(normalized(value) for value in (name,) + tuple(row[3]))
            if not row[4]:
                matches = candidates & codes
                if len(matches) > 1:
                    raise DataError('ambiguous calendar display source: '+name+' '+repr(sorted(matches)))
                if matches:
                    source = next(iter(matches))
                    for code in public:
                        if code in result and result[code] != source:
                            raise DataError('conflicting calendar display source')
                        result[code] = source
                break
            name = row[4]
    return result


def name_maps(data, calendar_projection):
    rows = {(locale, group): head for locale, group, head in data['rows']}
    indexed = []
    for values in data['maps']:
        fields = {}
        for code, alt, menu, scope, text in values:
            key = normalized(code), alt, menu, scope
            if key in fields:
                raise DataError('display-name normalization collision: '+repr(key))
            fields[key] = text
        indexed.append(fields)
    script_maps = {index for origin, group, index, parent in data['nodes'] if group == 'script'}
    scripts = {normalized(code) for index in script_maps
               for code, alt, menu, scope, text in data['maps'][index]
               if alt == 'stand-alone' and not menu and not scope}

    def resolve(locale, group, code, style):
        alternatives = ((['short'] if style else []) +
                        (['stand-alone'] if group == 'script' else []) + [''])
        blocked = set()
        head = rows[locale, group]
        while head:
            origin, kind, index, head = data['nodes'][head - 1]
            for alt in alternatives:
                if alt in blocked:
                    continue
                text = indexed[index].get((code, alt, '', ''))
                if text is None:
                    continue
                if text == BLOCK:
                    blocked.add(alt)
                    continue
                return alt, text
        return None

    if any(len(code) != 4 or not all('a' <= c <= 'z' for c in code) for code in scripts):
        raise DataError('invalid standalone script code')
    output = {}
    for locale, group in rows:
        labels = []
        for style in range(3):  # Narrow shares short for these source name types.
            for code in sorted(scripts if group == 'script' else calendar_projection):
                source = code if group == 'script' else calendar_projection[code]
                chosen = resolve(locale, group, source, style)
                # Do not copy ordinary script names. Suppress an inherited
                # standalone alternative when a nearer ordinary value wins.
                if chosen is not None and (group != 'script' or chosen[0] == 'stand-alone'):
                    labels.append((code.title() if group == 'script' else code, style, chosen[1]))
        output[locale, group] = tuple(sorted(labels))
    return output


def collect(sources):
    graph = parents.load(sources)
    registry, aliases = availability.complete(graph)
    aliases = dict(aliases)
    names = display_names.collect(sources, graph, groups=('script', 'calendar'))
    metadata = locale_metadata.collect(sources, graph)
    maps = name_maps(names, calendar_sources(metadata, names))
    fields = date_field_names.collect(sources, graph)
    field_maps = {locale: [] for locale in graph.tags}
    for row in fields['rows']:
        (locale, field, width, alt, state, text, origin,
         source_field, source_width, source_alt) = row
        if not alt and state == 'value':
            field_maps[locale].append((field, fields['widths'].index(width), text))
    contexts = context_flags(sources, graph)
    rows = []
    for locale in registry.tags:
        source = aliases.get(locale, locale)
        rows.append((locale, maps[source, 'script'], maps[source, 'calendar'],
                     tuple(sorted(field_maps[source])), contexts[source]))
    return {'locales': tuple(rows)}


def c_string(text):
    if '\0' in text:
        raise DataError('NUL in display supplement')
    raw = text.encode('utf-8')
    # Three-digit octal bytes are independent of the compiler source/execution
    # charset and cannot consume a following decimal digit or hex letter.
    # Escaping '?' also prevents C trigraphs before string-literal parsing.
    return '"' + ''.join(chr(value) if 32 <= value < 127 and value not in (34, 63, 92)
                         else '\\%03o' % value for value in raw) + '\\000"'


def render(data):
    locales = data['locales']
    if (not locales or tuple(sorted(locales)) != tuple(locales) or
            len({row[0] for row in locales}) != len(locales)):
        raise DataError('unordered display locales')

    def compact(values):
        if tuple(sorted(values)) != values or len({row[:2] for row in values}) != len(values):
            raise DataError('unordered or duplicate display labels')
        labels = {}
        for code, style, text in values:
            if type(style) is not int or not 0 <= style <= 2:
                raise DataError('invalid display label style')
            labels[code, text] = labels.get((code, text), 0) | (1 << style)
        # Equal text across widths needs one record and three presence bits,
        # not repeated rows. A missing width still has no bit, including when
        # another width has an explicit empty label.
        return tuple(sorted((code, mask, text) for (code, text), mask in labels.items()))

    compiled = {tuple(group): compact(tuple(group)) for row in locales for group in row[1:4]}
    maps = tuple(sorted(set(compiled.values())))
    map_ids = {values: index for index, values in enumerate(maps)}
    strings = {'', data['version']}
    strings.update(row[0] for row in locales)
    for values in maps:
        for code, styles, text in values:
            strings.update((code, text))
    pool, length = {}, 0
    for text in sorted(strings):
        c_string(text)
        pool[text] = length
        length += len(text.encode('utf-8')) + 1
    if length > 0xffffffff or sum(map(len, maps)) > 0xffffffff:
        raise DataError('display supplement exceeds compiled field bounds')
    lines = [WRAPPER_LICENSE, '/* CLDR '+data['version']+'; data license: data/intl/LICENSE-CLDR. */',
             '#include "src/intl/display/icu4x-data.h"', '',
             'const char qjs_icu4x_display_strings[] =']
    lines.extend('    '+c_string(text) for text in sorted(pool))
    lines += ['    ;', '', 'const QJSICU4XDisplayLabel qjs_icu4x_display_labels[] = {']
    for values in maps:
        for code, styles, text in values:
            lines.append('    {'+', '.join(str(value) for value in (pool[code], pool[text], styles))+'},')
    if not any(maps):
        lines.append('    {0, 0, 0},')
    lines += ['};', '', 'const QJSICU4XDisplayMap qjs_icu4x_display_maps[] = {']
    at = 0
    for values in maps:
        lines.append('    {'+str(at)+', '+str(len(values))+'},')
        at += len(values)
    lines += ['};', '', 'const QJSICU4XDisplayLocale qjs_icu4x_display_locales[] = {']
    for tag, script, calendar, fields, flags in locales:
        if not isinstance(flags, int) or flags & ~23:
            raise DataError('invalid display context mask')
        indexes = ', '.join(str(map_ids[compiled[tuple(values)]])
                            for values in (script, calendar, fields))
        lines.append('    {'+str(pool[tag])+', {'+indexes+'}, '+str(flags)+'},')
    lines += ['};', '', 'const size_t qjs_icu4x_display_locale_count = '+str(len(locales))+';',
              'const uint32_t qjs_icu4x_display_version = '+str(pool[data['version']])+';']
    return '\n'.join(lines)+'\n'
