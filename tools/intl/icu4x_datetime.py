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

"""Prepare the matching-CLDR sources required by shared date-program semantics.

ICU4X's public semantic skeletons are not a complete ECMA-402 format catalog.
Retain original style/available/append/interval programs, precise day-period
rules and fallback name/zone facts using the existing source codecs. No native
collation, segmentation, list, number patterns, units or plural data is included.
"""
import hashlib
import availability
import bundle
import calendar_eras
import calendar_era_tables
import calendar_names
import calendar_name_tables
import date_available_formats
import date_available_tables
import date_combinations
import date_combination_tables
import date_intervals
import date_interval_tables
import date_patterns
import date_style_tables
import dayperiod_rules
import dayperiod_tables
import display_names
import display_tables
from embed import WRAPPER_LICENSE, byte_array, c_string
import locale_metadata
import locale_tables
import numbering
import numbering_tables
import number_symbols
import number_symbol_tables
import parents
import preferences
import preference_tables
import tables
import timezone_names
import timezone_name_tables
import timezone_presentation
import timezone_presentation_tables

# The shared date service needs these owners together. Each keeps its existing
# schema/validator; this map does not define another runtime data representation.
CATALOGS = (
    ('styles', date_patterns, date_style_tables, bundle.DATE_STYLES),
    ('combinations', date_combinations, date_combination_tables, bundle.DATE_COMBINATIONS),
    ('available', date_available_formats, date_available_tables, bundle.DATE_AVAILABLE_FORMATS),
    ('intervals', date_intervals, date_interval_tables, bundle.DATE_INTERVALS),
    ('names', calendar_names, calendar_name_tables, bundle.CALENDAR_NAMES),
    ('zones', timezone_names, timezone_name_tables, bundle.TIMEZONE_NAMES),
)


def collect(sources):
    graph, _ = availability.complete(parents.load(sources))
    metadata = locale_metadata.collect(sources, graph)
    result = dict(graph=graph, metadata=metadata,
                  preferences=preferences.collect(sources),
                  numbering=numbering.collect(sources, graph),
                  symbols=number_symbols.collect(sources, graph),
                  eras=calendar_eras.collect(sources),
                  periods=dayperiod_rules.collect(sources),
                  regions=display_names.collect(sources, graph, groups=('region',)),
                  presentation=timezone_presentation_tables.runtime_records(
                      timezone_presentation.collect(sources, metadata)))
    for key, collector, codec, section in CATALOGS:
        result[key] = codec.select(collector.collect(sources, graph), graph.tags)
    # The shared name codec has fixed group columns. Preserve empty columns,
    # rather than collecting unrelated language/script/calendar display data.
    regions = result['regions']
    heads = {(tag, group): head for tag, group, head in regions['rows']}
    regions['groups'] = display_names.GROUPS
    regions['rows'] = tuple((tag, group, heads.get((tag, group), 0))
                            for tag in graph.tags for group in display_names.GROUPS)
    return result


def encode(data):
    graph = data['graph']
    tags = graph.tags
    pool = {'', *tags}
    for key, codec in (
            ('metadata', locale_tables), ('preferences', preference_tables),
            ('numbering', numbering_tables), ('symbols', number_symbol_tables),
            ('eras', calendar_era_tables), ('periods', dayperiod_tables),
            ('regions', display_tables), ('presentation', timezone_presentation_tables)):
        pool.update(codec.strings(data[key]))
    for key, collector, codec, section in CATALOGS:
        pool.update(codec.strings(data[key]))
    strings = tuple(sorted(pool, key=lambda value: value.encode('utf-8')))
    ids = {value: index for index, value in enumerate(strings)}
    sections = {
        bundle.STRINGS: tables.encode_strings(strings),
        bundle.LOCALES: tables.encode_locales(tags, graph, ids),
        bundle.NUMBER_SYMBOLS: number_symbol_tables.encode(data['symbols'], tags,
                                                         data['numbering'], ids),
        bundle.CALENDAR_ERAS: calendar_era_tables.encode(data['eras'], ids),
        bundle.DAYPERIOD_RULES: dayperiod_tables.encode(data['periods'], ids),
        bundle.DISPLAY_NAMES: display_tables.encode(data['regions'], tags, ids),
        bundle.TIMEZONE_PRESENTATION: timezone_presentation_tables.encode(data['presentation'], ids),
    }
    sections.update(locale_tables.encode(data['metadata'], ids))
    sections.update(preference_tables.encode(data['preferences'], ids))
    sections.update(numbering_tables.encode(data['numbering'], ids))
    for key, collector, codec, section in CATALOGS:
        sections[section] = codec.encode(data[key], tags, ids)
    # Describes the supplied content, not an acquisition-pin authentication.
    identity = hashlib.sha256(data['version'].encode('ascii') + b''.join(
        raw for section, (count, raw) in sorted(sections.items()))).digest()
    binary = bundle.pack(sections, identity, 'native')
    tables.validate(binary)
    return binary


def render(data):
    lines = ['/* Generated ICU4X date-program companion; do not edit.',
             ' * Derived from matching Unicode CLDR; Copyright Unicode, Inc.',
             ' * See data/intl/LICENSE-CLDR and data/intl/pin.json.',
             ' */', '#include "src/intl/icu4x/datetime-data.h"', '',
             'const char qjs_icu4x_datetime_version[] = '+c_string(data['version'])+';', '']
    return WRAPPER_LICENSE + '\n'.join(lines + byte_array(encode(data), 'qjs_icu4x_datetime'))
