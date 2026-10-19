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
import plural_tables
import unit_grammar_tables
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


def selector_pack():
    source = Sources()
    root, parent, child = {}, {}, {}
    for name in ('child', 'case', 'case-shadow', 'order', 'exact', 'exact-shadow', 'block', 'literal', 'empty',
                 'source-category', 'unselected', 'malformed', 'denominator', 'alternative',
                 'noun-middle'):
        root[name] = '<unitPattern count="other">ROOT {0}</unitPattern>'
    root['child'] += '<unitPattern count="one">ROOT_ONE {0}</unitPattern>'
    parent['child'] = '<unitPattern count="one">PARENT_ONE {0}</unitPattern>'
    child['child'] = '<unitPattern count="other">CHILD_OTHER {0}</unitPattern>'
    parent['case'] = '<unitPattern count="one" case="genitive">PARENT_GENITIVE {0}</unitPattern>'
    child['case'] = '<unitPattern count="one">CHILD_DEFAULT {0}</unitPattern>'
    parent['case-shadow'] = '''<unitPattern count="one" case="genitive">PARENT_GENITIVE {0}</unitPattern>
      <unitPattern count="one" case="nominative">PARENT_NOMINATIVE {0}</unitPattern>'''
    child['case-shadow'] = '<unitPattern count="one" case="genitive">' + BLOCK + '</unitPattern>'
    child['order'] = '''<unitPattern count="one" case="genitive">ONE_GENITIVE {0}</unitPattern>
      <unitPattern count="one" case="nominative">ONE_NOMINATIVE {0}</unitPattern>
      <unitPattern count="one">ONE_DEFAULT {0}</unitPattern>
      <unitPattern count="other" case="accusative">OTHER_ACCUSATIVE {0}</unitPattern>'''
    child['exact'] = '''<unitPattern count="1">EXACT_ONE {0}</unitPattern>
      <unitPattern count="0">EXACT_ZERO {0}</unitPattern>
      <unitPattern count="one">CATEGORY_ONE {0}</unitPattern>'''
    parent['exact-shadow'] = '<unitPattern count="1">PARENT_EXACT_ONE {0}</unitPattern>'
    child['exact-shadow'] = '<unitPattern count="1">one meter</unitPattern>'
    for name, text in (('block', BLOCK), ('literal', 'one meter'), ('empty', '')):
        parent[name] = '<unitPattern count="one">PARENT_ONE {0}</unitPattern>'
        child[name] = ('<unitPattern count="one">' + text + '</unitPattern>'
                       '<unitPattern count="other">other meters</unitPattern>')
    parent['unselected'] = '<unitPattern count="two">PARENT_TWO {0}</unitPattern>'
    child['unselected'] = ('<unitPattern count="one">one meter</unitPattern>'
                           '<unitPattern count="two">' + BLOCK + '</unitPattern>')
    parent['source-category'] = '<unitPattern count="two">PARENT_TWO {0}</unitPattern>'
    child['source-category'] = '<unitPattern count="one">one meter</unitPattern>'
    child['malformed'] = '<unitPattern count="one">bad {1}</unitPattern>'
    child['denominator'] = '''<unitPattern count="one">DENOMINATOR_ONE {0}</unitPattern>
      <unitPattern count="other">DENOMINATOR_OTHER {0}</unitPattern>'''
    child['alternative'] = ('<unitPattern count="one">' + BLOCK + '</unitPattern>'
                            '<unitPattern count="other">ALTERNATIVE {0}</unitPattern>')
    parent['noun-middle'] = '<unitPattern count="one">PARENT_ONE {0}</unitPattern>'
    child['noun-middle'] = '<unitPattern count="one">noun {0} tail</unitPattern>'
    for name, text in (('local', '{0} child per'), ('block', BLOCK), ('empty', ''),
                       ('literal', 'per second'), ('malformed', '{1}'), ('bare', '{0}'),
                       ('parent', None)):
        root['per-' + name] = '<perUnitPattern>{0} ROOT_PER</perUnitPattern>'
        if text is not None:
            child['per-' + name] = '<perUnitPattern>' + text + '</perUnitPattern>'
    parent['per-parent'] = '<perUnitPattern>PARENT_PER {0}</perUnitPattern>'
    for locale, fields in (('root', root), ('en', parent), ('en_GB', child)):
        content = ''.join('<unit type="length-' + name + '">' + text + '</unit>'
                          for name, text in sorted(fields.items()))
        if locale == 'root':
            content += '<compoundUnit type="per"><compoundUnitPattern>{0}/{1}</compoundUnitPattern></compoundUnit>'
        elif locale == 'en':
            content += '<compoundUnit type="per"><compoundUnitPattern>PARENT {0}/{1}</compoundUnitPattern></compoundUnit>'
        else:
            content += ('<compoundUnit type="per"><compoundUnitPattern>{1} child {0}</compoundUnitPattern>'
                        '<compoundUnitPattern case="genitive">' + BLOCK + '</compoundUnitPattern>'
                        '<compoundUnitPattern case="instrumental">{2}</compoundUnitPattern></compoundUnit>')
        extra = ''
        for width in ('short', 'narrow'):
            text = '' if locale == 'en_GB' and width == 'short' else (
                BLOCK if locale == 'en_GB' else '{0} ' + locale + '/' + width + ' {1}')
            extra += ('<unitLength type="' + width + '"><compoundUnit type="per">'
                      '<compoundUnitPattern>' + text + '</compoundUnitPattern>'
                      '</compoundUnit></unitLength>')
        source.set(locale, '<unitLength type="long">' + content + '</unitLength>' + extra)
    source.set('de', '''<unitLength type="long"><unit type="length-child">
      <alias source="en_GB" path="../unit[@type='length-child']"/></unit>
      <unit type="length-per-alias"><alias source="en_GB" path="../unit[@type='length-per-local']"/></unit>
      <compoundUnit type="per"><alias source="en_GB" path="../compoundUnit[@type='per']"/></compoundUnit>
      </unitLength>''')
    tags = ('de', 'en-GB', 'root')
    data = unit_tables.select(unit_patterns.collect(source, graph()), tags)
    pool = tuple(sorted(unit_tables.strings(data) | set(tags), key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    # Use a source graph whose selected rows are closed under locale parents;
    # source overlay dependencies intentionally retain unadvertised en origins.
    selected_graph = ParentGraph(tags, {'en-GB': 'root'}, {}, {'und': 'en-Latn-US'}, (), False)
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, selected_graph, ids),
                          bundle.UNIT_PATTERNS: unit_tables.encode(data, tags, ids)}, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


def composition_pack(projected=False):
    source = Sources()
    inheritance = graph()
    tags = inheritance.tags
    names = {'volume-liter': 'ROOT_L', 'volume-fluid-ounce': 'ROOT_F',
             'duration-hour': 'ROOT_H', 'length-meter': 'ROOT_M',
             'duration-second': 'ROOT_S', 'speed-meter-per-second': 'ROOT_FULL'}
    content = ''.join('<unit type="' + name + '"><unitPattern count="other">{0} ' + text +
                      '</unitPattern></unit>' for name, text in sorted(names.items()))
    content += '<compoundUnit type="per"><compoundUnitPattern>{0} ROOT_DIV {1}</compoundUnitPattern></compoundUnit>'
    source.set('root', '<unitLength type="long">' + content + '</unitLength>'
               '<unitLength type="short">' + content + '</unitLength>'
               '<unitLength type="narrow">' + content + '</unitLength>')
    source.set('en', '''<unitLength type="long">
      <unit type="volume-liter"><unitPattern count="1">{0} EXACT_L</unitPattern>
        <unitPattern count="one">{0} EN_ONE</unitPattern><unitPattern count="other">{0} EN_OTHER</unitPattern></unit>
      <unit type="volume-fluid-ounce"><unitPattern count="1">{0} EXACT_F</unitPattern>
        <unitPattern count="one" case="accusative">{0} ACC_ONE</unitPattern>
        <unitPattern count="two" case="accusative">{0} ACC_TWO</unitPattern>
        <unitPattern count="other">{0} EN_F_OTHER</unitPattern></unit>
      <unit type="duration-hour"><unitPattern count="one">BAD {1}</unitPattern>
        <perUnitPattern>EN_PER_H {0}</perUnitPattern></unit>
      <unit type="length-meter"><unitPattern count="other">BAD {1}</unitPattern></unit>
      <unit type="duration-second"><unitPattern count="other">BAD {1}</unitPattern></unit>
      <unit type="speed-meter-per-second"><unitPattern count="other">{0} EN_FULL</unitPattern></unit>
      <compoundUnit type="per"><compoundUnitPattern>{1} CHILD_DIV {0}</compoundUnitPattern></compoundUnit>
      </unitLength>''')
    source.set('en_GB', '''<unitLength type="long"><unit type="volume-fluid-ounce">
      <unitPattern count="one">{0} LOCAL_ONE</unitPattern></unit></unitLength>
      <unitLength type="short"><compoundUnit type="per"><compoundUnitPattern>{2}</compoundUnitPattern></compoundUnit></unitLength>
      <unitLength type="narrow"><unit type="volume-fluid-ounce">
        <unitPattern count="other">BEGIN {0} END</unitPattern></unit></unitLength>''')
    source.set('de', '''<unitLength type="long"><unit type="volume-liter">
      <unitPattern count="one">{0} DE_ONE</unitPattern><unitPattern count="other">{0} DE_OTHER</unitPattern></unit>
      <unit type="volume-fluid-ounce"><unitPattern count="one">{0} DE_NOM</unitPattern>
        <unitPattern count="one" case="accusative">{0} DE_ACC</unitPattern></unit>
      <compoundUnit type="per"><compoundUnitPattern>{0} DE_DIV {1}</compoundUnitPattern></compoundUnit></unitLength>''')
    units = unit_patterns.collect(source, inheritance)
    grammar_rows, plural_rows = [], []
    for locale in tags:
        origin = 'en' if locale in ('en', 'en-GB') else locale
        grammar_rows.append((locale, 'plural', origin) +
                            (('one', 'compound') if origin == 'en' else ('compound', 'one')))
        grammar_rows.append((locale, 'case', origin, 'compound',
                             'nominative' if locale == 'root' else 'accusative'))
        rules = ((1, 'i = 1 and v = 0'), (2, 'i = 2 and v = 0'), (5, '')) if locale in ('en', 'de') else ((5, ''),)
        plural_rows.extend(((locale, 0, rules, ()), (locale, 1, ((5, ''),), ())))
    grammar = {'features': ('plural', 'case'), 'rows': tuple(grammar_rows)}
    plurals = {'rows': tuple(plural_rows)}
    if projected:
        tags = ('en-GB', 'root')
        units = unit_tables.select(units, tags)
        grammar = unit_grammar_tables.select(grammar, tags)
        plurals = {'rows': tuple(row for row in plural_rows if row[0] in tags)}
        inheritance = ParentGraph(tags, {'en-GB': 'root'}, {}, {'und': 'en-Latn-US'}, (), False)
    pool = tuple(sorted(unit_tables.strings(units) | unit_grammar_tables.strings(grammar) |
                        plural_tables.strings(plurals) | set(tags), key=lambda text: text.encode('utf-8')))
    ids = {text: i for i, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                          bundle.UNIT_PATTERNS: unit_tables.encode(units, tags, ids),
                          bundle.UNIT_GRAMMAR: unit_grammar_tables.encode(grammar, tags, ids),
                          bundle.PLURALS: plural_tables.encode(plurals, tags, ids)}, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


def render():
    lines = ['/* Authored records; no downloaded source data. */',
             '#include <stddef.h>', '#include <stdint.h>']
    profiles = (('qjs_intl_native', False, False, False),
                ('qjs_intl_unit_empty', True, False, False),
                ('qjs_intl_unit_projected', False, True, False),
                ('qjs_intl_unit_variant', False, True, True))
    records = [(name, pack(empty, projected, variant))
               for name, empty, projected, variant in profiles]
    records.append(('qjs_intl_unit_selector', selector_pack()))
    records.append(('qjs_intl_unit_composition', composition_pack()))
    records.append(('qjs_intl_unit_composition_projected', composition_pack(True)))
    for name, binary in records:
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
