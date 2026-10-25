#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Supplemental metazone boundaries, identities and presentation overrides."""
import datetime
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import timezone_presentation as source
from test_intl_locale_metadata import Sources as MetadataSources

DTD = b'''<!ELEMENT metaZones ANY><!ELEMENT metazoneInfo ANY>
<!ELEMENT timezone ANY><!ATTLIST timezone type CDATA #REQUIRED>
<!ELEMENT usesMetazone EMPTY><!ATTLIST usesMetazone mzone NMTOKEN #REQUIRED>
<!ATTLIST usesMetazone from CDATA #IMPLIED><!ATTLIST usesMetazone to CDATA #IMPLIED>
<!ATTLIST usesMetazone stdOffset CDATA #IMPLIED><!ATTLIST usesMetazone dstOffset CDATA #IMPLIED>
<!ELEMENT mapTimezones ANY><!ATTLIST mapTimezones type NMTOKEN #IMPLIED>
<!ATTLIST mapTimezones typeVersion CDATA #IMPLIED><!--@METADATA-->
<!ATTLIST mapTimezones otherVersion CDATA #IMPLIED><!--@METADATA-->
<!ELEMENT mapZone EMPTY><!ATTLIST mapZone other CDATA #REQUIRED>
<!ATTLIST mapZone territory CDATA #IMPLIED><!ATTLIST mapZone type CDATA #REQUIRED>
<!ELEMENT metazoneIds ANY><!ELEMENT metazoneId EMPTY>
<!ATTLIST metazoneId shortId NMTOKEN #REQUIRED><!ATTLIST metazoneId longId CDATA #IMPLIED>
<!ATTLIST metazoneId deprecated (true | false) "false">
<!ATTLIST metazoneId preferred NMTOKEN #IMPLIED>
<!ATTLIST metazoneId since CDATA "40"><!--@METADATA-->
<!ELEMENT primaryZones ANY><!ELEMENT primaryZone (#PCDATA)>
<!ATTLIST primaryZone iso3166 NMTOKEN #REQUIRED>'''
PATH = 'common/supplemental/metaZones.xml'
XML = b'''<supplementalData><metaZones><metazoneInfo>
  <timezone type="Area/Alpha">
    <usesMetazone mzone="West" to="1972-01-07 00:44:30" stdOffset="+00" dstOffset="+01"/>
    <usesMetazone mzone="West" from="1972-01-08 00:00" to="2010-01-01 00:00"/>
    <usesMetazone mzone="East" from="2010-01-01 00:00"/>
  </timezone>
  <timezone type="Etc/GMT"><usesMetazone mzone="GMT" stdOffset="+00"/></timezone>
</metazoneInfo><mapTimezones type="metazones" typeVersion="authored">
  <mapZone other="West" territory="001" type="Area/Alpha"/>
  <mapZone other="East" territory="001" type="Area/Alpha"/>
  <mapZone other="GMT" territory="001" type="Etc/GMT"/>
</mapTimezones><metazoneIds>
  <metazoneId shortId="west" longId="West"/><metazoneId shortId="east" longId="East"/>
  <metazoneId shortId="mgmt" longId="GMT"/>
</metazoneIds></metaZones><primaryZones>
  <primaryZone iso3166="GB">Area/Alpha</primaryZone>
</primaryZones></supplementalData>'''


class Sources(MetadataSources):
    def __init__(self):
        super().__init__()
        self.files['common/dtd/ldmlSupplemental.dtd'] += DTD
        self.files[PATH] = XML
        path = 'common/bcp47/test.xml'
        root = ET.fromstring(self.files[path])
        key = root.find("./keyword/key[@name='tz']")
        ET.SubElement(key, 'type', name='gbalp', description='Alpha', alias='Area/Alpha Area/Alternate')
        ET.SubElement(key, 'type', name='gmt', description='GMT', alias='Etc/GMT')
        self.files[path] = ET.tostring(root, encoding='utf-8')


class TimezonePresentationTests(unittest.TestCase):
    def test_utc_seconds_and_gregorian_leaps_without_host_timezone(self):
        self.assertEqual(source.timestamp('1970-01-01 00:00'), 0)
        self.assertEqual(source.timestamp('1969-12-31 23:59:59'), -1)
        self.assertEqual(source.timestamp('1972-01-07 00:44:30'), 63593070)
        for year, month, day in ((1,1,1), (1600,2,29), (1900,3,1), (2000,2,29), (2100,3,1), (9999,12,31)):
            expected = (datetime.datetime(year, month, day) - datetime.datetime(1970,1,1)).days * 86400
            self.assertEqual(source.timestamp('{:04d}-{:02d}-{:02d} 00:00'.format(year,month,day)), expected)
        self.assertEqual(source.timestamp('0001-01-01 00:00') - source.timestamp('0000-01-01 00:00'), 366 * 86400)
        for text in ('1900-02-29 00:00', '2001-04-31 00:00', '2000-00-01 00:00', '2000-01-00 00:00',
                     '2000-01-01 24:00', '2000-01-01 00:60', '2000-01-01 00:00:60', '2000-1-1 0:00',
                     '2000-01-01T00:00', ' 2000-01-01 00:00'):
            with self.subTest(text=text), self.assertRaises(DataError):
                source.timestamp(text)

    def test_offsets_preserve_zero_and_reject_unreviewed_syntax(self):
        for text, value in (('+00',0), ('-00:00',0), ('+01',3600), ('-03:30',-12600), ('+23:59',86340)):
            self.assertEqual(source.offset(text), value)
        for text in ('00', '+1', '+24', '+01:60', '+01:00:00', '+00 ', '−01'):
            with self.subTest(text=text), self.assertRaises(DataError):
                source.offset(text)

    def test_gaps_shared_golden_zone_and_distinct_namespaces(self):
        data = source.collect(Sources())
        self.assertEqual(data['zones'], ('Area/Alpha', 'Etc/GMT'))
        self.assertEqual(data['periods'][0], ('Area/Alpha', None, 63593070, 'West', 0, 3600))
        self.assertLess(data['periods'][0][2], data['periods'][1][1])
        self.assertEqual(data['periods'][-1], ('Etc/GMT', None, None, 'GMT', 0, None))
        self.assertIn(('East','001','Area/Alpha'), data['preferred'])
        self.assertIn(('West','001','Area/Alpha'), data['preferred'])
        self.assertEqual(data['primary'], (('GB','Area/Alpha'),))
        self.assertEqual(data['versions'], (('typeVersion','authored'),))
        self.assertIn(('mgmt','GMT','40'), data['ids'])

    def test_adjacent_bounds_and_canonical_period_order(self):
        a, b = Sources(), Sources()
        a.files[PATH] = XML.replace(b'1972-01-08 00:00', b'1972-01-07 00:44:30')
        root = ET.fromstring(a.files[PATH])
        zone = root.find('./metaZones/metazoneInfo/timezone')
        zone[:] = list(reversed(list(zone)))
        b.files[PATH] = ET.tostring(root, encoding='utf-8')
        self.assertEqual(source.collect(a), source.collect(b))
        data = source.collect(a)
        self.assertEqual(data['periods'][0][2], data['periods'][1][1])

    def test_reject_bad_periods_shapes_references_and_duplicate_keys(self):
        changes = ((b'1972-01-08 00:00', b'1972-01-06 00:00'),
                   (b' to="1972-01-07 00:44:30"', b''),
                   (b'to="2010-01-01 00:00"', b'to="1972-01-08 00:00"'),
                   (b'mzone="West"', b'mzone="Unknown"'),
                   (b'type="Area/Alpha"', b'type="Area/Alternate"'),
                   (b'type="Area/Alpha"/>', b'type="Etc/GMT"/>'),
                   (b'type="metazones"', b'type="windows"'),
                   (b'territory="001"', b'territory="GBR"'),
                   (b'<usesMetazone ', b'<unknown '),
                   (b'</metazoneInfo>', b'<timezone type="Area/Alpha"/></metazoneInfo>'),
                   (b'</mapTimezones>', b'<mapZone other="GMT" territory="001" type="Etc/GMT"/></mapTimezones>'),
                   (b'</primaryZones>', b'<primaryZone iso3166="GB">Area/Alpha</primaryZone></primaryZones>'),
                   (b'<primaryZone ', b'<primaryZone unknown="x" '),
                   (b'</metaZones>', b'<metazoneInfo/></metaZones>'),
                   (b'</supplementalData>', b'<primaryZones/></supplementalData>'))
        for old, new in changes:
            sources = Sources()
            sources.files[PATH] = XML.replace(old, new)
            with self.subTest(old=old,new=new), self.assertRaises(DataError):
                source.collect(sources)

    def test_explicitly_reject_unreviewed_metazone_id_forms(self):
        for old, new in ((b'shortId="west"', b'shortId="West"'), (b'longId="West"', b'longId="Bad/ID"'),
                         (b'longId="West"', b'longId="East"'), (b'longId="West"', b''),
                         (b'shortId="west"', b'shortId="west" deprecated="true"'),
                         (b'shortId="west"', b'shortId="west" preferred="East"')):
            sources = Sources()
            sources.files[PATH] = XML.replace(old,new)
            with self.subTest(old=old,new=new), self.assertRaises(DataError):
                source.collect(sources)

    def test_empty_inventory_is_not_formatter_capability(self):
        sources = Sources()
        sources.files[PATH] = b'<supplementalData/>'
        self.assertTrue(all(not values for values in source.collect(sources).values()))


if __name__ == '__main__':
    unittest.main()
