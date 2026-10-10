"""CLDR48.2 exact plural rule/range sections for optional wire1.3.

Host generator source; no execution in this packet. Consumes manifest-verified
CLDR XML. Runtime reads packed bytes only. Unicode-3.0 attribution: NOTICE.txt.
"""
import re
import struct
import metadata as m

SCHEMA_MINOR = 3
WIDTHS = {30: 24, 31: 12, 32: 4}
CATEGORIES = ('zero', 'one', 'two', 'few', 'many', 'other')
CATEGORY = {name: index for index, name in enumerate(CATEGORIES)}
PATHS = ('common/supplemental/plurals.xml',
         'common/supplemental/ordinals.xml',
         'common/supplemental/pluralRanges.xml')
TOKEN = re.compile(r'\s*([0-9]+|!=|\.\.|[%=,]|[a-z]+)')


def validate_relation(relation):
    """Syntax gate mirrors the adopted exact C evaluator's actual grammar.

    No decimal constants, parentheses, signed constants, or uint32 overflow.
    Such grammar is an explicit DataError, never silently changed or omitted.
    CLDR @samples are removed before this gate and do not enter runtime data.
    """
    m.require(isinstance(relation, str), 'plural relation must be text')
    m.require(all(0x20 <= ord(c) <= 0x7e for c in relation) and relation.strip() == relation,
              'plural relation must be trimmed printable ASCII')
    tokens, at = [], 0
    while at < len(relation):
        match = TOKEN.match(relation, at)
        m.require(match is not None, 'unsupported plural grammar: ' + relation)
        tokens.append(match.group(1))
        at = match.end()
    if not tokens:
        return relation
    cursor = 0

    def take(value):
        nonlocal cursor
        if cursor < len(tokens) and tokens[cursor] == value:
            cursor += 1
            return True
        return False

    def integer():
        nonlocal cursor
        m.require(cursor < len(tokens) and tokens[cursor].isdigit(),
                  'missing plural integer: ' + relation)
        significant = tokens[cursor].lstrip('0') or '0'
        m.require(len(significant) <= 10, 'plural constant exceeds uint32')
        value = int(significant)
        cursor += 1
        m.require(value <= 0xffffffff, 'plural constant exceeds uint32')
        return value

    def relation_item():
        nonlocal cursor
        m.require(cursor < len(tokens) and tokens[cursor] in tuple('nivwftce'),
                  'invalid plural operand: ' + relation)
        cursor += 1
        if take('%') or take('mod'):
            m.require(integer() != 0, 'zero plural modulus')
        single = False
        if take('=') or take('!='):
            pass
        elif take('is'):
            take('not')
            single = True
        else:
            take('not')
            m.require(take('in') or take('within'), 'invalid plural relation operator')
        while True:
            lower = integer()
            if not single and take('..'):
                m.require(lower <= integer(), 'reversed plural integer range')
            if single or not take(','):
                break

    relation_item()
    while cursor < len(tokens):
        m.require(take('and') or take('or'), 'unexpected plural grammar token')
        relation_item()
    return relation


def tag(value):
    return 'root' if value == 'root' else m.canonical_tag(value)


def parse_rules(data, plural_type):
    root = m.parse_xml(data, 'supplementalData')
    groups = root.findall('./plurals')
    m.require(len(groups) == 1 and groups[0].get('type') == plural_type,
              'missing/wrong plural type')
    result = {}
    for group in groups[0]:
        m.require(group.tag == 'pluralRules' and set(group.attrib) == {'locales'},
                  'unsupported pluralRules element/attribute')
        rules = {}
        for child in group:
            m.require(child.tag == 'pluralRule' and not len(child) and
                      set(child.attrib) == {'count'}, 'unsupported pluralRule leaf')
            category = child.get('count')
            m.require(category in CATEGORY and category not in rules,
                      'unknown/repeated plural category')
            relation = (child.text or '').split('@', 1)[0].strip()
            validate_relation(relation)
            m.require(bool(relation) == (category != 'other'),
                      'other must be empty; named categories require relations')
            rules[category] = relation
        m.require('other' in rules, 'missing plural other category')
        ordered = tuple((CATEGORY[name], rules[name]) for name in CATEGORIES if name in rules)
        locales = tuple(tag(value) for value in m.words(group.get('locales', '')))
        m.require(locales and len(set(locales)) == len(locales), 'empty/repeated plural locales')
        for locale in locales:
            m.insert_unique(result, locale, ordered, 'plural rule locale')
    m.require('root' in result, 'missing root plural rule')
    return result


def parse_ranges(data):
    root = m.parse_xml(data, 'supplementalData')
    groups = root.findall('./plurals')
    m.require(len(groups) == 1 and not groups[0].attrib, 'unsupported range plural type')
    result = {}
    for group in groups[0]:
        m.require(group.tag == 'pluralRanges' and set(group.attrib) == {'locales'},
                  'unsupported pluralRanges group')
        ranges = {}
        for child in group:
            m.require(child.tag == 'pluralRange' and not len(child) and
                      set(child.attrib) == {'start', 'end', 'result'},
                      'unsupported pluralRange leaf')
            values = tuple(child.get(name) for name in ('start', 'end', 'result'))
            m.require(all(value in CATEGORY for value in values), 'unknown range category')
            start, end, selected = tuple(CATEGORY[value] for value in values)
            m.insert_unique(ranges, (start, end), selected, 'plural range pair')
        rows = tuple((start, end, selected) for (start, end), selected in sorted(ranges.items()))
        locales = tuple(tag(value) for value in m.words(group.get('locales', '')))
        m.require(locales and len(set(locales)) == len(locales), 'empty/repeated range locales')
        for locale in locales:
            m.insert_unique(result, locale, rows, 'plural range locale')
    return result


def resolve(mapping, requested, metadata, required):
    """CLDR plural component inheritance, including a specific parent override.

    Supplemental rule locales may be absent from common/main. Lookup searches
    actual keys in the metadata graph; no guessed language category templates.
    The CLDR root rule is explicit. Range data is sparse and absent pairs use
    LDML's documented 'other' default rather than a fabricated root record.
    """
    current = requested
    visited = set()
    overrides = metadata['component_parents']
    while current is not None:
        m.require(current not in visited and current in metadata['parents'],
                  'plural parent cycle or missing locale graph node')
        visited.add(current)
        if current in mapping:
            return mapping[current], current
        current = overrides.get(('plurals', current), metadata['parents'][current])
    m.require(not required, 'missing inherited plural rules: ' + requested)
    return (), None


def collect(cardinal_xml, ordinal_xml, range_xml, metadata):
    by_type = (parse_rules(cardinal_xml, 'cardinal'), parse_rules(ordinal_xml, 'ordinal'))
    ranges = parse_ranges(range_xml)
    locale_rows, rule_rows, range_rows, provenance = [], [], [], []
    rule_spans, range_spans = {}, {}
    for index, locale in enumerate(sorted(metadata['parents'])):
        selected_ranges, range_source = resolve(ranges, locale, metadata, False)
        if selected_ranges not in range_spans:
            range_spans[selected_ranges] = (len(range_rows) if selected_ranges else 0,
                                            len(selected_ranges))
            range_rows.extend(selected_ranges)
        for type_id, mapping in enumerate(by_type):
            selected_rules, rule_source = resolve(mapping, locale, metadata, True)
            if selected_rules not in rule_spans:
                rule_spans[selected_rules] = (len(rule_rows), len(selected_rules))
                rule_rows.extend(selected_rules)
            locale_rows.append((index, type_id, rule_spans[selected_rules],
                                range_spans[selected_ranges]))
            provenance.append({'locale': locale, 'type': ('cardinal', 'ordinal')[type_id],
                               'rule_source': rule_source, 'range_source': range_source})
    return {'locales': tuple(locale_rows), 'rules': tuple(rule_rows),
            'ranges': tuple(range_rows), 'evidence': {
                'provenance': provenance,
                'grammar': 'exact adopted C evaluator; uint32 constants; samples excluded',
                'range_policy': 'CLDR file has no type discriminator; same explicit locale map for cardinal/ordinal; absent pairs other per LDML',
                'service_coverage': 'candidate data only; no bits activated'}}


def load_pinned(args, metadata, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    inputs = tuple(m.verified_input(args.cldr_dir, manifest, path, consumed, 'cldr', context)
                   for path in PATHS)
    return collect(inputs[0], inputs[1], inputs[2], metadata)


def strings(data):
    return tuple(relation for unused_category, relation in data['rules'])


def encode(data, pool):
    locales, rules, ranges = [], [], []
    previous = None
    for index, plural_type, rule_span, range_span in data['locales']:
        key = (index, plural_type)
        m.require(previous is None or previous < key, 'unordered plural locale rows')
        previous = key
        m.require(plural_type in (0, 1), 'bad plural type')
        for span, target in ((rule_span, data['rules']), (range_span, data['ranges'])):
            first, count = span
            m.require((count == 0 and first == 0) or
                      (count > 0 and first <= len(target) and count <= len(target) - first),
                      'bad plural span')
        m.require(1 <= rule_span[1] <= 6 and range_span[1] <= 36, 'bad plural span length')
        locales.append(m.u32(index) + struct.pack('<BBBB', plural_type, 0, 0, 0) +
                       b''.join(m.u32(value) for value in rule_span + range_span))
    for category, relation in data['rules']:
        m.require(category in range(6), 'bad plural category')
        validate_relation(relation)
        m.require(bool(relation) == (category != CATEGORY['other']), 'bad other relation')
        rules.append(struct.pack('<BBBB', category, 0, 0, 0) + pool.ref(relation))
    for start, end, selected in data['ranges']:
        m.require(all(value in range(6) for value in (start, end, selected)), 'bad range enum')
        ranges.append(struct.pack('<BBBB', start, end, selected, 0))
    return {30: b''.join(locales), 31: b''.join(rules), 32: b''.join(ranges)}
