#!/usr/bin/env python3
"""Require a generated program's success marker without access to its sources.

Generated main can return zero after printing a JavaScript exception, so its
exit status alone does not prove that the script completed its assertions.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

prefix = shlex.split(os.environ.get('MESON_EXE_WRAPPER', ''))
if prefix and "/" in prefix[0] and not Path(prefix[0]).is_absolute():
    prefix[0] = str(Path(prefix[0]).resolve())
command = prefix + [str(Path(sys.argv[1]).resolve())]
with tempfile.TemporaryDirectory(prefix="qjsc-execution-") as directory:
    completed = subprocess.run(command, cwd=directory,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
assert completed.returncode == 0, (completed.returncode, completed.stderr)
assert sys.argv[2].encode("ascii") in completed.stdout.splitlines(), (
    "generated script did not complete its assertions",
    completed.stdout, completed.stderr)
