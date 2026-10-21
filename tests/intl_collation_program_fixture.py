#!/usr/bin/env python3
"""Authored executable programs and structurally valid semantic error streams."""
import argparse
from pathlib import Path

from test_intl_collation_ir import encode, program, rules
from intl_collation_root_input_fixture import emit


def nodes(items, locale='en'):
    return program('', locale)._replace(nodes=tuple(items))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    valid = program("[strength 2] [caseFirst upper] [reorder Hani others Latn] "
                    "&a <*g-i <j/😀 &j =k &a <a|x "
                    "&[last tertiary ignorable] = '\0' "
                    "[suppressContractions [a]] &a <a|z [optimize [a-z]]")
    reset, relation = program('&a<x').nodes
    before, _ = program('&[before 2]a<<x').nodes
    fixtures = {
        'success': [valid, program('[numericOrdering on] &a<g', 'fr')],
        'before': [program('&[before 2]a <<*xy')],
        'compatible': [program('&a<*ﬀ')],
        'inert': [program('&a<*À')],
        'combining': [program('&a<*\u0301')],
        'hangul': [program('&a<*가')],
        'interior': [program("&a<*'¿'-'Á'")],
        'prefix': [program('&a<x|\u0301')],
        'reset': [program('&[first variable]<x')],
        'unknown_reset': [nodes([reset._replace(text=(), special='unknown'), relation])],
        'setting': [nodes([rules.Setting(reset.position, 'strength', ('9',))])],
        'unsupported': [nodes([rules.Setting(reset.position, 'unknown', ('on',))])],
        'no_reset': [nodes([relation])],
        'dangling': [nodes([reset])],
        'double_reset': [nodes([reset, reset, relation])],
        'before_mismatch': [nodes([before, relation])],
        'empty': [program('')],
    }
    with args.output.open('w') as output:
        output.write('#include <stddef.h>\n')
        for name in sorted(fixtures):
            emit(output, 'intl_program_' + name, encode(fixtures[name]))


if __name__ == '__main__':
    main()
