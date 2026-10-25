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

"""Authored display-name source identities, layers, markers and provenance."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import display_names as names
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph

DTD = b'''<!ELEMENT ldml ANY><!ELEMENT localeDisplayNames ANY>
<!ELEMENT languages ANY><!ELEMENT scripts ANY><!ELEMENT territories ANY>
<!ELEMENT variants ANY><!ELEMENT types ANY><!ELEMENT localeDisplayPattern ANY>
<!ATTLIST languages validSubLocales NMTOKENS #IMPLIED><!--@VALUE-->
<!ATTLIST localeDisplayPattern alt NMTOKENS #IMPLIED>
<!ELEMENT language (#PCDATA)><!ATTLIST language type NMTOKEN #REQUIRED>
<!ATTLIST language alt NMTOKENS #IMPLIED><!ATTLIST language menu NMTOKEN #IMPLIED>
<!ATTLIST language draft NMTOKEN #IMPLIED><!--@METADATA-->
<!ATTLIST language references CDATA #IMPLIED><!--@METADATA-->
<!ELEMENT script (#PCDATA)><!ATTLIST script type NMTOKEN #REQUIRED>
<!ATTLIST script alt NMTOKENS #IMPLIED>
<!ELEMENT territory (#PCDATA)><!ATTLIST territory type NMTOKEN #REQUIRED>
<!ATTLIST territory alt NMTOKENS #IMPLIED>
<!ELEMENT variant (#PCDATA)><!ATTLIST variant type NMTOKEN #REQUIRED>
<!ATTLIST variant alt NMTOKENS #IMPLIED>
<!ELEMENT type (#PCDATA)><!ATTLIST type key NMTOKEN #REQUIRED>
<!ATTLIST type type NMTOKEN #REQUIRED><!ATTLIST type scope NMTOKEN #IMPLIED>
<!ATTLIST type alt NMTOKENS #IMPLIED>
<!ELEMENT localePattern (#PCDATA)><!ATTLIST localePattern alt NMTOKENS #IMPLIED>
<!ELEMENT localeSeparator (#PCDATA)><!ELEMENT localeKeyTypePattern (#PCDATA)>
<!ELEMENT characters ANY><!ELEMENT nestedBracketReplacement (#PCDATA)>
<!ATTLIST nestedBracketReplacement bracket CDATA #REQUIRED>
<!ATTLIST nestedBracketReplacement alt NMTOKENS #IMPLIED>
<!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED><!--@VALUE-->
<!ATTLIST alias path CDATA #REQUIRED><!--@VALUE-->
<!ATTLIST alias alt NMTOKENS #IMPLIED>
<!ELEMENT codePatterns ANY><!ELEMENT special ANY><!ELEMENT unknown ANY>'''


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': DTD}
        for locale in ('root', 'en', 'en_GB', 'de'):
            self.set(locale, '')

    def set(self, locale, content, characters=''):
        self.files['common/main/' + locale + '.xml'] = (
            '<ldml><localeDisplayNames>' + content + '</localeDisplayNames>' +
            '<characters>' + characters + '</characters></ldml>').encode('utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


def layers(data, locale, group):
    head = next(head for tag, kind, head in data['rows'] if (tag, kind) == (locale, group))
    result = []
    while head:
        origin, kind, index, head = data['nodes'][head - 1]
        result.append((origin, kind, data['maps'][index]))
    return result


class DisplayNamesTests(unittest.TestCase):
    def test_all_name_groups_and_exact_text(self):
        source = Sources()
        source.set('root', '''<languages><language type="zh_Hant_TW"> Traditional </language></languages>
            <scripts><script type="Hans">Simplified</script></scripts>
            <territories><territory type="001">World</territory></territories>
            <variants><variant type="POSIX">Computer</variant></variants>
            <types><type key="calendar" type="gregorian">Gregorian Calendar</type>
            <type key="collation" type="phonebook">Other owner</type></types>
            <localeDisplayPattern><localePattern>{0} ({1})</localePattern>
            <localeSeparator>{0}, {1}</localeSeparator>
            <localeKeyTypePattern>{0}: {1}</localeKeyTypePattern></localeDisplayPattern>''',
            '<nestedBracketReplacement bracket="(">[</nestedBracketReplacement>')
        data = names.collect(source, graph())
        self.assertEqual(data['groups'], names.GROUPS)
        self.assertEqual(len(data['rows']), 4 * len(names.GROUPS))
        self.assertEqual(data['source_records'], 9)
        self.assertEqual(data['source_aliases'], 0)
        self.assertEqual(layers(data, 'en-GB', 'language'),
                         [('root', 'language', (('zh_Hant_TW', '', '', '', ' Traditional '),))])
        self.assertEqual(layers(data, 'en', 'calendar')[0][2],
                         (('gregorian', '', '', '', 'Gregorian Calendar'),))
        self.assertEqual(layers(data, 'en', 'bracket')[0][2], (('(', '', '', '', '['),))
        self.assertEqual(data, names.collect(source, graph()))

    def test_alternate_menu_scope_are_independent_facts(self):
        source = Sources()
        source.set('en', '''<languages>
            <language type="ku">Kurdish</language>
            <language type="ku" alt="long">Kurdish languages</language>
            <language type="ku" menu="core">Kurdish</language>
            <language type="ku" menu="extension">Kurmanji</language></languages>
            <scripts><script type="Hans">Simplified</script>
            <script type="Hans" alt="stand-alone">Simplified Han</script></scripts>
            <types><type key="calendar" type="buddhist">Buddhist Calendar</type>
            <type key="calendar" type="buddhist" scope="core">Buddhist</type></types>''')
        data = names.collect(source, graph())
        self.assertEqual(len(layers(data, 'en', 'language')[0][2]), 4)
        self.assertIn(('ku', '', 'extension', '', 'Kurmanji'), layers(data, 'en', 'language')[0][2])
        self.assertIn(('ku', 'long', '', '', 'Kurdish languages'), layers(data, 'en', 'language')[0][2])
        self.assertIn(('Hans', 'stand-alone', '', '', 'Simplified Han'), layers(data, 'en', 'script')[0][2])
        self.assertIn(('buddhist', '', '', 'core', 'Buddhist'), layers(data, 'en', 'calendar')[0][2])

    def test_layers_preserve_lateral_selection_before_parent_lookup(self):
        source = Sources()
        source.set('root', '<languages><language type="de" alt="short">root short</language></languages>')
        source.set('en', '<languages><language type="de">child default</language></languages>')
        self.assertEqual(layers(names.collect(source, graph()), 'en-GB', 'language'), [
            ('en', 'language', (('de', '', '', '', 'child default'),)),
            ('root', 'language', (('de', 'short', '', '', 'root short'),))])

    def test_markers_missing_empty_and_equal_child_text_preserve_provenance(self):
        source = Sources()
        source.set('root', '<languages><language type="de">German</language></languages>')
        source.set('en', '<languages><language type="de">German</language><language type="fr">' +
                   BLOCK + '</language><language type="it"/></languages>')
        source.set('en_GB', '<languages><language type="de">' + INHERIT + '</language></languages>')
        data = names.collect(source, graph())
        self.assertEqual(layers(data, 'de', 'script'), [])
        self.assertEqual(layers(data, 'en-GB', 'language'), [
            ('en', 'language', (('de', '', '', '', 'German'), ('fr', '', '', '', BLOCK),
                                ('it', '', '', '', ''))),
            ('root', 'language', (('de', '', '', '', 'German'),))])

    def test_explicit_group_alias_preserves_child_override_and_source(self):
        source = Sources()
        source.set('de', '<languages><language type="fr">Französisch</language></languages>')
        source.set('en', '<languages><alias source="de" path="../languages"/></languages>')
        source.set('en_GB', '<languages><language type="it">Italian</language></languages>')
        data = names.collect(source, graph())
        self.assertEqual(data['source_aliases'], 1)
        self.assertEqual(layers(data, 'en-GB', 'language'), [
            ('en-GB', 'language', (('it', '', '', '', 'Italian'),)),
            ('de', 'language', (('fr', '', '', '', 'Französisch'),))])

    def test_metadata_is_not_a_distinguishing_attribute(self):
        source = Sources()
        source.set('en', '<languages><language type="de" draft="unconfirmed" references="a">Deutsch</language></languages>')
        self.assertEqual(layers(names.collect(source, graph()), 'en', 'language')[0][2],
                         (('de', '', '', '', 'Deutsch'),))
        source.set('en', '''<languages><language type="de">A</language>
            <language type="de" draft="contributed">B</language></languages>''')
        with self.assertRaises(DataError):
            names.collect(source, graph())

    def test_reject_ambiguous_or_unreviewed_structures(self):
        cases = (
            '<languages/><languages/>',
            '<languages>text<language type="de">German</language></languages>',
            '<languages><language type="de"><unknown/></language></languages>',
            '<languages><language type="bad code">bad</language></languages>',
            '<languages><unknown/></languages>',
            '<languages validSubLocales="en"/>',
            '<localeDisplayPattern alt="variant"/>',
            '<unknown/>',
            '<languages><alias source="de" path="../scripts"/></languages>',
            '<languages><alias source="de" path="../languages"/><language type="de">D</language></languages>',
            '<languages><alias source="zz" path="../languages"/></languages>',
            '<localeDisplayPattern><localePattern/><localePattern/></localeDisplayPattern>',
        )
        for content in cases:
            source = Sources()
            source.set('en', content)
            with self.subTest(content=content), self.assertRaises(DataError):
                names.collect(source, graph())
        source = Sources()
        source.set('en', '', '<alias source="de" path="../characters"/>')
        with self.assertRaises(DataError):
            names.collect(source, graph())

    def test_alias_cycles_are_rejected(self):
        source = Sources()
        source.set('en', '<languages><alias source="de" path="../languages"/></languages>')
        source.set('de', '<languages><alias source="en" path="../languages"/></languages>')
        with self.assertRaisesRegex(DataError, 'cycle'):
            names.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
