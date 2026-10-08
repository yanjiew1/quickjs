#!/usr/bin/env python3
"""Require a completed JS assertion path in an actual generated executable.

Generated qjsc main currently returns zero even when its void script helper
prints a JavaScript exception, so exit status alone is not a regression gate.
"""
import shlex
import subprocess
import sys

command = shlex.split(sys.argv[1]) + [sys.argv[2]]
completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
assert completed.returncode == 0, (completed.returncode, completed.stderr)
assert sys.argv[3].encode("ascii") in completed.stdout.splitlines(), (
    "generated script did not complete its assertions", completed.stdout,
    completed.stderr)
