"""Pinned CLDR48.2 sixteen-calendar DateTimeFormat extraction; host-only Python.

Runtime uses packed wire1.3 records100..107. Imports the frozen metadata and
PREFv2 modules; no formatter, ICU, JSON/XML runtime, zlib, or system timezone.
Exact Unicode text is preserved. Data existence never activates a service.
"""
import re
import struct
import metadata as m
import locale_subset

WIDTHS = {100: 32, 101: 28, 102: 48, 103: 16, 104: 72, 105: 32, 106: 16, 107: 20, 108: 36}
STYLES = ('full', 'long', 'medium', 'short')
PERIODS = ('am', 'pm', 'midnight', 'noon', 'morning1', 'morning2',
           'afternoon1', 'afternoon2', 'evening1', 'evening2', 'night1', 'night2')
WEEKDAYS = ('mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun')
INHERIT, MISSING = '\u2191\u2191\u2191', '\u2205\u2205\u2205'
# All non-alt distinguishing attributes in the pinned ldml.dtd dates/numbers
# subtrees. Grammatical minimal pairs and Hebrew leap-month names must retain
# their identity even though collect() emits only Gregorian/date symbol data.
# draft/references are metadata; numbers and symbol choice are value attributes.
DISTINGUISHING = frozenset(('type', 'id', 'count', 'numberSystem', 'request',
                          'ordinal', 'case', 'gender', 'yeartype'))
# ElementTree does not supply the pinned DTD's default type=standard.
STANDARD_TYPE = frozenset(('dateFormat', 'timeFormat', 'dateTimeFormat', 'pattern'))


def scalar_text(value):
    m.require(isinstance(value, str) and all(ord(c) != 0 and
              not 0xd800 <= ord(c) <= 0xdfff for c in value), 'invalid date text')
    return value


def segment(tag, **attrs):
    if tag in STANDARD_TYPE and 'type' not in attrs:
        attrs['type'] = 'standard'
    return tag, tuple(sorted(attrs.items()))


def path(*items):
    return tuple(segment(item) if isinstance(item, str) else item for item in items)


CALENDAR = path('dates', 'calendars', segment('calendar', type='gregorian'))
CALENDARS = (('iso8601', 'iso8601'), ('buddhist', 'buddhist'),
             ('chinese', 'chinese'), ('coptic', 'coptic'), ('dangi', 'dangi'),
             ('ethioaa', 'ethiopic-amete-alem'), ('ethiopic', 'ethiopic'),
             ('gregory', 'gregorian'), ('hebrew', 'hebrew'), ('indian', 'indian'),
             ('islamic-civil', 'islamic-civil'), ('islamic-tbla', 'islamic-tbla'),
             ('islamic-umalqura', 'islamic-umalqura'), ('japanese', 'japanese'),
             ('persian', 'persian'), ('roc', 'roc'))
# Section101 stable identities. They are data keys, never invented labels.
# Hebrew14 is the distinct CLDR month7@yeartype=leap. Japanese237/238 are
# exact Gregorian BCE/CE labels for the proposal's pre-1873 Gregorian eras.
HEBREW_ADAR_II = 14
JAPANESE_BCE, JAPANESE_CE = 237, 238
NAME_CYCLIC_YEAR, NAME_LEAP_TEMPLATE = 4, 5
TZ = path('dates', 'timeZoneNames')


def alias_path(base, expression):
    """Resolve the explicit CLDR relative XPath subset, including slashes in
    quoted zone attributes. Unsupported XPath fails only when consumed.
    """
    pieces, start, quote, depth = [], 0, None, 0
    for i, ch in enumerate(expression):
        if quote:
            if ch == quote:
                quote = None
        elif ch in ('\"', "'"):
            quote = ch
        elif ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
            m.require(depth >= 0, 'unbalanced date alias XPath')
        elif ch == '/' and depth == 0:
            pieces.append(expression[start:i])
            start = i + 1
    pieces.append(expression[start:])
    m.require(not quote and depth == 0 and all(pieces), 'invalid date alias XPath')
    result = list(base)
    for item in pieces:
        if item == '.':
            continue
        if item == '..':
            m.require(result, 'date alias escapes ldml')
            result.pop()
            continue
        match = re.fullmatch(r'([A-Za-z][A-Za-z0-9]*)(.*)', item)
        m.require(match is not None, 'unsupported date alias XPath node')
        attrs = {}
        remainder = match.group(2)
        while remainder:
            predicate = re.match(r"\[@([A-Za-z][A-Za-z0-9]*)=(['\"])(.*?)\2\]", remainder)
            m.require(predicate is not None and predicate.group(1) in DISTINGUISHING,
                      'unsupported date alias predicate')
            m.require(predicate.group(1) not in attrs, 'duplicate date alias predicate')
            attrs[predicate.group(1)] = predicate.group(3)
            remainder = remainder[predicate.end():]
        result.append(segment(match.group(1), **attrs))
    m.require(len(result) <= 64, 'date alias path growth')
    return tuple(result)


def flatten(data, source='<input>'):
    """Retain standard (no-alt) LDML paths, including every distinguishing
    attribute in dates/numbers and standard territory display names used by
    timezone composition. Distinct unconsumed records cannot collide or replace
    a consumed record; genuine duplicate standard leaves still fail.
    """
    root = m.parse_xml(data, 'ldml')
    values, aliases = {}, {}

    def walk(node, current):
        if node.get('alt') is not None:
            return
        if node.tag == 'alias':
            m.require(current not in aliases and current not in values,
                      'duplicate/mixed date alias')
            aliases[current] = (node.get('source', ''), node.get('path', ''))
            return
        attrs = {k: value for k, value in node.attrib.items() if k in DISTINGUISHING}
        current = current + (segment(node.tag, **attrs),)
        if not len(node):
            value = node.text or ''
            if value == INHERIT:
                return
            if current in values:
                raise m.DataError('duplicate standard date leaf: {}: {!r}'.format(source, current))
            values[current] = (scalar_text(value), node.get('numbers', ''))
            return
        for child in node:
            walk(child, current)

    for child in root:
        if child.tag in ('dates', 'numbers', 'alias'):
            walk(child, ())
        elif child.tag == 'localeDisplayNames':
            # Date zone composition needs the standard country name when no
            # short name exists and for preferred-zone country qualifiers.
            for group in child:
                if group.tag == 'territories':
                    walk(group, (segment('localeDisplayNames'),))
    return values, aliases


class Resolver:
    def __init__(self, inputs, parents, component_parents=None):
        self.locales = {}
        self.sources = {}
        self.parents = parents
        self.component_parents = component_parents or {}
        self.cache = {}
        for logical, data in sorted(inputs):
            m.require(logical.startswith('common/main/') and logical.endswith('.xml'),
                      'date input is not common/main XML')
            tag = logical.rsplit('/', 1)[1][:-4]
            tag = 'root' if tag == 'root' else m.canonical_tag(tag)
            m.require(tag not in self.locales and tag in parents, 'duplicate/unknown date locale')
            self.locales[tag] = flatten(data, logical)
            self.sources[tag] = logical
        m.require('root' in self.locales, 'missing root date data')

    def leaf(self, requested, wanted):
        cache_key = requested, wanted
        if cache_key in self.cache:
            result, self.last_origin = self.cache[cache_key]
            return result
        original, current, visited = requested, requested, set()
        for unused in range(max(64, len(self.parents) * 8)):
            if current is None:
                result = ('', '')
                break
            state = original, current, wanted
            m.require(state not in visited, 'date alias/parent cycle')
            visited.add(state)
            m.require(current in self.parents, 'date alias locale outside metadata')
            values, aliases = self.locales.get(current, ({}, {}))
            if wanted in values:
                result = values[wanted]
                if result[0] == MISSING:
                    result = ('', '')  # explicit absence blocks inheritance
                break
            redirected = False
            for n in range(len(wanted), -1, -1):
                prefix = wanted[:n]
                if prefix not in aliases:
                    continue
                source, expression = aliases[prefix]
                m.require(source, 'empty date alias source')
                target = alias_path(prefix, expression)
                wanted = target + wanted[n:]
                if source != 'locale':
                    original = 'root' if source == 'root' else m.canonical_tag(source)
                current, redirected = original, True
                break
            if not redirected:
                if wanted and wanted[0][0] == 'localeDisplayNames':
                    # Territory display names inherit through general parents,
                    # rather than the component-specific dates parent graph.
                    current = self.parents[current]
                else:
                    current = self.component_parents.get(('dates', current), self.parents[current])
        else:
            raise m.DataError('date alias substitution bound exceeded')
        self.last_origin = (self.sources.get(current, str(current)), wanted)
        self.cache[cache_key] = result, self.last_origin
        return result

    def text(self, requested, wanted):
        return self.leaf(requested, wanted)[0]


def pattern_fields(value, source='<input>', unsupported=None):
    """Independent host validation of the same documented LDML subset.
    Legal CLDR48.2 ordinal ddd and other unimplemented fields return None.
    Complete lexical validation still runs after unsupported fields. Malformed
    supported fields/quotation fail with exact source, value and token location.
    a is implicit and never resolves dayPeriod; ddd never becomes numeric d.
    """
    offset, ch, n = 0, '', 0

    def require(condition, message):
        if not condition:
            raise m.DataError('{}: {}; pattern={!r}; offset={}; field={!r}; width={}'.format(
                source, message, value, offset, ch, n))

    try:
        scalar_text(value)
    except m.DataError as exc:
        raise m.DataError('{}: {}; pattern={!r}'.format(source, exc, value))
    reasons = []
    fields, family, quote, i, year_symbols = [-1] * 11, 0, False, 0, set()
    while i < len(value):
        offset = i
        ch = value[i]
        i += 1
        if ch == "'":
            if i < len(value) and value[i] == "'":
                i += 1
            else:
                quote = not quote
            continue
        if quote or not ('a' <= ch <= 'z' or 'A' <= ch <= 'Z'):
            continue
        n = 1
        while i < len(value) and value[i] == ch:
            i, n = i + 1, n + 1
        f, width = None, -1
        if ch == 'G':
            require(n <= 5, 'invalid era pattern width')
            f, width = 1, 2 if n == 5 else 4 if n == 4 else 3
        elif ch == 'y':
            require(n <= 20, 'invalid year pattern width')
            f, width = 2, 0 if n == 2 else 1
        elif ch == 'r':
            require(n <= 20, 'invalid related-year pattern width')
            f, width = 2, 1
        elif ch == 'U':
            require(n <= 5, 'invalid cyclic-year pattern width')
            f, width = 2, 1
        elif ch in 'ML':
            require(n <= 5, 'invalid month pattern width')
            f, width = 3, {1: 1, 2: 0, 3: 3, 4: 4, 5: 2}[n]
        elif ch == 'd':
            # CLDR48.2 has abbreviated ordinal day names (ddd) and their
            # dayOfMonths labels. Native records implement numeric/2-digit d
            # only; preserving an unsupported gap avoids ordinal corruption.
            require(n <= 3, 'invalid day pattern width')
            if n == 3:
                reasons.append('unsupported ordinal day field ddd')
            f, width = 4, 0 if n == 2 else 1
        elif ch in 'Eec':
            require(n <= 6, 'invalid weekday pattern width')
            if ch != 'E' and n < 3:
                reasons.append('unsupported numeric weekday field ' + ch * n)
            f, width = 0, 2 if n == 5 else 4 if n == 4 else 3
        elif ch in 'aBb':
            require(n <= 5, 'invalid day-period pattern width')
            if ch != 'a':
                f, width = 5, 2 if n == 5 else 4 if n == 4 else 3
        elif ch in 'hHKkms':
            require(n <= 2, 'invalid time pattern width')
            f, width = 7 if ch == 'm' else 8 if ch == 's' else 6, 0 if n == 2 else 1
            if ch in 'hK':
                family = 1
            elif ch in 'Hk':
                family = 2
        elif ch == 'S':
            if n > 3:
                reasons.append('unsupported fractional-second field ' + ch * n)
            f, width = 9, n
        elif ch in 'zvO':
            require(n <= 4 and (ch == 'z' or n in (1, 4)), 'invalid zone pattern width')
            f = 10
            width = (1 if n == 4 else 0) + (4 if ch == 'v' else 2 if ch == 'O' else 0)
        else:
            reasons.append('unsupported date field ' + ch * n)
        if f is not None:
            if f == 2:
                # r(U) is one public year component with two different parts.
                # Repeated symbols, or y mixed with r/U, remain malformed.
                require(ch not in year_symbols and
                        (not year_symbols or ch in 'rU' and year_symbols <= {'r', 'U'}),
                        'duplicate date pattern year field')
                year_symbols.add(ch)
            else:
                require(fields[f] == -1, 'duplicate date pattern field')
            fields[f] = width
    if quote:
        offset, ch, n = len(value), "'", 0
    require(not quote, 'unclosed date pattern quote')
    if unsupported is not None:
        unsupported.extend(reasons)
    return None if reasons else (tuple(fields), family)


def placeholders(value, count, quoted=False):
    seen, quote, i = set(), False, 0
    scalar_text(value)
    while i < len(value):
        ch = value[i]
        if quoted and ch == "'":
            if i + 1 < len(value) and value[i + 1] == "'":
                i += 2
                continue
            quote = not quote
        if not quote and ch in '{}':
            m.require(ch == '{' and i + 2 < len(value) and value[i + 2] == '}' and
                      value[i + 1] in tuple(str(n) for n in range(count)), 'invalid date placeholder')
            number = int(value[i + 1])
            m.require(number not in seen, 'duplicate date placeholder')
            seen.add(number)
            i += 2
        i += 1
    m.require(not quote and seen == set(range(count)), 'missing date placeholder')
    return value


def time_second(value, allow_24=False):
    m.require(re.fullmatch(r'[0-2][0-9]:[0-5][0-9]', value or ''), 'invalid day-period time')
    hour, minute = (int(x) for x in value.split(':'))
    m.require(hour < 24 or (allow_24 and hour == 24 and minute == 0), 'invalid day-period boundary')
    return hour * 3600 + minute * 60


def day_periods(data):
    root = m.parse_xml(data, 'supplementalData')
    result = {}
    for group in root.findall('dayPeriodRuleSet'):
        if group.get('type') is not None:
            continue
        for rules in group.findall('dayPeriodRules'):
            current = []
            for element in rules:
                m.require(element.tag == 'dayPeriodRule' and element.get('type') in PERIODS,
                          'unknown day-period rule')
                period = PERIODS.index(element.get('type'))
                if element.get('at') is not None:
                    m.require(set(element.attrib) == {'type', 'at'}, 'mixed exact day-period rule')
                    second = time_second(element.get('at'), True) % 86400
                    current.append((period, 1, second, second))
                else:
                    m.require(set(element.attrib) == {'type', 'from', 'before'}, 'unsupported day-period interval')
                    first = time_second(element.get('from'))
                    before = time_second(element.get('before'), True)
                    m.require(first != before, 'empty day-period interval')
                    current.append((period, 0, first, before))
            for i, a in enumerate(current):
                for b in current[:i]:
                    if a[1] and b[1]:
                        m.require(a[2] != b[2], 'duplicate exact day-period point')
                    elif not a[1] and not b[1]:
                        def contains(rule, second):
                            return (rule[2] <= second < rule[3] if rule[2] < rule[3]
                                    else second >= rule[2] or second < rule[3])
                        m.require(not contains(a, b[2]) and not contains(b, a[2]),
                                  'overlapping day-period intervals')
            for tag in m.words(rules.get('locales', '')):
                tag = 'root' if tag == 'root' else m.canonical_tag(tag)
                m.insert_unique(result, tag, tuple(sorted(current)), 'day-period locale')
    m.require('root' in result, 'missing root day-period rules')
    return result


def epoch_ms(value, fallback):
    # Proleptic Gregorian ordinal arithmetic. No host localtime/locale/TZ.
    if value is None:
        return fallback
    import datetime
    # CLDR48.2 has minute boundaries and Africa/Monrovia's 00:44:30 boundary.
    m.require(re.fullmatch(r'[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}(?::[0-9]{2})?', value),
              'invalid metazone boundary: {!r}'.format(value))
    fields = [int(x) for x in re.split('[- :]', value)]
    year, month, day, hour, minute = fields[:5]
    second = fields[5] if len(fields) == 6 else 0
    m.require(hour < 24 and minute < 60 and second < 60, 'invalid metazone clock')
    try:
        days = datetime.date(year, month, day).toordinal() - datetime.date(1970, 1, 1).toordinal()
    except ValueError as exc:
        raise m.DataError('invalid metazone date: {}'.format(exc))
    return (days * 86400 + hour * 3600 + minute * 60 + second) * 1000


def metazones(data):
    root = m.parse_xml(data, 'supplementalData')
    result = []
    for zone in root.findall('./metaZones/metazoneInfo/timezone'):
        key = zone.get('type', '')
        m.require(key, 'empty metazone zone')
        previous = -(1 << 63)
        for element in zone:
            m.require(element.tag == 'usesMetazone' and element.get('mzone'), 'invalid metazone record')
            first = epoch_ms(element.get('from'), -(1 << 63))
            before = epoch_ms(element.get('to'), (1 << 63) - 1)
            m.require(previous <= first < before, 'overlapping/unsorted metazone periods')
            previous = before
            result.append((key, element.get('mzone'), first, before))
    m.require(result, 'missing metazone periods')
    return tuple(sorted(result, key=lambda row: (row[0], row[2])))


def zone_aliases(data, known):
    root = m.parse_xml(data, 'ldmlBCP47')
    result = {}
    for key in root.findall('./keyword/key'):
        if key.get('name') != 'tz':
            continue
        for element in key.findall('type'):
            names = m.words(element.get('alias', ''))
            if not names:
                continue
            # Explicit CLDR aliases may keep a historical spelling while
            # the native IANA provider accepts a newer primary identifier.
            target = next((name for name in names if name in known), names[0])
            for name in names:
                m.insert_unique(result, name, target, 'CLDR timezone alias')
    return tuple(sorted(result.items()))


def collect_calendar(resolver, index, tag, canonical, ldml, available_ids,
                     era_ids, patterns, names, fallbacks, gaps, origins):
    """Resolve each actual calendar independently through its CLDR aliases.
    Name indices preserve CLDR month/era identity; only the Hebrew leap leaf
    and proposal Gregorian Japanese eras use explicitly documented extra IDs.
    """
    calendar = path('dates', 'calendars', segment('calendar', type=ldml))

    def leaf(current, kind, key):
        result = resolver.leaf(tag, current)
        if result[0]:
            source, resolved = resolver.last_origin
            origins.append((kind, index, canonical, key, source, repr(resolved)))
        return result

    def label(current, field, context, width, number):
        value, override = leaf(current, 'name', str((field, context, width, number)))
        m.require(not override, 'numbering override on calendar name')
        if value:
            names.append((index, canonical, field, context, width, number, value))

    fallback, override = leaf(calendar + path('dateTimeFormats', 'intervalFormats',
        'intervalFormatFallback'), 'range_fallback', '')
    m.require(fallback and not override, 'missing/numbered calendar interval fallback: ' + tag + '/' + canonical)
    m.require("'" not in fallback, 'unsupported quoted interval fallback: ' + tag + '/' + canonical)
    placeholders(fallback, 2)
    fallbacks.append((index, canonical, fallback))
    for skeleton in sorted(available_ids):
        current = calendar + path('dateTimeFormats', 'availableFormats', segment('dateFormatItem', id=skeleton))
        value, override = leaf(current, 'available', skeleton)
        if not value:
            continue
        source, resolved = resolver.last_origin
        reasons = []
        parsed = pattern_fields(value, '{}: requested={}; calendar={}; path={!r}'.format(
            source, tag, canonical, resolved), reasons)
        if parsed is None or override:
            if override:
                reasons.append('unsupported numbering override ' + override)
            gaps.append((tag, canonical + '/available', skeleton, '; '.join(reasons) +
                '; source={}; path={!r}; pattern={!r}'.format(source, resolved, value)))
            continue
        patterns.append((index, canonical, 0, 255, parsed[1], skeleton, value))
    for style, style_name in enumerate(STYLES):
        for kind, group, length_tag, format_tag in (
                (1, 'dateFormats', 'dateFormatLength', 'dateFormat'),
                (2, 'timeFormats', 'timeFormatLength', 'timeFormat'),
                (3, 'dateTimeFormats', 'dateTimeFormatLength', 'dateTimeFormat')):
            current = calendar + path(group, segment(length_tag, type=style_name), format_tag, 'pattern')
            value, override = leaf(current, 'style', str((kind, style)))
            m.require(value, 'missing calendar style: {}/{}/{}/{}'.format(tag, canonical, kind, style_name))
            source, resolved = resolver.last_origin
            reasons = []
            parsed = ((), 0) if kind == 3 else pattern_fields(value,
                '{}: requested={}; calendar={}; path={!r}'.format(source, tag, canonical, resolved), reasons)
            if parsed is None or override:
                if override:
                    reasons.append('unsupported numbering override ' + override)
                gaps.append((tag, canonical + '/style', str((kind, style)), '; '.join(reasons) +
                    '; source={}; path={!r}; pattern={!r}'.format(source, resolved, value)))
                continue
            if kind == 3:
                placeholders(value, 2, True)
            patterns.append((index, canonical, kind, style, parsed[1], '', value))
    for context, context_name in enumerate(('format', 'stand-alone')):
        for width, width_name in enumerate(('narrow', 'abbreviated', 'wide', 'short')):
            for field, group, context_tag, width_tag, item_tag, indices in (
                    (1, 'months', 'monthContext', 'monthWidth', 'month', tuple(str(n) for n in range(1, 14))),
                    (2, 'days', 'dayContext', 'dayWidth', 'day', WEEKDAYS),
                    (3, 'dayPeriods', 'dayPeriodContext', 'dayPeriodWidth', 'dayPeriod', PERIODS)):
                if width == 3 and field != 2:
                    continue
                for number, source_index in enumerate(indices):
                    current = calendar + path(group, segment(context_tag, type=context_name),
                        segment(width_tag, type=width_name), segment(item_tag, type=source_index))
                    label(current, field, context, width, number + 1 if field in (1, 2) else number)
            if width < 3 and canonical == 'hebrew':
                current = calendar + path('months', segment('monthContext', type=context_name),
                    segment('monthWidth', type=width_name), segment('month', type='7', yeartype='leap'))
                label(current, 1, context, width, HEBREW_ADAR_II)
            if width < 3 and canonical in ('chinese', 'dangi'):
                current = calendar + path('monthPatterns', segment('monthPatternContext', type=context_name),
                    segment('monthPatternWidth', type=width_name), segment('monthPattern', type='leap'))
                value, override = leaf(current, 'leap_template', str((context, width, 1)))
                m.require(value and not override and "'" not in value, 'missing/unsupported named leap-month template')
                placeholders(value, 1)
                names.append((index, canonical, NAME_LEAP_TEMPLATE, context, width, 1, value))
    for width, era_group in enumerate(('eraNarrow', 'eraAbbr', 'eraNames')):
        for era in sorted(era_ids):
            label(calendar + path('eras', era_group, segment('era', type=str(era))), 0, 0, width, era)
        if canonical == 'japanese':
            for source_era, target_era in ((0, JAPANESE_BCE), (1, JAPANESE_CE)):
                label(CALENDAR + path('eras', era_group, segment('era', type=str(source_era))),
                      0, 0, width, target_era)
    if canonical in ('chinese', 'dangi'):
        for width, width_name in enumerate(('narrow', 'abbreviated', 'wide')):
            for year in range(1, 61):
                current = calendar + path('cyclicNameSets', segment('cyclicNameSet', type='years'),
                    segment('cyclicNameContext', type='format'), segment('cyclicNameWidth', type=width_name),
                    segment('cyclicName', type=str(year)))
                label(current, NAME_CYCLIC_YEAR, 0, width, year)
        current = calendar + path('monthPatterns', segment('monthPatternContext', type='numeric'),
            segment('monthPatternWidth', type='all'), segment('monthPattern', type='leap'))
        value, override = leaf(current, 'leap_template', str((0, 0, 0)))
        m.require(value and not override and "'" not in value, 'missing/unsupported numeric leap-month template')
        placeholders(value, 1)
        names.append((index, canonical, NAME_LEAP_TEMPLATE, 0, 0, 0, value))


def zone_format_records(inputs, metadata, resolver, meta_xml, timezone_xml,
                        likely_xml, meta, aliases):
    """Compile CLDR generic-location and preferred-zone name composition.

    No IANA offset/DST inference: these records contain only immutable CLDR
    strings and zone/metazone identities. The runtime selects active periods.
    """
    if likely_xml is None:
        # Legacy synthetic collector fixtures have no new auxiliary input.
        return ()
    alias = dict(aliases)
    canonical = lambda name: alias.get(name, name)
    countries, by_country = {}, {}
    root = m.parse_xml(timezone_xml, 'ldmlBCP47')
    for element in root.findall('./keyword/key[@name="tz"]/type'):
        names = m.words(element.get('alias', ''))
        if not names or element.get('preferred'):
            continue
        name = canonical(names[0])
        short = element.get('name', '')
        # TR35 Time_Zone_Identifiers: length5 prefix region, overridden by
        # explicit region; other lengths have no inferred region.
        region = element.get('region', short[:2].upper() if len(short) == 5 else '')
        if region and region not in ('001', 'ZZ'):
            m.require(name not in countries or countries[name] == region, 'conflicting Date zone countries')
            countries[name] = region
            by_country.setdefault(region, set()).add(name)
    doc = m.parse_xml(meta_xml, 'supplementalData')
    preferred = {}
    for element in doc.findall('./metaZones/mapTimezones/mapZone'):
        names = m.words(element.get('type', ''))
        m.require(len(names) == 1, 'metazone preferred mapping must name one zone')
        m.insert_unique(preferred, (element.get('other', ''), element.get('territory', '')),
                        canonical(names[0]), 'Date metazone preferred zone')
    primary = {canonical(e.text or '') for e in doc.findall('./primaryZones/primaryZone')}
    likely = dict(m.parse_likely(likely_xml))
    short_countries = {}
    for logical, content in inputs:
        tag = locale_subset.tag(logical.rsplit('/', 1)[-1][:-4])
        doc = m.parse_xml(content, 'ldml')
        values = {}
        for node in doc.findall('./localeDisplayNames/territories/territory[@alt="short"]'):
            value = node.text or ''
            if value not in ('', INHERIT, MISSING):
                m.insert_unique(values, node.get('type', ''), scalar_text(value), 'short country name')
        short_countries[tag] = values
    def locale_region(tag):
        if tag == 'root':
            return '001'
        fields = tag.split('-')
        for value in fields[1:]:
            if re.fullmatch(r'[A-Z]{2}|[0-9]{3}', value):
                return value
        expanded = likely.get(tag, likely.get(fields[0], ''))
        return expanded.rsplit('-', 1)[-1] if expanded else '001'
    def country_label(tag, region, short):
        if short:
            current = tag
            while current is not None:
                value = short_countries.get(current, {}).get(region, '')
                if value:
                    return value
                current = metadata['parents'][current]
        return resolver.text(tag, path('localeDisplayNames', 'territories',
            segment('territory', type=region))) or region
    def composed(template, value):
        placeholders(template, 1)
        return template.replace('{0}', value)
    pairs = {(canonical(zone), name) for zone, name, unused_from, unused_before in meta}
    by_zone = {}
    for zone, name in sorted(pairs):
        by_zone.setdefault(zone, []).append(name)
    zones = {canonical(zone) for zone in countries} | {zone for zone, unused in pairs}
    result = []
    for index, tag in enumerate(sorted(metadata['parents'])):
        locale_country = locale_region(tag)
        region_format = resolver.text(tag, TZ + path('regionFormat'))
        fallback = resolver.text(tag, TZ + path('fallbackFormat'))
        placeholders(region_format, 1)
        placeholders(fallback, 2)
        for zone in sorted(zones):
            country = countries.get(zone, '')
            city = resolver.text(tag, TZ + path(segment('zone', type=zone), 'exemplarCity'))
            if not city:
                city = zone.rsplit('/', 1)[-1].replace('_', ' ')
            location = ''
            if country:
                value = country_label(tag, country, True) if len(by_country[country]) == 1 or zone in primary else city
                location = composed(region_format, value)
            result.append((index, zone, '', location, ''))
            for metazone in by_zone.get(zone, ()):
                best = preferred.get((metazone, locale_country), preferred.get((metazone, '001'), ''))
                pattern = ''
                if best != zone:
                    value = country_label(tag, country, False) if country and preferred.get((metazone, country)) == zone else city
                    # Keep one argument for the runtime-selected metazone label.
                    # Replace literal tokens in one pass so argument text cannot
                    # become a template token during a second substitution.
                    pattern = re.sub(r'\{[01]\}', lambda match: value if match.group() == '{0}' else '{0}', fallback)
                    placeholders(pattern, 1)
                result.append((index, zone, metazone, location, pattern))
    return tuple(sorted(result))


def collect(inputs, metadata, period_xml, meta_xml, timezone_xml, likely_xml=None):
    inputs = tuple(inputs)
    resolver = Resolver(inputs, metadata['parents'], metadata.get('component_parents'))
    period_data, meta = day_periods(period_xml), metazones(meta_xml)
    available_ids, era_ids, zone_keys = set(), set(), set()
    for values, unused_aliases in resolver.locales.values():
        for current in values:
            if len(current) >= 6 and current[-2][0] == 'availableFormats' and current[-1][0] == 'dateFormatItem':
                attrs = dict(current[-1][1])
                if 'count' not in attrs:
                    available_ids.add(attrs['id'])
            if len(current) == 6 and current[3][0] == 'eras' and current[-1][0] == 'era':
                raw = dict(current[-1][1]).get('type', '')
                m.require(re.fullmatch(r'[0-9]+', raw), 'invalid CLDR era index')
                era_ids.add(int(raw))
            if len(current) >= 3 and current[:2] == TZ and current[2][0] in ('zone', 'metazone'):
                zone_keys.add((current[2][0], dict(current[2][1])['type']))
    patterns, names, symbols, rules, zone_names, fallbacks, gaps, origins = [], [], [], [], [], [], [], []
    numeric = [(i, identifier) for i, identifier in enumerate(sorted(metadata['numbering']))
               if metadata['numbering'][identifier][1] == 0]
    for index, tag in enumerate(sorted(metadata['parents'])):
        def resolve(current):
            return resolver.text(tag, current)

        for canonical, ldml in CALENDARS:
            collect_calendar(resolver, index, tag, canonical, ldml, available_ids,
                             era_ids, patterns, names, fallbacks, gaps, origins)
        gmt = placeholders(resolve(TZ + path('gmtFormat')), 1)
        zero = resolve(TZ + path('gmtZeroFormat'))
        if not zero:
            # CLDR48.2 removed many explicit gmtZeroFormat records. Derive the
            # zero form from the exact localized GMT template, preserving
            # prefix/suffix characters; do not substitute an ASCII constant.
            zero = gmt.replace('{0}', '')
        hour = resolve(TZ + path('hourFormat'))
        m.require(hour.count(';') == 1, 'invalid timezone hourFormat')
        positive, negative = hour.split(';')
        m.require(re.fullmatch(r'[^Hm]*H{1,2}[^Hm]*mm[^Hm]*', positive) and
                  re.fullmatch(r'[^Hm]*H{1,2}[^Hm]*mm[^Hm]*', negative), 'unsupported timezone hourFormat')
        for numbering_index, identifier in numeric:
            decimal = resolve(path('numbers', segment('symbols', numberSystem=identifier), 'decimal'))
            if decimal:
                symbols.append((index, numbering_index, decimal, gmt, zero, positive, negative))
            else:
                gaps.append((tag, 'numbering', identifier, 'no inherited decimal symbol'))
        current = tag
        while current not in period_data:
            m.require(current is not None, 'missing inherited day-period rules')
            current = metadata['parents'][current]
        rules.extend((index,) + item for item in period_data[current])
        for kind, key in sorted(zone_keys):
            prefix = TZ + path(segment(kind, type=key))
            labels = tuple(resolve(prefix + path(width, field)) for width in ('short', 'long')
                           for field in ('standard', 'daylight', 'generic'))
            exemplar = resolve(prefix + path('exemplarCity')) if kind == 'zone' else ''
            if any(labels) or exemplar:
                zone_names.append((index, int(kind == 'metazone'), key) + labels + (exemplar,))
    aliases = zone_aliases(timezone_xml, {item[0] for item in meta} | {key for kind, key in zone_keys if kind == 'zone'})
    formats = zone_format_records(inputs, metadata, resolver, meta_xml, timezone_xml,
                                  likely_xml, meta, aliases)
    return {'patterns': tuple(sorted(patterns)), 'names': tuple(sorted(names)),
            'symbols': tuple(sorted(symbols)), 'rules': tuple(sorted(rules)),
            'zones': tuple(sorted(zone_names)), 'meta': meta, 'aliases': aliases,
            'range_fallbacks': tuple(sorted(fallbacks)), 'zone_formats': formats,
            'evidence': {'calendars': tuple(canonical for canonical, unused in CALENDARS),
                         'calendar_policy': 'each canonical calendar resolves actual CLDR calendar subtree and aliases; ISO8601 has its own source patterns',
                         'record_origins': tuple(sorted(origins)),
                         'name_index_policy': {'hebrew14': 'CLDR month7@yeartype=leap',
                            'japanese237': 'exact Gregorian BCE label for proposal pre-1873 Gregorian era',
                            'japanese238': 'exact Gregorian CE label for proposal pre-1873 Gregorian era',
                            'field4': 'actual cyclicNameSet years1..60',
                            'field5': 'actual monthPattern leap; index0 numeric/all; index1 context/width'},
                         'text_policy': 'standard records; exact UTF8, quotes, bidi and U+202F preserved',
                         'parent_policy': 'frozen PREFv2 graph; explicit aliases in requesting locale',
                         'gmt_zero': 'explicit value or exact gmtFormat with {0} removed',
                         'unsupported_records': tuple(sorted(gaps)),
                         'range': 'complete endpoint patterns for every Table 6 comparator; exact inherited fallback and placeholder order', 'service_coverage': 0}}


def load_pinned(args, metadata, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)

    def read(logical):
        return m.verified_input(args.cldr_dir, manifest, logical, consumed, 'cldr', context)

    inputs = ((logical, read(logical)) for logical in locale_subset.main_paths(manifest, metadata))
    return collect(inputs, metadata, read('common/supplemental/dayPeriods.xml'),
                   read('common/supplemental/metaZones.xml'), read('common/bcp47/timezone.xml'),
                   read('common/supplemental/likelySubtags.xml'))


def strings(data):
    values = []
    for row in data['patterns']:
        values.extend((row[1], row[5], row[6]))
    for row in data['names']:
        values.extend((row[1], row[6]))
    for row in data['symbols']:
        values.extend(row[2:])
    for row in data['zones']:
        values.extend(row[2:])
    for row in data['meta']:
        values.extend(row[:2])
    for row in data['aliases']:
        values.extend(row)
    for row in data['range_fallbacks']:
        values.extend(row[1:])
    for row in data['zone_formats']:
        values.extend(row[1:])
    return tuple(values)


def encode(data, pool):
    sections = {}
    sections[100] = b''.join(m.u32(row[0]) + pool.ref(row[1]) +
        struct.pack('<BBBB', row[2], row[3], row[4], 0) + pool.ref(row[5]) + pool.ref(row[6])
        for row in data['patterns'])
    sections[101] = b''.join(m.u32(row[0]) + pool.ref(row[1]) +
        struct.pack('<BBBB', row[2], row[3], row[4], 0) + m.u32(row[5]) + pool.ref(row[6])
        for row in data['names'])
    sections[102] = b''.join(m.u32(row[0]) + m.u32(row[1]) + b''.join(pool.ref(value) for value in row[2:])
        for row in data['symbols'])
    sections[103] = b''.join(m.u32(row[0]) + struct.pack('<BBH', row[1], row[2], 0) + m.u32(row[3]) + m.u32(row[4])
        for row in data['rules'])
    sections[104] = b''.join(m.u32(row[0]) + struct.pack('<B3x', row[1]) + b''.join(pool.ref(value) for value in row[2:])
        for row in data['zones'])
    sections[105] = b''.join(pool.ref(row[0]) + pool.ref(row[1]) + struct.pack('<qq', row[2], row[3])
        for row in data['meta'])
    sections[106] = b''.join(pool.ref(row[0]) + pool.ref(row[1]) for row in data['aliases'])
    sections[107] = b''.join(m.u32(row[0]) + pool.ref(row[1]) + pool.ref(row[2])
        for row in data['range_fallbacks'])
    sections[108] = b''.join(m.u32(row[0]) + b''.join(pool.ref(value) for value in row[1:])
        for row in data['zone_formats'])
    for section_id, value in sections.items():
        m.require(len(value) % WIDTHS[section_id] == 0, 'DateTimeFormat record width drift')
    return sections
