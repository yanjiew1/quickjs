#!/usr/bin/env python3
"""Typed CLDR XML collector primitives, without source downloads."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

TOOLS = Path(__file__).resolve().parents[1] / 'tools' / 'intl'
sys.path.insert(0, str(TOOLS))
import cldr_xml as cldr

DTD = b'''<!-- Authored fixture, not Unicode data. -->
<!ELEMENT ldml (pattern*)>
<!ELEMENT pattern (#PCDATA)>
<!ATTLIST pattern type NMTOKEN #REQUIRED>
<!ATTLIST pattern draft (approved | contributed | provisional) "approved">
    <!--@METADATA-->
<!ATTLIST pattern description CDATA #IMPLIED>
    <!--@VALUE-->
<!ATTLIST pattern width (short | long) "long">
<!ATTLIST pattern enabled (true | false) #FIXED "true">
<!ATTLIST pattern tokens NMTOKENS "a   b">
<!ATTLIST pattern escaped CDATA "a&amp;b">
    <!--@VALUE-->
'''


class SchemaTests(unittest.TestCase):
    def test_roles_defaults_and_whitespace(self):
        schema = cldr.Schema(DTD)
        element = ET.fromstring(
            '<pattern type="  date  " draft="contributed" description=" a  b "/>'
        )
        fields = schema.attributes(element)
        self.assertEqual(fields, {'type': 'date', 'draft': 'contributed',
                                 'description': ' a  b ', 'width': 'long',
                                 'enabled': 'true', 'tokens': 'a b', 'escaped': 'a&b'})
        self.assertEqual(schema.key(element),
                         ('pattern', (('enabled', 'true'), ('tokens', 'a b'),
                                      ('type', 'date'), ('width', 'long'))))
        other = ET.fromstring('<pattern type="date" draft="provisional"/>')
        self.assertEqual(schema.key(element), schema.key(other))

    def test_invalid_consumed_attributes(self):
        schema = cldr.Schema(DTD)
        for text in ('<unknown/>', '<pattern/>', '<pattern type="a" new="x"/>',
                     '<pattern type="a" width="wide"/>',
                     '<pattern type="a" width="&#160;long&#160;"/>',
                     '<pattern type="a" width="&#9;long&#9;"/>',
                     '<pattern type="a" width="&#10;long&#10;"/>',
                     '<pattern type="a" width="&#13;long&#13;"/>',
                     '<pattern type="a" enabled="false"/>'):
            with self.assertRaises(cldr.DataError):
                schema.attributes(ET.fromstring(text))

    def test_unsupported_or_conflicting_schema(self):
        cases = (
            b'<!ELEMENT pattern EMPTY><!ATTLIST pattern a ID #IMPLIED>',
            b'<!ELEMENT pattern EMPTY><!ATTLIST pattern a CDATA #IMPLIED>'
            b'<!--@VALUE--><!--@METADATA-->',
            b'<!ELEMENT pattern EMPTY><!ATTLIST pattern a (x | x) "x">',
            b'<!ELEMENT pattern EMPTY><!ATTLIST pattern a (x | y) "z">',
            b'<!ELEMENT pattern EMPTY><!ATTLIST pattern a (x | y) "&#9;x&#9;">',
            b'<!ELEMENT pattern EMPTY><!ATTLIST pattern a (x | y) "&#10;x&#10;">',
            b'<!ELEMENT pattern EMPTY><!ATTLIST pattern a CDATA "&unresolved;">',
            b'<!ATTLIST pattern a CDATA #IMPLIED>',
            b'<!ELEMENT pattern EMPTY><!ENTITY extra "x">',
            DTD + b'<!ATTLIST pattern type NMTOKEN #IMPLIED>',
        )
        for data in cases:
            with self.assertRaises(cldr.DataError):
                cldr.Schema(data)

    def test_comment_declarations_are_not_schema(self):
        schema = cldr.Schema(b'<!-- <!ATTLIST fake type ID #IMPLIED> -->' + DTD)
        self.assertNotIn('fake', schema.fields)

    def test_literal_xml_whitespace_is_normalized(self):
        schema = cldr.Schema(DTD)
        element = ET.fromstring('<pattern type="a" width="\tlong\n"/>')
        self.assertEqual(schema.attributes(element)['width'], 'long')
        token = ET.fromstring('<pattern type="a" tokens="x&#9;y"/>')
        self.assertEqual(schema.attributes(token)['tokens'], 'x\ty')


class XMLTests(unittest.TestCase):
    def test_exact_text_and_child_order(self):
        root = cldr.parse_xml(
            '<ldml><pattern> a\u00a0{0} </pattern><pattern>&amp;{1}</pattern></ldml>'
            .encode('utf-8'), 'ldml')
        self.assertEqual([child.text for child in root], [' a\u00a0{0} ', '&{1}'])

    def test_external_dtd_is_not_required(self):
        root = cldr.parse_xml(
            b'<!DOCTYPE ldml SYSTEM "https://example.invalid/missing.dtd"><ldml/>',
            'ldml')
        self.assertEqual(root.tag, 'ldml')

    def test_internal_entities_malformed_and_wrong_root(self):
        for data in (b'<!DOCTYPE ldml [<!ENTITY value "expanded">]><ldml>&value;</ldml>',
                     b'<ldml>', b'<other/>'):
            with self.assertRaises(cldr.DataError):
                cldr.parse_xml(data, 'ldml', 'test fixture')

    def test_alternative_encodings_cannot_hide_entities(self):
        text = '<!DOCTYPE ldml [<!ENTITY value "expanded">]><ldml>&value;</ldml>'
        for encoding in ('utf-16', 'utf-16-le', 'utf-32'):
            with self.assertRaises(cldr.DataError):
                cldr.parse_xml(text.encode(encoding), 'ldml')


if __name__ == '__main__':
    unittest.main()
