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

"""Locale inheritance rules using small authored CLDR-shaped fixtures."""
from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'tools' / 'intl'
sys.path.insert(0, str(TOOLS))
import parents
from cldr_xml import DataError

DTD = b'''<!ELEMENT supplementalData ANY>
<!ELEMENT parentLocales ANY>
<!ATTLIST parentLocales component NMTOKENS #IMPLIED>
<!ELEMENT parentLocale EMPTY>
<!ATTLIST parentLocale parent NMTOKEN #REQUIRED>
<!ATTLIST parentLocale locales NMTOKENS #REQUIRED>
<!ATTLIST parentLocale localeRules NMTOKENS #IMPLIED>
<!ELEMENT likelySubtags ANY>
<!ELEMENT likelySubtag EMPTY>
<!ATTLIST likelySubtag from NMTOKEN #REQUIRED>
<!ATTLIST likelySubtag to NMTOKEN #REQUIRED>
<!ELEMENT defaultContent EMPTY>
<!ATTLIST defaultContent locales NMTOKENS #REQUIRED>
'''


class Sources:
    def __init__(self):
        self.files = {
            'common/dtd/ldmlSupplemental.dtd': DTD,
            'common/supplemental/supplementalData.xml': b'''<supplementalData>
              <parentLocales>
                <parentLocale parent="root" localeRules="nonlikelyScript" locales="az_Arab"/>
                <parentLocale parent="en_001" locales="en_GB"/>
                <parentLocale parent="en_IN" locales="hi_Latn"/>
              </parentLocales>
              <parentLocales component="plurals"/>
              <parentLocales component="collations">
                <parentLocale parent="zh_Hant" locales="yue"/>
              </parentLocales>
            </supplementalData>''',
            'common/supplemental/likelySubtags.xml': b'''<supplementalData><likelySubtags>
              <likelySubtag from="und" to="en_Latn_US"/>
              <likelySubtag from="ru" to="ru_Cyrl_RU"/>
              <likelySubtag from="hi" to="hi_Deva_IN"/>
              <likelySubtag from="zh" to="zh_Hans_CN"/>
            </likelySubtags></supplementalData>''',
            'common/supplemental/supplementalMetadata.xml': b'''<supplementalData><metadata>
              <defaultContent locales="en_Latn_US ru_Cyrl_RU"/>
            </metadata></supplementalData>''',
        }
        for tag in ('root', 'en', 'en_GB', 'ru', 'ru_Latn', 'ru_Cyrl', 'hi_Latn', 'yue'):
            self.files['common/main/' + tag + '.xml'] = b''

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


class ParentTests(unittest.TestCase):
    def test_rule_applies_beyond_migration_examples(self):
        graph = parents.load(Sources())
        self.assertEqual(graph.parent('ru-Latn'), 'root')
        self.assertEqual(graph.parent('ru-Cyrl'), 'ru')
        self.assertEqual(graph.parent('qaa-Cyrl'), 'root')
        self.assertEqual(graph.parent('qaa-Latn'), 'qaa')
        self.assertEqual(tuple(graph.chain('ru-Latn-RU')), ('ru-Latn-RU', 'ru-Latn', 'root'))
        # Explicit exceptions override the general nonlikely-script rule.
        self.assertEqual(graph.parent('hi-Latn'), 'en-IN')

    def test_component_maps_are_independent(self):
        graph = parents.load(Sources())
        self.assertEqual(graph.parent('en-GB'), 'en-001')
        self.assertEqual(graph.parent('en-GB', 'plurals'), 'en')
        self.assertEqual(graph.parent('ru-Latn', 'plurals'), 'ru')
        self.assertEqual(graph.parent('yue', 'collations'), 'zh-Hant')
        self.assertEqual(tuple(graph.chain('yue', 'collations')), ('yue', 'zh-Hant', 'zh', 'root'))
        with self.assertRaises(DataError):
            graph.parent('en-GB', 'unknown')

    def test_default_content_and_structural_closure(self):
        graph = parents.load(Sources())
        self.assertEqual(graph.default_content, ('en-Latn-US', 'ru-Cyrl-RU'))
        self.assertIn('en-Latn', graph.tags)
        self.assertNotIn('en-Latn', graph.source_locales)
        self.assertEqual(tuple(graph.chain('en-Latn-US')), ('en-Latn-US', 'en-Latn', 'en', 'root'))

    def test_invalid_graphs_and_rules(self):
        invalid = (
            b'<parentLocale parent="en_GB" locales="en_GB"/>',
            b'<parentLocale parent="en" locales="root"/>',
            b'<parentLocale parent="en_GB" locales="en_001"/>',
            b'<parentLocale parent="root" localeRules="unknown" locales="xx_Latn"/>',
            b'<parentLocale parent="root" localeRules="nonlikelyScript" locales="xx_001"/>',
        )
        for entry in invalid:
            sources = Sources()
            name = 'common/supplemental/supplementalData.xml'
            sources.files[name] = sources.files[name].replace(b'</parentLocales>', entry + b'</parentLocales>', 1)
            with self.assertRaises(DataError):
                parents.load(sources)

    def test_invalid_metadata_is_not_silently_normalized(self):
        for name, old, new in (
            ('supplementalData', b'component="plurals"', b'component="plurals&#9;collations"'),
            ('supplementalData', b'locales="en_GB"', b'locales="en_GB&#160;en_US"'),
            ('likelySubtags', b'to="en_Latn_US"', b'to="en_US"'),
            ('supplementalMetadata', b'en_Latn_US ru_Cyrl_RU', b'en_Latn_US en_Latn_US'),
        ):
            sources = Sources()
            path = 'common/supplemental/' + name + '.xml'
            sources.files[path] = sources.files[path].replace(old, new)
            with self.assertRaises(DataError):
                parents.load(sources)

    def test_source_identifiers(self):
        self.assertEqual(parents.locale_id('EN_latn_us_POSIX'), 'en-Latn-US-posix')
        for value in ('en_', 'Latn', 'x-private', 'en-u-ca-gregory', 'sl-rozaj-ROZAJ'):
            with self.assertRaises(DataError):
                parents.locale_id(value)


if __name__ == '__main__':
    unittest.main()
