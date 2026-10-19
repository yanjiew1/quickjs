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


def pack(empty=False, selection=False):
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
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
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
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack(args.empty, args.selection)).encode('ascii'))
