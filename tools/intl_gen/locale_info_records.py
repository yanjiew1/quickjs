"""Optional wire1.3 LocaleInfo metadata; never fabricates service coverage.

Copyright Unicode, Inc. CLDR49 / Unicode18 input attribution is retained in
the consumed manifest and bundled LICENSE. Prepared source only, not run.
"""
import re
import metadata as m

SCRIPT_SECTION = 24
CALENDAR_SECTION = 25
SERVICE_SECTION = 26
WIDTHS = {SCRIPT_SECTION: 12, CALENDAR_SECTION: 8, SERVICE_SECTION: 28}


def parse_script_metadata(data):
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError as exc:
        raise m.DataError('invalid script metadata encoding') from exc
    rows = {}
    for line in text.splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        fields = [value.strip() for value in line.split(';')]
        m.require(len(fields) == 11, 'unexpected script metadata field count')
        script, rtl = fields[0], fields[6]
        m.require(re.fullmatch('[A-Z][a-z]{3}', script), 'invalid script code')
        m.require(rtl in ('YES', 'NO', 'UNKNOWN'), 'unknown script direction value')
        m.require(script not in rows, 'duplicate script direction')
        rows[script] = {'UNKNOWN': 0, 'NO': 1, 'YES': 2}[rtl]
    m.require(rows and rows.get('Latn') == 1 and rows.get('Arab') == 2,
              'missing script metadata anchor rows')
    return tuple(sorted(rows.items()))


def load_pinned(args, consumed, context=None):
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    relative = 'common/properties/scriptMetadata.txt'
    data = m.verified_input(args.cldr_dir, manifest, relative, consumed, 'cldr', context)
    return parse_script_metadata(data)


def strings(rows):
    return [script for script, direction in rows]


def encode(rows, pool):
    return {SCRIPT_SECTION: b''.join(pool.ref(script) + m.u32(direction)
                                     for script, direction in rows)}


def encode_installed_calendars(identifiers, pool):
    """For the calendar implementation owner only, after capability review.

    The caller supplies the actual installed canonical inventory. The main
    LocaleInfo bundle never calls this hook; source ca names cannot enable it.
    """
    values = tuple(identifiers)
    m.require(values == tuple(sorted(set(values))) and 'gregory' in values,
              'invalid installed calendar inventory')
    m.require(all(re.fullmatch('[a-z0-9]{3,8}(?:-[a-z0-9]{3,8})*', v)
                  for v in values), 'invalid canonical calendar type')
    return {CALENDAR_SECTION: b''.join(pool.ref(v) for v in values)}


def encode_installed_services(rows, pool, locale_indices, list_span):
    """Service owner supplies exact proven AvailableLocales and co/default nu.

    rows: iterable(service,locale,default_value,collations). Service0 collation
    has default_value='' and actual unique co strings without leading null;
    service1 NumberFormat has its actual default nu and collations=(). All
    identifiers must already be alias-canonicalized by the service owner.
    The owner sets corresponding16 coverage bits and performs manifest/engine
    compatibility review. This function never changes16 or creates coverage.
    list_span receives the tuple and returns the section18 span bytes while
    its owner appends those list records, preserving semantic order.
    """
    result, seen = [], set()
    rows = tuple(rows)
    for service, locale, default, collations in rows:
        m.require(service in (0, 1) and locale != 'root' and locale in locale_indices,
                  'invalid installed service locale')
    for service, locale, default, collations in sorted(rows, key=lambda row: (row[0], locale_indices[row[1]])):
        key = (service, locale)
        m.require(key not in seen, 'duplicate installed service locale')
        seen.add(key)
        values = tuple(collations)
        m.require(len(values) == len(set(values)), 'duplicate installed collation')
        if service == 0:
            m.require(default == '' and not {'standard', 'search'} & set(values),
                      'reserved installed collation')
            m.require(all(re.fullmatch('[a-z0-9]{3,8}(?:-[a-z0-9]{3,8})*', v)
                          for v in values), 'invalid installed collation type')
        else:
            m.require(not values and re.fullmatch('[a-z0-9]{3,8}(?:-[a-z0-9]{3,8})*', default),
                      'invalid installed numbering default')
        span = list_span(values) if values else m.u32(0) + m.u32(0)
        m.require(isinstance(span, bytes) and len(span) == 8, 'invalid owner list span')
        result.append(m.u32(locale_indices[locale]) + m.u32(service) +
                      pool.ref(default) + span + m.u32(0))
    return {SERVICE_SECTION: b''.join(result)}
