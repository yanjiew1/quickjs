#!/usr/bin/env python3
"""Emit authored sparse unit forms and an empty-entry boundary pack."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
import unit_patterns
import unit_tables
from test_intl_list_patterns import graph
from test_intl_unit_patterns import Sources


def pack(empty, projected=False, variant=False):
    source = Sources()
    source.set('en', '''<unitLength type="long"><unit type="length-meter">
      <unitPattern count="one" case="genitive">''' + BLOCK + '''</unitPattern>
      <unitPattern count="two"/><gender>common</gender></unit>
      <compoundUnit type="per"><compoundUnitPattern case="genitive">from {0}/{1}</compoundUnitPattern>
      </compoundUnit></unitLength><unitLength type="short"><unit type="length-meter">
      <unitPattern count="other">{0} mt</unitPattern></unit></unitLength>''')
    source.set('en_GB', '''<unitLength type="long"><unit type="length-meter">
      <unitPattern count="other">CHILD_OTHER {0}</unitPattern></unit></unitLength>''')
    source.set('de', '''<unitLength type="narrow"><alias source="en_GB"
      path="../unitLength[@type='long']"/></unitLength>''')
    inheritance = graph()
    if variant:
        source.set('en_1901_1996', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="other">VARIANT {0}</unitPattern></unit></unitLength>''')
        source.set('de', '''<unitLength type="narrow"><alias source="en_1901_1996"
          path="../unitLength[@type='long']"/></unitLength>''')
        inheritance = ParentGraph(inheritance.tags + ('en-1901-1996',), {}, {},
                                  {'und': 'en-Latn-US'}, (), False)
    tags = inheritance.tags
    data = unit_patterns.collect(source, inheritance)
    if projected:
        tags = ('de', 'root')
        data = unit_tables.select(data, tags)
    if empty:
        data = dict(data, maps=((),), rows=tuple(row[:-1] + (0,) for row in data['rows']),
                    overlay_maps=(), overlay_nodes=(),
                    overlay_rows=tuple(row[:-1] + (0,) for row in data['overlay_rows']))
    pool = tuple(sorted(unit_tables.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'en-1901-1901', 'En', 'en-us', 'bad_tag'},
                        key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                bundle.UNIT_PATTERNS: unit_tables.encode(data, tags, ids)}
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


def render():
    lines = ['/* Authored records; no downloaded source data. */',
             '#include <stddef.h>', '#include <stdint.h>']
    for name, empty, projected, variant in (('qjs_intl_native', False, False, False),
                                            ('qjs_intl_unit_empty', True, False, False),
                                            ('qjs_intl_unit_projected', False, True, False),
                                            ('qjs_intl_unit_variant', False, True, True)):
        binary = pack(empty, projected, variant)
        lines.append('const uint8_t %s_bytes[] = {' % name)
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % byte for byte in binary[at:at + 16]) + ',')
        lines += ['};', 'const size_t %s_length = sizeof(%s_bytes);' % (name, name), '']
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, render().encode('ascii'))
