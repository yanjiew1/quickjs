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

"""Generate prepared data offline from supplied compatible source files."""
import argparse
import subprocess
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = {
    'calendar': ROOT / 'tools/calendar/generate.py',
    'timezone': ROOT / 'tools/timezone/generate.py',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     epilog='Place --help after a family to see its input/output options.')
    parser.add_argument('family', choices=('intl',) + tuple(SCRIPTS))
    args = parser.parse_args(sys.argv[1:2])
    # Other families do not need to import the Intl preparation pipeline.
    if args.family == 'intl':
        from _support.intl_pipeline import main as generate_intl
        return generate_intl(sys.argv[2:], prog=parser.prog + ' intl')
    return subprocess.call([sys.executable, str(SCRIPTS[args.family])] + sys.argv[2:])


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print('generate: ' + str(error), file=sys.stderr)
        sys.exit(1)
