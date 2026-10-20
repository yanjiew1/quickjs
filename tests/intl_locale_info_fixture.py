#!/usr/bin/env python3
"""Emit tiny authored locale preference and timezone territory metadata."""
from pathlib import Path
import argparse
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import embed
import locale_tables
import preference_tables
import tables


def render():
    # The world entry is intentionally distinct: an absent exact preference
    # must remain absent instead of silently reviving the world list.
    metadata = dict(aliases=(), likely=(('en', 'en-Latn-US'), ('und', 'en-Latn-US')),
                    keys=(('u', 'ca', 'single', (), '', False),
                          ('u', 'tz', 'single', (), '', False)),
                    types=(('u', 'ca', 'foo', ('oldfoo',), '', False, False, '', ''),
                           ('u', 'tz', 'oldny', (), 'usnyc', True, False, '', ''),
                           ('u', 'tz', 'usnyc', ('America/New_York', 'US/Eastern'), '', False, False, '', ''),
                           ('u', 'tz', 'ustest', ('America/Detroit',), '', False, False, '', 'CA'),
                           ('u', 'tz', 'utc', ('Etc/UTC',), '', False, False, '', '')))
    preferences = dict(weeks=(('001', 1, 1, (6, 7)),),
                       calendars=(('001', ('world',)), ('TH', ('buddhist', 'gregorian')),
                                  ('US', ('gregorian',)),
                                  ('en-TH', ('oldfoo', 'foo', 'unavailable'))),
                       hours=(('001', ('h23',)),), scripts=(('Latn', 'ltr'),))
    pool = tuple(sorted(locale_tables.strings(metadata) |
                        preference_tables.strings(preferences) | {'root'},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: (1, tables.LOCALE.pack(ids['root'], tables.ROOT))}
    sections.update(locale_tables.encode(metadata, ids))
    sections.update(preference_tables.encode(preferences, ids))
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    return embed.render(binary)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, render().encode('ascii'))
