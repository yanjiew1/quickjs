"""Original CLDR49 DisplayNames extension; Python3.6+, source-only packet.

Exact XML Unicode text; general locale parents from validated metadata.
No XML access on the native runtime path. Unicode-3.0 data attribution.
"""
import re
import metadata as m
import locale_subset

WIDTHS = {50: 28, 51: 16}
FIELDS = (('era', 'era'), ('year', 'year'), ('quarter', 'quarter'),
          ('month', 'month'), ('weekOfYear', 'week'), ('weekday', 'weekday'),
          ('day', 'day'), ('dayPeriod', 'dayperiod'), ('hour', 'hour'),
          ('minute', 'minute'), ('second', 'second'), ('timeZoneName', 'zone'))
BRACKETS = ('(', ')', '\uff08', '\uff09')
TOKEN = re.compile(r"([A-Za-z][A-Za-z0-9]*)(?:\[@([A-Za-z][A-Za-z0-9]*)=['\"]([^'\"]*)['\"]\])?")
# CLDR49 LDML language@menu distinguishes core/extension menu labels from
# the unqualified DisplayNames label. draft/references are metadata, not
# path selectors. Retain menu alternatives even though collect does not
# select them for standard DisplayNames output.
DISTINGUISHING = ('type', 'key', 'alt', 'scope', 'count', 'bracket', 'menu')
INHERIT = '\u2191\u2191\u2191'
MISSING = '\u2205\u2205\u2205'


def step(tag, **attributes):
    return (tag, tuple(sorted(attributes.items())))


def path_description(path):
    return '/'.join(tag + ''.join('[@{}={!r}]'.format(key, value)
        for key, value in attributes) for tag, attributes in path) or '.'


def scalar_text(value):
    m.require(isinstance(value, str) and bool(value), 'empty display name')
    m.require(all(ord(c) and not 0xd800 <= ord(c) <= 0xdfff for c in value),
              'invalid scalar in display name')
    return value


def template(value):
    scalar_text(value)
    m.require(value.count('{0}') == value.count('{1}') == 1,
              'display pattern requires both placeholders once')
    rest = value.replace('{0}', '').replace('{1}', '')
    m.require('{' not in rest and '}' not in rest, 'unsupported display pattern')
    return value


def alias_path(prefix, expression):
    """Checked subset of LDML XPath used by these source subtrees.

    Reject unknown syntax instead of silently ignoring a source alias.
    Ancestor aliases rewrite the selected descendant suffix as well.
    """
    result = list(prefix)
    for token in expression.split('/'):
        if token == '.':
            continue
        if token == '..':
            m.require(result, 'display alias escapes ldml')
            result.pop()
            continue
        match = TOKEN.fullmatch(token)
        m.require(match is not None, 'unsupported display alias XPath: ' + expression)
        attributes = {match.group(2): match.group(3)} if match.group(2) else {}
        result.append(step(match.group(1), **attributes))
    return tuple(result)


def parse_inputs(inputs):
    trees = {}
    for path, data in sorted(inputs):
        m.require(path.startswith('common/main/') and path.endswith('.xml'),
                  'display input outside CLDR common/main')
        tag = path.rsplit('/', 1)[1][:-4]
        tag = 'root' if tag == 'root' else m.canonical_tag(tag)
        m.require(tag not in trees, 'duplicate display locale')
        xml = m.parse_xml(data, 'ldml')
        nodes, aliases = {}, {}

        def visit(element, parent):
            attrs = {k: v for k, v in element.attrib.items() if k in DISTINGUISHING}
            current = parent + (step(element.tag, **attrs),)
            m.require(current not in nodes,
                      'duplicate display XML path: locale={} input={} path={}'.format(
                          tag, path, path_description(current)))
            nodes[current] = element.text if not len(element) else None
            for child in element:
                if child.tag == 'alias':
                    m.require(current not in aliases,
                              'duplicate display alias: locale={} input={} path={}'.format(
                                  tag, path, path_description(current)))
                    source = child.get('source', '')
                    m.require(source, 'missing display alias source')
                    aliases[current] = (source, alias_path(current, child.get('path', '')))
                else:
                    visit(child, current)

        # Only selected data subtrees are retained. Currency count forms and
        # variant/short/menu alternatives stay distinguishable during resolution.
        for prefix, subtree in (
                ((), xml.find('localeDisplayNames')),
                ((step('dates'),), xml.find('./dates/fields')),
                ((step('numbers'),), xml.find('./numbers/currencies')),
                ((), xml.find('./contextTransforms')),
                ((step('characters'),), None)):
            if subtree is not None:
                visit(subtree, prefix)
        for element in xml.findall('./characters/nestedBracketReplacement'):
            visit(element, (step('characters'),))
        # Retain aliases on the selected subtrees' otherwise omitted parents.
        for branch in ('dates', 'numbers', 'characters'):
            parent = xml.find(branch)
            if parent is not None:
                for element in parent.findall('./alias'):
                    prefix = (step(branch),)
                    m.require(prefix not in aliases, 'duplicate display ancestor alias')
                    aliases[prefix] = (element.get('source', ''),
                        alias_path(prefix, element.get('path', '')))
        # Whole-locale aliases are represented at the implicit document root.
        for element in xml.findall('./alias'):
            m.require(() not in aliases, 'duplicate whole-locale alias')
            expression = element.get('path', '.')
            aliases[()] = (element.get('source', ''), alias_path((), expression))
        trees[tag] = (nodes, aliases)
    m.require('root' in trees, 'missing CLDR root DisplayNames data')
    return trees


def resolve(trees, parents, requested, path):
    current, original, visited = requested, requested, set()
    while current is not None:
        state = (current, path)
        m.require(state not in visited, 'display alias/inheritance cycle')
        visited.add(state)
        m.require(current in parents, 'display locale absent from metadata parents')
        nodes, aliases = trees.get(current, ({}, {}))
        rewritten = False
        for length in range(len(path), -1, -1):
            if path[:length] in aliases:
                source, target = aliases[path[:length]]
                m.require(source, 'empty display alias source')
                current = original if source == 'locale' else (
                    'root' if source == 'root' else m.canonical_tag(source))
                path = target + path[length:]
                rewritten = True
                break
        if rewritten:
            continue
        value = nodes.get(path)
        if value == MISSING:
            return None   # explicit suppression, not a fabricated code name
        if value is not None and value != INHERIT:
            return scalar_text(value)
        current = parents[current]
    return None


def with_alt(path, value):
    tag, attrs = path[-1]
    attrs = dict(attrs)
    attrs['alt'] = value
    return path[:-1] + (step(tag, **attrs),)


def calendar_codes(xml):
    result = {}
    root = m.parse_xml(xml, 'ldmlBCP47')
    for element in root.findall('./keyword/key[@name="ca"]/type'):
        name = element.get('name')
        preferred = element.get('preferred', name)
        m.require(name and preferred, 'missing calendar BCP47 name')
        aliases = element.get('alias', '').split()
        for alias in [name] + aliases:
            if alias in result:
                # Deprecated islamicc repeats the islamic-civil LDML alias.
                m.require(result[alias] == preferred, 'calendar alias disagreement')
            result[alias] = preferred
    return result


def candidates(trees, calendars):
    records = {}
    groups = {'languages': (0, 'language'), 'territories': (1, 'territory'),
              'scripts': (2, 'script'), 'variants': (6, 'variant')}
    for nodes, unused in trees.values():
        for path in nodes:
            tags = tuple(p[0] for p in path)
            attrs = dict(path[-1][1])
            if any(attribute in attrs for attribute in ('alt', 'scope', 'count', 'menu')):
                continue
            code = attrs.get('type')
            if len(path) == 3 and tags[0] == 'localeDisplayNames' and tags[1] in groups:
                type_id, leaf = groups[tags[1]]
                if tags[2] != leaf or not code:
                    continue
                if type_id == 0:
                    code = m.canonical_tag(code)
                elif type_id == 2:
                    code = code.title()
                elif type_id == 6:
                    code = code.lower()
                else:
                    code = code.upper()
            elif tags == ('localeDisplayNames', 'types', 'type') and attrs.get('key') == 'calendar':
                type_id = 4
                m.require(code in calendars, 'unmapped CLDR calendar label: ' + str(code))
                code = calendars[code]
            elif tags == ('numbers', 'currencies', 'currency', 'displayName'):
                type_id, code = 3, dict(path[-2][1]).get('type', '').upper()
                m.require(re.fullmatch('[A-Z]{3}', code), 'invalid currency label code')
            else:
                continue
            key = (type_id, code)
            m.require(key not in records or records[key] == path,
                      'conflicting display label canonical keys')
            records[key] = path
    # Map valid deprecated calendar names to their actual localized preferred
    # labels, but of() canonicalization itself only regularizes ASCII case.
    for alias, preferred in calendars.items():
        if alias != preferred and re.fullmatch('[a-z0-9]{3,8}(?:-[a-z0-9]{3,8})*', alias):
            if (4, preferred) in records:
                records[(4, alias)] = records[(4, preferred)]
    return records


def collect(inputs, metadata, calendar_xml):
    trees = parse_inputs(inputs)
    parents = metadata['parents']
    m.require(set(trees) <= set(parents), 'display source outside metadata locale graph')
    labels = candidates(trees, calendar_codes(calendar_xml))
    rows, patterns, uncovered = [], [], []
    for index, locale in enumerate(sorted(parents)):
        counts = [0] * 6
        for (type_id, code), path in sorted(labels.items()):
            default = resolve(trees, parents, locale, path)
            short = resolve(trees, parents, locale, with_alt(path, 'short'))
            # Currency names never come from symbols, count variants, the
            # root ISO-code fallback, or a synthesized identity mapping.
            for style in range(3):
                value = default if style == 0 or type_id in (3, 6) else (short or default)
                if value is not None:
                    if type_id == 3 and value == code:
                        continue
                    rows.append((index, type_id, style, code, value))
                    if type_id < 6:
                        counts[type_id] += 1
        for code, field in FIELDS:
            for style, suffix in enumerate(('', '-short', '-narrow')):
                path = (step('dates'), step('fields'), step('field', type=field+suffix), step('displayName'))
                value = resolve(trees, parents, locale, path)
                if value is None:
                    uncovered.append({'locale': locale, 'type': 'dateTimeField',
                                      'style': style, 'code': code})
                else:
                    rows.append((index, 5, style, code, value))
                    counts[5] += 1
        for kind, name in enumerate(('localePattern', 'localeSeparator', 'localeKeyTypePattern')):
            path = (step('localeDisplayNames'), step('localeDisplayPattern'), step(name))
            value = resolve(trees, parents, locale, path)
            if value is None:
                uncovered.append({'locale': locale, 'type': 'pattern', 'code': name})
            else:
                patterns.append((index, kind, template(value)))
        for kind, bracket in enumerate(BRACKETS, 3):
            path = (step('characters'), step('nestedBracketReplacement', bracket=bracket))
            value = resolve(trees, parents, locale, path)
            if value is None:
                uncovered.append({'locale': locale, 'type': 'nestedBracketReplacement', 'code': bracket})
            else:
                patterns.append((index, kind, value))
        for type_id, count in enumerate(counts):
            if not count:
                uncovered.append({'locale': locale, 'type': type_id, 'code': '*',
                                  'reason': 'no actual inherited label records'})
        # The pinned frontend requests standalone capitalization. A complete
        # Unicode18 titlecase transform belongs to the case engine, rather
        # than the host interpreter's unpinned str.title(). Report each
        # affected locale/usage exactly; integration must gate these records.
        for usage in ('languages', 'script', 'territory', 'variant', 'keyValue'):
            path = (step('contextTransforms'), step('contextTransformUsage', type=usage),
                    step('contextTransform', type='stand-alone'))
            transform = resolve(trees, parents, locale, path)
            if transform not in (None, 'no-change'):
                m.require(transform == 'titlecase-firstword', 'unknown capitalization context transform')
                uncovered.append({'locale': locale, 'type': 'standaloneCapitalization',
                    'code': usage, 'transform': transform,
                    'reason': 'requires shared pinned Unicode18 titlecase integration'})
    rows.sort(key=lambda row: (row[0], row[1], row[2], row[3].encode('utf-8')))
    patterns.sort(key=lambda row: row[:2])
    return {'rows': tuple(rows), 'patterns': tuple(patterns), 'uncovered': uncovered,
            'evidence': {'policy': 'exact scalar text; verified general inheritance and requesting-locale aliases',
                         'currency': 'unqualified displayName only; no code identity, symbol, or plural fallback',
                         'style': 'short/narrow language-region-script-calendar names use alt=short then standard; date fields use width aliases',
                         'coverage': 'none activated; uncovered records explicit; availability requires separate root acceptance'}}


def load_pinned(args, metadata, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    paths = locale_subset.main_paths(manifest, metadata)
    inputs = ((path, m.verified_input(args.cldr_dir, manifest, path, consumed, 'cldr', context)) for path in paths)
    calendar = m.verified_input(args.cldr_dir, manifest, 'common/bcp47/calendar.xml', consumed, 'cldr', context)
    return collect(inputs, metadata, calendar)


def strings(data):
    return tuple(value for row in data['rows'] for value in row[3:]) + tuple(row[2] for row in data['patterns'])


def encode(data, pool):
    labels, patterns, previous = [], [], None
    for locale, type_id, style, code, value in data['rows']:
        key = (locale, type_id, style, code.encode('utf-8'))
        m.require(previous is None or previous < key, 'unsorted/duplicate display labels')
        previous = key
        m.require(type_id in range(7) and style in range(3) and code and
                  all(0 < ord(c) < 128 for c in code),
                  'invalid display discriminator/key')
        labels.append(m.u32(locale) + m.u32(type_id) + m.u32(style) + pool.ref(code) + pool.ref(scalar_text(value)))
    previous = None
    for locale, kind, value in data['patterns']:
        key = (locale, kind)
        m.require(previous is None or previous < key, 'unsorted/duplicate display patterns')
        previous = key
        m.require(kind in range(7), 'invalid display pattern kind')
        patterns.append(m.u32(locale) + m.u32(kind) + pool.ref(template(value) if kind < 3 else scalar_text(value)))
    return {50: b''.join(labels), 51: b''.join(patterns)}
