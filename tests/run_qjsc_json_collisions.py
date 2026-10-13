#!/usr/bin/env python3
"""Run each compiled collision witness in an empty source-free directory."""
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

prefix = shlex.split(sys.argv[1])
if prefix and "/" in prefix[0] and not Path(prefix[0]).is_absolute():
    prefix[0] = str(Path(prefix[0]).resolve())
command = prefix + [str(Path(sys.argv[2]).resolve())]
with tempfile.TemporaryDirectory(prefix="qjsc-collision-execution-") as directory:
    completed = subprocess.run(command, cwd=directory,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
assert completed.returncode == 0, (completed.returncode, completed.stderr)
assert sys.argv[3].encode("ascii") in completed.stdout.splitlines(), (
    "source-free collision script did not complete its assertions",
    completed.stdout, completed.stderr)
