#!/usr/bin/env python3
"""Localized zone source identity, exact templates and sparse name inheritance."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from resolve import BLOCK, INHERIT
import timezone_names as names
from test_intl_date_patterns import Sources as DateSources
from test_intl_date_intervals import alias, leaf
from test_intl_list_patterns import graph


class Sources(DateSources):
    def __init__(self):
        super().__init__()
        for tag in sorted(names.FORMATS | names.NAMES | {'timeZoneNames', 'zone', 'metazone', 'long', 'short', 'exemplarCity'}):
            self.files['common/dtd/ldml.dtd'] += ('<!ELEMENT ' + tag + ' ANY>'
                '<!ATTLIST ' + tag + ' alt NMTOKENS #IMPLIED>'
                '<!ATTLIST ' + tag + ' draft NMTOKEN #IMPLIED><!--@METADATA-->'
                '<!ATTLIST ' + tag + ' references CDATA #IMPLIED><!--@METADATA-->'
                '<!ATTLIST ' + tag + ' validSubLocales CDATA #IMPLIED><!--@VALUE-->').encode()
        for tag in ('zone', 'metazone'):
            self.files['common/dtd/ldml.dtd'] += ('<!ATTLIST ' + tag + ' type CDATA #REQUIRED>').encode()
        self.files['common/dtd/ldml.dtd'] += b'<!ATTLIST regionFormat type (standard | daylight) #IMPLIED>'

    def set(self, locale, body):
        self.files['common/main/' + locale + '.xml'] = (
            '<ldml><dates><timeZoneNames>' + body + '</timeZoneNames></dates></ldml>').encode('utf-8')


def entity(body, identity='Europe/London', kind='zone', alt=''):
    return '<' + kind + ' type="' + identity + '" alt="' + alt + '">' + body + '</' + kind + '>'


def width(body, code='long', alt=''):
    return '<' + code + ' alt="' + alt + '">' + body + '</' + code + '>'


def query(data, group, code, subtype='', alt='', locale='en-GB'):
    head = next((head for tag, key, head in data['rows'] if (tag, key) == (locale, group)), 0)
    while head:
        origin, source_group, fields, head = data['nodes'][head - 1]
        for field in data['maps'][fields]:
            if field[:3] == (code, subtype, alt):
                return field[3], origin, source_group
    return None


def key(identity='Europe/London', family='zone-name', entity_alt='', width='long', width_alt=''):
    return family, identity, entity_alt, width, width_alt


class TimezoneNamesTests(unittest.TestCase):
    def test_distinct_families_formats_and_three_alternate_levels(self):
        source = Sources()
        source.set('root', leaf('hourFormat', '+HH:mm;−HH:mm') + leaf('gmtFormat', 'GMT{0}') +
            leaf('gmtFormat', 'UTC{0}', alt='utc') + leaf('gmtUnknownFormat', 'GMT+?') +
            leaf('dualOffsetFormat', '{0}/{1}') + leaf('fallbackFormat', '{1} ({0})') +
            leaf('regionFormat', '{0} Time') + leaf('regionFormat', '{0} Summer', type='daylight') +
            entity(leaf('exemplarCity', 'London') + width(leaf('standard', 'London Standard')) +
                   width(leaf('generic', 'London'), code='short')) +
            entity(width(leaf('daylight', 'London Summer', alt='variant'), alt='ascii'), alt='variant') +
            entity(width(leaf('generic', 'GMT')), identity='GMT', kind='metazone'))
        data = names.collect(source, graph())
        self.assertEqual(query(data, names.FORMAT_GROUP, 'gmtFormat', alt='utc')[0], 'UTC{0}')
        self.assertEqual(query(data, names.FORMAT_GROUP, 'regionFormat', subtype='daylight')[0], '{0} Summer')
        self.assertEqual(query(data, names.FORMAT_GROUP, 'fallbackFormat')[0], '{1} ({0})')
        self.assertIsNone(query(data, names.FORMAT_GROUP, 'gmtZeroFormat'))
        self.assertEqual(query(data, ('city', 'Europe/London', '', '', ''), 'exemplarCity')[0], 'London')
        self.assertEqual(query(data, key(), 'standard')[0], 'London Standard')
        self.assertEqual(query(data, key(width='short'), 'generic')[0], 'London')
        self.assertEqual(query(data, key(entity_alt='variant', width_alt='ascii'), 'daylight', alt='variant')[0],
                         'London Summer')
        self.assertEqual(query(data, key('GMT', 'metazone-name'), 'generic')[0], 'GMT')
        self.assertEqual(data, names.collect(source, graph()))

    def test_empty_block_inherit_and_missing_remain_distinct(self):
        source = Sources()
        source.set('root', leaf('gmtFormat', 'GMT{0}') +
            entity(leaf('exemplarCity', 'London') + width(leaf('standard', 'Standard') + leaf('generic', 'Generic'))))
        source.set('en', leaf('gmtFormat', '') + entity(leaf('exemplarCity', INHERIT) +
            leaf('exemplarCity', BLOCK, alt='secondary') +
            width(leaf('standard', INHERIT) + leaf('generic', BLOCK) + leaf('daylight', ''))))
        data = names.collect(source, graph())
        self.assertEqual(query(data, names.FORMAT_GROUP, 'gmtFormat')[:2], ('', 'en'))
        self.assertEqual(query(data, key(), 'standard')[:2], ('Standard', 'root'))
        self.assertEqual(query(data, key(), 'generic')[:2], (BLOCK, 'en'))
        self.assertEqual(query(data, key(), 'daylight')[:2], ('', 'en'))
        self.assertEqual(query(data, ('city', 'Europe/London', '', '', ''), 'exemplarCity')[0], 'London')
        self.assertEqual(query(data, ('city', 'Europe/London', '', '', ''), 'exemplarCity', alt='secondary')[0], BLOCK)
        self.assertIsNone(query(data, key(), 'standard', alt='variant'))

    def test_width_aliases_restart_at_requester_and_keep_source_identity(self):
        source = Sources()
        source.set('root', entity(width(leaf('standard', 'GMT')), 'GMT', 'metazone') +
            entity(width(alias("../../metazone[@type='GMT']/long")), 'British', 'metazone'))
        source.set('en', entity(width(leaf('standard', 'Greenwich')), 'GMT', 'metazone'))
        source.set('en_GB', entity(width(leaf('daylight', 'British Summer')), 'British', 'metazone'))
        source.set('de', entity(width(alias("../../metazone[@type='British']/long", 'en')), 'Irish', 'metazone') +
            entity(width(leaf('standard', 'German GMT')), 'GMT', 'metazone'))
        data = names.collect(source, graph())
        self.assertEqual(query(data, key('British', 'metazone-name'), 'standard'),
                         ('Greenwich', 'en', key('GMT', 'metazone-name')))
        self.assertEqual(query(data, key('British', 'metazone-name'), 'daylight')[:2], ('British Summer', 'en-GB'))
        self.assertEqual(query(data, key('Irish', 'metazone-name'), 'standard', locale='de'),
                         ('Greenwich', 'en', key('GMT', 'metazone-name')))

    def test_alias_ids_with_slashes_and_alternate_selectors_are_atomic(self):
        source = Sources()
        source.set('root', entity(width(leaf('generic', 'London'), code='short', alt='ascii'), alt='variant') +
            entity(width(alias("../../zone[@alt='variant'][@type='Europe/London']/short[@alt='ascii']")),
                   'Etc/Unknown'))
        data = names.collect(source, graph())
        self.assertEqual(query(data, key('Etc/Unknown'), 'generic'),
                         ('London', 'root', key(entity_alt='variant', width='short', width_alt='ascii')))

    def test_metadata_and_unrelated_dates_are_not_presentation_selectors(self):
        source = Sources()
        source.set('root', entity(width(leaf('standard', 'Standard', draft='approved', references='source'))))
        self.assertEqual(names.collect(source, graph())['source_records'], 1)
        source.files['common/main/en.xml'] = b'<ldml><dates><calendars><unknown/></calendars></dates></ldml>'
        self.assertEqual(names.collect(source, graph())['source_records'], 1)

    def test_empty_alias_path_does_not_imply_same_group_in_another_locale(self):
        source = Sources()
        source.set('en', entity(width(leaf('generic', 'English'))))
        source.set('de', entity(width(alias('', 'en'))))
        with self.assertRaisesRegex(DataError, 'empty timezone-name alias path'):
            names.collect(source, graph())

    def test_reject_unknown_duplicate_or_mixed_structures(self):
        bodies = (leaf('gmtFormat', '{0}') * 2, leaf('gmtFormat', '{1}'),
            leaf('fallbackFormat', '{0}'), leaf('gmtFormat', '{0}', validSubLocales='en'),
            leaf('hourFormat', '+HH:mm'), entity('') * 2,
            entity(width(leaf('generic', 'G') * 2)), entity(width('') * 2),
            entity(width(leaf('exemplarCity', 'London'))), entity(leaf('generic', 'G')),
            entity(width(alias("../../metazone[@type='Missing']/long"))),
            entity(width(alias('../long'))), entity(width(alias('../long') + leaf('generic', 'G'))),
            entity(width(alias("../long[@alt='va\"riant']"))), entity(width(alias('../long/'))),
            entity(width(alias("../../zone[@type='Europe/London'][@type='GMT']/long"))),
            entity(width(alias('../../../../zone[@type="Europe/London"]/long'))),
            alias('../timeZoneNames'), '<unknown/>', 'unexpected text',
            entity(width(''), identity='Invalid ID'),
            entity(width(leaf('standard', 'S')), kind='metazone', identity='Invalid/ID'))
        for body in bodies:
            source = Sources()
            source.set('root', body)
            with self.subTest(body=body), self.assertRaises(DataError):
                names.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
