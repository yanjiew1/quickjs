#!/usr/bin/env python3
"""Emit owned source-candidate, masking and late syntax-error fixtures."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_available_tables as wire
import embed
from parents import ParentGraph
from resolve import BLOCK
import tables


def pack(bad=None):
    tags = ('en', 'en-GB', 'root')
    graph = ParentGraph(tags, {}, {}, {'und': 'en-Latn-US'}, (), False)
    ordinary = ('gregorian', 'availableFormats', '')
    alias = ('generic', 'availableFormats', '')
    append = ('gregorian', 'appendItems', '')
    root = [('y', '', '', 'yyyy'), ('yy', '', '', 'yy'), ('yM', '', '', 'yyyy'),
            ('M', '', '', 'LLLL'), ('d', '', '', 'dd'), ('h', '', '', 'h a'),
            ('K', '', '', 'K a'), ('Bhm', '', '', 'h BBBB:mm'), ('B', '', '', 'BBBB'),
            ('rU', '', '', 'r(U)'), ('SSS', '', '', 's.SSS'), ('E', '', '', 'EEEEEE'),
            ('Q', '', '', 'Q'), ('j', '', '', "'literal'"), ('a', '', '', 'a'),
            ('y', 'other', '', "'"), ('y', '', 'ascii', "'"),
            ('y', '', 'variant', 'yyyy'), ('y', '', 'unsupported', 'Q'),
            ('a', '', 'unsupported', 'a'), ('j', '', 'unsupported', "'only'")]
    if bad is not None:
        root.append(('zzzz', '', '', bad))
    child = [('y', '', '', 'Q'), ('d', '', '', BLOCK), ('M', '', '', ''),
             ('K', '', '', "'literal'"), ('h', '', '', 'a')]
    data = dict(calendars=('generic', 'gregorian'), groups=tuple(sorted((ordinary, alias, append))),
                maps=(tuple(root), tuple(child), (('Year', '', '', '{0} {1}'),)),
                nodes=(('root', ordinary, 0, 0), ('en', ordinary, 1, 1),
                       ('root', append, 2, 0)),
                rows=(('root', ordinary, 1), ('en', ordinary, 2),
                      ('en-GB', alias, 1), ('en', append, 3)))
    pool = tuple(sorted(wire.strings(data) | set(tags), key=lambda value: value.encode('utf-8')))
    ids = {value: index for index, value in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: tables.encode_locales(tags, graph, ids),
                        bundle.DATE_AVAILABLE_FORMATS: wire.encode(data, tags, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack())
    for name, source in (('candidate_bad_quote', "'"), ('candidate_bad_field', 'P'),
                         ('candidate_calendar_year', 'Y/M/d'), ('candidate_week_year', 'Y-w-M-d')):
        text += '\n' + embed.render(pack(source)).replace('qjs_intl_native', name)
    atomic_write(args.output, text.encode('ascii'))
