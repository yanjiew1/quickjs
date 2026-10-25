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
