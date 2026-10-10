"""Original CLDR49 RelativeTimeFormat section70/71 host extension.

Runtime never parses XML/JSON. Unicode data attribution: evidence/CLDR-LICENSE.
Standard alt-free records of every draft level; exact scalar UTF8 and bidi.
"""
import re
import struct
import metadata as m
import locale_subset

SCHEMA_MINOR = 3
WIDTHS = {70: 16, 71: 20}
UNITS = ('second', 'minute', 'hour', 'day', 'week', 'month', 'quarter', 'year')
STYLES = ('', '-short', '-narrow')
CATEGORIES = ('zero', 'one', 'two', 'few', 'many', 'other')
FIELDS = frozenset(unit + style for unit in UNITS for style in STYLES)
ALIAS = re.compile(r"\.\./field\[@type=['\"]([^'\"]+)['\"]\]")
INHERIT = '\u2191\u2191\u2191'
BLOCK = '\u2205\u2205\u2205'
MISSING = object()


def text(value, numeric):
    m.require(isinstance(value, str) and value, 'empty relative text')
    m.require(all(ord(c) and not 0xd800 <= ord(c) <= 0xdfff for c in value),
              'invalid relative Unicode scalar')
    if numeric:
        m.require(value.count('{0}') <= 1, 'relative pattern repeats {0}')
        literal = value.replace('{0}', '')
        m.require('{' not in literal and '}' not in literal, 'unknown relative placeholder')
    return value


def parse_inputs(inputs):
    locales = {}
    offsets = set()
    for path, data in sorted(inputs):
        m.require(path.startswith('common/main/') and path.endswith('.xml'), 'bad relative XML path')
        tag = path.rsplit('/', 1)[1][:-4]
        tag = 'root' if tag == 'root' else m.canonical_tag(tag)
        m.require(tag not in locales, 'duplicate relative locale')
        root = m.parse_xml(data, 'ldml')
        fields = {}
        for field in root.findall('./dates/fields/field'):
            name = field.get('type', '')
            if name not in FIELDS or field.get('alt') is not None:
                continue
            m.require(name not in fields, 'duplicate relative field')
            alias, values = None, {}
            for child in field:
                if child.get('alt') is not None:
                    continue
                if child.tag == 'alias':
                    match = ALIAS.fullmatch(child.get('path', ''))
                    m.require(child.get('source') == 'locale' and match is not None and
                              match.group(1) in FIELDS and alias is None and not values,
                              'unsupported relative field alias')
                    alias = match.group(1)
                elif child.tag == 'relative':
                    m.require(alias is None and not len(child), 'mixed/leaf relative alias')
                    offset = child.get('type', '')
                    m.require(re.fullmatch(r'-?(?:0|[1-9][0-9]*)', offset) is not None and
                              offset != '-0', 'bad relative literal key')
                    offset = int(offset)
                    m.require(-(1 << 31) <= offset < (1 << 31), 'relative offset exceeds int32')
                    key = ('literal', offset)
                    offsets.add(offset)
                    m.require(key not in values, 'duplicate relative literal')
                    value = child.text or ''
                    values[key] = None if value == INHERIT else value if value == BLOCK else text(value, False)
                elif child.tag == 'relativeTime':
                    m.require(alias is None and child.get('type') in ('past', 'future'), 'bad relative tense')
                    for leaf in child:
                        if leaf.get('alt') is not None:
                            continue
                        m.require(leaf.tag == 'relativeTimePattern' and not len(leaf) and
                                  leaf.get('count') in CATEGORIES, 'unsupported relativeTime child/alias')
                        key = (child.get('type'), leaf.get('count'))
                        m.require(key not in values, 'duplicate relative pattern')
                        value = leaf.text or ''
                        values[key] = None if value == INHERIT else value if value == BLOCK else text(value, True)
                elif child.tag not in ('displayName', 'relativePeriod'):
                    raise m.DataError('unknown relative field child: ' + child.tag)
            fields[name] = (alias, values)
        locales[tag] = fields
    m.require('root' in locales, 'missing relative root')
    return locales, tuple(sorted(offsets))


def resolve(locales, parents, requested, field, key):
    current = requested
    seen = set()
    while current is not None:
        state = (current, field)
        m.require(state not in seen, 'relative alias/parent cycle')
        seen.add(state)
        m.require(current in parents, 'relative locale outside parent graph')
        alias, values = locales.get(current, {}).get(field, (None, {}))
        if alias is not None:
            field, current = alias, requested
            continue
        value = values.get(key)
        if value == BLOCK:
            return MISSING
        if value is not None:
            return value
        current = parents[current]
    return MISSING


def collect(inputs, metadata):
    locales, offsets = parse_inputs(inputs)
    parents = metadata['parents']
    m.require(set(locales) <= set(parents), 'relative input outside locale metadata')
    numeric, literals = [], []
    for index, tag in enumerate(sorted(parents)):
        for unit, name in enumerate(UNITS):
            for style, suffix in enumerate(STYLES):
                field = name + suffix
                for tense, tense_name in enumerate(('past', 'future')):
                    other = resolve(locales, parents, tag, field, (tense_name, 'other'))
                    m.require(other is not MISSING, 'missing inherited relative other pattern')
                    for category, category_name in enumerate(CATEGORIES):
                        value = resolve(locales, parents, tag, field, (tense_name, category_name))
                        if value is MISSING:
                            value = other
                        numeric.append((index, unit, style, tense, category, value))
                for offset in offsets:
                    value = resolve(locales, parents, tag, field, ('literal', offset))
                    if value is not MISSING:
                        literals.append((index, unit, style, offset, value))
    return {'patterns': tuple(numeric), 'literals': tuple(literals), 'evidence': {
        'policy': 'exact CLDR49 scalar UTF8; standard alt-free all draft levels; requesting-locale field aliases; metadata general parents',
        'fallback': 'resolve category through parents, then selected field other; missing literal stays absent',
        'signed_number': 'pinned ECMA402 7ae78cf §18.5.2 retains signed Number for number insertion',
        'coverage': 'candidate data; provider/service coverage bits remain0'}}


def load_pinned(args, metadata, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    paths = locale_subset.main_paths(manifest, metadata)
    inputs = ((p, m.verified_input(args.cldr_dir, manifest, p, consumed, 'cldr', context)) for p in paths)
    return collect(inputs, metadata)


def strings(data):
    return tuple(row[-1] for group in ('patterns', 'literals') for row in data[group])


def encode(data, pool):
    patterns, literals = [], []
    previous = None
    for index, unit, style, tense, category, value in data['patterns']:
        key = (index, unit, style, tense, category)
        m.require(previous is None or previous < key, 'unordered relative patterns')
        previous = key
        m.require(unit in range(8) and style in range(3) and tense in range(2) and category in range(6),
                  'bad relative pattern discriminator')
        patterns.append(m.u32(index) + struct.pack('<BBBB', unit, style, tense, category) + pool.ref(text(value, True)))
    previous = None
    for index, unit, style, offset, value in data['literals']:
        key = (index, unit, style, offset)
        m.require(previous is None or previous < key, 'unordered relative literals')
        previous = key
        m.require(unit in range(8) and style in range(3) and -(1 << 31) <= offset < (1 << 31),
                  'bad relative literal discriminator')
        literals.append(m.u32(index) + struct.pack('<BBHi', unit, style, 0, offset) + pool.ref(text(value, False)))
    return {70: b''.join(patterns), 71: b''.join(literals)}
