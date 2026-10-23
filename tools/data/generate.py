#!/usr/bin/env python3
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
