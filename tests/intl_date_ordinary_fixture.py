#!/usr/bin/env python3
"""Required date subsets with real append/connector and flexible clock evidence."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_available_tables as available
import date_combination_tables as combinations
import date_combinations
import embed
from parents import ParentGraph
from resolve import BLOCK
import tables


def pack(blocked=False):
    tags = ('en', 'root')
    graph = ParentGraph(tags, {}, {}, {'und': 'en-Latn-US'}, (), False)
    ordinary, append = ('gregorian', 'availableFormats', ''), ('gregorian', 'appendItems', '')
    patterns = (('M', 'L'), ('Md', 'M/d'), ('GyM', 'y M G'), ('GyMd', 'y M d G'),
                ('Hm', 'H:mm'), ('Hms', 'H:mm:ss'), ('Bh', 'B h'), ('Bhm', 'B h:mm'),
                ('Bhms', 'B h:mm:ss'), ('Eh', 'E h a'), ('ms', 'mm:ss'))
    source = dict(calendars=('gregorian',), groups=tuple(sorted((ordinary, append))),
                  maps=(tuple((key, '', '', value) for key, value in patterns),
                        (('Year', '', '', BLOCK if blocked else '{1} [year] {0}'),
                         ('Day-Of-Week', '', '', '{1}, {0}'))),
                  nodes=(('root', ordinary, 0, 0), ('root', append, 1, 0)),
                  rows=tuple((tag, key, head) for tag in tags for key, head in ((ordinary, 1), (append, 2))))
    connector = dict(calendars=('gregorian',), alternates=('',), widths=date_combinations.WIDTHS,
                     forms=date_combinations.FORMS,
                     rows=tuple((tag, 'gregorian', width, form, '', 'value', "{1} 'at' {0}", '',
                                 'root', 'gregorian', width, form, '')
                                for tag in tags for width in date_combinations.WIDTHS
                                for form in date_combinations.FORMS))
    pool = tuple(sorted(available.strings(source) | combinations.strings(connector) | set(tags),
                        key=lambda text: text.encode('utf-8')))
    ids = {text: i for i, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, graph, ids),
                          bundle.DATE_AVAILABLE_FORMATS: available.encode(source, tags, ids),
                          bundle.DATE_COMBINATIONS: combinations.encode(connector, tags, ids)}, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()).replace('qjs_intl_native', 'qjs_intl_date_ordinary_fixture')
    text += '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'qjs_intl_date_ordinary_blocked')
    atomic_write(args.output, text.encode('ascii'))
