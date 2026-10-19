#!/usr/bin/env python3
"""Emit layered name fixtures, including empty profiles and external aliases."""
import argparse
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import display_names
import display_tables
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_display_names import Sources
from test_intl_list_patterns import graph


def pack(empty=False, selection=False, language=False, render=False):
    sources = Sources()
    if not empty:
        sources.set('root', '''<languages><language type="en">English</language>
            <language type="ca_VALENCIA">Valencian</language></languages>
            <scripts><script type="Latn">Latin</script></scripts>
            <territories><territory type="US">United States</territory></territories>
            <variants><variant type="VALENCIA">Valencian variant</variant></variants>
            <types><type key="calendar" type="gregorian">Gregorian Calendar</type></types>
            <localeDisplayPattern><localePattern>{0} ({1})</localePattern>
            <localeSeparator>{0}, {1}</localeSeparator></localeDisplayPattern>''',
            '<nestedBracketReplacement bracket="(">[</nestedBracketReplacement>')
        sources.set('en', '''<languages><language type="ZH_hant" alt="short">short</language>
            <language type="zh_Hant">default</language><language type="fr"/>
            <language type="de">''' + BLOCK + '''</language>
            <language type="en" menu="core">English core</language></languages>
            <types><type key="calendar" type="gregorian" scope="core">Gregorian</type></types>''')
        sources.set('en_GB', '<languages><language type="en">English</language></languages>')
        sources.set('en_1901_1996', '''<territories><territory type="US">external region</territory>
            </territories>''')
        sources.set('de', '''<languages><alias source="en" path="../languages"/></languages>
            <territories><alias source="en_1901_1996" path="../territories"/></territories>''')
    if selection:
        root = ET.fromstring(sources.files['common/main/root.xml'])
        names = root.find('./localeDisplayNames/languages')
        for code, alt, text in (('en', 'short', 'Root EN'), ('en', 'long', 'Wrong long alternative'),
                                ('de', '', 'German'),
                                ('de', 'short', 'Germ.'), ('it', '', 'Italian'),
                                ('it', 'short', 'IT'), ('es', 'short', 'ES'), ('xy', '', 'xy')):
            fields = {'type': code}
            if alt:
                fields['alt'] = alt
            ET.SubElement(names, 'language', fields).text = text
        scripts = root.find('./localeDisplayNames/scripts')
        ET.SubElement(scripts, 'script', type='Latn', alt='stand-alone').text = 'Standalone Latin'
        ET.SubElement(scripts, 'script', type='Latn', alt='short').text = 'Lat.'
        ET.SubElement(root.find('./localeDisplayNames/localeDisplayPattern'),
                      'localePattern', alt='long').text = 'Wrong pattern alternative'
        sources.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        root = ET.fromstring(sources.files['common/main/en.xml'])
        names = root.find('./localeDisplayNames/languages')
        ET.SubElement(names, 'language', type='it', alt='short').text = BLOCK
        ET.SubElement(names, 'language', type='es').text = ''
        scripts = ET.SubElement(root.find('./localeDisplayNames'), 'scripts')
        ET.SubElement(scripts, 'script', type='Latn', alt='stand-alone').text = 'Child standalone Latin'
        sources.files['common/main/en.xml'] = ET.tostring(root, encoding='utf-8')
        root = ET.fromstring(sources.files['common/main/en_GB.xml'])
        scripts = ET.SubElement(root.find('./localeDisplayNames'), 'scripts')
        ET.SubElement(scripts, 'script', type='Latn').text = 'British Latin'
        sources.files['common/main/en_GB.xml'] = ET.tostring(root, encoding='utf-8')
    if language or render:
        root = ET.fromstring(sources.files['common/main/root.xml'])
        names = root.find('./localeDisplayNames/languages')
        for code, text in (('nl', 'Dutch'), ('nl_BE', 'Flemish'),
                           ('az', 'Azerbaijani'), ('az_Cyrl', 'Cyrillic Azerbaijani'),
                           ('az_AZ', 'Azerbaijan Azerbaijani'), ('ca', 'Catalan'),
                           ('de', 'German'), ('de_1901', 'Traditional German'),
                           ('de_1996', 'Reformed German'), ('fr', 'French'),
                           ('fr_CA', 'Canadian French'), ('es', 'Spanish'), ('es_MX', ''),
                           ('it', 'Italian'), ('sr', 'Serbian'), ('sr_Latn', 'Latin Serbian'),
                           ('sr_Latn_RS', 'Serbia Latin Serbian'), ('ro', 'Romanian'),
                           ('ro_MD', 'ro-MD'), ('eng', 'Wrong prefix')):
            ET.SubElement(names, 'language', type=code).text = text
        ET.SubElement(names, 'language', type='it_CH', menu='core').text = 'Menu Italian'
        ET.SubElement(names, 'language', type='pt_BR', alt='short').text = 'Brazilian short'
        sources.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        root = ET.fromstring(sources.files['common/main/en.xml'])
        names = root.find('./localeDisplayNames/languages')
        ET.SubElement(names, 'language', type='nl').text = 'Child Dutch'
        ET.SubElement(names, 'language', type='fr_CA').text = BLOCK
        ET.SubElement(names, 'language', type='pt').text = 'Child Portuguese'
        sources.files['common/main/en.xml'] = ET.tostring(root, encoding='utf-8')
    if render:
        root = ET.fromstring(sources.files['common/main/root.xml'])
        root.find('./localeDisplayNames/languages/language[@type="en"]').text = 'English (base)'
        root.find('./localeDisplayNames/scripts/script').text = 'Latin (script)'
        root.find('./localeDisplayNames/territories/territory').text = 'United States (region)'
        ET.SubElement(root.find('./localeDisplayNames/territories'), 'territory', type='AQ').text = ''
        ET.SubElement(root.find('./localeDisplayNames/territories'), 'territory', type='CA').text = 'Region 😀'
        root.find('./localeDisplayNames/variants/variant').text = 'Valencian (variant)'
        chars = root.find('./characters')
        ET.SubElement(chars, 'nestedBracketReplacement', bracket=')').text = ']'
        ET.SubElement(chars, 'nestedBracketReplacement', bracket='（').text = '［'
        ET.SubElement(chars, 'nestedBracketReplacement', bracket='）').text = '］'
        ET.SubElement(root.find('./localeDisplayNames/scripts'), 'script', type='Cyrl').text = 'Cyrillic （script）'
        sources.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        for tag, outer, separator in (
                ('en', '{0} ({1})', 'P{0}S{1}Z'),
                ('en_GB', "O'{1}'M{0}E", 'P{1}S{0}Z'),
                ('de', '{0}（{1}）', '{0}, {1}')):
            root = ET.fromstring(sources.files['common/main/' + tag + '.xml'])
            if tag == 'en_GB':
                root.find('./localeDisplayNames/languages/language[@type="en"]').text = 'English (base)'
            patterns = ET.SubElement(root.find('./localeDisplayNames'), 'localeDisplayPattern')
            ET.SubElement(patterns, 'localePattern').text = outer
            ET.SubElement(patterns, 'localeSeparator').text = separator
            sources.files['common/main/' + tag + '.xml'] = ET.tostring(root, encoding='utf-8')
        sources.set('en_1901_1996', '''<localeDisplayPattern>
            <localePattern>{0}</localePattern></localeDisplayPattern>''')
        sources.set('en_1901', '<localeDisplayPattern><localePattern>' + BLOCK +
                    '</localePattern></localeDisplayPattern>')
        sources.set('en_1996', '''<localeDisplayPattern>
            <localeSeparator>{2}</localeSeparator></localeDisplayPattern>''',
            '<nestedBracketReplacement bracket="(">bad</nestedBracketReplacement>')
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',) + (('en-1901', 'en-1996') if render else ()), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
    if render:
        tags = tuple(sorted(tags + ('en-1901', 'en-1901-1996', 'en-1996')))
    data = display_tables.select(display_names.collect(sources, inheritance), tags)
    pool = tuple(sorted(display_tables.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'En', 'en-us', 'bad_tag', 'en-1901-1901', 'WRONG'},
                        key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                          bundle.DISPLAY_NAMES: display_tables.encode(data, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--empty', action='store_true')
    modes.add_argument('--selection', action='store_true')
    modes.add_argument('--language', action='store_true')
    modes.add_argument('--render', action='store_true')
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack(args.empty, args.selection, args.language, args.render)).encode('ascii'))
