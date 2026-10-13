"""Original CLDR48.2 compact and per-unit preparation. Host only; no runtime XML.

Magnitude/divisor data follows source zero-placeholder counts. Compound
grammar is resolved from pinned grammaticalFeatures.xml before encoding.
"""
import re
from functools import lru_cache
import metadata as m
import number_patterns as n

WIDTHS = {120: 76, 121: 32, 122: 16}


@lru_cache(maxsize=None)
def compact_pattern(value, magnitude, default_exponent=None):
    n.scalar_text(value)
    chars, quoted, i = [], False, 0
    while i < len(value):
        c = value[i]
        if c == "'":
            if i + 1 < len(value) and value[i + 1] == "'":
                chars.append(("'", True)); i += 2; continue
            quoted = not quoted
        else:
            m.require(c not in '{}', 'compact literal braces')
            chars.append((c, quoted))
        i += 1
    m.require(not quoted, 'unclosed compact quote')
    numeric = [i for i, (c, q) in enumerate(chars) if c == '0' and not q]
    if not numeric:
        m.require(default_exponent is not None and default_exponent > 0,
                  'compact other pattern has no numeric divisor')
        return default_exponent, ''.join(c for c, q in chars)
    first, last = numeric[0], numeric[-1]
    m.require(last - first + 1 == len(numeric), 'noncontiguous compact zeros')
    m.require(all(q or c not in '#@.;\u00a4%' for c, q in chars), 'unsupported compact numeric symbol')
    if len(chars) == 1 and chars[0] == ('0', False):
        return 0, '{number}'
    exponent = magnitude - len(numeric) + 1
    m.require(0 < exponent <= magnitude, 'invalid compact divisor')
    output = ''.join(c for c, q in chars[:first]) + '{number}' + ''.join(c for c, q in chars[last + 1:])
    m.require(output != '{number}', 'compact missing magnitude affix')
    return exponent, output


class Derivations:
    def __init__(self, source, parents):
        self.parents, self.rows = parents, {}
        root = m.parse_xml(source, 'supplementalData')
        for group in root.findall('./grammaticalData/grammaticalDerivations'):
            for locale in group.get('locales', '').split():
                locale = 'root' if locale == 'root' else m.canonical_tag(locale)
                for leaf in group.findall('deriveComponent'):
                    if leaf.get('structure') != 'per' or leaf.get('feature') not in ('case', 'plural'):
                        continue
                    key = (locale, leaf.get('feature'))
                    value = (leaf.get('value0'), leaf.get('value1'))
                    m.insert_unique(self.rows, key, value, 'per component derivation')
        m.require(('root', 'case') in self.rows and ('root', 'plural') in self.rows,
                  'missing root per derivations')

    def get(self, locale, feature):
        while locale is not None:
            if (locale, feature) in self.rows:
                return self.rows[(locale, feature)]
            locale = self.parents[locale]
        raise m.DataError('missing per component derivation')


@lru_cache(maxsize=None)
def numbered_name(value):
    """Source unit one-form without its optional numeric field; Zs trim only."""
    n.scalar_text(value)
    m.require(value.count('{0}') <= 1 and '{' not in value.replace('{0}', '') and
              '}' not in value.replace('{0}', ''), 'bad denominator unit template')
    value = value.replace('{0}', '')
    # Unicode Zs set, matching LDML unit-component whitespace trimming.
    value = value.strip(' \u00a0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u202f\u205f\u3000')
    return n.scalar_text(value)


@lru_cache(maxsize=None)
def compound_pattern(value):
    n.scalar_text(value)
    m.require(value.count('{0}') == 1 and value.count('{1}') == 1 and
              '{' not in value.replace('{0}', '').replace('{1}', '') and
              '}' not in value.replace('{0}', '').replace('{1}', ''), 'bad compound per pattern')
    return value.replace('{0}', '{numerator}').replace('{1}', '{denominator}')


def collect(resolver, metadata, base_tables, grammatical, gaps):
    tables = {i: [] for i in WIDTHS}
    derivations = Derivations(grammatical, metadata['parents'])
    thresholds = set()
    for root in resolver.roots.values():
        for pattern in root.findall('./numbers/decimalFormats/decimalFormatLength/decimalFormat/pattern'):
            value = pattern.get('type', '')
            if re.fullmatch('10+', value):
                thresholds.add(len(value) - 1)
    m.require(thresholds, 'no compact decimal magnitudes')

    def attempt(key, operation):
        try:
            return operation()
        except m.DataError as exc:
            gaps.append({'key': list(key), 'reason': str(exc)})
            return None

    systems = sorted(metadata['numbering'])
    locales = sorted(metadata['parents'])
    for locale_index, system_index, minimum, symbols in base_tables[80]:
        locale, system = locales[locale_index], systems[system_index]
        for display, width in enumerate(('short', 'long')):
            base = n.numbered('decimalFormats', system) + (n.segment('decimalFormatLength', type=width), n.segment('decimalFormat'))
            for magnitude in sorted(thresholds):
                power = '1' + '0' * magnitude
                other = resolver.get(locale, base + (n.segment('pattern', type=power, count='other'),))
                if other is None:
                    continue  # this locale has no discrete threshold here
                def one_row():
                    exponent, unused = compact_pattern(other, magnitude)
                    values = []
                    for category in n.CATEGORIES:
                        value = resolver.get(locale, base + (n.segment('pattern', type=power, count=category),)) or other
                        candidate, pattern = compact_pattern(value, magnitude, exponent)
                        m.require(candidate == exponent, 'plural compact divisor disagreement')
                        values.append(pattern)
                    exact = resolver.get(locale, base + (n.segment('pattern', type=power, count='1'),))
                    if exact is not None:
                        candidate, exact = compact_pattern(exact, magnitude, exponent)
                        m.require(candidate == exponent, 'exact-one compact divisor disagreement')
                    return locale_index, system_index, display, magnitude, exponent, tuple(values), exact or ''
                row = attempt((locale, system, width, magnitude, 'compact'), one_row)
                if row is not None:
                    tables[120].append(row)
    available = {row[:3] for row in base_tables[83]}
    for locale_index, locale in enumerate(locales):
        def grammar():
            numerator_plural, denominator_plural = derivations.get(locale, 'plural')
            numerator_case, denominator_case = derivations.get(locale, 'case')
            m.require(numerator_plural == 'compound' and denominator_plural in n.CATEGORIES and
                      numerator_case in ('compound', 'nominative'), 'unsupported per numerator derivation')
            return denominator_plural, denominator_case
        resolved = attempt((locale, 'per-grammar'), grammar)
        if resolved is None:
            continue
        denominator_plural, denominator_case = resolved
        for display, width in enumerate(('long', 'short', 'narrow')):
            width_base = (n.segment('units'), n.segment('unitLength', type=width))
            value = attempt((locale, width, 'compound-per'), lambda: compound_pattern(resolver.get(locale,
                width_base + (n.segment('compoundUnit', type='per'), n.segment('compoundUnitPattern')))))
            if value is None:
                continue
            tables[122].append((locale_index, display, value))
            for identifier, cldr_identifier in sorted(n.UNITS.items()):
                if (locale_index, identifier, display) not in available:
                    continue
                base = width_base + (n.segment('unit', type=cldr_identifier),)
                def denominator_row():
                    specialized = resolver.get(locale, base + (n.segment('perUnitPattern'),))
                    if specialized is not None:
                        specialized = n.placeholder(specialized)
                    selected = resolver.get(locale, base + (n.segment('unitPattern', count=denominator_plural,
                        **{'case': denominator_case}),))
                    if selected is None:
                        selected = resolver.get(locale, base + (n.segment('unitPattern', count=denominator_plural),))
                    if selected is None:
                        selected = resolver.get(locale, base + (n.segment('unitPattern', count='other'),))
                    return locale_index, identifier, display, specialized or '', numbered_name(selected)
                row = attempt((locale, identifier, width, 'denominator'), denominator_row)
                if row is not None:
                    tables[121].append(row)
    for rows in tables.values():
        rows.sort()
    return tables


def encode_row(section_id, row, pool):
    if section_id == 120:
        a, b, display, magnitude, exponent, patterns, exact = row
        return (m.u32(a) + m.u32(b) + bytes((display,)) + b'\0' * 3 + m.u32(magnitude) +
                m.u32(exponent) + b''.join(pool.ref(v) for v in patterns) + pool.ref(exact))
    if section_id == 121:
        a, identifier, display, per_unit, name = row
        return m.u32(a) + pool.ref(identifier) + bytes((display,)) + b'\0' * 3 + pool.ref(per_unit) + pool.ref(name)
    a, display, pattern = row
    return m.u32(a) + bytes((display,)) + b'\0' * 3 + pool.ref(pattern)
