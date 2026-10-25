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

"""Copy declared runtime fixtures, keeping generated binaries out of the source tree."""
from pathlib import Path
import filecmp
import shutil
import sys

source, build, manifest, stamp = (Path(arg).resolve() for arg in sys.argv[1:5])
for name in manifest.read_text().splitlines():
    relative = Path(name)
    if relative.is_absolute() or '..' in relative.parts:
        raise ValueError('fixture paths must stay within the source tree')
    target = build / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or not filecmp.cmp(str(source / relative), str(target), shallow=False):
        shutil.copyfile(str(source / relative), str(target))
stamp.write_text('Runtime fixture tree prepared.\n')
