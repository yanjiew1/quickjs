#!/usr/bin/env python3
"""Authored locale alias/extension metadata and semantic-default regressions."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import locale_metadata
from test_intl_parents import Sources as ParentSources

BCP_DTD = b'''<!ELEMENT ldmlBCP47 ANY><!ELEMENT keyword ANY><!ELEMENT key ANY>
<!ATTLIST key name NMTOKEN #REQUIRED><!ATTLIST key extension NMTOKEN #IMPLIED>
<!ATTLIST key valueType (single | multiple | incremental | any) #IMPLIED>
<!ATTLIST key alias NMTOKEN #IMPLIED><!ATTLIST key preferred NMTOKEN #IMPLIED>
<!ATTLIST key deprecated (true | false) "false">
<!ELEMENT type EMPTY><!ATTLIST type name NMTOKEN #REQUIRED>
<!ATTLIST type description CDATA #REQUIRED>
<!ATTLIST type preferred NMTOKEN #IMPLIED><!ATTLIST type alias CDATA #IMPLIED>
<!ATTLIST type deprecated (true | false) "false">
<!ATTLIST type iana CDATA #IMPLIED><!ATTLIST type region CDATA #IMPLIED>
'''


class Sources(ParentSources):
    def __init__(self):
        super().__init__()
        self.files['common/dtd/ldmlBCP47.dtd'] = BCP_DTD
        for kind in locale_metadata.ALIASES:
            self.files['common/dtd/ldmlSupplemental.dtd'] += (
                '<!ELEMENT {0}Alias EMPTY><!ATTLIST {0}Alias type NMTOKEN #REQUIRED>'
                '<!ATTLIST {0}Alias replacement NMTOKENS #REQUIRED>'.format(kind)).encode('ascii')
        self.files['common/supplemental/supplementalMetadata.xml'] = b'''<supplementalData><metadata><alias>
          <languageAlias type="iw" replacement="he"/>
          <languageAlias type="i_klingon" replacement="tlh"/>
          <territoryAlias type="SU" replacement="RU AM"/>
          <scriptAlias type="Qaai" replacement="Zinh"/>
          <variantAlias type="heploc" replacement="alalc97"/>
          <subdivisionAlias type="fi01" replacement="AX"/>
        </alias></metadata></supplementalData>'''
        self.files['common/bcp47/test.xml'] = b'''<ldmlBCP47><keyword>
          <key name="ca" valueType="incremental" alias="calendar">
            <type name="gregory" description="Gregorian" alias="gregorian"/>
            <type name="islamic-civil" description="Civil Islamic"/>
            <type name="islamicc" description="Old name" deprecated="true" preferred="islamic-civil"/>
          </key>
          <key name="kn"><type name="true" description="Numeric"/></key>
          <key name="tz"><type name="inccu" description="Kolkata" alias="Asia/Calcutta Asia/Kolkata"
              iana="Asia/Kolkata" region="IN"/></key>
          <key extension="t" name="x0" valueType="any">
            <type name="PRIVATE_USE" description="Variable"/>
          </key>
        </keyword></ldmlBCP47>'''


class MetadataTests(unittest.TestCase):
    def test_semantic_defaults_alias_priority_and_schema_variables(self):
        result = locale_metadata.collect(Sources())
        self.assertIn(('territory', 'su', ('ru', 'am')), result['aliases'])
        self.assertIn(('language', 'i-klingon', ('tlh',)), result['aliases'])
        self.assertIn(('u', 'kn', 'single', (), '', False), result['keys'])
        self.assertIn(('u', 'ca', 'incremental', ('calendar',), '', False), result['keys'])
        self.assertIn(('t', 'x0', 'PRIVATE_USE', (), '', False, True, '', ''), result['types'])
        self.assertIn(('u', 'ca', 'islamicc', (), 'islamic-civil', True, False, '', ''), result['types'])
        self.assertIn(('u', 'tz', 'inccu', ('Asia/Calcutta', 'Asia/Kolkata'), '', False, False,
                       'Asia/Kolkata', 'IN'), result['types'])
        self.assertIn(('und', 'en-Latn-US'), result['likely'])

    def test_invalid_and_conflicting_aliases(self):
        changes = ((b'type="iw"', b'type="iw!"'),
                   (b'replacement="he"', b'replacement="he&#9;en"'),
                   (b'replacement="he"', b'replacement="he en"'),
                   (b'replacement="RU AM"', b'replacement="RU ru"'),
                   (b'</alias>', b'<languageAlias type="iw" replacement="he"/></alias>'),
                   (b'</alias>', b'<unknownAlias type="x" replacement="y"/></alias>'))
        for old, new in changes:
            source = Sources()
            path = 'common/supplemental/supplementalMetadata.xml'
            source.files[path] = source.files[path].replace(old, new)
            with self.assertRaises(DataError):
                locale_metadata.collect(source)

    def test_invalid_extensions_types_and_preferred_chains(self):
        changes = ((b'name="kn"', b'name="k0"'),
                   (b'extension="t"', b'extension="x"'),
                   (b'name="gregory"', b'name="ab"'),
                   (b'name="PRIVATE_USE"', b'name="UNKNOWN_VARIABLE"'),
                   (b'preferred="islamic-civil"', b'preferred="unknown"'),
                   (b'preferred="islamic-civil"', b'preferred="islamicc"'),
                   (b'name="kn"', b'name="kn" preferred="kn"'),
                   (b'</keyword>', b'<key name="kn"/></keyword>'),
                   (b'alias="gregorian"', b'alias="gregorian&#9;old"'))
        for old, new in changes:
            source = Sources()
            path = 'common/bcp47/test.xml'
            source.files[path] = source.files[path].replace(old, new)
            with self.assertRaises(DataError):
                locale_metadata.collect(source)

    def test_unhandled_root_content_and_timezone_metadata_fail(self):
        changes = ((b'</ldmlBCP47>', b'<attribute name="new" description="future"/></ldmlBCP47>'),
                   (b'<keyword>', b'<keyword unknown="x">'),
                   (b'region="IN"', b'region="india"'),
                   (b'iana="Asia/Kolkata"', b'iana="Asia/Kolkata extra"'),
                   (b'iana="Asia/Kolkata"', b'iana="/"'),
                   (b'iana="Asia/Kolkata"', b'iana="Asia//Kolkata"'),
                   (b'iana="Asia/Kolkata"', b'iana="Asia/../Kolkata"'))
        for old, new in changes:
            source = Sources()
            path = 'common/bcp47/test.xml'
            source.files[path] = source.files[path].replace(old, new)
            with self.assertRaises(DataError):
                locale_metadata.collect(source)


if __name__ == '__main__':
    unittest.main()
