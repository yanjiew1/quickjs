#!/usr/bin/env python3
"""Join benchmark inputs in their explicitly declared order."""
from pathlib import Path
import sys

with Path(sys.argv[1]).open('wb') as output:
    for name in sys.argv[2:]:
        output.write(Path(name).read_bytes())
        output.write(b'\n')
