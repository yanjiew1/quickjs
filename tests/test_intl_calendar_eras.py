#!/usr/bin/env python3
"""Explicit source identities, canonical wire mapping and future era IDs."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import calendar_eras as source
import calendar_era_tables as wire
import tables

DTD = b'''<!ELEMENT supplementalData ANY><!ELEMENT calendarData ANY>
<!ELEMENT calendar ANY><!ATTLIST calendar type NMTOKEN #REQUIRED>
<!ELEMENT calendarSystem EMPTY><!ATTLIST calendarSystem type (solar|lunar|lunisolar|other) #REQUIRED>
<!ELEMENT inheritEras EMPTY><!ATTLIST inheritEras calendar NMTOKEN #REQUIRED>
<!ELEMENT eras ANY><!ELEMENT era EMPTY><!ATTLIST era type NMTOKEN #REQUIRED>
<!ATTLIST era code NMTOKEN #IMPLIED><!ATTLIST era start CDATA #IMPLIED>
<!ATTLIST era end CDATA #IMPLIED><!ATTLIST era aliases NMTOKENS #IMPLIED>
<!ATTLIST era named (true|false) #IMPLIED>'''
XML = b'''<supplementalData><calendarData>
  <calendar type="japanese"><inheritEras calendar="gregorian"/><eras>
    <era type="236" code="reiwa" start="2019-05-01"/>
    <era type="999" code="next-era" start="2100-01-01"/>
    <era type="232" code="meiji" start="1868-10-23"/>
  </eras></calendar>
  <calendar type="gregorian"><calendarSystem type="solar"/><eras>
    <era type="1" code="ce" aliases="ad"/><era type="0" code="bce" aliases="bc"/>
  </eras></calendar>
  <calendar type="coptic"><eras><era type="1" code="am"/></eras></calendar>
  <calendar type="hebrew"><eras><era type="0" code="am"/></eras></calendar>
  <calendar type="chinese"><eras><era type="0" start="-2636-02-15"/></eras></calendar>
</calendarData></supplementalData>'''


class Sources:
    def __init__(self, xml=XML, dtd=DTD):
        self.files = {'common/dtd/ldmlSupplemental.dtd': dtd,
                      'common/supplemental/supplementalData.xml': xml}

    def read(self, name):
        return self.files[name]


def fixture(data=None):
    if data is None:
        data = source.collect(Sources())
    pool = tuple(sorted(wire.strings(data) | {'root', '01', 'BAD', 'a', 'aa--bb', 'x_y'}))
    ids = {value: index for index, value in enumerate(pool)}
    return data, pool, ids, wire.encode(data, ids)


class CalendarErasTests(unittest.TestCase):
    def test_explicit_mapping_not_position_or_inheritance(self):
        rows = source.collect(Sources())['rows']
        self.assertEqual(rows, (('coptic','am','1'), ('gregorian','bce','0'),
                                ('gregorian','ce','1'), ('hebrew','am','0'),
                                ('japanese','meiji','232'), ('japanese','next-era','999'),
                                ('japanese','reiwa','236')))
        self.assertEqual(source.collect(Sources(b'<supplementalData/>')), {'rows': ()})
        # Start/end/alias text does not define era arithmetic or display codes.
        self.assertEqual(source.collect(Sources(XML.replace(b'aliases="ad"', b'aliases="anno domini"'))),
                         {'rows': rows})

    def test_reject_ambiguous_or_unreviewed_source(self):
        cases = (XML.replace(b'code="reiwa"', b'code="meiji"'),
                 XML.replace(b'type="999"', b'type="236"'),
                 XML.replace(b'type="999"', b'type="0999"'),
                 XML.replace(b'code="next-era"', b'code="Next-era"'),
                 XML.replace(b'<eras>', b'<unknown/>', 1),
                 XML.replace(b'code="reiwa"', b'code="reiwa" extra="x"'),
                 XML.replace(b'</calendarData>', b'<calendar type="japanese"/></calendarData>'))
        for xml in cases:
            with self.subTest(xml=xml), self.assertRaises(ValueError):
                source.collect(Sources(xml))

    def test_canonical_roundtrip_optional_and_empty(self):
        data, pool, ids, section = fixture()
        self.assertEqual(wire.decode(section, pool), data)
        self.assertEqual(wire.encode({'rows': tuple(reversed(data['rows']))}, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: (1, struct.pack('<II', ids['root'], 0xffffffff)),
                              bundle.CALENDAR_ERAS: section}, bytes(32), 'minimal')
        self.assertEqual(tables.validate(binary)['calendar_eras'], data)
        self.assertIsNone(wire.decode(None, pool))
        empty = wire.encode({'rows': ()}, {'': 0})
        self.assertEqual(empty, (0, struct.pack('<I', 1)))
        self.assertEqual(wire.decode(empty, ('',)), {'rows': ()})

    def test_repeated_long_pool_values_and_separate_grammar_domains(self):
        calendar, raw = 'long-' + 'a' * 4096, '9' * 4096
        rows = tuple((calendar, 'era-' + str(100 + i), str(i)) for i in range(64))
        rows += (('other', 'am', raw), ('third', 'am', raw))
        data, pool, ids, section = fixture({'rows': rows})
        self.assertEqual(wire.decode(section, pool), {'rows': tuple(sorted(rows))})
        # A value accepted as an identity must still be checked as a raw key.
        with self.assertRaises(ValueError):
            wire.encode({'rows': (('am', 'am', 'am'),)}, {'': 0, 'am': 1})

    def test_bad_wire_dimensions_ids_grammar_order_and_ambiguity(self):
        data, pool, ids, section = fixture()
        words = list(struct.unpack('<' + 'I' * (len(section[1]) // 4), section[1]))
        changes = ((0, 2), (1, len(pool)), (1, ids['x_y']), (2, ids['a']),
                   (2, ids['BAD']), (2, ids['aa--bb']), (3, ids['01']),
                   (3, ids['']), (2 + 6, ids['bce']), (3 + 6, ids['0']))
        for index, value in changes:
            changed = words[:]
            changed[index] = value
            raw = struct.pack('<' + 'I' * len(changed), *changed)
            with self.subTest(index=index,value=value), self.assertRaises(ValueError):
                wire.decode((section[0], raw), pool)
        for raw in (section[1][:-1], section[1] + bytes(4)):
            with self.assertRaises(ValueError):
                wire.decode((section[0], raw), pool)
        for case in ({'rows': list(data['rows'])}, {'rows': data['rows'] + (data['rows'][0],)},
                     {'rows': (('gregorian','ce','0'), ('gregorian','bce','0'))}):
            with self.assertRaises(ValueError):
                wire.encode(case, ids)


if __name__ == '__main__':
    unittest.main()
