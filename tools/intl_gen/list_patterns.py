"""CLDR48.2 ListFormat section40 extension; host Python3.6+, no runtime XML.

Original generator; source-only packet, caller owns verified inputs and blob
assembly. Data derived from Unicode CLDR48.2, see evidence/CLDR-LICENSE.
Context alternate templates follow ICU78.3 listformatter.cpp at commit
21d1eb0f306e1141c10931e914dfc038c06121da; see evidence/ICU-LICENSE.
"""
import re
import struct
import metadata as m
import locale_subset

SECTION_ID = 40
RECORD_WIDTH = 40
SCRIPT_SECTION_ID = 41
SCRIPT_RECORD_WIDTH = 8
SCHEMA_MINOR = 2
PARTS = ('2', 'start', 'middle', 'end')
VARIANTS = (('standard', 0, 0), ('standard-short', 0, 1),
            ('standard-narrow', 0, 2), ('or', 1, 0), ('or-short', 1, 1),
            ('or-narrow', 1, 2), ('unit', 2, 0), ('unit-short', 2, 1),
            ('unit-narrow', 2, 2))
VARIANT_NAMES = frozenset(v[0] for v in VARIANTS)
ALIAS_PATH = re.compile(r"\.\./listPattern(?:\[@type=['\"]([^'\"]+)['\"]\])?")
CONTEXT_NONE, SPANISH_AND, SPANISH_OR, HEBREW_AND = range(4)


def pattern(value):
    # Exact XML text: never strip, normalize, ASCII-convert, or erase bidi.
    m.require(isinstance(value, str) and bool(value), 'missing list pattern')
    m.require(all(ord(c) != 0 and not 0xd800 <= ord(c) <= 0xdfff for c in value),
              'invalid Unicode scalar in list pattern')
    m.require(value.count('{0}') == 1 and value.count('{1}') == 1,
              'list pattern requires each placeholder once')
    literals = value.replace('{0}', '').replace('{1}', '')
    m.require('{' not in literals and '}' not in literals,
              'unsupported list placeholder or brace')
    return value


def parse_inputs(inputs):
    """Iterable (logical manifest path, verified XML bytes), sorted internally.

    Only standard records enter this baseline. Alt records are deliberately
    ignored under the same standard-record policy as locale metadata.
    ↑↑↑ is inheritance; explicit ∅∅∅ is rejected rather than invented text.
    Whole-pattern source=locale aliases are resolved in the requesting locale,
    including aliases reached through an ancestor. Cross-locale and unknown
    XPath aliases fail explicitly.
    """
    locales = {}
    for path, data in sorted(inputs):
        m.require(path.startswith('common/main/') and path.endswith('.xml'),
                  'ListFormat input must be CLDR common/main XML')
        tag = path.rsplit('/', 1)[1][:-4]
        tag = 'root' if tag == 'root' else m.canonical_tag(tag)
        m.require(tag not in locales, 'duplicate list locale')
        root = m.parse_xml(data, 'ldml')
        current = {}
        for group in root.findall('./listPatterns/listPattern'):
            if group.get('alt') is not None:
                continue
            name = group.get('type', 'standard')
            m.require(name in VARIANT_NAMES and name not in current,
                      'unknown or duplicate list pattern type')
            values, alias = {}, None
            for element in group:
                if element.get('alt') is not None:
                    continue
                if element.tag == 'alias':
                    match = ALIAS_PATH.fullmatch(element.get('path', ''))
                    m.require(element.get('source') == 'locale' and match is not None,
                              'unsupported list alias source/path')
                    target = match.group(1) or 'standard'
                    m.require(target in VARIANT_NAMES and alias is None and not values,
                              'unknown/repeated/mixed list alias')
                    alias = target
                    continue
                m.require(element.tag == 'listPatternPart' and alias is None and not len(element),
                          'unsupported list pattern child/leaf alias')
                part = element.get('type', '')
                m.require(part in PARTS and part not in values,
                          'unknown or repeated list pattern part')
                value = element.text or ''
                if value == '\u2191\u2191\u2191':
                    values[part] = None
                else:
                    m.require(value != '\u2205\u2205\u2205', 'explicit missing list pattern')
                    values[part] = pattern(value)
            current[name] = (alias, values)
        locales[tag] = current
    m.require('root' in locales, 'missing root list patterns')
    return locales


def resolve(locales, parents, requested, variant, part):
    """Iterative alias/parent lookup, with a checked finite-state bound."""
    current, original = requested, requested
    visited = set()
    while current is not None:
        state = (current, variant)
        m.require(state not in visited, 'list pattern alias/parent cycle')
        visited.add(state)
        m.require(current in parents, 'list locale absent from metadata parent graph')
        alias, fields = locales.get(current, {}).get(variant, (None, {}))
        if alias is not None:
            variant, current = alias, original
            continue
        value = fields.get(part)
        if value is not None:
            return value
        current = parents[current]
    raise m.DataError('missing inherited list pattern: {}/{}/{}'.format(requested, variant, part))


def alternate(language, templates):
    """ICU78.3 context applies to exact pair/end patterns only.

    Context enum specifies its predicate; alternate rows contain all four
    complete templates, preserving start/middle and nonmatching pair/end.
    There is at most one alternate per base row.
    """
    triggers = (templates[0], templates[3])
    m.require(language != 'es' or not ('{0} y {1}' in triggers and '{0} o {1}' in triggers),
              'ambiguous Spanish list contexts')
    for languages, source, target, context in (
            (('es',), '{0} y {1}', '{0} e {1}', SPANISH_AND),
            (('es',), '{0} o {1}', '{0} u {1}', SPANISH_OR),
            (('he', 'iw'), '{0} \u05d5{1}', '{0} \u05d5-{1}', HEBREW_AND)):
        if language in languages and (templates[0] == source or templates[3] == source):
            return context, tuple(target if i in (0, 3) and value == source else value
                                  for i, value in enumerate(templates))
    return CONTEXT_NONE, None


def collect(inputs, metadata):
    """Use M05-M07's exact general parent graph, no guessed locale defaults."""
    locales = parse_inputs(inputs)
    parents = metadata['parents']
    m.require(set(locales) <= set(parents), 'list input outside metadata locale graph')
    rows = []
    for index, tag in enumerate(sorted(parents)):
        language = tag.split('-', 1)[0]
        for name, type_id, style_id in VARIANTS:
            templates = tuple(resolve(locales, parents, tag, name, part) for part in PARTS)
            rows.append((index, type_id, style_id, CONTEXT_NONE, templates))
            context, alternative = alternate(language, templates)
            if alternative is not None:
                rows.append((index, type_id, style_id, context, alternative))
    rows.sort(key=lambda row: row[:4])
    return {'rows': tuple(rows), 'hebrew_script': (), 'evidence': {
        'policy': 'standard records; exact UTF8 text; requesting-locale aliases; metadata general parents',
        'contexts': 'ICU78.3 predicates; Unicode18 Script=Hebrew ranges in section41',
        'service_coverage': 'candidate data only; no metadata coverage bits activated'}}


def parse_hebrew_script(data):
    m.require(isinstance(data, bytes), 'Scripts.txt must be verified bytes')
    try:
        text = data.decode('utf-8')
    except UnicodeError as exc:
        raise m.DataError('invalid Scripts.txt UTF8: {}'.format(exc))
    m.require(re.search(r'^# Scripts-18\.0\.0\.txt\s*$', text, re.M),
              'ListFormat requires Unicode18 Scripts.txt')
    ranges = []
    for line in text.splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        match = re.fullmatch(r'([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?\s*;\s*([A-Za-z_]+)', line)
        m.require(match is not None, 'malformed Scripts.txt record')
        if match.group(3) != 'Hebrew':
            continue
        first = int(match.group(1), 16)
        last = int(match.group(2) or match.group(1), 16)
        m.require(first <= last <= 0x10ffff and not (first <= 0xdfff and last >= 0xd800),
                  'invalid Hebrew scalar range')
        ranges.append((first, last))
    ranges.sort()
    m.require(ranges, 'missing Hebrew script ranges')
    merged = []
    for first, last in ranges:
        m.require(not merged or first > merged[-1][1], 'overlapping Hebrew script ranges')
        if merged and first == merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], last)
        else:
            merged.append((first, last))
    return tuple(merged)


def load_pinned(args, metadata, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    paths = locale_subset.main_paths(manifest, metadata)
    inputs = ((p, m.verified_input(args.cldr_dir, manifest, p, consumed, 'cldr', context)) for p in paths)
    result = collect(inputs, metadata)
    ucd = m.load_pin_manifest(args.ucd_manifest, m.UCD_MANIFEST_SHA256, context)
    result['hebrew_script'] = parse_hebrew_script(
        m.verified_input(args.ucd_dir, ucd, 'Scripts.txt', consumed, 'ucd', context))
    return result


def strings(data):
    return tuple(value for row in data['rows'] for value in row[4])


def encode(data, pool):
    """Caller must build m.UTF8StringPool once with metadata + strings(data).

    The reviewed 1.2 assembler admits section40 and minor2; do not submit this
    section to the frozen 1.1 encode_blob. pool.ref returns offset/UTF8 length.
    """
    result = []
    previous = None
    for index, type_id, style_id, context, templates in data['rows']:
        key = (index, type_id, style_id, context)
        m.require(previous is None or previous < key, 'unsorted/duplicate list rows')
        previous = key
        m.require(type_id in range(3) and style_id in range(3) and context in range(4),
                  'invalid list record discriminator')
        m.require(len(templates) == 4, 'invalid list template count')
        result.append(m.u32(index) + struct.pack('<BBBB', type_id, style_id, context, 0) +
                      b''.join(pool.ref(pattern(value)) for value in templates))
    ranges = data.get('hebrew_script', ())
    m.require(ranges or not any(row[3] == HEBREW_AND for row in data['rows']),
              'Hebrew contextual rows require Script data')
    previous = -1
    encoded_ranges = []
    for first, last in ranges:
        m.require(previous < first <= last <= 0x10ffff and
                  not (first <= 0xdfff and last >= 0xd800), 'invalid Hebrew script range')
        previous = last
        encoded_ranges.append(m.u32(first) + m.u32(last))
    return {SECTION_ID: b''.join(result), SCRIPT_SECTION_ID: b''.join(encoded_ranges)}
