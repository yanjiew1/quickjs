"""Sparse CLDR metadata for wire 1.1; host parsing, no service engines.

Parent links carry inheritance. Only explicit defaults and direction enter
LOCALE/LOCALE_AUX; this module never copies formatter data down the tree.
"""
from pathlib import PurePosixPath
import re
import struct

import metadata as m

WIDTHS = {14: 12, 15: 36, 16: 48, 17: 52, 18: 8, 19: 20,
          21: 44, 22: 12, 23: 20}
DAYS = dict(zip(('mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun'), range(1, 8)))
CYCLES = {'H': 'h23', 'h': 'h12', 'K': 'h11', 'k': 'h24'}
COMPONENTS = {'collations', 'segmentations', 'plurals', 'grammaticalFeatures'}
INHERIT = '\u2191\u2191\u2191'


def locale_tag(value):
    return 'root' if value == 'root' else m.canonical_tag(value)


def region(value):
    m.require(re.fullmatch('[A-Z]{2}|[0-9]{3}', value), 'invalid region: ' + value)
    return value


def preference_key(value):
    if '_' not in value and '-' not in value:
        return 0, region(value)
    tag = m.canonical_tag(value, strict=True)
    m.require(re.fullmatch('[a-z]{2,8}-(?:[A-Z]{2}|[0-9]{3})', tag),
              'hour key must be region or language-region')
    return 1, tag


def structural_parent(tag):
    if tag == 'root':
        return None
    return tag.rsplit('-', 1)[0] if '-' in tag else 'root'


def components(tag):
    if tag == 'root':
        return 'root', '', ''
    parts = tag.split('-')
    script = next((p for p in parts[1:] if re.fullmatch('[A-Z][a-z]{3}', p)), '')
    territory = next((p for p in parts[1:] if re.fullmatch('[A-Z]{2}|[0-9]{3}', p)), '')
    return parts[0], script, territory


def parse_parents(data):
    root = m.parse_xml(data, 'supplementalData')
    general, specific, rules = {}, {}, []
    declared = set()
    for group in root.findall('parentLocales'):
        names = m.words(group.get('component', ''))
        m.require(len(set(names)) == len(names) and set(names) <= COMPONENTS,
                  'unknown or repeated parent component')
        declared.update(names)
        for element in group:
            m.require(element.tag == 'parentLocale', 'unknown parent locale element')
            parent = locale_tag(element.get('parent', ''))
            children = tuple(locale_tag(v) for v in m.words(element.get('locales', '')))
            m.require(children and len(set(children)) == len(children),
                      'empty or repeated parent children')
            rule = element.get('localeRules', '')
            m.require(rule in ('', 'nonlikelyScript'), 'unknown parent locale rule')
            m.require(not rule or (not names and parent == 'root'),
                      'nonlikelyScript must be general root parent')
            if rule:
                m.require(all(components(v)[1] for v in children),
                          'nonlikelyScript child lacks script')
                rules.append((rule, parent, children))
            for child in children:
                m.require(child != 'root' and child != parent, 'invalid self/root parent')
                if names:
                    for name in names:
                        m.insert_unique(specific, (name, child), parent, 'component parent')
                else:
                    m.insert_unique(general, child, parent, 'general parent')
    return general, specific, tuple(sorted(declared)), tuple(sorted(rules))


def field(root, path):
    values = []
    for element in root.findall(path):
        if element.get('alt') is not None:
            continue
        m.require(len(element) == 0, 'unsupported alias in metadata leaf: ' + path)
        value = (element.text or '').strip()
        if value == INHERIT:
            continue
        m.require(value and value != '\u2205\u2205\u2205',
                  'empty/blocked metadata value: ' + path)
        values.append(value)
    m.require(len(set(values)) <= 1, 'conflicting duplicate metadata leaf: ' + path)
    return values[0] if values else ''


def parse_locale(path, data):
    root = m.parse_xml(data, 'ldml')
    identity = root.find('identity')
    m.require(identity is not None, 'locale lacks identity')
    parts = []
    for name in ('language', 'script', 'territory', 'variant'):
        elements = identity.findall(name)
        m.require(len(elements) <= 1, 'repeated locale identity component')
        if elements:
            parts.append(elements[0].get('type', ''))
    m.require(parts, 'identity lacks language')
    tag = locale_tag('_'.join(parts))
    m.require(tag == locale_tag(PurePosixPath(path).stem), 'locale filename/identity mismatch')
    # These exact metadata paths need no general XPath alias engine. Fail
    # closed if a future snapshot puts aliases on their ancestor containers.
    for selected in ('numbers', 'numbers/otherNumberingSystems',
                     'layout', 'layout/orientation', 'collations'):
        for parent in root.findall(selected):
            m.require(parent.find('alias') is None,
                      'unsupported alias on metadata container: ' + selected)
    defaults = {'numbering': field(root, './numbers/defaultNumberingSystem'),
                'native': field(root, './numbers/otherNumberingSystems/native'),
                'traditional': field(root, './numbers/otherNumberingSystems/traditional'),
                'finance': field(root, './numbers/otherNumberingSystems/finance'),
                'direction': field(root, './layout/orientation/characterOrder'),
                'collation': field(root, './collations/defaultCollation')}
    character_order = defaults['direction']
    line_order = field(root, './layout/orientation/lineOrder')
    orientations = ('', 'left-to-right', 'right-to-left', 'top-to-bottom', 'bottom-to-top')
    m.require(character_order in orientations,
              'unsupported locale character order: {}: {}'.format(path, character_order))
    m.require(line_order in orientations,
              'unsupported locale line order: {}: {}'.format(path, line_order))
    if character_order in ('top-to-bottom', 'bottom-to-top'):
        # Intl exposes horizontal ltr/rtl direction. For an explicit vertical
        # character layout, use its explicit horizontal line order. This
        # projects mn_Mong's top-to-bottom/left-to-right layout to LTR without
        # guessing from a script name or claiming vertical rendering support.
        m.require(line_order in ('left-to-right', 'right-to-left'),
                  'vertical layout lacks explicit horizontal line order: ' + path)
        defaults['direction'] = line_order
    for value in defaults.values():
        m.ascii_text(value, empty=True)
    identity_only = all(e.tag in ('identity', 'version', 'generation') for e in root)
    return tag, defaults, identity_only


def merge_locale_inputs(inputs):
    locales, identity_only = {}, []
    for path, data in sorted(inputs):
        tag, values, only_identity = parse_locale(path, data)
        if only_identity and '/main/' in path:
            identity_only.append(tag)
        current = locales.setdefault(tag, dict.fromkeys(values, ''))
        for key, value in values.items():
            if value:
                m.require(not current[key] or current[key] == value,
                          'conflicting locale default: ' + tag + '/' + key)
                current[key] = value
    return locales, tuple(sorted(identity_only))


def build_locale_graph(locales, defaults, parents):
    general, specific, declared, rules = parents
    tags = set(locales) | set(defaults) | {'root'} | set(general) | set(general.values())
    tags.update(child for component, child in specific)
    tags.update(specific.values())
    pending = list(tags)
    while pending:
        parent = structural_parent(pending.pop())
        if parent is not None and parent not in tags:
            tags.add(parent)
            pending.append(parent)
    general_graph = {tag: general.get(tag, structural_parent(tag)) for tag in sorted(tags)}
    m.check_cycles({tag: (parent,) if parent is not None else ()
                    for tag, parent in general_graph.items()}, 'locale parent')
    overrides = {}
    for component in declared:
        # A component group defines its own parent map. Empty groups use
        # structural inheritance, rather than the general exceptional map.
        graph = {tag: specific.get((component, tag), structural_parent(tag))
                 for tag in sorted(tags)}
        m.check_cycles({tag: (parent,) if parent is not None else ()
                        for tag, parent in graph.items()}, component + ' parent')
        for tag, parent in graph.items():
            if parent != general_graph[tag]:
                overrides[(component, tag)] = parent
    for tag in defaults:
        m.require(tag != 'root' and general_graph[tag] is not None,
                  'invalid default-content root')
    return general_graph, overrides


def resolve_default(tag, name, locales, general, overrides):
    """Generator checks and fixtures only; runtime follows serialized links."""
    for unused in range(len(general) + 1):
        if tag is None:
            return ''
        value = locales.get(tag, {}).get(name, '')
        if value:
            return value
        tag = (overrides.get(('collations', tag), general[tag])
               if name == 'collation' else general[tag])
    raise m.DataError('default inheritance cycle')


def bcp_aliases(records, key):
    selected = {record[2]: record for record in records
                if record[0] == 'u' and record[1] == key and record[2]}
    aliases = {}
    for name, record in sorted(selected.items()):
        target = name
        for unused in range(len(selected) + 1):
            preferred = selected[target][4]
            if not preferred:
                break
            m.require(preferred in selected, 'unknown preferred BCP47 type')
            target = preferred
        else:
            raise m.DataError('BCP47 preferred cycle')
        for token in (name,) + m.words(record[3]):
            m.insert_unique(aliases, token.lower(), target, 'BCP47 preference alias')
    return aliases


def parse_numbering(data):
    root = m.parse_xml(data, 'supplementalData')
    records, rules = {}, {}
    containers = root.findall('numberingSystems')
    m.require(len(containers) == 1, 'missing/repeated numbering systems container')
    for element in containers[0]:
        m.require(element.tag == 'numberingSystem', 'unknown numbering systems element')
        identifier = element.get('id', '')
        m.require(re.fullmatch('[a-z0-9]{3,8}', identifier), 'invalid numbering ID')
        kind = element.get('type')
        m.require(kind in ('numeric', 'algorithmic'), 'invalid numbering kind')
        radix = element.get('radix', '10')
        m.require(radix == '10', 'only decimal numbering metadata is supported')
        if kind == 'numeric':
            digits = tuple(ord(c) for c in element.get('digits', ''))
            m.require(len(digits) == 10 and len(set(digits)) == 10,
                      'decimal system must have ten distinct digits')
            m.require(all(0 <= c <= 0x10ffff and not 0xd800 <= c <= 0xdfff
                          for c in digits), 'invalid digit scalar')
            m.require(not element.get('rules'), 'numeric numbering has rules')
            value = (10, 0, digits)
        else:
            m.require(not element.get('digits'), 'algorithmic numbering has digits')
            rule = m.ascii_text(element.get('rules', ''))
            m.insert_unique(rules, identifier, rule, 'numbering rule')
            value = (10, 1, (0,) * 10)
        m.insert_unique(records, identifier, value, 'numbering system')
    m.require(records, 'empty numbering systems')
    return records, rules


def parse_region_data(data, calendar_aliases):
    root = m.parse_xml(data, 'supplementalData')
    fields = {name: {} for name in ('minDays', 'firstDay', 'weekendStart', 'weekendEnd')}
    for name, values in fields.items():
        for element in root.findall('./weekData/' + name):
            if element.get('alt') is not None:
                continue  # Fixed policy: CLDR standard, including GB Monday.
            if name == 'minDays':
                text = element.get('count', '')
                m.require(text in tuple(str(n) for n in range(1, 8)), 'invalid minimal days')
                value = int(text)
            else:
                m.require(element.get('day') in DAYS, 'invalid week day')
                value = DAYS[element.get('day')]
            keys = m.words(element.get('territories', ''))
            m.require(keys and len(set(keys)) == len(keys), 'empty/repeated week regions')
            for key in keys:
                m.insert_unique(values, region(key), value, name)
        m.require('001' in values, 'missing world week default: ' + name)
    territories = set().union(*(set(v) for v in fields.values()))
    weeks = []
    for key in sorted(territories):
        get = lambda name: fields[name].get(key, fields[name]['001'])
        start, end = get('weekendStart'), get('weekendEnd')
        mask = 0
        for offset in range((end - start) % 7 + 1):
            mask |= 1 << ((start - 1 + offset) % 7)
        weeks.append((key, get('firstDay'), mask, get('minDays')))
    calendars, hours, preferred = {}, {}, {}
    for element in root.findall('./calendarPreferenceData/calendarPreference'):
        values = []
        for token in m.words(element.get('ordering', '')):
            m.require(token.lower() in calendar_aliases, 'unknown calendar preference alias')
            values.append(calendar_aliases[token.lower()])
        m.require(values and len(set(values)) == len(values), 'empty/repeated calendar order')
        keys = m.words(element.get('territories', ''))
        m.require(keys and len(set(keys)) == len(keys), 'empty/repeated calendar regions')
        for key in keys:
            m.insert_unique(calendars, (0, region(key)), tuple(values), 'calendar preference')
    for element in root.findall('./timeData/hours'):
        values = m.words(element.get('allowed', ''))
        choice = element.get('preferred', '')
        m.require(values and len(set(values)) == len(values), 'empty/repeated hour order')
        m.require(all(re.fullmatch('[HhKk](?:[bB])?', v) for v in values),
                  'invalid raw hour symbol')
        m.require(choice in values, 'preferred hour symbol absent from allowed list')
        keys = m.words(element.get('regions', ''))
        m.require(keys and len(set(keys)) == len(keys), 'empty/repeated hour regions')
        for key in keys:
            key = preference_key(key)
            m.insert_unique(hours, key, values, 'hour preference')
            m.insert_unique(preferred, key, choice, 'preferred hour')
    m.require((0, '001') in calendars and (0, '001') in hours,
              'missing world calendar/hour default')
    preferences = []
    for scope, key in sorted(set(calendars) | set(hours)):
        raw = hours.get((scope, key), ())
        cycles = []
        for value in raw:
            cycle = CYCLES[value[0]]
            if cycle not in cycles:
                cycles.append(cycle)
        cycles = tuple(cycles)
        # Absent lists inherit 001 at lookup time; language-region calendar
        # lookup falls back through explicit region then 001.
        preferences.append((scope, key, calendars.get((scope, key), ()), cycles, raw))
    return tuple(weeks), tuple(preferences), tuple((scope, key, value)
                                                 for (scope, key), value in sorted(preferred.items()))


def collect(supplemental, numbering_xml, inputs, defaults, bcp):
    parents = parse_parents(supplemental)
    locales, identity_only = merge_locale_inputs(inputs)
    general, overrides = build_locale_graph(locales, defaults, parents)
    numbering, rules = parse_numbering(numbering_xml)
    calendar_aliases, collation_aliases = bcp_aliases(bcp, 'ca'), bcp_aliases(bcp, 'co')
    for tag, values in locales.items():
        for name in ('numbering', 'native', 'traditional', 'finance'):
            m.require(not values[name] or values[name] in numbering,
                      'unknown numbering default: ' + tag + '/' + name)
        value = values['collation']
        if value and value not in ('standard', 'search'):
            m.require(value.lower() in collation_aliases, 'unknown collation default')
            values['collation'] = collation_aliases[value.lower()]
    for tag in general:
        m.require(resolve_default(tag, 'numbering', locales, general, overrides),
                  'locale has no inherited numbering default: ' + tag)
        m.require(resolve_default(tag, 'direction', locales, general, overrides),
                  'locale has no inherited direction: ' + tag)
    weeks, preferences, hours = parse_region_data(supplemental, calendar_aliases)
    return {'locales': locales, 'parents': general, 'component_parents': overrides,
            'default_content': tuple(sorted(defaults)), 'numbering': numbering,
            'numbering_rules': rules, 'weeks': weeks, 'preferences': preferences,
            'preferred_hours': hours,
            'evidence': {'identity_only': identity_only, 'parent_rules': parents[3],
                         'direction_policy': 'horizontal characterOrder, or explicit horizontal lineOrder for vertical characterOrder; general inheritance; no script-name RTL inference; no vertical rendering claim',
                         'week_policy': 'standard (ignore alt records)',
                         'collation_policy': 'metadata only; standard/search and unsupported types are filtered by service engines',
                         'service_coverage': 0}}


def load_pinned(args, defaults, bcp, consumed, context=None):
    cldr = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)

    def read(relative):
        return m.verified_input(args.cldr_dir, cldr, relative, consumed, 'cldr', context)

    paths = sorted(path for path in cldr if path.endswith('.xml') and
                   (path.startswith('common/main/') or path.startswith('common/collation/')))
    # Direction uses CLDR characterOrder, never heuristics from UCD script
    # names. The baseline verifies final UCD18; unused script files are not
    # reported as consumed inputs. All actual parsed inputs are sealed hashes.
    return collect(read('common/supplemental/supplementalData.xml'),
                   read('common/supplemental/numberingSystems.xml'),
                   ((path, read(path)) for path in paths), defaults, bcp)


def strings(data):
    result = []
    for tag in data['parents']:
        result.extend((tag,) + components(tag))
        local = data['locales'].get(tag, {})
        result.extend(local.get(name, '') for name in
                      ('numbering', 'native', 'traditional', 'finance', 'collation'))
    result.extend(component for component, tag in data['component_parents'])
    result.extend(data['numbering'])
    result.extend(data['numbering_rules'].values())
    result.extend(record[0] for record in data['weeks'])
    for scope, key, calendars, cycles, raw in data['preferences']:
        result.append(key)
        result.extend(calendars + cycles + raw)
    for scope, key, value in data['preferred_hours']:
        result.extend((key, value))
    return result


def encode(data, pool):
    tags = sorted(data['parents'])
    indices = {tag: index for index, tag in enumerate(tags)}
    numbering = sorted(data['numbering'])
    number_indices = {name: index for index, name in enumerate(numbering)}
    parent_ref = lambda tag: struct.pack('<I', m.U32_LIMIT) if tag is None else m.u32(indices[tag])
    lists = sorted(set(values for scope, key, cal, cycles, raw in data['preferences']
                       for values in (cal, cycles, raw) if values))
    spans, list_records = {(): (0, 0)}, []
    for values in lists:
        spans[values] = (len(list_records), len(values))
        m.u32(len(list_records) + len(values))
        list_records.extend(pool.ref(v) for v in values)
    span = lambda values: b''.join(m.u32(v) for v in spans[values])
    sections = {18: b''.join(list_records)}
    sections[14] = b''.join(pool.ref(key) + bytes((first, mask, minimal, 0))
                            for key, first, mask, minimal in data['weeks'])
    sections[15] = b''.join(pool.ref(key) + m.u32(scope) + span(cal) + span(cycles) + span(raw)
                            for scope, key, cal, cycles, raw in data['preferences'])
    sections[16] = b''.join(pool.ref(tag) + parent_ref(data['parents'][tag]) +
                            b''.join(pool.ref(v) for v in components(tag)) +
                            pool.ref(data['locales'].get(tag, {}).get('numbering', '')) + m.u32(0)
                            for tag in tags)
    sections[17] = b''.join(pool.ref(name) + bytes((radix, algorithmic, 0, 0)) +
                            b''.join(m.u32(v) for v in digits)
                            for name in numbering
                            for radix, algorithmic, digits in (data['numbering'][name],))
    sections[19] = b''.join(pool.ref(component) + m.u32(indices[tag]) + parent_ref(parent) + m.u32(0)
                            for (component, tag), parent in sorted(data['component_parents'].items(),
                                                               key=lambda item: (item[0][0], indices[item[0][1]])))
    auxiliaries = []
    default_content = set(data['default_content'])
    for tag in tags:
        values = data['locales'].get(tag, {})
        direction = {'': 0, 'left-to-right': 1, 'right-to-left': 2}[values.get('direction', '')]
        extra = tuple(values.get(name, '') for name in ('native', 'traditional', 'finance', 'collation'))
        flags = int(tag in default_content)
        if flags or direction or any(extra):
            auxiliaries.append(m.u32(indices[tag]) + m.u32(flags) + m.u32(direction) +
                               b''.join(pool.ref(value) for value in extra))
    sections[21] = b''.join(auxiliaries)
    sections[22] = b''.join(m.u32(number_indices[name]) + pool.ref(value)
                            for name, value in sorted(data['numbering_rules'].items()))
    sections[23] = b''.join(pool.ref(key) + m.u32(scope) + pool.ref(value)
                            for scope, key, value in data['preferred_hours'])
    for section, payload in sections.items():
        m.require(len(payload) % WIDTHS[section] == 0, 'preference section width drift')
    return sections
