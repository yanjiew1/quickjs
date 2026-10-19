#!/usr/bin/env python3
"""Currency source maps: exact names, sparse inheritance and fraction metadata."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import currencies
from parents import ParentGraph
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': b'''<!ELEMENT ldml ANY>
          <!ELEMENT numbers ANY><!ATTLIST numbers alt NMTOKENS #IMPLIED>
          <!ELEMENT currencies ANY><!ATTLIST currencies alt NMTOKENS #IMPLIED>
          <!ELEMENT currency ANY><!ATTLIST currency type NMTOKEN #REQUIRED>
          <!ATTLIST currency draft CDATA #IMPLIED><!ATTLIST currency alt NMTOKENS #IMPLIED>
          <!ATTLIST currency numberSystem NMTOKEN #IMPLIED>
          <!ELEMENT displayName (#PCDATA)><!ATTLIST displayName count CDATA #IMPLIED>
          <!ATTLIST displayName alt NMTOKENS #IMPLIED><!ATTLIST displayName draft CDATA #IMPLIED>
          <!ELEMENT symbol (#PCDATA)><!ATTLIST symbol alt NMTOKENS #IMPLIED>
          <!ELEMENT decimal (#PCDATA)><!ELEMENT group (#PCDATA)><!ELEMENT pattern (#PCDATA)>
          <!ATTLIST pattern type NMTOKEN "standard"><!ATTLIST pattern count CDATA #IMPLIED>
          <!ATTLIST pattern numbers NMTOKENS #IMPLIED>
          <!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED>
          <!ATTLIST alias path CDATA #IMPLIED><!ELEMENT unknown ANY>''',
          'common/dtd/ldmlSupplemental.dtd': b'''<!ELEMENT supplementalData ANY>
          <!ELEMENT currencyData ANY><!ELEMENT fractions ANY><!ELEMENT info EMPTY>
          <!ATTLIST info iso4217 NMTOKEN #REQUIRED><!ATTLIST info digits NMTOKEN #IMPLIED>
          <!ATTLIST info rounding NMTOKEN #IMPLIED><!ATTLIST info cashDigits NMTOKEN #IMPLIED>
          <!ATTLIST info cashRounding NMTOKEN #IMPLIED>'''}
        self.fractions('<info iso4217="DEFAULT" digits="2" rounding="0"/>'
                       '<info iso4217="JPY" digits="0" rounding="0"/>'
                       '<info iso4217="CHF" cashRounding="5"/>')
        self.set('root', '<currency type="USD"><displayName>US dollar</displayName>'
                        '<displayName count="other">US dollars</displayName><symbol>$</symbol>'
                        '<symbol alt="narrow">$</symbol><symbol alt="formal">US$</symbol>'
                        '<symbol alt="variant">USD</symbol></currency>'
                        '<currency type="JPY"><symbol>¥</symbol></currency>')
        for locale in ('en', 'en_GB', 'de'):
            self.set(locale, '')

    def fractions(self, text):
        self.files['common/supplemental/supplementalData.xml'] = (
            '<supplementalData><currencyData><fractions>' + text +
            '</fractions></currencyData></supplementalData>').encode('utf-8')

    def set(self, locale, contents):
        self.files['common/main/' + locale + '.xml'] = (
            '<ldml><numbers><currencies>' + contents + '</currencies></numbers></ldml>').encode('utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


class CurrencyTests(unittest.TestCase):
    def maps(self, data):
        return {locale: {(code, field, count): value for code, field, count, value in data['maps'][index]}
                for locale, index in data['rows']}

    def test_sparse_sharing_and_exact_fields(self):
        data = currencies.collect(Sources(), graph())
        self.assertEqual(len(data['maps']), 1)
        self.assertEqual(data['codes'], ('JPY', 'USD'))
        values = self.maps(data)['en']
        self.assertEqual(values[('USD', 'name', '')], 'US dollar')
        self.assertEqual(values[('USD', 'name', 'other')], 'US dollars')
        self.assertEqual(values[('USD', 'formal', '')], 'US$')
        self.assertEqual(values[('USD', 'variant', '')], 'USD')
        self.assertNotIn(('USD', 'name', 'one'), values) # No invented plural fallback.
        self.assertNotIn(('JPY', 'name', ''), values) # No invented currency-code text.

    def test_counted_name_origin_survives_equal_text_overrides(self):
        source = Sources()
        source.set('en', '<currency type="USD"><displayName count="other">US dollars</displayName></currency>')
        source.set('en_GB', '<currency type="USD"><displayName count="other">' + INHERIT +
                           '</displayName></currency>')
        data = currencies.collect(source, graph())
        self.assertEqual(len(data['maps']), 1)
        self.assertEqual(dict(data['root_names']), {'de': (('USD', 'other'),),
                                                   'root': (('USD', 'other'),)})
        source.set('en', '<currency type="USD"><displayName count="other">' + INHERIT +
                        '</displayName></currency>')
        data = currencies.collect(source, graph())
        self.assertEqual(set(dict(data['root_names'])), set(graph().tags))

    def test_inheritance_markers_empty_and_whitespace(self):
        source = Sources()
        source.set('en', '<currency type="USD"><displayName count="one"> dollar </displayName>'
                        '<decimal>;</decimal><group> </group><pattern>¤0.00</pattern></currency>')
        source.set('en_GB', '<currency type="USD"><displayName count="one">' + INHERIT +
                            '</displayName><displayName count="other">' + BLOCK +
                            '</displayName><symbol/><symbol alt="narrow">' + INHERIT +
                            '</symbol></currency>')
        rows = self.maps(currencies.collect(source, graph()))
        self.assertEqual(rows['en-GB'][('USD', 'name', 'one')], ' dollar ')
        self.assertEqual(rows['en-GB'][('USD', 'name', 'other')], BLOCK)
        self.assertEqual(rows['en-GB'][('USD', 'symbol', '')], '')
        self.assertEqual(rows['en-GB'][('USD', 'narrow', '')], '$')
        self.assertEqual(rows['en-GB'][('USD', 'group', '')], ' ')
        self.assertEqual(rows['en-GB'][('USD', 'pattern', '')], '¤0.00')
        self.assertNotIn(('USD', 'decimal', ''), rows['de'])

    def test_alternates_and_metadata_do_not_change_identity(self):
        source = Sources()
        source.set('en', '<currency type="USD"><symbol alt="unknown">ignore</symbol>'
                        '<displayName alt="short">ignore</displayName>'
                        '<displayName draft="unconfirmed">dollar</displayName></currency>'
                        '<currency type="ABC" alt="variant"><symbol>ignore</symbol></currency>')
        data = currencies.collect(source, graph())
        self.assertEqual(data['skipped_alternates'], 2)
        self.assertNotIn('ABC', data['codes'])
        self.assertEqual(self.maps(data)['en'][('USD', 'name', '')], 'dollar')

    def test_fraction_defaults_and_cash_metadata(self):
        source = Sources()
        result = {row[0]: row[1:] for row in currencies.collect(source, graph())['fractions']}
        self.assertEqual(result['DEFAULT'], (2, 0, 2, 0))
        self.assertEqual(result['JPY'], (0, 0, 0, 0))
        self.assertEqual(result['CHF'], (2, 0, 2, 5))
        source.fractions('<info iso4217="AAA" digits="3" rounding="5"/>')
        result = dict((row[0], row[1:]) for row in currencies.fraction_data(source))
        self.assertEqual(result['AAA'], (3, 5, 3, 5))
        self.assertEqual(result['DEFAULT'], (2, 0, 2, 0))

    def test_invalid_fraction_records(self):
        for contents in ('<info iso4217="usd"/>', '<info iso4217="ABCD"/>',
                         '<info iso4217="USD" digits="-1"/>', '<info iso4217="USD" digits="101"/>',
                         '<info iso4217="USD" rounding="1.5"/>', '<info iso4217="USD" cashDigits="101"/>',
                         '<info iso4217="USD" rounding="4294967296"/>',
                         '<info iso4217="USD"/><info iso4217="USD"/>',
                         'unexpected<info iso4217="USD"/>',
                         '<info iso4217="USD">unexpected</info>'):
            source = Sources()
            source.fractions(contents)
            with self.assertRaises(DataError, msg=contents):
                currencies.fraction_data(source)

    def test_malformed_containers_fields_and_aliases(self):
        for contents in ('<currency type="usd"/>', '<currency type="USD"/><currency type="USD"/>',
                         '<currency type="USD"><displayName count="unknown">x</displayName></currency>',
                         '<currency type="USD"><symbol>x</symbol><symbol>y</symbol></currency>',
                         '<currency type="USD"><pattern count="one">0</pattern></currency>',
                         '<currency type="USD" numberSystem="arab"/>',
                         '<currency type="USD"><pattern numbers="arab">0</pattern></currency>',
                         '<currency type="USD">unexpected<symbol>x</symbol></currency>',
                         '<currency type="USD"><symbol><unknown/></symbol></currency>',
                         '<currency type="USD"><unknown/></currency>',
                         '<currency type="USD"><alias source="locale" path="../currency"/></currency>',
                         '<alias source="locale" path="../currencies"/>'):
            source = Sources()
            source.set('en', contents)
            with self.assertRaises(DataError, msg=contents):
                currencies.collect(source, graph())

    def test_long_parent_chain_uses_one_shared_map(self):
        tags = tuple('en-' + str(i).zfill(4) for i in range(1100))
        general = {tags[i]: tags[i + 1] for i in range(len(tags) - 1)}
        general[tags[-1]] = 'root'
        parents = ParentGraph(tags + ('root',), general, {}, {'und': 'en-Latn-US'}, (), False)
        source = Sources()
        source.files = {name: value for name, value in source.files.items()
                        if not name.startswith('common/main/') or name.endswith('/root.xml')}
        data = currencies.collect(source, parents)
        self.assertEqual(len(data['maps']), 1)
        self.assertGreaterEqual(len(data['rows']), 1101)
        self.assertEqual(len(data['rows']), len(parents.tags))


if __name__ == '__main__':
    unittest.main()
