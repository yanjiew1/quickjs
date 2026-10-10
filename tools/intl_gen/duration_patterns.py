"""Original CLDR48.2 DurationFormat host extractor. No runtime XML/JSON.

Reuses shared NumberFormat exact LDML resolver, metadata parent graph, numeric
numbering enumeration and scalar pool. Emits only optional clock section110;
units/plurals/digits/list composition remain shared NumberFormat/List data.
"""
import metadata as m
import locale_subset
import number_patterns as numbers

WIDTHS = {110: 32}
UNITS = ('year', 'month', 'week', 'day', 'hour', 'minute', 'second',
         'millisecond', 'microsecond', 'nanosecond')


def clock_pattern(pattern):
    """Read quoted LDML hms fields preserving literal separators exactly.

    Only h/m/s duration fields in order, no literal prefix/suffix, are admitted
    by this revision. Other grammar is a recorded data gap, never guessed.
    Doubled apostrophes represent one literal apostrophe; quotes delimit
    literals, exactly as the pinned existing ICU frontend's clock extraction.
    """
    numbers.scalar_text(pattern)
    tokens, literal, quoted, i = [], [], False, 0
    while i < len(pattern):
        c = pattern[i]
        if c == "'":
            if i + 1 < len(pattern) and pattern[i + 1] == "'":
                literal.append("'"); i += 2; continue
            quoted = not quoted; i += 1; continue
        if not quoted and ('a' <= c <= 'z' or 'A' <= c <= 'Z'):
            m.require(c in 'hms', 'unsupported duration clock field')
            if literal:
                tokens.append(('literal', ''.join(literal))); literal = []
            end = i + 1
            while end < len(pattern) and pattern[end] == c:
                end += 1
            tokens.append((c, end-i)); i = end; continue
        literal.append(c); i += 1
    m.require(not quoted, 'unterminated duration clock quote')
    if literal:
        tokens.append(('literal', ''.join(literal)))
    m.require(len(tokens) == 5 and tuple(t[0] for t in tokens) ==
              ('h', 'literal', 'm', 'literal', 's'), 'unsupported duration hms structure')
    m.require(all(1 <= tokens[i][1] <= 2 for i in (0, 2, 4)), 'unsupported duration clock width')
    hm, ms = tokens[1][1], tokens[3][1]
    numbers.scalar_text(hm); numbers.scalar_text(ms)
    return int(tokens[0][1] == 2), hm, ms


def collect(inputs, metadata, number_data=None):
    resolver = numbers.Resolver(inputs, metadata['parents'])
    rows, gaps = [], []
    systems = sorted(metadata['numbering'])
    admitted = None if number_data is None else set(row[:2] for row in number_data['tables'][80])
    path = (numbers.segment('units'), numbers.segment('durationUnit', type='hms'),
            numbers.segment('durationUnitPattern'))
    for locale_index, locale in enumerate(sorted(metadata['parents'])):
        try:
            value = resolver.get(locale, path)
            m.require(value is not None, 'missing inherited hms duration pattern')
            two, hm, ms = clock_pattern(value)
        except m.DataError as exc:
            gaps.append({'locale': locale, 'reason': str(exc)}); continue
        for system_index, system in enumerate(systems):
            if metadata['numbering'][system][1]:
                gaps.append({'locale': locale, 'numbering': system,
                             'reason': 'algorithmic numbering has no numeric clock engine'}); continue
            if admitted is not None and (locale_index, system_index) not in admitted:
                gaps.append({'locale': locale, 'numbering': system,
                             'reason': 'missing shared NumberFormat decimal core'}); continue
            rows.append((locale_index, system_index, two, hm, ms))
    return {'rows': tuple(rows), 'evidence': {
        'clock_source': 'units/durationUnit[@type="hms"]/durationUnitPattern',
        'policy': 'metadata general parents; requesting-locale aliases; draft retained; alt excluded',
        'numbering_policy': 'same hms clock literals per admitted numeric numbering system, matching frozen ICU frontend',
        'required_shared_data': ['Number80/81 decimal core', 'Number83 selected unit styles',
                                 'cardinal Plural30/31/32', 'unit List40/41 selected list style'],
        'gaps': gaps, 'coverage': 0, 'activation': 'pending root semantic/data/JS gates'}}


def load_pinned(args, metadata, number_data, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    paths = locale_subset.main_paths(manifest, metadata)
    inputs = ((path, m.verified_input(args.cldr_dir, manifest, path, consumed, 'cldr', context)) for path in paths)
    return collect(inputs, metadata, number_data)


def strings(data):
    return tuple(value for row in data['rows'] for value in row[3:])


def encode(data, pool):
    records, previous = [], None
    for a, b, two, hm, ms in data['rows']:
        key = (a, b)
        m.require(previous is None or previous < key, 'duplicate/unsorted duration clock keys')
        m.require(two in (0, 1), 'invalid TwoDigitHours flag')
        numbers.scalar_text(hm); numbers.scalar_text(ms)
        row = m.u32(a) + m.u32(b) + bytes((two,)) + b'\0' * 7 + pool.ref(hm) + pool.ref(ms)
        m.require(len(row) == 32, 'duration clock record width drift')
        records.append(row); previous = key
    return {110: b''.join(records)}
