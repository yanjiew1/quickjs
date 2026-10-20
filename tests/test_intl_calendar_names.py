#!/usr/bin/env python3
"""Authored calendar-name selectors, sparse inheritance and typed aliases."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import calendar_names as names
from cldr_xml import DataError
from resolve import BLOCK, INHERIT
from test_intl_date_patterns import Sources as StyleSources, calendar
from test_intl_list_patterns import graph


class Sources(StyleSources):
    def __init__(self):
        super().__init__()
        dtd = b''
        for tag in ('months', 'days', 'quarters', 'dayPeriods', 'eras', 'cyclicNameSets',
                    'monthPatterns', 'eraNames', 'eraAbbr', 'eraNarrow'):
            dtd += ('<!ELEMENT ' + tag + ' ANY><!ATTLIST ' + tag + ' alt NMTOKENS #IMPLIED>').encode()
        for tag in ('monthContext', 'monthWidth', 'dayContext', 'dayWidth', 'quarterContext',
                    'quarterWidth', 'dayPeriodContext', 'dayPeriodWidth', 'cyclicNameSet',
                    'cyclicNameContext', 'cyclicNameWidth', 'monthPatternContext', 'monthPatternWidth',
                    'month', 'day', 'quarter', 'dayPeriod', 'era', 'cyclicName', 'monthPattern'):
            dtd += ('<!ELEMENT ' + tag + ' ANY><!ATTLIST ' + tag + ' type NMTOKEN #REQUIRED>'
                    '<!ATTLIST ' + tag + ' alt NMTOKENS #IMPLIED>'
                    '<!ATTLIST ' + tag + ' draft NMTOKEN #IMPLIED><!--@METADATA-->'
                    '<!ATTLIST ' + tag + ' references CDATA #IMPLIED><!--@METADATA-->').encode()
        self.files['common/dtd/ldml.dtd'] += dtd + b'<!ATTLIST month yeartype NMTOKEN #IMPLIED>'


def group(family, body, context='format', width='wide', cycle='years'):
    if family == 'eras':
        tag = {'wide': 'eraNames', 'abbreviated': 'eraAbbr', 'narrow': 'eraNarrow'}[width]
        return '<eras><' + tag + '>' + body + '</' + tag + '></eras>'
    if family == 'cyclicNameSets':
        stem = 'cyclicName'
        prefix = '<cyclicNameSets><cyclicNameSet type="' + cycle + '">'
        suffix = '</cyclicNameSet></cyclicNameSets>'
    else:
        stem = {'months': 'month', 'days': 'day', 'quarters': 'quarter',
                'dayPeriods': 'dayPeriod', 'monthPatterns': 'monthPattern'}[family]
        prefix, suffix = '<' + family + '>', '</' + family + '>'
    return (prefix + '<' + stem + 'Context type="' + context + '"><' + stem +
            'Width type="' + width + '">' + body + '</' + stem + 'Width></' +
            stem + 'Context>' + suffix)


def alias(path, source='locale'):
    return '<alias source="' + source + '" path="' + path + '"/>'


def cyclic_groups(parts):
    return '<cyclicNameSets>' + ''.join(part[len('<cyclicNameSets>'):-len('</cyclicNameSets>')]
                                      for part in parts) + '</cyclicNameSets>'


def layers(data, locale, key):
    head = next((head for tag, group, head in data['rows'] if (tag, group) == (locale, key)), 0)
    result = []
    while head:
        tag, group, index, head = data['nodes'][head - 1]
        result.append((tag, group, data['maps'][index]))
    return result


def query(data, locale, group, code, year='', alt=''):
    for tag, origin, fields in layers(data, locale, group):
        for field in fields:
            if field[:3] == (code, year, alt):
                return ('blocked' if field[3] == BLOCK else 'value', field[3], tag, origin)
    return None


class CalendarNameTests(unittest.TestCase):
    def test_families_preserve_exact_text_and_distinct_selectors(self):
        source = Sources()
        source.set('root', calendar('gregorian',
            group('months', '<month type="13">  M😀  </month>'
                  '<month type="7"/><month type="7" yeartype="standard">S</month>'
                  '<month type="7" yeartype="leap" alt="variant">L</month>') +
            group('days', '<day type="mon" alt="variant">Mon</day>', width='short') +
            group('quarters', '<quarter type="4">Q</quarter>', context='stand-alone') +
            group('dayPeriods', '<dayPeriod type="midnight">N</dayPeriod>') +
            group('eras', '<era type="232">Meiji</era><era type="236">Reiwa</era>'
                  '<era type="62">raw historical ID</era>') +
            group('monthPatterns', '<monthPattern type="leap">閏{0}</monthPattern>',
                  context='numeric', width='all')))
        data = names.collect(source, graph())
        g = ('gregorian', 'months', '', 'format', 'wide')
        self.assertEqual(query(data, 'en-GB', g, '13')[:3], ('value', '  M😀  ', 'root'))
        self.assertEqual(query(data, 'en-GB', g, '7')[1], '')
        self.assertEqual(query(data, 'en-GB', g, '7', year='standard')[1], 'S')
        self.assertEqual(query(data, 'en-GB', g, '7', year='leap', alt='variant')[1], 'L')
        for family, context, width, code, alt, expected in (
                ('days', 'format', 'short', 'mon', 'variant', 'Mon'),
                ('quarters', 'stand-alone', 'wide', '4', '', 'Q'),
                ('dayPeriods', 'format', 'wide', 'midnight', '', 'N'),
                ('eras', '', 'wide', '232', '', 'Meiji'),
                ('eras', '', 'wide', '236', '', 'Reiwa'),
                ('eras', '', 'wide', '62', '', 'raw historical ID'),
                ('monthPatterns', 'numeric', 'all', 'leap', '', '閏{0}')):
            self.assertEqual(query(data, 'en-GB', ('gregorian', family, '', context, width),
                                   code, alt=alt)[1], expected)
        self.assertIsNone(query(data, 'en-GB', g, '1'))
        self.assertEqual(data, names.collect(source, graph()))

    def test_all_cyclic_sets_and_cross_set_width_alias(self):
        source = Sources()
        source.set('root', calendar('chinese', cyclic_groups(
            group('cyclicNameSets', '<cyclicName type="60">' + cycle + '</cyclicName>', cycle=cycle)
            for cycle in ('years', 'months', 'days', 'dayParts', 'zodiacs', 'solarTerms'))))
        source.set('en', calendar('chinese', cyclic_groups((
            group('cyclicNameSets', '<cyclicName type="1">target</cyclicName>',
                  cycle='dayParts', width='abbreviated'),
            group('cyclicNameSets', alias("../../../cyclicNameSet[@type='dayParts']/"
                  "cyclicNameContext[@type='format']/cyclicNameWidth[@type='abbreviated']"),
                  cycle='solarTerms', width='narrow')))))
        data = names.collect(source, graph())
        for cycle in names.CYCLIC_SETS:
            self.assertEqual(query(data, 'en-GB', ('chinese', 'cyclicNameSets', cycle, 'format', 'wide'),
                                   '60')[1], cycle)
        result = query(data, 'en-GB', ('chinese', 'cyclicNameSets', 'solarTerms', 'format', 'narrow'), '1')
        self.assertEqual(result, ('value', 'target', 'en',
                                 ('chinese', 'cyclicNameSets', 'dayParts', 'format', 'abbreviated')))

    def test_parent_aliases_restart_at_requester_and_keep_terminal_provenance(self):
        source = Sources()
        source.set('root', calendar('gregorian', group('months', '<month type="1">root</month>')) +
            calendar('generic', '<months>' + alias("../../calendar[@type='gregorian']/months") + '</months>') +
            calendar('japanese', alias("../calendar[@type='generic']")))
        source.set('en', calendar('gregorian', group('months', '<month type="1">child</month>')))
        source.set('en_GB', calendar('japanese', group('months', '<month type="2">own</month>')))
        data = names.collect(source, graph())
        key = ('japanese', 'months', '', 'format', 'wide')
        self.assertEqual(query(data, 'en-GB', key, '1'),
                         ('value', 'child', 'en', ('gregorian', 'months', '', 'format', 'wide')))
        self.assertEqual(query(data, 'en-GB', key, '2'), ('value', 'own', 'en-GB', key))

    def test_explicit_alias_anchor_survives_later_locale_alias(self):
        source = Sources()
        source.set('root', calendar('generic', '<months>' +
            alias("../../calendar[@type='gregorian']/months") + '</months>') +
            calendar('gregorian', group('months', '<month type="1">root</month>')))
        source.set('en', calendar('japanese', alias("../calendar[@type='generic']", source='de')) +
            calendar('gregorian', group('months', '<month type="1">wrong</month>')))
        source.set('de', calendar('gregorian', group('months', '<month type="1">Deutsch</month>')))
        data = names.collect(source, graph())
        self.assertEqual(query(data, 'en-GB', ('japanese', 'months', '', 'format', 'wide'), '1')[:3],
                         ('value', 'Deutsch', 'de'))

    def test_context_width_cyclic_set_and_era_aliases(self):
        source = Sources()
        source.set('root', calendar('gregorian', '''<months>
            <monthContext type="format"><monthWidth type="wide"><month type="1">M</month></monthWidth>
            <monthWidth type="abbreviated">''' + alias("../monthWidth[@type='wide']") + '''</monthWidth>
            </monthContext><monthContext type="stand-alone">''' + alias("../monthContext[@type='format']") +
            '''</monthContext></months><eras><eraAbbr><era type="1">E</era></eraAbbr>
            <eraNames>''' + alias('../eraAbbr') + '''</eraNames></eras>
            <cyclicNameSets><cyclicNameSet type="years"><cyclicNameContext type="format">
            <cyclicNameWidth type="wide"><cyclicName type="1">Y</cyclicName></cyclicNameWidth>
            </cyclicNameContext></cyclicNameSet><cyclicNameSet type="months">''' +
            alias("../cyclicNameSet[@type='years']") + '</cyclicNameSet></cyclicNameSets>'))
        data = names.collect(source, graph())
        self.assertEqual(query(data, 'en-GB', ('gregorian', 'months', '', 'stand-alone', 'abbreviated'), '1')[1], 'M')
        self.assertEqual(query(data, 'en-GB', ('gregorian', 'eras', '', '', 'wide'), '1')[3],
                         ('gregorian', 'eras', '', '', 'abbreviated'))
        self.assertEqual(query(data, 'en-GB', ('gregorian', 'cyclicNameSets', 'months', 'format', 'wide'), '1')[1], 'Y')

    def test_layers_preserve_alternates_before_parent_lookup_and_markers(self):
        source = Sources()
        source.set('root', calendar('gregorian', group('months',
            '<month type="1">root plain</month><month type="2">root second</month>'
            '<month type="3">root third</month><month type="4">root fourth</month>')))
        source.set('en', calendar('gregorian', group('months',
            '<month type="1" alt="variant">child variant</month>'
            '<month type="2">' + INHERIT + '</month><month type="3">' + BLOCK + '</month>'
            '<month type="4"/>')))
        data = names.collect(source, graph())
        key = ('gregorian', 'months', '', 'format', 'wide')
        chain = layers(data, 'en-GB', key)
        self.assertEqual([row[0] for row in chain], ['en', 'root'])
        self.assertIn(('1', '', 'variant', 'child variant'), chain[0][2])
        self.assertNotIn(('1', '', '', 'root plain'), chain[0][2])
        self.assertEqual(query(data, 'en-GB', key, '2')[:3], ('value', 'root second', 'root'))
        self.assertEqual(query(data, 'en-GB', key, '3')[:3], ('blocked', BLOCK, 'en'))
        self.assertEqual(query(data, 'en-GB', key, '4')[:3], ('value', '', 'en'))
        self.assertEqual(layers(data, 'en-GB', ('gregorian', 'months', '', 'format', 'narrow')), [])
        self.assertTrue(all(tail < index for index, (_, _, _, tail) in enumerate(data['nodes'], 1)))
        self.assertEqual(tuple(sorted(data['maps'])), data['maps'])

    def test_other_calendar_owners_remain_uninterpreted(self):
        source = Sources()
        source.set('root', calendar('gregorian', '<dateTimeFormats><unknown/></dateTimeFormats>'
            '<dateFormats><unknown/></dateFormats><fields><unknown/></fields>'))
        data = names.collect(source, graph())
        self.assertEqual((data['source_records'], data['rows'], data['nodes']), (0, (), ()))

    def test_aliases_preserve_block_empty_and_metadata_independent_identity(self):
        source = Sources()
        source.set('root', calendar('gregorian', group('months',
            '<month type="1">' + BLOCK + '</month><month type="2"/>'
            '<month type="3" alt="variant" draft="unconfirmed" references="ref">V</month>')) +
            calendar('generic', '<months>' + alias("../../calendar[@type='gregorian']/months") + '</months>'))
        data = names.collect(source, graph())
        key = ('generic', 'months', '', 'format', 'wide')
        origin = ('gregorian', 'months', '', 'format', 'wide')
        self.assertEqual(query(data, 'en-GB', key, '1'), ('blocked', BLOCK, 'root', origin))
        self.assertEqual(query(data, 'en-GB', key, '2'), ('value', '', 'root', origin))
        self.assertEqual(query(data, 'en-GB', key, '3', alt='variant'), ('value', 'V', 'root', origin))

    def test_reject_duplicate_unknown_mixed_and_structured_sources(self):
        leaf = '<month type="1">M</month>'
        cases = (
            group('months', leaf + '<month type="1" draft="approved">duplicate</month>'),
            group('months', '<month type="0">bad</month>'),
            group('months', '<month type="1" yeartype="unknown">bad</month>'),
            group('days', '<day type="1">bad</day>'),
            group('eras', '<era type="01">bad</era>'),
            group('dayPeriods', '<dayPeriod type="unknown">bad</dayPeriod>'),
            group('cyclicNameSets', '<cyclicName type="61">bad</cyclicName>'),
            group('monthPatterns', '<monthPattern type="unknown">bad</monthPattern>'),
            group('months', '<month type="1"><cp hex="41"/></month>'),
            group('months', leaf) * 2,
            '<months><monthContext type="format"/><monthContext type="format"/></months>',
            '<months><monthContext type="numeric"/></months>',
            '<months alt="variant"/>',
            '<months>unexpected text</months>',
            '<months><unknown/></months>',
            '<months><monthContext type="format"><monthWidth type="wide" alt="variant"/></monthContext></months>',
            group('months', alias("../monthWidth[@type='wide']") + leaf),
            group('months', alias("../monthWidth[@type='unknown']")),
            group('months', alias("../../../../calendar[@type='gregorian']/months")),
            '<months>' + alias("../../calendar[@type='unknown']/months") + '</months>',
            '<months>' + alias("../days") + '</months>',
            '<months>' + alias(".//month") + '</months>',
        )
        for body in cases:
            source = Sources()
            source.set('root', calendar('gregorian', body))
            with self.subTest(body=body), self.assertRaises(DataError):
                names.collect(source, graph())

    def test_reject_alias_cycles_and_unknown_explicit_sources(self):
        for body in (
                '<months>' + alias('../months') + '</months>',
                '<months><monthContext type="format"><monthWidth type="wide">' +
                alias("../monthWidth[@type='narrow']") + '</monthWidth><monthWidth type="narrow">' +
                alias("../monthWidth[@type='wide']") + '</monthWidth></monthContext></months>',
                '<months>' + alias('../months', source='unknown') + '</months>'):
            source = Sources()
            source.set('root', calendar('gregorian', body))
            with self.subTest(body=body), self.assertRaises(DataError):
                names.collect(source, graph())

        source = Sources()
        source.set('root', calendar('gregorian', '<months>' + alias('../months', source='en') + '</months>'))
        source.set('en', calendar('gregorian', '<months>' + alias('../months', source='de') + '</months>'))
        source.set('de', calendar('gregorian', '<months>' + alias('../months', source='root') + '</months>'))
        with self.assertRaises(DataError):
            names.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
