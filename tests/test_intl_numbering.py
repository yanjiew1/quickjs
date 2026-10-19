#!/usr/bin/env python3
"""Authored numbering repertoire and locale inheritance regressions."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import numbering
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph


class Sources:
    def __init__(self):
        self.files = {
            'common/dtd/ldmlSupplemental.dtd': b'''<!ELEMENT supplementalData ANY>
              <!ELEMENT numberingSystems ANY><!ELEMENT numberingSystem EMPTY>
              <!ATTLIST numberingSystem id NMTOKEN #REQUIRED>
              <!ATTLIST numberingSystem type (numeric | algorithmic) #REQUIRED>
              <!ATTLIST numberingSystem digits CDATA #IMPLIED>
              <!ATTLIST numberingSystem rules CDATA #IMPLIED>''',
            'common/dtd/ldml.dtd': b'''<!ELEMENT ldml ANY><!ELEMENT numbers ANY>
              <!ATTLIST numbers alt NMTOKEN #IMPLIED>
              <!ELEMENT defaultNumberingSystem (#PCDATA)>
              <!ATTLIST defaultNumberingSystem alt NMTOKEN #IMPLIED>
              <!ELEMENT otherNumberingSystems ANY>
              <!ATTLIST otherNumberingSystems alt NMTOKEN #IMPLIED>
              <!ELEMENT native (#PCDATA)>
              <!ELEMENT traditional (#PCDATA)><!ELEMENT finance (#PCDATA)>
              <!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED>''',
            'common/supplemental/numberingSystems.xml': '''<supplementalData><numberingSystems>
              <numberingSystem id="latn" type="numeric" digits="0123456789"/>
              <numberingSystem id="mathbold" type="numeric" digits="𝟎𝟏𝟐𝟑𝟒𝟓𝟔𝟕𝟖𝟗"/>
              <numberingSystem id="roman" type="algorithmic" rules="roman-upper"/>
            </numberingSystems></supplementalData>'''.encode('utf-8'),
        }
        self.files['common/dtd/ldmlSupplemental.dtd'] += b'<!ATTLIST numberingSystem radix NMTOKEN #IMPLIED>'
        self.set('root', '''<ldml><numbers>
          <defaultNumberingSystem>latn</defaultNumberingSystem>
          <otherNumberingSystems><native>latn</native></otherNumberingSystems>
        </numbers></ldml>''')
        self.set('en', '''<ldml><numbers>
          <defaultNumberingSystem>mathbold</defaultNumberingSystem>
          <defaultNumberingSystem alt="variant">roman</defaultNumberingSystem>
          <otherNumberingSystems><traditional>roman</traditional></otherNumberingSystems>
        </numbers></ldml>''')
        self.set('en_GB', '<ldml/>')
        self.set('de', '<ldml/>')

    def set(self, locale, xml):
        self.files['common/main/' + locale + '.xml'] = xml.encode('utf-8')

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))

    def read(self, name):
        return self.files[name]


class NumberingTests(unittest.TestCase):
    def test_scalar_repertoires_inheritance_and_optional_fields(self):
        source = Sources()
        source.set('en_GB', '<ldml><numbers><defaultNumberingSystem>' + INHERIT +
                   '</defaultNumberingSystem><otherNumberingSystems><traditional>' + BLOCK +
                   '</traditional></otherNumberingSystems></numbers></ldml>')
        value = numbering.collect(source, graph())
        self.assertIn(('mathbold', 'numeric', '𝟎𝟏𝟐𝟑𝟒𝟓𝟔𝟕𝟖𝟗'), value['systems'])
        self.assertIn(('roman', 'algorithmic', 'roman-upper'), value['systems'])
        self.assertIn(('en-GB', 'mathbold', 'latn', '', None), value['locales'])
        self.assertIn(('de', 'latn', 'latn', None, None), value['locales'])
        self.assertIn(('en', 'mathbold', 'latn', 'roman', None), value['locales'])
        self.assertEqual(value['skipped_alternates'], 1)

    def test_invalid_repertoires_rule_references_and_definitions(self):
        name = 'common/supplemental/numberingSystems.xml'
        changes = ((b'0123456789', b'0123456788'), (b'0123456789', b'012345678'),
                   (b'id="latn"', b'id="a"'), (b'id="roman"', b'id="latn"'),
                   (b'id="latn"', b'id="latn" radix="2"'),
                   (b'id="roman"', b'id="roman" radix="16"'),
                   (b'rules="roman-upper"', b'rules="roman upper"'),
                   (b'rules="roman-upper"', b'rules="a//b"'),
                   (b'rules="roman-upper"', b'digits="0123456789" rules="roman-upper"'),
                   (b'digits="0123456789"', b'rules="roman" digits="0123456789"'),
                   (b'</numberingSystems>', b'<unknown/></numberingSystems>'))
        for old, new in changes:
            source = Sources()
            source.files[name] = source.files[name].replace(old, new)
            with self.assertRaises(DataError, msg=repr((old, new))):
                numbering.collect(source, graph())

    def test_alternate_containers_do_not_replace_inherited_data(self):
        source = Sources()
        source.set('en_GB', '''<ldml>
          <numbers alt="variant"><defaultNumberingSystem>roman</defaultNumberingSystem></numbers>
          <numbers><otherNumberingSystems alt="variant"><native>roman</native></otherNumberingSystems>
          <otherNumberingSystems><finance>roman</finance></otherNumberingSystems></numbers>
        </ldml>''')
        value = numbering.collect(source, graph())
        self.assertIn(('en-GB', 'mathbold', 'latn', 'roman', 'roman'), value['locales'])
        self.assertEqual(value['skipped_alternates'], 3)
        source.files['common/supplemental/numberingSystems.xml'] = source.files[
            'common/supplemental/numberingSystems.xml'].replace(b'id="latn"', b'id="latn" radix="10"')
        self.assertEqual(numbering.collect(source, graph()), value)

    def test_invalid_and_missing_locale_preferences(self):
        for group in (
            '<defaultNumberingSystem>unknown</defaultNumberingSystem>',
            '<defaultNumberingSystem>latn</defaultNumberingSystem><defaultNumberingSystem>roman</defaultNumberingSystem>',
            '<defaultNumberingSystem>' + BLOCK + '</defaultNumberingSystem>',
            '<defaultNumberingSystem><unknown/></defaultNumberingSystem>',
            '<alias source="root"/>',
            '<otherNumberingSystems><unknown>latn</unknown></otherNumberingSystems>',
        ):
            source = Sources()
            source.set('en', '<ldml><numbers>' + group + '</numbers></ldml>')
            with self.assertRaises(DataError, msg=group):
                numbering.collect(source, graph())
        source = Sources()
        source.set('root', '<ldml/>')
        with self.assertRaises(DataError):
            numbering.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
