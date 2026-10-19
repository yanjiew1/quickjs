#!/usr/bin/env python3
"""Authored division-unit grammar, independent feature fallback and bad data."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
import unit_grammar as grammar

DTD = b'''<!ELEMENT supplementalData ANY><!ELEMENT grammaticalData ANY>
<!ELEMENT grammaticalFeatures ANY>
<!ATTLIST grammaticalFeatures locales NMTOKENS #REQUIRED>
<!ATTLIST grammaticalFeatures targets NMTOKENS #REQUIRED>
<!ELEMENT grammaticalDerivations ANY>
<!ATTLIST grammaticalDerivations locales NMTOKENS #REQUIRED>
<!ELEMENT deriveComponent EMPTY>
<!ATTLIST deriveComponent feature NMTOKENS #REQUIRED>
<!ATTLIST deriveComponent structure NMTOKENS #REQUIRED>
<!ATTLIST deriveComponent value0 NMTOKEN #REQUIRED>
<!ATTLIST deriveComponent value1 NMTOKEN #REQUIRED>
<!ELEMENT deriveCompound EMPTY>
<!ATTLIST deriveCompound feature NMTOKENS #REQUIRED>
<!ATTLIST deriveCompound structure NMTOKENS #REQUIRED>
<!ATTLIST deriveCompound value NMTOKEN #REQUIRED>
<!ELEMENT unknown EMPTY>'''
ROOT = '''<grammaticalDerivations locales="root">
  <deriveComponent feature="plural" structure="per" value0="compound" value1="one"/>
  <deriveComponent feature="case" structure="per" value0="compound" value1="nominative"/>
  </grammaticalDerivations>'''


class Sources:
    def __init__(self, extra='', root=ROOT):
        self.files = {'common/dtd/ldmlSupplemental.dtd': DTD,
                      'common/supplemental/grammaticalFeatures.xml':
                      ('<supplementalData><grammaticalData>' + root + extra +
                       '</grammaticalData></supplementalData>').encode('utf-8')}

    def read(self, name):
        return self.files[name]


def graph(component=None, general=None):
    return ParentGraph(('root', 'en', 'en-GB', 'de', 'de-AT', 'fr', 'fr-CA'),
                       general or {}, {'grammaticalFeatures': component or {}},
                       {'und': 'en-Latn-US'}, (), False)


def rows(data):
    return {(locale, feature): (origin, value0, value1)
            for locale, feature, origin, value0, value1 in data['rows']}


class UnitGrammarTests(unittest.TestCase):
    def test_per_feature_inheritance_and_provenance(self):
        source = Sources('''<grammaticalDerivations locales="de">
          <deriveComponent feature="case" structure="per" value0="compound" value1="accusative"/>
          </grammaticalDerivations><grammaticalDerivations locales="fr">
          <deriveComponent feature="plural" structure="per" value0="compound" value1="one"/>
          </grammaticalDerivations>''')
        data = grammar.collect(source, graph())
        values = rows(data)
        self.assertEqual(data['features'], ('plural', 'case'))
        self.assertEqual(data['source_records'], 4)
        self.assertEqual(data['skipped_derivations'], 0)
        self.assertEqual(len(values), len(graph().tags) * 2)
        self.assertEqual(values['de-AT', 'case'], ('de', 'compound', 'accusative'))
        self.assertEqual(values['de-AT', 'plural'], ('root', 'compound', 'one'))
        self.assertEqual(values['fr-CA', 'plural'], ('fr', 'compound', 'one'))
        self.assertEqual(values['fr-CA', 'case'], ('root', 'compound', 'nominative'))
        self.assertEqual(values['en-GB', 'plural'], ('root', 'compound', 'one'))

    def test_normalized_locale_lists_and_explicit_parent(self):
        source = Sources('''<grammaticalDerivations locales="de_AT fr_CA">
          <deriveComponent feature="plural" structure="per" value0="other" value1="many"/>
          </grammaticalDerivations>''')
        values = rows(grammar.collect(source, graph({'en-GB': 'fr-CA'})))
        self.assertEqual(values['en-GB', 'plural'], ('fr-CA', 'other', 'many'))
        self.assertEqual(values['de-AT', 'plural'], ('de-AT', 'other', 'many'))
        self.assertEqual(values['en-GB', 'case'], ('root', 'compound', 'nominative'))

    def test_unconsumed_structures_and_gender_remain_separate(self):
        source = Sources('''<grammaticalFeatures targets="nominal" locales="de"/>
          <grammaticalDerivations locales="de">
          <deriveComponent feature="plural" structure="times" value0="one" value1="compound"/>
          <deriveComponent feature="case" structure="power" value0="nominative" value1="compound"/>
          <deriveCompound feature="gender" structure="per" value="feminine"/>
          <deriveCompound feature="gender" structure="prefix" value="0"/>
          </grammaticalDerivations>''')
        data = grammar.collect(source, graph())
        self.assertEqual(data['source_records'], 2)
        self.assertEqual(data['skipped_derivations'], 4)
        self.assertEqual(rows(data)['de', 'case'], ('root', 'compound', 'nominative'))

    def test_grammar_parent_component_is_independent_of_main(self):
        source = Sources('''<grammaticalDerivations locales="de">
          <deriveComponent feature="case" structure="per" value0="compound" value1="accusative"/>
          </grammaticalDerivations>''')
        values = rows(grammar.collect(source, graph(general={'fr-CA': 'de'})))
        self.assertEqual(values['fr-CA', 'case'], ('root', 'compound', 'nominative'))
        unusual = ParentGraph(('root', 'de', 'de-Cyrl'), {}, {'grammaticalFeatures': {}},
                              {'und': 'en-Latn-US', 'de': 'de-Latn-DE'}, (), True)
        self.assertEqual(tuple(unusual.chain('de-Cyrl')), ('de-Cyrl', 'root'))
        self.assertEqual(rows(grammar.collect(source, unusual))['de-Cyrl', 'case'],
                         ('de', 'compound', 'accusative'))
        missing = ParentGraph(('root', 'de'), {}, {}, {'und': 'en-Latn-US'}, (), False)
        with self.assertRaisesRegex(DataError, 'undeclared grammatical'):
            grammar.collect(source, missing)

    def test_sparse_records_can_share_a_locale_in_separate_groups(self):
        source = Sources('''<grammaticalDerivations locales="de">
          <deriveComponent feature="plural" structure="per" value0="compound" value1="other"/>
          </grammaticalDerivations><grammaticalDerivations locales="de">
          <deriveComponent feature="case" structure="per" value0="genitive" value1="compound"/>
          </grammaticalDerivations>''')
        values = rows(grammar.collect(source, graph()))
        self.assertEqual(values['de', 'plural'], ('de', 'compound', 'other'))
        self.assertEqual(values['de', 'case'], ('de', 'genitive', 'compound'))

    def test_source_order_does_not_change_resolved_rows(self):
        plural = '''<grammaticalDerivations locales="fr de">
          <deriveComponent feature="plural" structure="per" value0="compound" value1="one"/>
          </grammaticalDerivations>'''
        case = '''<grammaticalDerivations locales="de">
          <deriveComponent feature="case" structure="per" value0="compound" value1="accusative"/>
          </grammaticalDerivations>'''
        self.assertEqual(grammar.collect(Sources(plural + case), graph()),
                         grammar.collect(Sources(case + plural), graph()))

    def test_invalid_source_records_fail(self):
        entries = (
            '<unknown/>',
            '<deriveComponent feature="plural" structure="per" value0="0" value1="one"/>',
            '<deriveComponent feature="plural" structure="per" value0="one" value1="bogus"/>',
            '<deriveComponent feature="plural case" structure="per" value0="one" value1="one"/>',
            '<deriveComponent feature="case" structure="per" value0="bad case" value1="one"/>',
            '<deriveComponent feature="case" structure="per times" value0="compound" value1="nominative"/>',
            '<deriveComponent feature="gender" structure="per" value0="compound" value1="feminine"/>',
            '<deriveComponent feature="case" structure="per" value0="compound"/>',
            '<deriveComponent feature="case" structure="per" value0="compound" value1="nominative" unknown="x"/>',
            '<deriveComponent feature="case" structure="per" value0="compound" value1="nominative">x</deriveComponent>',
            '<deriveComponent feature="case" structure="per" value0="compound" value1="nominative"><unknown/></deriveComponent>',
            '<deriveCompound feature="gender" structure="per" value="2"/>',
            '<deriveCompound feature="case" structure="per" value="nominative"/>',
            '<deriveComponent feature="case" structure="per" value0="↑↑↑" value1="nominative"/>',
            '<deriveComponent feature="case" structure="per" value0="compound" value1="∅∅∅"/>',
            '<deriveComponent feature="plural" structure="per" value0="compound" value1=""/>',
        )
        for entry in entries:
            source = Sources('<grammaticalDerivations locales="de">' + entry + '</grammaticalDerivations>')
            with self.subTest(entry=entry), self.assertRaises(DataError):
                grammar.collect(source, graph())

    def test_repeated_missing_and_malformed_containers(self):
        extras = (
            ROOT,
            '<grammaticalDerivations locales="de de"><deriveComponent feature="plural" structure="per" value0="one" value1="one"/></grammaticalDerivations>',
            '<grammaticalDerivations locales="de_AT de-AT"/>',
            '<grammaticalDerivations locales=""/>',
            '<grammaticalDerivations locales="de-u-nu-latn"/>',
            '<grammaticalDerivations locales="de">text</grammaticalDerivations>',
            '<grammaticalDerivations locales="de"><deriveComponent feature="case" structure="per" value0="compound" value1="nominative"/>text</grammaticalDerivations>',
            '<unknown/>',
            'text',
            '</grammaticalData><grammaticalData>',
        )
        for extra in extras:
            with self.subTest(extra=extra), self.assertRaises(DataError):
                grammar.collect(Sources(extra), graph())
        for root in ('', ROOT.replace('feature="case"', 'feature="plural"'),
                     '<grammaticalDerivations locales="root"><deriveComponent '
                     'feature="plural" structure="per" value0="compound" value1="one"/>'
                     '</grammaticalDerivations>'):
            with self.subTest(root=root), self.assertRaises(DataError):
                grammar.collect(Sources(root=root), graph())


if __name__ == '__main__':
    unittest.main()
