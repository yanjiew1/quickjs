#!/usr/bin/env python3
"""Interval source families, grammar boundaries and typed sparse inheritance."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import date_intervals as intervals
from resolve import BLOCK, INHERIT
from test_intl_date_patterns import Sources as StyleSources, calendar
from test_intl_list_patterns import graph


class Sources(StyleSources):
    def __init__(self):
        super().__init__()
        dtd = b'<!ELEMENT dateTimeFormats ANY><!ELEMENT intervalFormatRanges ANY>'
        for tag in ('intervalFormats', 'intervalFormatItem', 'intervalFormatFallback',
                    'greatestDifference', 'intervalFormatRange'):
            dtd += ('<!ELEMENT ' + tag + ' ANY><!ATTLIST ' + tag + ' alt NMTOKENS #IMPLIED>'
                    '<!ATTLIST ' + tag + ' draft NMTOKEN #IMPLIED><!--@METADATA-->'
                    '<!ATTLIST ' + tag + ' references CDATA #IMPLIED><!--@METADATA-->'
                    '<!ATTLIST ' + tag + ' validSubLocales CDATA #IMPLIED><!--@VALUE-->').encode()
        for tag, attribute in (('intervalFormatItem', 'id'), ('greatestDifference', 'id'),
                               ('intervalFormatRange', 'type')):
            dtd += ('<!ATTLIST ' + tag + ' ' + attribute + ' NMTOKEN #REQUIRED>').encode()
        self.files['common/dtd/ldml.dtd'] += dtd


def alias(path, source='locale'):
    return ET.tostring(ET.Element('alias', source=source, path=path), encoding='unicode')


def leaf(tag, value='', **attributes):
    node = ET.Element(tag, attributes)
    node.text = value
    return ET.tostring(node, encoding='unicode')


def item(body, skeleton='yMd', alt=''):
    return '<intervalFormatItem id="' + skeleton + '" alt="' + alt + '">' + body + '</intervalFormatItem>'


def formats(body, alt=''):
    return '<intervalFormats alt="' + alt + '">' + body + '</intervalFormats>'


def data_calendar(body, code='gregorian'):
    return calendar(code, '<dateTimeFormats>' + body + '</dateTimeFormats>')


def layers(data, locale, group):
    head = next((head for tag, key, head in data['rows'] if (tag, key) == (locale, group)), 0)
    while head:
        source, origin, fields, head = data['nodes'][head - 1]
        yield source, origin, data['maps'][fields]


def query(data, family, code='', alt='', locale='en-GB', calendar='gregorian',
          container_alt='', skeleton='yMd', item_alt=''):
    key = calendar, container_alt, family, skeleton if family == 'item' else '', item_alt if family == 'item' else ''
    for source, origin, fields in layers(data, locale, key):
        for field in fields:
            if field[:2] == (code, alt):
                return field[2], source, origin
    return None


class DateIntervalsTests(unittest.TestCase):
    def test_families_keep_alt_levels_and_distinct_grammars(self):
        source = Sources()
        source.set('root', data_calendar(formats(
            leaf('intervalFormatFallback', 'Start {1} to {0}') +
            leaf('intervalFormatFallback', '{0}–{1}', alt='variant') +
            item(leaf('greatestDifference', "latestFirst:LLL d – MMM d, y", id='M') +
                 leaf('greatestDifference', "earliestFirst:d 'to' d", id='d', alt='variant')) +
            '<intervalFormatRanges>' + leaf('intervalFormatRange', 'Range {0} – {1}', type='mixed') +
            leaf('intervalFormatRange', '{1}–{0}', type='numeric', alt='variant') + '</intervalFormatRanges>') +
            formats(item(leaf('greatestDifference', 'dd–dd', id='d', alt='variant'), alt='ascii'), alt='variant')))
        data = intervals.collect(source, graph())
        self.assertEqual(query(data, 'fallback')[0], 'Start {1} to {0}')
        self.assertEqual(query(data, 'fallback', alt='variant')[0], '{0}–{1}')
        self.assertEqual(query(data, 'item', 'M')[0], 'latestFirst:LLL d – MMM d, y')
        self.assertEqual(query(data, 'item', 'd', alt='variant')[0], "earliestFirst:d 'to' d")
        self.assertEqual(query(data, 'item', 'd', alt='variant', container_alt='variant', item_alt='ascii')[0], 'dd–dd')
        self.assertEqual(query(data, 'range', 'mixed')[0], 'Range {0} – {1}')
        self.assertEqual(query(data, 'range', 'numeric', alt='variant')[0], '{1}–{0}')
        self.assertIsNone(query(data, 'range', 'numeric'))
        self.assertEqual(data, intervals.collect(source, graph()))

    def test_sparse_layers_preserve_states_and_lateral_choices(self):
        source = Sources()
        source.set('root', data_calendar(formats(item(leaf('greatestDifference', 'd–d', id='d') +
            leaf('greatestDifference', 'M–M', id='M') + leaf('greatestDifference', 'y–y', id='y')) +
            leaf('intervalFormatFallback', '{0}–{1}'))))
        source.set('en', data_calendar(formats(item(leaf('greatestDifference', INHERIT, id='d') +
            leaf('greatestDifference', BLOCK, id='M') + leaf('greatestDifference', '', id='y') +
            leaf('greatestDifference', 'dd–dd', id='d', alt='variant')) + leaf('intervalFormatFallback', ''))))
        data = intervals.collect(source, graph())
        self.assertEqual(query(data, 'item', 'd')[:2], ('d–d', 'root'))
        self.assertEqual(query(data, 'item', 'M')[:2], (BLOCK, 'en'))
        self.assertEqual(query(data, 'item', 'y')[:2], ('', 'en'))
        self.assertEqual(query(data, 'fallback')[:2], ('', 'en'))
        self.assertIsNone(query(data, 'item', 'G'))
        chain = list(layers(data, 'en-GB', ('gregorian', '', 'item', 'yMd', '')))
        self.assertEqual([tag for tag, group, fields in chain], ['en', 'root'])
        self.assertIn(('d', 'variant', 'dd–dd'), chain[0][2])

    def test_ancestor_aliases_restart_at_requester_and_keep_overrides(self):
        source = Sources()
        source.set('root', data_calendar(formats(item(leaf('greatestDifference', 'd–d', id='d')) +
            leaf('intervalFormatFallback', '{0}–{1}'))) +
            calendar('generic', '<dateTimeFormats>' +
                alias("../../calendar[@type='gregorian']/dateTimeFormats") + '</dateTimeFormats>') +
            calendar('japanese', alias("../calendar[@type='generic']")))
        source.set('en', data_calendar(formats(item(leaf('greatestDifference', 'dd–dd', id='d')))))
        source.set('en_GB', data_calendar(formats(item(leaf('greatestDifference', 'MMM–MMM', id='M'))), code='japanese'))
        data = intervals.collect(source, graph())
        self.assertEqual(query(data, 'item', 'd', calendar='japanese'),
                         ('dd–dd', 'en', ('gregorian', '', 'item', 'yMd', '')))
        self.assertEqual(query(data, 'item', 'M', calendar='japanese')[1], 'en-GB')
        self.assertEqual(query(data, 'fallback', calendar='japanese')[0], '{0}–{1}')

    def test_interval_item_and_range_aliases_keep_exact_sources(self):
        source = Sources()
        source.set('root', data_calendar(formats(item(leaf('greatestDifference', 'd–d', id='d')) +
            item(alias("../intervalFormatItem[@id='yMd']"), skeleton='yMMMd') +
            '<intervalFormatRanges>' + leaf('intervalFormatRange', '{0}–{1}', type='mixed') +
            '</intervalFormatRanges>')) +
            data_calendar(formats(alias("../../../calendar[@type='gregorian']/dateTimeFormats/intervalFormats")),
                          code='generic') +
            data_calendar(formats('<intervalFormatRanges>' + alias("../../../../calendar[@type='gregorian']/"
                          'dateTimeFormats/intervalFormats/intervalFormatRanges') + '</intervalFormatRanges>'), code='chinese'))
        source.set('en', data_calendar(formats(item(leaf('greatestDifference', 'dd–dd', id='d')))))
        # An explicit source stays the anchor through the inherited locale alias.
        source.set('de', calendar('japanese', '<dateTimeFormats>' +
            alias("../../calendar[@type='generic']/dateTimeFormats", source='en') + '</dateTimeFormats>'))
        data = intervals.collect(source, graph())
        self.assertEqual(query(data, 'item', 'd', skeleton='yMMMd')[0], 'dd–dd')
        self.assertEqual(query(data, 'range', 'mixed', calendar='chinese')[0], '{0}–{1}')
        self.assertEqual(query(data, 'item', 'd', locale='de', calendar='japanese')[:2], ('dd–dd', 'en'))

    def test_item_alias_maps_container_and_item_alternates(self):
        source = Sources()
        source.set('root', data_calendar(formats(item(alias("../../intervalFormats[@alt='variant']/"
            "intervalFormatItem[@alt='ascii'][@id='yMd']"), skeleton='hm')) +
            formats(item(leaf('greatestDifference', 'dd–dd', id='d', alt='variant'), alt='ascii'), alt='variant')))
        data = intervals.collect(source, graph())
        self.assertEqual(query(data, 'item', 'd', alt='variant', skeleton='hm'),
                         ('dd–dd', 'root', ('gregorian', 'variant', 'item', 'yMd', 'ascii')))

    def test_unowned_subtrees_and_metadata_do_not_change_identity(self):
        source = Sources()
        source.set('root', data_calendar('<numericSeparators><unknown/></numericSeparators>' +
            '<availableFormats><unknown/></availableFormats>' + formats(item(
                leaf('greatestDifference', 'd–d', id='d', draft='approved', references='source')))))
        self.assertEqual(intervals.collect(source, graph())['source_records'], 1)

    def test_reject_duplicate_unknown_or_mixed_structures(self):
        bodies = (
            item('') * 2,
            item(leaf('greatestDifference', 'd–d', id='d') * 2),
            item(leaf('greatestDifference', 'd–d', id='s')),
            item(leaf('greatestDifference', 'd–d', id='d', validSubLocales='en')),
            item(leaf('greatestDifference', 'd–d', id='d'), skeleton='y-M-d'),
            item('<greatestDifference id="d"><child/></greatestDifference>'),
            item(leaf('greatestDifference', 'latestFirst:j', id='d')),
            item(alias("../intervalFormatItem[@id='yMd']") + leaf('greatestDifference', 'd–d', id='d')),
            item(alias("../intervalFormatRanges")),
            item(alias("../intervalFormatItem[@type='yMd']")),
            item(alias("../intervalFormatItem[@id='yMd'][@id='yMd']")),
            item(alias("../intervalFormatItem[@id='missing']")),
            item(alias("../../intervalFormats[@alt='va\"riant']/intervalFormatItem[@id='yMd']")),
            '<intervalFormatRanges/>' * 2,
            '<intervalFormatRanges>' + leaf('intervalFormatRange', '{0}–{1}', type='wrong') + '</intervalFormatRanges>',
            leaf('intervalFormatFallback', '{0}–{1}') * 2,
            leaf('intervalFormatFallback', '{0}'),
            leaf('intervalFormatFallback', '{0}{1}{0}'),
            leaf('intervalFormatFallback', '{0}{1}{2}'),
            leaf('intervalFormatFallback', '{0}{1}}'),
            leaf('intervalFormatFallback', '{0}{1}', validSubLocales='en'),
            '<unknown/>',
            'unexpected text',
        )
        for body in bodies:
            source = Sources()
            source.set('root', data_calendar(formats(body)))
            with self.subTest(body=body), self.assertRaises(DataError):
                intervals.collect(source, graph())
        source = Sources()
        source.set('root', data_calendar(formats('') * 2))
        with self.assertRaises(DataError):
            intervals.collect(source, graph())

    def test_alias_cycles_fail_with_or_without_explicit_sources(self):
        source = Sources()
        source.set('root', data_calendar(formats(alias('../intervalFormats'))))
        with self.assertRaises(DataError):
            intervals.collect(source, graph())
        source.set('root', data_calendar(formats(item(leaf('greatestDifference', 'd–d', id='d')))))
        source.set('en', data_calendar(formats(item(alias("../intervalFormatItem[@id='yMd']", source='de')))))
        source.set('de', data_calendar(formats(item(alias("../intervalFormatItem[@id='yMd']", source='en')))))
        with self.assertRaises(DataError):
            intervals.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
