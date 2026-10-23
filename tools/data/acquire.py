#!/usr/bin/env python3
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
