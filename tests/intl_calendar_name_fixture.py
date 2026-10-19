#!/usr/bin/env python3
"""Calendar identity projection fixtures without hardcoded production aliases."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import display_names
import display_tables
import embed
import locale_tables
import tables
from test_intl_display_names import Sources
from test_intl_list_patterns import graph


def pack(mode):
    sources = Sources()
    types = [
        ('u', 'ca', 'ancient', (), 'oldname', True, False, '', ''),
        ('u', 'ca', 'modern', ('legacy_calendar', 'BADALIAS'), '', False, False, '', ''),
        ('u', 'ca', 'oldname', ('modern',), 'modern', True, False, '', ''),
        ('u', 'ca', 'unbacked', (), '', False, False, '', ''),
        ('u', 'ca', 'valid', ('ninechars',), '', False, False, '', ''),
    ]
    if mode != 'empty':
        extra = '<type key="calendar" type="modern">ambiguous</type>' if mode == 'ambiguous' else ''
        if mode == 'conflicting':
            extra = '<type key="calendar" type="other">conflicting</type>'
            types.append(('u', 'ca', 'other', ('BADALIAS',), '', False, False, '', ''))
        sources.set('root', '''<types><type key="calendar" type="legacy_calendar">Legacy calendar</type>
            <type key="calendar" type="ninechars">Nine</type>
            <type key="calendar" type="extra">Extra</type>''' + extra + '</types>')
        sources.set('en', '<types><type key="calendar" type="LEGACY-CALENDAR">Child calendar</type></types>')
    metadata = {'aliases': (), 'likely': (('und', 'en-Latn-US'),),
                'keys': (('u', 'ca', 'single', (), '', False),),
                'types': tuple(sorted(types))}
    names = display_names.collect(sources, graph())
    tags = graph().tags
    pool = tuple(sorted(display_tables.strings(names) | locale_tables.strings(metadata) | set(tags),
                        key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                bundle.DISPLAY_NAMES: display_tables.encode(names, tags, ids)}
    sections.update(locale_tables.encode(metadata, ids))
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('regular', 'ambiguous', 'conflicting', 'empty'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack(args.mode)).encode('ascii'))
