#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

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
