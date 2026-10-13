# Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
"""Pinned CLDR48.2 root / UCA18 host compiler. Runtime uses only packed bytes.

No Python unicodedata: canonical decomposition and properties come from the
sealed Unicode18 files, independent of the host Python Unicode version.
Literal NFD keys are retained exactly; unreachable non-NFD allkeys mappings
are counted in evidence. This is root data compilation, not an LDML rule
compiler. Both usages have explicit rows and independently chosen defaults;
the implementation-defined search policy deliberately shares the root table.
This is not the distinct CLDR root/search tailoring. Other tailorings remain
explicitly unsupported.
"""
import hashlib
from pathlib import Path
import re
import struct
import metadata as m

WIDTHS = {90: 20, 91: 16, 92: 16, 93: 4, 94: 20, 95: 16}
UCA_REFERENCE_SHA256 = 'e5ec6a9933495e327d9dd3e981f0f89bbf8acc40eeb2f12a52e7249c8a3a34db'
CLDR_REFERENCE_SHA256 = '3c9c4deb78464ab8eca9d5a1852e9ab69999daa87872f2bf0700120c32ce0822'
CE_RE = re.compile(r'\[([.*])([0-9A-Fa-f]{4})\.([0-9A-Fa-f]{4})\.([0-9A-Fa-f]{4})\]')
KEY_RE = re.compile(r'[0-9A-Fa-f]{4,6}(?:\s+[0-9A-Fa-f]{4,6})*')
SINIFORM = {
    'Tangut': (0xfb00, 0x17000),
    'Tangut Supplement': (0xfb00, 0x17000),
    'Tangut Components': (0xfb01, 0x18800),
    'Tangut Components Supplement': (0xfb01, 0x18800),
    'Nushu': (0xfb02, 0x1b170),
    'Khitan Small Script': (0xfb03, 0x18b00),
    'Jurchen': (0xfb04, 0x18e00),
    'Jurchen Radicals': (0xfb04, 0x18e00),
    'Seal': (0xfb05, 0x3d000),
}


def scalar(cp):
    return isinstance(cp, int) and 0 <= cp <= 0x10ffff and not 0xd800 <= cp <= 0xdfff


def upper_tertiary(t):
    # Pinned CLDR48.2 Case_Untailored, not General_Category guessing.
    return t in (8, 9, 10, 11, 12, 14, 17, 18, 29)


def text(data, label):
    try:
        return data.decode('utf-8-sig')
    except UnicodeError as exc:
        raise m.DataError(label + ': invalid UTF8: ' + str(exc))


def parse_allkeys(data, expected_version):
    result, version = {}, None
    source = text(data, 'allkeys')
    m.require(('# UCA Version: ' + expected_version) in source and
              ('# UCD Version: ' + expected_version) in source,
              'allkeys UCA/UCD source headers disagree with explicit pinned data version')
    for line_number, line in enumerate(source.splitlines(), 1):
        body = line.split('#', 1)[0].strip()
        if not body:
            continue
        if body.startswith('@version'):
            m.require(version is None and body == '@version ' + expected_version,
                      'wrong/repeated allkeys version')
            version = expected_version
            continue
        m.require(not body.startswith('@'), 'unknown allkeys directive at ' + str(line_number))
        pieces = body.split(';')
        m.require(len(pieces) == 2 and KEY_RE.fullmatch(pieces[0].strip()),
                  'bad allkeys key at ' + str(line_number))
        key = tuple(int(token, 16) for token in pieces[0].split())
        m.require(key and len(key) <= 64 and all(scalar(cp) for cp in key),
                  'invalid allkeys scalar/key size')
        weights = re.sub(r'\s+', '', pieces[1])
        matches = tuple(CE_RE.finditer(weights))
        m.require(matches and ''.join(match.group(0) for match in matches) == weights,
                  'bad allkeys CE at ' + str(line_number))
        ces = []
        for match in matches:
            p, s, t = (int(match.group(i), 16) for i in (2, 3, 4))
            variable = match.group(1) == '*'
            m.require(not variable or p, 'zero-primary variable CE')
            ces.append((p, s, t, int(variable) | (int(upper_tertiary(t)) << 1)))
        m.insert_unique(result, key, tuple(ces), 'allkeys mapping')
    m.require(version and result, 'empty allkeys/version absent')
    return result


def parse_ucd(data):
    assigned, ccc, decompositions, digits = [], {}, {}, {}
    pending, previous = None, -1
    for line in text(data, 'UnicodeData').splitlines():
        if not line.strip():
            continue
        fields = line.split(';')
        m.require(len(fields) == 15, 'wrong UnicodeData field count')
        m.require(re.fullmatch('[0-9A-F]{4,6}', fields[0]), 'bad UnicodeData cp')
        cp = int(fields[0], 16)
        m.require(previous < cp <= 0x10ffff, 'unordered UnicodeData')
        previous = cp
        m.require(re.fullmatch('[0-9]{1,3}', fields[3]), 'bad combining class')
        cc = int(fields[3])
        m.require(cc <= 255, 'combining class overflow')
        if fields[1].endswith(', First>'):
            m.require(pending is None and not cc and not fields[5] and not fields[6],
                      'unsupported UnicodeData range fields')
            pending = (cp, fields[1][:-8], fields[2])
            continue
        if fields[1].endswith(', Last>'):
            m.require(pending is not None and pending[1] == fields[1][:-7] and
                      pending[2] == fields[2] and not cc and not fields[5] and not fields[6],
                      'unpaired UnicodeData range')
            assigned.append((pending[0], cp))
            pending = None
            continue
        m.require(pending is None, 'unterminated UnicodeData range')
        assigned.append((cp, cp))
        if cc:
            ccc[cp] = cc
        if fields[5] and not fields[5].startswith('<'):
            m.require(KEY_RE.fullmatch(fields[5]), 'bad canonical decomposition')
            value = tuple(int(token, 16) for token in fields[5].split())
            m.require(all(scalar(v) for v in value), 'non-scalar decomposition')
            decompositions[cp] = value
        if fields[2] == 'Nd':
            m.require(re.fullmatch('[0-9]', fields[6]), 'Nd lacks decimal value')
            digits[cp] = int(fields[6])
        else:
            m.require(not fields[6], 'non-Nd has decimal value')
    m.require(pending is None and assigned, 'empty/unclosed UnicodeData')
    zeros = sorted(cp for cp, value in digits.items() if value == 0)
    m.require(len(zeros) * 10 == len(digits) and
              all(digits.get(zero + value) == value for zero in zeros for value in range(10)),
              'Nd must be complete contiguous sets0..9')
    return {'assigned': tuple(assigned), 'ccc': ccc, 'decomposition': decompositions,
            'digit_zeros': tuple(zeros)}


def nfd(sequence, ucd):
    result = []

    def decompose(cp, visiting):
        m.require(cp not in visiting and len(visiting) < 32, 'canonical decomposition cycle/depth')
        if 0xac00 <= cp <= 0xd7a3:
            value = cp - 0xac00
            parts = [0x1100 + value // 588, 0x1161 + value % 588 // 28]
            if value % 28:
                parts.append(0x11a7 + value % 28)
            result.extend(parts)
        elif cp in ucd['decomposition']:
            for item in ucd['decomposition'][cp]:
                decompose(item, visiting + (cp,))
        else:
            result.append(cp)

    for cp in sequence:
        decompose(cp, ())
    # Canonical ordering is stable; never cross a starter or equal CCC.
    for i in range(1, len(result)):
        cc = ucd['ccc'].get(result[i], 0)
        if cc:
            j = i
            while j and ucd['ccc'].get(result[j - 1], 0) > cc:
                result[j - 1], result[j] = result[j], result[j - 1]
                j -= 1
    return tuple(result)


def parse_ranges(data, selected=None):
    result, previous = [], -1
    for raw in text(data, 'ranges').splitlines():
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        fields = tuple(value.strip() for value in line.split(';'))
        m.require(len(fields) == 2, 'bad property/block line')
        if selected is not None and fields[1] != selected:
            continue
        match = re.fullmatch(r'([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?', fields[0])
        m.require(match is not None, 'bad property range')
        first, last = int(match[1], 16), int(match[2] or match[1], 16)
        m.require(previous < first <= last <= 0x10ffff, 'unordered/overlapping property range')
        previous = last
        result.append((first, last, fields[1]))
    m.require(result, 'empty selected property/block data')
    return tuple(result)


def implicit_ranges(ucd, block_data, property_data):
    blocks = parse_ranges(block_data)
    unified = parse_ranges(property_data, 'Unified_Ideograph')
    selected = {}
    for first, last, name in blocks:
        if name not in SINIFORM:
            continue
        base, origin = SINIFORM[name]
        for start, end in ucd['assigned']:
            if start > last:
                break
            for cp in range(max(start, first), min(end, last) + 1):
                m.require(scalar(cp) and cp - origin <= 0x7fff,
                          'siniform implicit range exceeds formula')
                selected[cp] = (base, origin)
    core = tuple((first, last) for first, last, name in blocks
                 if name in ('CJK Unified Ideographs', 'CJK Compatibility Ideographs'))
    m.require(len(core) == 2 and all(name in tuple(n for f, l, n in blocks)
                                   for name in SINIFORM), 'missing UCA18 implicit blocks')
    for first, last, unused in unified:
        for cp in range(first, last + 1):
            m.require(scalar(cp) and cp not in selected, 'overlapping implicit classes')
            base = 0xfb40 if any(a <= cp <= b for a, b in core) else 0xfb80
            selected[cp] = (0x80000000 | base, 0)
    result = []
    for cp, (base, origin) in sorted(selected.items()):
        if result and result[-1][1] + 1 == cp and result[-1][2:] == (base, origin):
            result[-1] = (result[-1][0], cp, base, origin)
        else:
            result.append((cp, cp, base, origin))
    return tuple(result)


def trie(mappings):
    root = {'children': {}, 'ce': ()}
    max_depth = 0
    for key, ces in sorted(mappings.items()):
        m.require(key and len(key) <= 64 and all(scalar(cp) for cp in key) and ces,
                  'invalid trie mapping')
        node = root
        max_depth = max(max_depth, len(key))
        for cp in key:
            node = node['children'].setdefault(cp, {'children': {}, 'ce': ()})
        m.require(not node['ce'] or node['ce'] == ces, 'conflicting trie key')
        node['ce'] = ces
    ce_pool, spans = [], {(): (0, 0)}
    for ces in sorted(set(mappings.values())):
        spans[ces] = (len(ce_pool), len(ces))
        ce_pool.extend(ces)
    queue, rows = [(0xffffffff, root)], []
    index = 0
    while index < len(queue):
        cp, node = queue[index]
        children = sorted(node['children'].items())
        first = len(queue) if children else 0
        queue.extend(children)
        ce_first, ce_count = spans[node['ce']]
        rows.append((cp, first, len(children), ce_first, ce_count))
        index += 1
    m.require(rows and max_depth, 'empty collation trie')
    return tuple(rows), tuple(ce_pool), max_depth


def reference(path, expected, logical, consumed):
    data = Path(path).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    m.require(digest == expected, 'wrong normative reference: ' + logical)
    consumed[logical] = {'sha256': digest, 'bytes': len(data)}
    return data


def load_pinned(args, metadata, consumed, context=None):
    cldr = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    ucd_manifest = m.load_pin_manifest(args.ucd_manifest, m.UCD_MANIFEST_SHA256, context)
    read_cldr = lambda path: m.verified_input(args.cldr_dir, cldr, path, consumed, 'cldr', context)
    read_ucd = lambda path: m.verified_input(args.ucd_dir, ucd_manifest, path, consumed, 'ucd', context)
    reference(args.uca_reference, UCA_REFERENCE_SHA256, 'references/uts10-revision55.html', consumed)
    reference(args.cldr_collation_reference, CLDR_REFERENCE_SHA256,
              'references/cldr48.2-tr35-collation.md', consumed)
    m.require(args.uca_data_version == m.UCA_DATA_VERSION,
              'unsupported CLDR release/UCA data pair')
    root = parse_allkeys(read_cldr('common/uca/allkeys_CLDR.txt'), args.uca_data_version)
    ducet = parse_allkeys(read_cldr('common/uca/allkeys_DUCET.txt'), args.uca_data_version)
    ucd = parse_ucd(read_ucd('UnicodeData.txt'))
    implicit = implicit_ranges(ucd, read_ucd('Blocks.txt'), read_ucd('PropList.txt'))
    # S1 normalizes input. Keeping composed-only mappings in a normalized-key
    # trie would invent contractions absent from the normative root table.
    reachable = {key: ces for key, ces in root.items() if nfd(key, ucd) == key}
    nodes, ces, depth = trie(reachable)
    zero = root.get((0x30,))
    m.require(zero and len(zero) == 1 and zero[0][0] and not zero[0][3] & 1,
              'root lacks ordinary digit zero mapping')
    numeric_primary = (zero[0][0] << 16) - 1
    m.require(all(root.get((cp,)) and root[(cp,)][0][0] >= zero[0][0]
                  for cp in ucd['digit_zeros']), 'digit group starts before numeric prefix')
    # Deliberate initial data choice, independently proved by these LDML
    # files. No locale fallback makes sv, th or de-phonebk available. Both
    # ECMA402 usages deliberately use the root comparison algorithm, with
    # independent SearchLocaleData rows and default sensitivity=variant.
    tags = sorted(metadata['parents'])
    capabilities = []
    for tag in ('en', 'en-US', 'root'):
        path = 'common/collation/' + tag.replace('-', '_') + '.xml'
        document = m.parse_xml(read_cldr(path), 'ldml')
        if tag == 'root':
            standard = document.findall('./collations/collation[@type="standard"]')
            m.require(len(standard) == 1 and not list(standard[0]) and
                      not (standard[0].text or '').strip(), 'root standard must be empty')
        else:
            m.require(document.find('collations') is None,
                      'initial root-valid locale contains tailoring')
        m.require(tag in metadata['parents'], 'capability locale absent from metadata')
        for usage in (0, 1):
            capabilities.append((tags.index(tag), usage, 31, 3, 0))
    capabilities.sort()
    # Conformance source is sealed for the root-owned C fixture execution;
    # retaining it in provenance does not claim those fixtures were executed.
    for name in ('NON_IGNORABLE_SHORT', 'SHIFTED_SHORT'):
        read_cldr('common/uca/CollationTest_CLDR_' + name + '.txt')
    return {'nodes': nodes, 'ces': ces, 'implicit': implicit,
            'digits': ucd['digit_zeros'], 'capabilities': tuple(capabilities),
            'config': (2, numeric_primary, depth, int(args.uca_data_version.split('.')[0]) << 16),
            'evidence': {'root_table': 'allkeys_CLDR' + args.uca_data_version,
                         'root_data_version': args.uca_data_version,
                         'normalization_and_implicit_property_version': '18.0.0',
                         'algorithm_reference_version': '18.0.0',
                         'ducet_mappings': len(ducet), 'root_mappings': len(root),
                         'root_ducet_differences': sum(root.get(key) != ducet.get(key)
                                                       for key in set(root) | set(ducet)),
                         'reachable_nfd_mappings': len(reachable),
                         'unreachable_non_nfd_mappings': len(root) - len(reachable),
                         'supported_sort_locales': ['en', 'en-US', 'root'],
                         'supported_search_locales': ['en', 'en-US', 'root'],
                         'supported_collations': ['default'], 'search_supported': True,
                         'search_policy': 'Explicit implementation-defined shared CLDR root comparator; not CLDR root/search tailoring',
                         'search_defaults': {'sensitivity': 'variant', 'ignorePunctuation': False, 'co': [None]},
                         'tailoring_policy': 'Independently proved root-based rows for both required usages; missing capability returns UNSUPPORTED',
                         'mandatory_gap': 'Actual generation/C/frontend gates and paired SortLocaleData/SearchLocaleData integration',
                         'optional_data_choices': 'Other language sort/search tailorings, co specializations and locale coverage are not compiled',
                         'actual_fixture_execution': False}}


def encode(data):
    pack = lambda rows: b''.join(b''.join(m.u32(value) for value in row) for row in rows)
    nodes = data['nodes']
    m.require(nodes and len(nodes[0]) == 5 and nodes[0][0] == 0xffffffff,
              'collation trie root must carry the reserved UINT32_MAX marker')
    # The root marker is a fixed wire token, excluded from metadata.u32's
    # ordinary integer domain. Every span/count and every other row still
    # passes through that frozen bounds check; never admit UINT32_MAX there.
    node_bytes = b'\xff' * 4 + pack((nodes[0][1:],)) + pack(nodes[1:])
    return {90: node_bytes, 91: pack(data['ces']), 92: pack(data['implicit']),
            93: pack((value,) for value in data['digits']),
            94: pack(data['capabilities']), 95: pack((data['config'],))}
