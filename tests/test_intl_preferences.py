#!/usr/bin/env python3
"""Authored region preference ordering, fallback and script direction tests."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import preferences

DTD = b'''<!ELEMENT supplementalData ANY>
<!ELEMENT weekData ANY><!ELEMENT calendarPreferenceData ANY><!ELEMENT timeData ANY>
<!ELEMENT minDays EMPTY><!ATTLIST minDays count NMTOKEN #REQUIRED>
<!ATTLIST minDays territories NMTOKENS #REQUIRED>
<!ELEMENT firstDay EMPTY><!ATTLIST firstDay day NMTOKEN #REQUIRED>
<!ATTLIST firstDay territories NMTOKENS #REQUIRED><!ATTLIST firstDay alt NMTOKEN #IMPLIED>
<!ELEMENT weekendStart EMPTY><!ATTLIST weekendStart day NMTOKEN #REQUIRED>
<!ATTLIST weekendStart territories NMTOKENS #REQUIRED>
<!ATTLIST weekendStart time CDATA #IMPLIED>
<!ELEMENT weekendEnd EMPTY><!ATTLIST weekendEnd day NMTOKEN #REQUIRED>
<!ATTLIST weekendEnd territories NMTOKENS #REQUIRED>
<!ELEMENT weekOfPreference EMPTY><!ATTLIST weekOfPreference ordering NMTOKENS #REQUIRED>
<!ATTLIST weekOfPreference locales NMTOKENS #REQUIRED>
<!ELEMENT calendarPreference EMPTY><!ATTLIST calendarPreference territories NMTOKENS #REQUIRED>
<!ATTLIST calendarPreference ordering NMTOKENS #REQUIRED>
<!ELEMENT hours EMPTY><!ATTLIST hours preferred NMTOKEN #REQUIRED>
<!ATTLIST hours allowed NMTOKENS #REQUIRED><!ATTLIST hours regions NMTOKENS #REQUIRED>
'''


class Sources:
    def __init__(self):
        self.files = {
            'common/dtd/ldmlSupplemental.dtd': DTD,
            'common/supplemental/supplementalData.xml': b'''<supplementalData>
              <weekData>
                <firstDay day="mon" territories="001 GB"/>
                <firstDay day="sun" territories="GB" alt="variant"/>
                <firstDay day="sun" territories="US"/>
                <minDays count="1" territories="001"/>
                <minDays count="4" territories="GB"/>
                <weekendStart day="sat" territories="001"/>
                <weekendEnd day="sun" territories="001"/>
                <weekendStart day="sun" territories="ZZ"/>
                <weekendEnd day="mon" territories="ZZ"/>
                <weekOfPreference ordering="weekOfYear" locales="und"/>
              </weekData>
              <calendarPreferenceData>
                <calendarPreference territories="001" ordering="gregorian"/>
                <calendarPreference territories="TH" ordering="buddhist gregorian"/>
              </calendarPreferenceData>
              <timeData>
                <hours preferred="H" allowed="hB H hb K k" regions="001 en_IL"/>
                <hours preferred="h" allowed="h H" regions="US"/>
              </timeData>
            </supplementalData>''',
            'common/properties/scriptMetadata.txt': b'''# authored script records
Latn;2;004C;IT;1;RECOMMENDED;NO;NO;MIN;NO;YES
Arab;8;0628;SA;1;RECOMMENDED;YES;NO;YES;NO;NO # comment
Zzzz;25;FDD0;ZZ;-1;UNKNOWN;UNKNOWN;UNKNOWN;UNKNOWN;UNKNOWN;UNKNOWN
''',
        }

    def read(self, name):
        return self.files[name]


class PreferenceTests(unittest.TestCase):
    def test_order_world_fallback_wrapping_weekend_and_direction(self):
        value = preferences.collect(Sources())
        self.assertEqual(value['weeks'], (('001', 1, 1, (6, 7)), ('GB', 1, 4, (6, 7)),
                                          ('US', 7, 1, (6, 7)), ('ZZ', 1, 1, (1, 7))))
        self.assertEqual(dict(value['calendars'])['TH'], ('buddhist', 'gregorian'))
        self.assertEqual(dict(value['hours'])['001'], ('h23', 'h12', 'h11', 'h24'))
        self.assertEqual(dict(value['hours'])['en-IL'], ('h23', 'h12', 'h11', 'h24'))
        self.assertEqual(dict(value['scripts']), {'Arab': 'rtl', 'Latn': 'ltr', 'Zzzz': ''})

    def test_invalid_and_duplicate_consumed_records(self):
        changes = ((b'day="mon"', b'day="xxx"'), (b'count="1"', b'count="0"'),
                   (b'territories="001 GB"', b'territories="001 GB GB"'),
                   (b'territories="001 GB"', b'territories="001 gb"'),
                   (b'territories="001 GB"', b'territories="001&#9;GB"'),
                   (b'preferred="H"', b'preferred="j"'),
                   (b'preferred="H"', b'preferred="K"'),
                   (b'allowed="h H"', b'allowed="h j"'),
                   (b'allowed="h H"', b'allowed="h HB"'),
                   (b'regions="US"', b'regions="en_Latn_US"'),
                   (b'ordering="buddhist gregorian"', b'ordering="buddhist buddhist"'),
                   (b'<weekendStart day="sat"', b'<weekendStart time="12:00" day="sat"'),
                   (b'</weekData>', b'<firstDay day="sun" territories="US"/></weekData>'),
                   (b'</timeData>', b'<hours preferred="h" allowed="h" regions="US"/></timeData>'),
                   (b'</calendarPreferenceData>', b'<calendarPreference territories="TH" ordering="gregorian"/></calendarPreferenceData>'),
                   (b'</weekData>', b'<unknown/></weekData>'),
                   (b'</supplementalData>', b'<timeData/></supplementalData>'))
        for old, new in changes:
            source = Sources()
            name = 'common/supplemental/supplementalData.xml'
            source.files[name] = source.files[name].replace(old, new)
            with self.assertRaises(DataError, msg=repr((old, new))):
                preferences.collect(source)

    def test_missing_world_defaults_and_script_schema(self):
        for name, old, new in (
            ('common/supplemental/supplementalData.xml', b'territories="001 GB"', b'territories="GB"'),
            ('common/supplemental/supplementalData.xml', b'regions="001 en_IL"', b'regions="en_IL"'),
            ('common/supplemental/supplementalData.xml', b'territories="001" ordering="gregorian"', b'territories="US" ordering="gregorian"'),
            ('common/properties/scriptMetadata.txt', b';YES;NO;YES;', b';MAYBE;NO;YES;'),
            ('common/properties/scriptMetadata.txt', b'Arab;', b'Latn;'),
            ('common/properties/scriptMetadata.txt', b'Arab;', b'arab;'),
            ('common/properties/scriptMetadata.txt', b';RECOMMENDED;YES;NO;YES;NO;NO', b';RECOMMENDED;YES'),
        ):
            source = Sources()
            source.files[name] = source.files[name].replace(old, new)
            with self.assertRaises(DataError):
                preferences.collect(source)


if __name__ == '__main__':
    unittest.main()
