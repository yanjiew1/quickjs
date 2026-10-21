#!/usr/bin/env python3
"""Preserve source order and reject unsupported segmentation source shapes."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import segment_sources as source

DTD = b'''<!ELEMENT ldml ANY><!ELEMENT identity ANY>
<!ELEMENT segmentations ANY><!ATTLIST segmentations alt NMTOKEN #IMPLIED>
<!ELEMENT segmentation ANY><!ATTLIST segmentation type NMTOKEN #REQUIRED>
<!ATTLIST segmentation alt NMTOKEN #IMPLIED>
<!ELEMENT variables ANY><!ELEMENT variable ANY><!ATTLIST variable id CDATA #REQUIRED>
<!ATTLIST variable alt NMTOKEN #IMPLIED>
<!ELEMENT segmentRules ANY><!ELEMENT rule ANY><!ATTLIST rule id NMTOKEN #REQUIRED>
<!ELEMENT suppressions ANY><!ATTLIST suppressions type NMTOKEN "standard">
<!ELEMENT suppression ANY><!ATTLIST suppression alt NMTOKEN #IMPLIED>'''
PATH = 'common/segments/root.xml'


class Sources:
    def __init__(self, text):
        self.files = {'common/dtd/ldml.dtd': DTD, PATH: text.encode('utf-8')}
        self.reads = []

    def read(self, name):
        self.reads.append(name)
        return self.files[name]

    def paths(self, prefix):
        return tuple(name for name in self.files if name.startswith(prefix))


def document(text):
    return '<ldml><segmentations>' + text + '</segmentations></ldml>'


class SegmentSourcesTests(unittest.TestCase):
    def test_ordered_previous_value_redefinitions_and_exact_text(self):
        data = Sources(document('''<segmentation type="WordBreak"><variables>
<variable id="$Hiragana">\\p{Hiragana}</variable>
<variable id="$Hiragana">($Hiragana $FE*)</variable>
</variables><segmentRules><rule id="13.3"> $Hiragana × $Hiragana </rule>
<rule id="3">$CR × $LF</rule></segmentRules></segmentation>'''))
        row, = source.collect(data)['records']
        self.assertEqual(row.variables, (source.Definition('$Hiragana', r'\p{Hiragana}'),
                                        source.Definition('$Hiragana', '($Hiragana $FE*)')))
        self.assertEqual(row.rules[0], source.Definition('13.3', ' $Hiragana × $Hiragana '))
        self.assertEqual([rule.identifier for rule in row.rules], ['13.3', '3'])
        self.assertEqual((row.locale, row.kind, row.source), ('root', 'WordBreak', PATH))
        self.assertEqual(data.reads, ['common/dtd/ldml.dtd', PATH])

    def test_empty_absent_suppressions_and_alternate_selection(self):
        data = source.collect(Sources(document('''
<segmentation type="WordBreak"><variables/></segmentation>
<segmentation type="SentenceBreak"><suppressions>
<suppression>Mr.</suppression><suppression> A\u00a0B. </suppression>
<suppression alt="variant">discard</suppression></suppressions></segmentation>
<segmentation type="WordBreak" alt="variant"><unknown/></segmentation>''')))
        word, sentence = data['records']
        self.assertEqual(word.variables, ())
        self.assertIsNone(word.rules)
        self.assertIsNone(word.suppressions)
        self.assertEqual(sentence.suppressions, ('Mr.', ' A\u00a0B. '))
        self.assertEqual(data['skipped_alternates'], 2)

    def test_variable_names_follow_ldml_not_c_identifier_syntax(self):
        row, = source.collect(Sources(document('''<segmentation type="WordBreak">
<variables><variable id="$1">a</variable><variable id="$_2">b</variable></variables>
</segmentation>''')))['records']
        self.assertEqual([value.identifier for value in row.variables], ['$1', '$_2'])

    def test_line_breaking_is_inventoried_without_claiming_execution(self):
        data = source.collect(Sources(document('''
<segmentation type="LineBreak"><notAnIntlRule/></segmentation>
<segmentation type="GraphemeClusterBreak"><variables/></segmentation>''')))
        self.assertEqual(data['excluded_line_groups'], (('root', PATH),))
        self.assertEqual(len(data['records']), 1)

    def test_reject_unknown_or_ambiguous_relevant_structure(self):
        cases = [
            '<segmentation type="unknown"/>',
            '<segmentation type="WordBreak" extra="value"/>',
            '<segmentation type="WordBreak"><alias/></segmentation>',
            '<segmentation type="WordBreak">unexpected<variables/></segmentation>',
            '<segmentation type="WordBreak"><variables>\u00a0</variables></segmentation>',
            '<segmentation type="WordBreak"><variables><variable id="$X">a</variable>unexpected</variables></segmentation>',
            '<segmentation type="WordBreak"><segmentRules>unexpected<rule id="3">a</rule></segmentRules></segmentation>',
            '<segmentation type="SentenceBreak"><suppressions>unexpected<suppression>A.</suppression></suppressions></segmentation>',
            '<segmentation type="WordBreak"><variables/><variables/></segmentation>',
            '<segmentation type="WordBreak"><segmentRules/><variables/></segmentation>',
            '<segmentation type="WordBreak"><variables><variable id="bad"/></variables></segmentation>',
            '<segmentation type="WordBreak"><segmentRules><rule id="x"/></segmentRules></segmentation>',
            '<segmentation type="WordBreak"><segmentRules><rule id="1"/><rule id="1"/></segmentRules></segmentation>',
            '<segmentation type="SentenceBreak"><suppressions type="unknown"/></segmentation>',
            '<segmentation type="WordBreak"/><segmentation type="WordBreak"/>',
        ]
        for case in cases:
            with self.subTest(case=case), self.assertRaises(DataError):
                source.collect(Sources(document(case)))
        with self.assertRaises(DataError):
            source.collect(Sources(document('unexpected<segmentation type="WordBreak"/>')))

    def test_reject_duplicate_locale_identity_and_missing_root(self):
        data = Sources(document('<segmentation type="WordBreak"/>'))
        data.files['common/segments/en_US.xml'] = data.files[PATH]
        data.files['common/segments/en-US.xml'] = data.files[PATH]
        with self.assertRaises(DataError):
            source.collect(data)
        del data.files['common/segments/en-US.xml']
        del data.files[PATH]
        with self.assertRaises(DataError):
            source.collect(data)


if __name__ == '__main__':
    unittest.main()
