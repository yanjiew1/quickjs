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

"""Acquire and authenticate structured source data using its selected pin."""
import argparse
import subprocess
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = {
    'cldr': ROOT / 'tools/intl/acquire.py',
    'dictionaries': ROOT / 'tools/intl/segment_dictionary_sources.py',
    'calendar': ROOT / 'tools/calendar/acquire.py',
    'timezone': ROOT / 'tools/timezone/acquire.py',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     epilog='Place --help after a family to see its source options.')
    parser.add_argument('family', choices=tuple(SCRIPTS))
    args = parser.parse_args(sys.argv[1:2])
    # subprocess preserves spaced paths/arguments on Windows too; the CRT's
    # execv does not provide that quoting. Family operations keep their bounds.
    return subprocess.call([sys.executable, str(SCRIPTS[args.family])] + sys.argv[2:])


if __name__ == '__main__':
    sys.exit(main())
