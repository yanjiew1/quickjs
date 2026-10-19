#!/usr/bin/env python3
"""Emit an authored locale-metadata fixture with unused malformed test strings."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import acquire
import bundle
import locale_tables
import tables


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    metadata = {
        'aliases': (('territory', 'su', ('ru', 'am')),),
        'likely': (('sl-1994-rozaj', 'sl-Latn-SI'), ('und', 'en-Latn-US')),
        'keys': (('u', 'ca', 'single', ('calendar', 'oldcalendar'), '', False),
                 ('u', 'tz', 'single', (), '', False),
                 ('u', 'zz', 'single', (), '', False)),
        'types': (('u', 'ca', 'foo', ('legacy', 'oldlegacy'), '', False, False, '', ''),
                  ('u', 'ca', 'zoo', (), 'foo', True, False, '', ''),
                  ('u', 'tz', 'ustest', (), '', False, False, 'America/Detroit', 'US'),
                  ('u', 'zz', 'CODEPOINTS', (), '', False, True, '', '')),
    }
    bad = ('EN', 'en-u-ca-gregory', 'en-x-foo', 'en-latn',
           'sl-rozaj-rozaj', 'ru ru', 'legacy legacy', 'en-', 'en--US',
           'latn', 'abcdefghijklmnop')
    strings = tuple(sorted(locale_tables.strings(metadata) | set(bad) | {'root'},
                           key=lambda value: value.encode('utf-8')))
    ids = {value: index for index, value in enumerate(strings)}
    sections = {bundle.STRINGS: tables.encode_strings(strings),
                bundle.LOCALES: (1, tables.LOCALE.pack(ids['root'], tables.ROOT))}
    sections.update(locale_tables.encode(metadata, ids))
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    lines = ['/* Authored fixture; no downloaded source data. */',
             '#include <stddef.h>', '#include <stdint.h>',
             'const uint8_t qjs_intl_locale_fixture[] = {']
    for at in range(0, len(binary), 16):
        lines.append('    ' + ', '.join('0x%02x' % value for value in binary[at:at + 16]) + ',')
    lines += ['};', 'const size_t qjs_intl_locale_fixture_size = sizeof(qjs_intl_locale_fixture);', '']
    acquire.atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
