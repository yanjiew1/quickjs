"""Original CLDR48.2 NumberFormat host generator; runtime consumes bytes only.

Python3.6+; candidate data never enables metadata service coverage. Explicit
gaps omit unsupported rows. CLDR/UCD attribution is in NOTICE.txt.
"""
import re
import struct
from functools import lru_cache
import metadata as m
import locale_subset

SCHEMA_MINOR = 3
WIDTHS = {80: 96, 81: 40, 82: 76, 83: 64, 84: 56, 85: 12,
          86: 24, 87: 12, 88: 24, 120: 76, 121: 32, 122: 16}
CATEGORIES = ('zero', 'one', 'two', 'few', 'many', 'other')
SYMBOLS = ('decimal', 'group', 'plusSign', 'minusSign', 'percentSign',
           'exponential', 'infinity', 'nan', 'currencyDecimal', 'currencyGroup')
UNITS = {}
for family, identifiers in (
        ('area', 'acre hectare'), ('digital', 'bit byte gigabit gigabyte kilobit kilobyte megabit megabyte petabyte terabit terabyte'),
        ('temperature', 'celsius fahrenheit'), ('length', 'centimeter foot inch kilometer meter mile mile-scandinavian millimeter yard'),
        ('duration', 'day hour microsecond millisecond minute month nanosecond second week year'),
        ('angle', 'degree'), ('volume', 'fluid-ounce gallon liter milliliter'),
        ('mass', 'gram kilogram ounce pound stone'), ('concentr', 'percent')):
    for identifier in identifiers.split():
        UNITS[identifier] = family + '-' + identifier
DISCRIMINATORS = frozenset(('type', 'numberSystem', 'count', 'alt', 'case', 'gender'))
_SEGMENTS = {}


def scalar_text(value, empty=False):
    m.require(isinstance(value, str) and (empty or bool(value)) and
              all(ord(c) and not 0xd800 <= ord(c) <= 0xdfff for c in value),
              'invalid/missing scalar text')
    m.require(value not in ('\u2205\u2205\u2205', '\u2191\u2191\u2191'), 'blocked/inherited text')
    return value


def segment(tag, **attrs):
    key = tag, tuple(sorted(attrs.items()))
    return _SEGMENTS.setdefault(key, key)


def xpath(base, value):
    """Restricted exact LDML relative XPath, never eval or guessed locale."""
    path = list(base)
    for piece in value.split('/'):
        if piece == '.':
            continue
        if piece == '..':
            m.require(path, 'alias escapes LDML root')
            path.pop()
            continue
        match = re.fullmatch(r'([A-Za-z][A-Za-z0-9]*)(.*)', piece)
        m.require(match is not None, 'unsupported number alias path')
        rest, attrs = match.group(2), {}
        while rest:
            field = re.match(r"\[@([A-Za-z][A-Za-z0-9]*)=(['\"])([^'\"]*)\2\]", rest)
            m.require(field is not None and field.group(1) in DISCRIMINATORS and
                      field.group(1) not in attrs, 'unsupported number alias predicate')
            attrs[field.group(1)] = field.group(3)
            rest = rest[field.end():]
        path.append(segment(match.group(1), **attrs))
    return tuple(path)


class Resolver:
    def __init__(self, inputs, parents):
        self.roots, self.parents, self.cache = {}, parents, {}
        self.child_indexes, self.step_cache, self.cached_locale = {}, {}, None
        for path, data in sorted(inputs):
            tag = path.rsplit('/', 1)[1][:-4]
            tag = 'root' if tag == 'root' else m.canonical_tag(tag)
            m.require(tag in parents and tag not in self.roots, 'bad/duplicate number locale')
            self.roots[tag] = m.parse_xml(data, 'ldml')
        m.require('root' in self.roots, 'missing number root locale')

    @staticmethod
    def matches(element, selected):
        tag, pairs = selected
        if element.tag != tag:
            return False
        attrs = dict(pairs)
        actual = {k: v for k, v in element.attrib.items() if k in DISCRIMINATORS}
        # LDML standard type may be absent or explicit. Short/long/count/alt
        # records never satisfy a query for the default standard record.
        if actual.get('type') == 'standard' and 'type' not in attrs:
            actual.pop('type')
        if attrs.get('type') == 'standard' and 'type' not in actual:
            attrs.pop('type')
        return attrs == actual

    def get(self, requested, path):
        # Results depending on source="locale" aliases belong to the current
        # requesting locale. Bound this memo to one requesting locale; direct
        # parent steps below remain reusable without changing alias context.
        if requested != self.cached_locale:
            self.cache.clear(); self.cached_locale = requested
        original = (requested, path)
        if original in self.cache:
            return self.cache[original]
        current, visited = requested, set()
        while current is not None:
            state = (current, path)
            m.require(state not in visited, 'number inheritance/alias cycle')
            visited.add(state)
            m.require(current in self.parents, 'number parent absent from metadata')
            kind, value = self.step(current, path)
            if kind == 'value':
                self.cache[original] = value
                return value
            if kind == 'alias':
                path, current = value, requested
            else:
                current = self.parents[current]
        self.cache[original] = None
        return None

    def children(self, node):
        if node in self.child_indexes:
            return self.child_indexes[node]
        index = {}
        for child in node:
            attrs = {k: v for k, v in child.attrib.items() if k in DISCRIMINATORS}
            if attrs.get('type') == 'standard':
                attrs.pop('type')
            key = segment(child.tag, **attrs)
            index.setdefault(key, []).append(child)
        self.child_indexes[node] = index
        return index

    def step(self, locale, path):
        """One locale-tree lookup; alias target is a path, never resolved text.

        Indexing avoids repeatedly scanning hundreds of currencies/units.
        Reusing direct inherited leaves avoids walking the same parent trees
        for every child locale. Missing child steps are not retained, keeping
        memory proportional to present source fields rather than all queries.
        """
        state = locale, path
        if state in self.step_cache:
            return self.step_cache[state]
        node = self.roots.get(locale)
        result = ('missing', None)
        for depth in range(len(path) + 1):
            if node is None:
                break
            index = self.children(node) if len(node) else {}
            aliases = index.get(segment('alias'), ())
            if aliases:
                m.require(len(aliases) == 1 and len(node) == 1 and
                          aliases[0].get('source') == 'locale', 'unsupported mixed/cross-locale alias')
                result = ('alias', xpath(path[:depth], aliases[0].get('path', '')) + path[depth:])
                break
            if depth == len(path):
                m.require(not len(node), 'number value is not a leaf')
                value = node.text or ''
                if value != '\u2191\u2191\u2191':
                    result = ('value', scalar_text(value))
                break
            tag, pairs = path[depth]
            attrs = dict(pairs)
            if attrs.get('type') == 'standard':
                attrs.pop('type')
            candidates = index.get(segment(tag, **attrs), ())
            m.require(len(candidates) <= 1, 'duplicate number field')
            node = candidates[0] if candidates else None
        if result[0] != 'missing' or locale == 'root':
            self.step_cache[state] = result
        return result


@lru_cache(maxsize=None)
def compile_pattern(value, style):
    """LDML fixed pattern -> exact affix tokens and grouping sizes.

    Numeric digit defaults are frontend options, not pattern digit minima.
    Scientific exponents come from ECMA402 notation operations. Unsupported
    padding/per-mille/multiple currency placeholders become explicit gaps.
    """
    scalar_text(value)
    pieces, current, quoted, i = [], [], False, 0
    while i < len(value):
        c = value[i]
        if c == "'":
            if i + 1 < len(value) and value[i + 1] == "'":
                current.append(("'", True)); i += 2; continue
            quoted = not quoted
        elif c == ';' and not quoted:
            pieces.append(current); current = []
        else:
            current.append((c, quoted))
        i += 1
    m.require(not quoted, 'unclosed LDML quote')
    pieces.append(current)
    m.require(1 <= len(pieces) <= 2, 'too many LDML subpatterns')

    def subpattern(chars):
        positions = [i for i, (c, q) in enumerate(chars) if not q and c in '#0@']
        m.require(positions, 'missing numeric LDML skeleton')
        first, last = positions[0], positions[-1]
        skeleton = ''.join(c for c, q in chars[first:last + 1] if not q)
        m.require(all(not q for c, q in chars[first:last + 1]) and
                  re.fullmatch(r'[#,]*0+[0,]*(?:\.[#0]+)?', skeleton), 'unsupported numeric LDML skeleton')
        integer = skeleton.split('.', 1)[0]
        groups = integer.split(',')
        primary = len(groups[-1]) if len(groups) > 1 else 0
        secondary = len(groups[-2]) if len(groups) > 2 else primary
        m.require(primary <= 9 and secondary <= 9 and bool(primary) == bool(secondary), 'bad LDML grouping')
        output = []
        for index, (c, quoted_char) in enumerate(chars):
            if index == first:
                output.append('{number}')
            if first <= index <= last:
                continue
            if quoted_char:
                m.require(c not in '{}', 'literal brace unsupported')
                output.append(c)
            elif c in '-+%\u00a4':
                output.append({'-': '{minusSign}', '+': '{plusSign}', '%': '{percentSign}', '\u00a4': '{currency}'}[c])
            else:
                m.require(c not in '{}*\u2030#0@.,E', 'unsupported LDML affix token')
                output.append(c)
        result = ''.join(output)
        m.require(result.count('{currency}') == int(style >= 2) and
                  result.count('{percentSign}') == int(style == 1), 'style/LDML token mismatch')
        return result, primary, secondary

    positive, primary, secondary = subpattern(pieces[0])
    m.require('{minusSign}' not in positive and '{plusSign}' not in positive, 'signed positive LDML pattern')
    negative = subpattern(pieces[1])[0] if len(pieces) == 2 else '{minusSign}' + positive
    m.require(negative.count('{minusSign}') <= 1 and '{plusSign}' not in negative, 'bad LDML negative sign')
    signed_positive = negative.replace('{minusSign}', '{plusSign}') if '{minusSign}' in negative else '{plusSign}' + positive
    return primary, secondary, (positive, negative, signed_positive)


def numbered(group, system):
    return (segment('numbers'), segment(group, numberSystem=system))


def plural_values(resolver, locale, base, leaf, **attrs):
    other = resolver.get(locale, base + (segment(leaf, count='other', **attrs),))
    m.require(other is not None, 'missing other plural fallback')
    return tuple(resolver.get(locale, base + (segment(leaf, count=count, **attrs),)) or other
                 for count in CATEGORIES)


@lru_cache(maxsize=None)
def placeholder(value, currency=False, number_required=True):
    scalar_text(value)
    m.require((value.count('{0}') == 1 or (not number_required and not value.count('{0}'))) and
              value.count('{1}') == int(currency), 'wrong LDML placeholder count')
    rest = value.replace('{0}', '').replace('{1}', '')
    m.require('{' not in rest and '}' not in rest, 'unknown LDML placeholder')
    return value.replace('{0}', '{number}').replace('{1}', '{currency}')


def unicode_classes(data):
    """UnicodeData.txt categories; absent scalars Cn -> !S&&!Z (flags2).

    Sparse canonical ranges store L, Nd, S and Z. Surrogates are excluded.
    First/Last ranges are checked and expanded as ranges, not code points.
    """
    rows, pending, previous = [], None, -1
    for line in data.decode('utf-8').splitlines():
        if not line:
            continue
        fields = line.split(';')
        m.require(len(fields) == 15, 'malformed UnicodeData record')
        first = last = int(fields[0], 16)
        name, category = fields[1], fields[2]
        if name.endswith(', First>'):
            m.require(pending is None, 'nested UnicodeData First')
            pending = first, name[:-8], category
            continue
        if name.endswith(', Last>'):
            m.require(pending and pending[1] == name[:-7] and pending[2] == category, 'bad UnicodeData Last')
            first, pending = pending[0], None
        else:
            m.require(pending is None, 'missing UnicodeData Last')
        m.require(previous < first <= last <= 0x10ffff, 'unsorted UnicodeData')
        previous = last
        if category == 'Cs':
            continue
        m.require(not (first <= 0xdfff and last >= 0xd800), 'UnicodeData scalar crosses surrogates')
        flags = int(category.startswith('L')) | (0 if category[0] in 'SZ' else 2) | (4 if category == 'Nd' else 0)
        if flags == 2:
            continue
        if rows and rows[-1][1] + 1 == first and rows[-1][2] == flags:
            rows[-1] = rows[-1][0], last, flags
        else:
            rows.append((first, last, flags))
    m.require(pending is None and rows, 'incomplete UnicodeData')
    return tuple(rows)


def currency_digits(data):
    root, rows = m.parse_xml(data, 'supplementalData'), {}
    for leaf in root.findall('./currencyData/fractions/info'):
        code, digits = leaf.get('iso4217', ''), leaf.get('digits', '')
        m.require((code == 'DEFAULT' or re.fullmatch('[A-Z]{3}', code)) and
                  re.fullmatch('[0-9]+', digits) and int(digits) <= 100,
                  'invalid currency fraction info')
        m.insert_unique(rows, code, int(digits), 'currency digits')
    m.require('DEFAULT' in rows, 'missing DEFAULT CurrencyDigits')
    m.require(rows['DEFAULT'] == 2, 'ECMA402 undefined CurrencyDigits fallback must be2')
    return tuple(sorted(rows.items()))


def collect(inputs, metadata, supplemental, unicode_data, grammatical):
    resolver = Resolver(inputs, metadata['parents'])
    tables, gaps = {i: [] for i in WIDTHS}, []
    codes = sorted({e.get('type', '') for root in resolver.roots.values()
                    for e in root.findall('./numbers/currencies/currency')
                    if e.get('alt') is None})
    m.require(all(re.fullmatch('[A-Z]{3}', code) for code in codes), 'invalid CLDR currency code')
    systems = sorted(metadata['numbering'])

    def attempt(key, operation):
        try:
            return operation()
        except m.DataError as exc:
            gaps.append({'key': list(key), 'reason': str(exc)})
            return None

    for locale_index, locale in enumerate(sorted(metadata['parents'])):
        for system_index, system in enumerate(systems):
            if metadata['numbering'][system][1]:
                continue  # algorithmic numbering is an explicit engine gap
            def symbols_and_decimal():
                minimum = resolver.get(locale, (segment('numbers'), segment('minimumGroupingDigits')))
                m.require(minimum is not None and re.fullmatch('[1-9]', minimum), 'bad minimumGroupingDigits')
                values = []
                for symbol in SYMBOLS:
                    value = resolver.get(locale, numbered('symbols', system) + (segment(symbol),))
                    if value is None and symbol in ('currencyDecimal', 'currencyGroup'):
                        value = values[0 if symbol == 'currencyDecimal' else 1]  # CLDR documented fallback
                    values.append(scalar_text(value))
                path = numbered('decimalFormats', system) + (segment('decimalFormatLength'), segment('decimalFormat'), segment('pattern'))
                compiled = compile_pattern(resolver.get(locale, path), 0)
                return int(minimum), tuple(values), compiled
            core = attempt((locale, system, 'core'), symbols_and_decimal)
            if core is None:
                continue
            minimum, symbols, compiled = core
            tables[80].append((locale_index, system_index, minimum, symbols))
            tables[81].append((locale_index, system_index, 0, 0) + compiled)
            for style, group, kind in ((1, 'percentFormats', None), (2, 'currencyFormats', 'standard'), (3, 'currencyFormats', 'accounting')):
                format_tag = 'percentFormat' if style == 1 else 'currencyFormat'
                base = numbered(group, system) + (segment(format_tag + 'Length'), segment(format_tag, **({'type': kind} if kind else {})))
                value = resolver.get(locale, base + (segment('pattern'),))
                result = attempt((locale, system, 'pattern', style), lambda: compile_pattern(value, style))
                if result is not None:
                    tables[81].append((locale_index, system_index, style, 0) + result)
                    if style >= 2:
                        alpha = resolver.get(locale, base + (segment('pattern', alt='alphaNextToNumber'),))
                        if alpha is not None:
                            result = attempt((locale, system, 'alpha', style), lambda: compile_pattern(alpha, style))
                            if result is not None:
                                tables[81].append((locale_index, system_index, style, 1) + result)
            names = attempt((locale, system, 'currency-name'), lambda: tuple(placeholder(v, True) for v in
                plural_values(resolver, locale, numbered('currencyFormats', system), 'unitPattern')))
            if names is not None:
                tables[84].append((locale_index, system_index, names))
            def spacing():
                insertions = []
                for side in ('beforeCurrency', 'afterCurrency'):
                    base = numbered('currencyFormats', system) + (segment('currencySpacing'), segment(side))
                    m.require(resolver.get(locale, base + (segment('currencyMatch'),)) == '[[:^S:]&[:^Z:]]' and
                              resolver.get(locale, base + (segment('surroundingMatch'),)) == '[:digit:]', 'unsupported currency spacing rule')
                    insertions.append(scalar_text(resolver.get(locale, base + (segment('insertBetween'),))))
                return tuple(insertions)
            insertions = attempt((locale, system, 'spacing'), spacing)
            if insertions is not None:
                tables[88].append((locale_index, system_index, insertions))
            def misc():
                base = numbered('miscPatterns', system)
                values = tuple(resolver.get(locale, base + (segment('pattern', type=kind),)) for kind in ('approximately', 'range'))
                for index, value in enumerate(values):
                    placeholder(value, bool(index))
                return values
            misc_values = attempt((locale, system, 'misc'), misc)
            if misc_values is not None:
                tables[86].append((locale_index, system_index, misc_values))
        for code in codes:
            base = (segment('numbers'), segment('currencies'), segment('currency', type=code))
            symbol = resolver.get(locale, base + (segment('symbol'),)) or ''
            narrow = resolver.get(locale, base + (segment('symbol', alt='narrow'),)) or symbol
            other = resolver.get(locale, base + (segment('displayName', count='other'),)) or resolver.get(locale, base + (segment('displayName'),)) or code
            names = tuple(resolver.get(locale, base + (segment('displayName', count=count),)) or other for count in CATEGORIES)
            tables[82].append((locale_index, code, symbol, narrow, names))
        for identifier, cldr_identifier in sorted(UNITS.items()):
            for display, width in enumerate(('long', 'short', 'narrow')):
                base = (segment('units'), segment('unitLength', type=width), segment('unit', type=cldr_identifier))
                values = attempt((locale, identifier, width), lambda: tuple(placeholder(v, number_required=False) for v in
                    plural_values(resolver, locale, base, 'unitPattern')))
                if values is not None:
                    tables[83].append((locale_index, identifier, display, values))
    tables[85] = list(currency_digits(supplemental))
    tables[87] = list(unicode_classes(unicode_data))
    import number_extensions as extensions
    tables.update(extensions.collect(resolver, metadata, tables, grammatical, gaps))
    for rows in tables.values():
        rows.sort()
    return {'tables': {key: tuple(value) for key, value in tables.items()}, 'evidence': {
        'policy': 'exact scalar text; general metadata parents; requesting-locale aliases; draft levels retained',
        'gaps': gaps, 'unsupported': ['algorithmic numbering', 'nonstandard currency spacing rules'],
        'range_policy': 'complete endpoint representations; exact source pattern order; rendered identity approximation',
        'compact_style_policy': 'decimal short/long notation data shared by all styles',
        'coverage': 0}}


def load_pinned(args, metadata, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    paths = locale_subset.main_paths(manifest, metadata)
    inputs = ((p, m.verified_input(args.cldr_dir, manifest, p, consumed, 'cldr', context)) for p in paths)
    ucd = m.load_pin_manifest(args.ucd_manifest, m.UCD_MANIFEST_SHA256, context)
    return collect(inputs, metadata,
        m.verified_input(args.cldr_dir, manifest, 'common/supplemental/supplementalData.xml', consumed, 'cldr', context),
        m.verified_input(args.ucd_dir, ucd, 'UnicodeData.txt', consumed, 'ucd', context),
        m.verified_input(args.cldr_dir, manifest, 'common/supplemental/grammaticalFeatures.xml', consumed, 'cldr', context))


def strings(data):
    def values(item):
        if isinstance(item, str):
            yield item
        elif isinstance(item, tuple):
            for child in item:
                for value in values(child):
                    yield value
    return tuple(value for rows in data['tables'].values() for row in rows for value in values(row))


def encode(data, pool):
    result = {}
    for section_id, rows in sorted(data['tables'].items()):
        encoded, previous = [], None
        for row in rows:
            key = row[:4] if section_id in (81, 120) else row[:3] if section_id in (83, 121) else row[:2] if section_id in (80, 82, 84, 86, 87, 88, 122) else row[:1]
            m.require(previous is None or previous < key, 'unsorted/duplicate number rows')
            previous = key
            if section_id >= 120:
                import number_extensions as extensions
                record = extensions.encode_row(section_id, row, pool)
            elif section_id == 80:
                a, b, minimum, values = row
                record = m.u32(a) + m.u32(b) + bytes((minimum,)) + b'\0' * 7 + b''.join(pool.ref(v) for v in values)
            elif section_id == 81:
                a, b, style, variant, primary, secondary, values = row
                record = m.u32(a) + m.u32(b) + struct.pack('<BBHBBH', style, variant, 0, primary, secondary, 0) + b''.join(pool.ref(v) for v in values)
            elif section_id == 82:
                a, code, symbol, narrow, names = row
                record = m.u32(a) + b''.join(pool.ref(v) for v in (code, symbol, narrow) + names)
            elif section_id == 83:
                a, identifier, display, values = row
                record = m.u32(a) + pool.ref(identifier) + bytes((display,)) + b'\0' * 3 + b''.join(pool.ref(v) for v in values)
            elif section_id in (84, 86, 88):
                a, b, values = row
                record = m.u32(a) + m.u32(b) + b''.join(pool.ref(v) for v in values)
            elif section_id == 85:
                code, digits = row
                record = pool.ref(code) + bytes((digits,)) + b'\0' * 3
            else:
                record = b''.join(m.u32(value) for value in row)
            m.require(len(record) == WIDTHS[section_id], 'number record width drift')
            encoded.append(record)
        result[section_id] = b''.join(encoded)
    return result
