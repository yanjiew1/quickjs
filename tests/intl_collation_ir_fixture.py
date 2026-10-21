#!/usr/bin/env python3
"""Counted unaligned operands and independent program identities for C visits."""
import argparse
from pathlib import Path

from test_intl_collation_ir import encode, program


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = encode([program("&'\0'<a|b/😀 <*AB-C [normalization on] "
                          "[suppressContractions [x-z]] [optimize [k-m]]"),
                  program('', 'fr')])
    with args.output.open('w') as output:
        output.write('#include <stddef.h>\nconst unsigned char intl_ir_fixture[] = {\n')
        for start in range(0, len(raw), 16):
            output.write('    ' + ', '.join(str(value) for value in raw[start:start + 16]) + ',\n')
        output.write('};\nconst size_t intl_ir_fixture_size = sizeof(intl_ir_fixture);\n')


if __name__ == '__main__':
    main()
