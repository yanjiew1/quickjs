"""Unicode18 UAX29/revision49 packed properties; original host generator.

Source-only preparation. No ICU, network, or runtime XML/JSON dependence.
The frozen metadata owner supplies verified_input and manifest handling.
"""
import hashlib
import re
import struct
from pathlib import Path
import metadata as m

UNICODE_VERSION = 18 << 16
UAX29_REVISION = 49
ALGORITHM_VERSION = 1
UAX29_SHA256 = 'd5f4babf0e23edba0b5b6eb3de43f2dbee25ea2ac9f0155fe32a631e44d7103a'
SECTION_WIDTHS = {60: 12, 61: 12, 62: 12, 63: 12, 64: 12, 65: 24}
GCB = dict((value, i) for i, value in enumerate((
    'Other', 'CR', 'LF', 'Control', 'Extend', 'ZWJ', 'Regional_Indicator',
    'Prepend', 'SpacingMark', 'L', 'V', 'T', 'LV', 'LVT')))
WB = dict((value, i) for i, value in enumerate((
    'Other', 'CR', 'LF', 'Newline', 'Extend', 'Format', 'ZWJ', 'WSegSpace',
    'ALetter', 'Hebrew_Letter', 'Numeric', 'Katakana', 'ExtendNumLet',
    'MidLetter', 'MidNum', 'MidNumLet', 'Single_Quote', 'Double_Quote',
    'Regional_Indicator')))
SB = dict((value, i) for i, value in enumerate((
    'Other', 'CR', 'LF', 'Sep', 'Extend', 'Format', 'Sp', 'Lower', 'Upper',
    'OLetter', 'Numeric', 'ATerm', 'STerm', 'Close', 'SContinue')))
INCB = {'None': 0, 'Extend': 1, 'Consonant': 2, 'Linker': 3}
EP = {'Extended_Pictographic': 1}
RANGE = re.compile(r'([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?')


def scalar_range(value):
    match = RANGE.fullmatch(value)
    m.require(match is not None, 'invalid scalar range syntax')
    first = int(match.group(1), 16)
    last = int(match.group(2) or match.group(1), 16)
    m.require(first <= last <= 0x10ffff, 'out of order/out of bounds scalar range')
    m.require(first > 0xdfff or last < 0xd800, 'surrogate range is not a scalar range')
    return first, last


def canonical_ranges(rows, maximum):
    """Sort input property-group order; reject overlap and merge adjacency."""
    result = []
    for first, last, value in rows:
        m.require(isinstance(first, int) and isinstance(last, int) and
                  isinstance(value, int), 'noninteger range field')
        m.require(0 <= first <= last <= 0x10ffff and
                  (first > 0xdfff or last < 0xd800), 'invalid scalar range')
        m.require(0 < value < maximum, 'invalid/implicit default property enum')
    for first, last, value in sorted(rows):
        if result:
            old_first, old_last, old_value = result[-1]
            m.require(first > old_last, 'overlapping/duplicate property ranges')
            if first == old_last + 1 and value == old_value:
                result[-1] = (old_first, last, value)
                continue
        result.append((first, last, value))
    m.require(bool(result), 'empty property table')
    return result


def parse_properties(data, values, selector=None):
    """Strict UCD ranges; selector handles InCB and emoji multi-properties.

    InCB rows have exactly range;InCB;value. Other DerivedCoreProperties
    property rows are deliberately excluded. Emoji rows have range;property.
    No default value is serialized; absence means Other/None/false.
    """
    try:
        text = data.decode('utf-8')
    except UnicodeError as exc:
        raise m.DataError('invalid Unicode property UTF8: ' + str(exc))
    rows = []
    for original in text.splitlines():
        line = original.split('#', 1)[0].strip()
        if not line:
            continue
        fields = [field.strip() for field in line.split(';')]
        m.require(len(fields) >= 2, 'missing property field')
        if selector is not None and fields[1] != selector:
            continue
        m.require(len(fields) == (3 if selector == 'InCB' else 2),
                  'wrong property field count')
        name = fields[2] if selector == 'InCB' else fields[1]
        m.require(name in values, 'unknown Unicode property enum: ' + name)
        first, last = scalar_range(fields[0])
        if values[name]:
            rows.append((first, last, values[name]))
    return canonical_ranges(rows, max(values.values()) + 1)


def parse_break_tests(data):
    """Preserve every fixture and convert scalar offsets into original UTF16."""
    try:
        text = data.decode('utf-8')
    except UnicodeError as exc:
        raise m.DataError('invalid BreakTest UTF8: ' + str(exc))
    cases = []
    for line_number, original in enumerate(text.splitlines(), 1):
        tokens = original.split('#', 1)[0].split()
        if not tokens:
            continue
        m.require(len(tokens) % 2 == 1 and tokens[0] == '\u00f7' and tokens[-1] == '\u00f7',
                  'BreakTest must have start/end boundaries')
        text16, boundaries = [], []
        for i, token in enumerate(tokens):
            if i % 2 == 0:
                m.require(token in ('\u00f7', '\u00d7'), 'invalid BreakTest marker')
                if token == '\u00f7':
                    boundaries.append(len(text16))
            else:
                first, last = scalar_range(token)
                m.require(first == last, 'BreakTest code point cannot be a range')
                if first < 0x10000:
                    text16.append(first)
                else:
                    value = first - 0x10000
                    text16.extend((0xd800 + (value >> 10), 0xdc00 + (value & 1023)))
        m.require(bool(text16), 'empty BreakTest fixture')
        cases.append({'line': line_number, 'text': text16, 'boundaries': boundaries})
    m.require(bool(cases), 'empty BreakTest input')
    return cases


def load_pinned(args, consumed, context=None):
    ucd = m.load_pin_manifest(args.ucd_manifest, m.UCD_MANIFEST_SHA256, context)
    cldr = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    document = Path(args.uax29_reference).read_bytes()
    m.require(hashlib.sha256(document).hexdigest() == UAX29_SHA256,
              'Unicode18 UAX29/revision49 normative reference hash mismatch')
    consumed['references/uax29-49'] = {'sha256': UAX29_SHA256, 'bytes': len(document)}
    definitions = (
        (60, 'auxiliary/GraphemeBreakProperty.txt', GCB, None),
        (61, 'auxiliary/WordBreakProperty.txt', WB, None),
        (62, 'auxiliary/SentenceBreakProperty.txt', SB, None),
        (63, 'DerivedCoreProperties.txt', INCB, 'InCB'),
        (64, 'emoji/emoji-data.txt', EP, 'Extended_Pictographic'))
    ranges = {}
    for section, relative, values, selector in definitions:
        data = m.verified_input(args.ucd_dir, ucd, relative, consumed, 'ucd', context)
        version_marker = (b'# Version: 18.0.0' if section == 64 else
                          (relative.rsplit('/', 1)[-1][:-4] + '-18.0.0.txt').encode('ascii'))
        m.require(version_marker in data[:2048], 'wrong Unicode property source version')
        if section <= 62:
            m.require(b'# @missing: 0000..10FFFF; Other' in data,
                      'wrong/absent default break property declaration')
        elif section == 63:
            m.require(b'# @missing: 0000..10FFFF; InCB; None' in data,
                      'wrong/absent default InCB declaration')
        ranges[section] = parse_properties(data, values, selector)
    fixtures = {}
    for granularity, name in enumerate(('Grapheme', 'Word', 'Sentence')):
        relative = 'auxiliary/' + name + 'BreakTest.txt'
        data = m.verified_input(args.ucd_dir, ucd, relative, consumed, 'ucd', context)
        m.require((name + 'BreakTest-18.0.0.txt').encode('ascii') in data[:512],
                  'wrong BreakTest version')
        fixtures[granularity] = parse_break_tests(data)
    tailoring = []
    for relative in sorted(cldr):
        if not relative.startswith('common/segments/') or not relative.endswith('.xml'):
            continue
        data = m.verified_input(args.cldr_dir, cldr, relative, consumed, 'cldr', context)
        root = m.parse_xml(data, 'ldml')
        for group in root.findall('./segmentations/segmentation'):
            if group.get('type') not in ('GraphemeClusterBreak', 'WordBreak', 'SentenceBreak'):
                continue
            tailoring.append({'path': relative, 'type': group.get('type'),
                'variables': [{'id': node.get('id'), 'expression': node.text or ''}
                              for node in group.findall('./variables/variable')],
                'rules': [{'id': node.get('id'), 'expression': node.text or ''}
                          for node in group.findall('./segmentRules/rule')],
                'suppressions': [node.text or '' for node in group.findall('./suppressions/suppression')],
                'implemented': False})
    return {'ranges': ranges, 'fixtures': fixtures, 'tailoring': tailoring,
            'standalone_ep': bool(args.standalone_extended_pictographic)}


def encode(service):
    sections = {}
    for section, rows in sorted(service['ranges'].items()):
        if section == 64 and not service['standalone_ep']:
            continue
        sections[section] = b''.join(struct.pack('<III', *row) for row in rows)
    mode = 1 if service['standalone_ep'] else 2
    sections[65] = b''.join(struct.pack('<IIIIII', granularity, UNICODE_VERSION,
        UAX29_REVISION, ALGORITHM_VERSION, mode, 0) for granularity in range(3))
    return sections


def fixture_c(service):
    header = '''/* Generated from Unicode18 BreakTest; Unicode data license in NOTICE.txt. */
#ifndef QJS_INTL_SEGMENTER_BREAK_FIXTURES_H
#define QJS_INTL_SEGMENTER_BREAK_FIXTURES_H
#include <stddef.h>
#include <stdint.h>
typedef struct QJSIntlSegmenterBreakFixture {
    uint32_t granularity, source_line;
    size_t text_offset, text_length, boundary_offset, boundary_count;
} QJSIntlSegmenterBreakFixture;
extern const uint16_t qjs_intl_segmenter_fixture_text[];
extern const size_t qjs_intl_segmenter_fixture_boundaries[];
extern const QJSIntlSegmenterBreakFixture qjs_intl_segmenter_break_fixtures[];
extern const size_t qjs_intl_segmenter_break_fixture_count;
#endif
'''
    texts, boundaries, cases = [], [], []
    for granularity, group in sorted(service['fixtures'].items()):
        for case in group:
            cases.append((granularity, case['line'], len(texts), len(case['text']),
                          len(boundaries), len(case['boundaries'])))
            texts.extend(case['text'])
            boundaries.extend(case['boundaries'])
    source = ['/* Generated Unicode18 fixtures. Single definition owner. */',
              '#include "segmenter-break-fixtures.h"',
              'const uint16_t qjs_intl_segmenter_fixture_text[] = {']
    for at in range(0, len(texts), 12):
        source.append('    ' + ', '.join('0x{:04x}'.format(cp) for cp in texts[at:at+12]) + ',')
    source.append('};\nconst size_t qjs_intl_segmenter_fixture_boundaries[] = {')
    for at in range(0, len(boundaries), 16):
        source.append('    ' + ', '.join(str(i) for i in boundaries[at:at+16]) + ',')
    source.append('};\nconst QJSIntlSegmenterBreakFixture qjs_intl_segmenter_break_fixtures[] = {')
    for case in cases:
        source.append('    {' + ', '.join(str(i) for i in case) + '},')
    source.append('}};\nconst size_t qjs_intl_segmenter_break_fixture_count = {};\n'.format(len(cases)))
    return header.encode('ascii'), ('\n'.join(source)).encode('ascii')
