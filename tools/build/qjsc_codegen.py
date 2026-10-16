#!/usr/bin/env python3
"""Run the native bytecode compiler with source-relative module identities."""
from pathlib import Path
import subprocess
import sys

source = Path(sys.argv[1]).resolve()
compiler = Path(sys.argv[2]).resolve()
output = Path(sys.argv[3]).resolve()
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.check_call([str(compiler), '-o', str(output)] + sys.argv[4:], cwd=str(source))
