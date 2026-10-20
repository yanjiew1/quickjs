"""Typed string, locale and list-template sections of the QJIN native pack."""
import struct

import bundle
import calendar_name_tables
import compact_tables
import currency_tables
import timezone_name_tables
import timezone_presentation_tables
import date_interval_tables
import date_available_tables
import number_layout_tables
import unit_tables
import unit_grammar_tables
import relative_tables
import duration_tables
import date_field_tables
import date_style_tables
import date_combination_tables
import display_tables
import locale_tables
import numbering_tables
import number_symbol_tables
import number_pattern_tables
import preference_tables
import plural_tables
from list_patterns import PARTS, VARIANTS, pattern
from parents import locale_id
from resolve import BLOCK, INHERIT

U32 = struct.Struct('<I')
LOCALE = struct.Struct('<II')
ROOT = 0xffffffff
LIST_WIDTH = len(PARTS) * len(VARIANTS)
CONTEXT_WIDTH = len(VARIANTS) * 3


def words(values):
    return b''.join(U32.pack(bundle.uint32(value)) for value in values)


def read_words(data):
    if len(data) % 4:
        raise ValueError('misaligned Intl word table')
    return tuple(value[0] for value in struct.iter_unpack('<I', data))


def encode_strings(strings):
    if (not strings or strings[0] != '' or
            list(strings) != sorted(set(strings), key=lambda text: text.encode('utf-8'))):
        raise ValueError('Intl strings must be unique, sorted and start with empty')
    payload, offsets = bytearray(), [0]
    for text in strings:
        if '\0' in text:
            raise ValueError('NUL in Intl text')
        payload.extend(text.encode('utf-8'))
        offsets.append(len(payload))
    return len(strings), words(offsets) + payload


def decode_strings(section):
    count, data = section
    if not 1 <= count < len(data) // 4:
        raise ValueError('invalid Intl string index size')
    start = (count + 1) * 4
    offsets = read_words(data[:start])
    if offsets[0] or offsets[-1] != len(data) - start:
        raise ValueError('invalid Intl string pool extent')
    strings, previous = [], None
    for index in range(count):
        low, high = offsets[index:index + 2]
        if high < low or high > len(data) - start:
            raise ValueError('invalid Intl string extent')
        raw = data[start + low:start + high]
        if b'\0' in raw or previous is not None and raw <= previous:
            raise ValueError('noncanonical Intl string order/text')
        strings.append(raw.decode('utf-8'))
        previous = raw
    if strings[0] != '':
        raise ValueError('Intl string zero must be empty')
    return tuple(strings)


def encode_locales(tags, graph, string_ids):
    indices = {tag: index for index, tag in enumerate(tags)}
    rows = []
    for tag in tags:
        parent = graph.parent(tag)
        rows.append(LOCALE.pack(string_ids[tag], ROOT if parent is None else indices[parent]))
    return len(tags), b''.join(rows)


def decode_locales(section, strings):
    count, data = section
    if not count or count * LOCALE.size != len(data):
        raise ValueError('invalid Intl locale table extent')
    rows = tuple(struct.iter_unpack('<II', data))
    tags = []
    for name, parent in rows:
        if name >= len(strings) or parent != ROOT and parent >= count:
            raise ValueError('invalid Intl locale reference')
        tag = strings[name]
        if locale_id(tag) != tag or tags and tag <= tags[-1]:
            raise ValueError('noncanonical Intl locale identifier/order')
        if (tag == 'root') != (parent == ROOT):
            raise ValueError('Intl locale root/parent mismatch')
        tags.append(tag)
    if 'root' not in tags:
        raise ValueError('Intl locale registry lacks root')
    # Each state is finished once. Do not walk every complete chain again for
    # a corrupt long parent chain, which would make validation quadratic.
    done = set()
    for index in range(count):
        active = set()
        while index != ROOT and index not in done:
            if index in active:
                raise ValueError('Intl locale parent cycle')
            active.add(index)
            index = rows[index][1]
        done.update(active)
    return tuple(tags), tuple(parent for name, parent in rows)


def encode_lists(rows, tags, string_ids):
    fields = {(locale, variant): values for locale, variant, values in rows}
    vectors = tuple(tuple(string_ids[text] for variant in VARIANTS
                          for text in fields[(locale, variant)]) for locale in tags)
    unique = sorted(set(vectors))
    indices = {vector: index for index, vector in enumerate(unique)}
    return len(tags), words([len(unique)] + [indices[vector] for vector in vectors] +
                            [value for vector in unique for value in vector])


def decode_lists(section, strings, locale_count):
    count, data = section
    if count != locale_count or len(data) < 4:
        raise ValueError('invalid Intl list locale count')
    values = read_words(data)
    sets = values[0]
    if not 1 <= sets <= count or len(values) != 1 + count + sets * LIST_WIDTH:
        raise ValueError('invalid Intl list set extent')
    indices = values[1:1 + count]
    if set(indices) != set(range(sets)):
        raise ValueError('invalid or unused Intl list set')
    vectors, previous, checked = [], None, set()
    for offset in range(1 + count, len(values), LIST_WIDTH):
        vector = values[offset:offset + LIST_WIDTH]
        if any(value >= len(strings) for value in vector) or previous is not None and vector <= previous:
            raise ValueError('invalid/noncanonical Intl list string references')
        for value in vector:
            if value not in checked:
                if pattern(strings[value]) != strings[value] or strings[value] in (INHERIT, BLOCK):
                    raise ValueError('unresolved Intl list template')
                checked.add(value)
        previous = vector
        vectors.append(tuple(strings[value] for value in vector))
    return tuple(vectors[index] for index in indices)


def encode_contexts(rows, tags, string_ids):
    fields = {(locale, variant): (kind, string_ids[pair], string_ids[end])
              for locale, variant, kind, pair, end in rows}
    vectors = tuple(tuple(value for variant in VARIANTS for value in fields[(locale, variant)])
                    for locale in tags)
    unique = sorted(set(vectors))
    indices = {vector: index for index, vector in enumerate(unique)}
    return len(tags), words([len(unique)] + [indices[vector] for vector in vectors] +
                            [value for vector in unique for value in vector])


def decode_contexts(section, strings, locale_count):
    count, data = section
    if count != locale_count or len(data) < 4:
        raise ValueError('invalid Intl list context locale count')
    values = read_words(data)
    sets = values[0]
    if not 1 <= sets <= count or len(values) != 1 + count + sets * CONTEXT_WIDTH:
        raise ValueError('invalid Intl list context set extent')
    indices = values[1:1 + count]
    if set(indices) != set(range(sets)):
        raise ValueError('invalid or unused Intl list context set')
    vectors, previous, checked = [], None, set()
    for offset in range(1 + count, len(values), CONTEXT_WIDTH):
        vector = values[offset:offset + CONTEXT_WIDTH]
        if previous is not None and vector <= previous:
            raise ValueError('noncanonical Intl list context order')
        row = []
        for i in range(0, CONTEXT_WIDTH, 3):
            kind, pair, end = vector[i:i + 3]
            if kind > 3 or pair >= len(strings) or end >= len(strings):
                raise ValueError('invalid Intl list context fields')
            if kind == 0:
                if pair or end:
                    raise ValueError('unused Intl list context templates')
            else:
                for value in (pair, end):
                    if value not in checked:
                        if pattern(strings[value]) != strings[value] or strings[value] in (INHERIT, BLOCK):
                            raise ValueError('unresolved Intl context template')
                        checked.add(value)
            row.append((kind, strings[pair], strings[end]))
        previous = vector
        vectors.append(tuple(row))
    return tuple(vectors[index] for index in indices)


def validate(binary):
    result = bundle.unpack(binary)
    sections = result['sections']
    if bundle.STRINGS not in sections or bundle.LOCALES not in sections:
        raise ValueError('Intl pack lacks shared sections')
    strings = decode_strings(sections[bundle.STRINGS])
    tags, parents = decode_locales(sections[bundle.LOCALES], strings)
    lists = None
    if bundle.LISTS in sections:
        lists = decode_lists(sections[bundle.LISTS], strings, len(tags))
    contexts = None
    if bundle.LIST_CONTEXTS in sections:
        if lists is None:
            raise ValueError('Intl list contexts lack base templates')
        contexts = decode_contexts(sections[bundle.LIST_CONTEXTS], strings, len(tags))
    numbering = numbering_tables.decode(sections, strings)
    return {'envelope': result, 'strings': strings, 'locales': tags,
            'parents': parents, 'lists': lists, 'locale_metadata': locale_tables.decode(sections, strings),
            'preferences': preference_tables.decode(sections, strings), 'list_contexts': contexts,
            'numbering': numbering,
            'number_symbols': number_symbol_tables.decode(sections.get(bundle.NUMBER_SYMBOLS),
                                                          strings, tags, numbering),
            'plurals': plural_tables.decode(sections.get(bundle.PLURALS), strings, tags),
            'number_patterns': number_pattern_tables.decode(sections.get(bundle.NUMBER_PATTERNS),
                                                            strings, tags, numbering),
            'compact_patterns': compact_tables.decode(sections.get(bundle.COMPACT_PATTERNS),
                                                       strings, tags, numbering),
            'currencies': currency_tables.decode(sections.get(bundle.CURRENCIES), strings, tags),
            'number_layouts': number_layout_tables.decode(sections.get(bundle.NUMBER_LAYOUTS),
                                                        strings, tags, numbering),
            'unit_patterns': unit_tables.decode(sections.get(bundle.UNIT_PATTERNS), strings, tags),
            'unit_grammar': unit_grammar_tables.decode(sections.get(bundle.UNIT_GRAMMAR), strings, tags),
            'relative_patterns': relative_tables.decode(sections.get(bundle.RELATIVE_PATTERNS), strings, tags),
            'duration_patterns': duration_tables.decode(sections.get(bundle.DURATION_PATTERNS), strings, tags),
            'date_field_names': date_field_tables.decode(sections.get(bundle.DATE_FIELD_NAMES), strings, tags),
            'display_names': display_tables.decode(sections.get(bundle.DISPLAY_NAMES), strings, tags),
            'date_styles': date_style_tables.decode(sections.get(bundle.DATE_STYLES), strings, tags),
            'date_combinations': date_combination_tables.decode(sections.get(bundle.DATE_COMBINATIONS), strings, tags),
            'calendar_names': calendar_name_tables.decode(sections.get(bundle.CALENDAR_NAMES), strings, tags),
            'timezone_names': timezone_name_tables.decode(sections.get(bundle.TIMEZONE_NAMES), strings, tags),
            'timezone_presentation': timezone_presentation_tables.decode(
                sections.get(bundle.TIMEZONE_PRESENTATION), strings),
            'date_intervals': date_interval_tables.decode(sections.get(bundle.DATE_INTERVALS), strings, tags),
            'date_available_formats': date_available_tables.decode(sections.get(bundle.DATE_AVAILABLE_FORMATS),
                                                                  strings, tags)}
