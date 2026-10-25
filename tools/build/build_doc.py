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

"""Build documentation with generated version files and no source-tree products."""
from pathlib import Path
import subprocess
import sys

mode, tool, source, output, directory = sys.argv[1:6]
source, output, directory = (Path(value).resolve() for value in [source, output, directory])
if mode == 'html':
    command = [tool, '--html', '--no-headers', '--no-split', '--number-sections',
               '-I', str(directory), '-o', str(output), str(source)]
elif mode == 'pdf':
    command = [tool, '--clean', '-o', str(output), '-q', str(source)]
else:
    raise ValueError('Unknown documentation format: ' + mode)
subprocess.check_call(command, cwd=str(directory))
if mode == 'html':
    text = output.read_text(encoding='utf-8')
    text = text.replace('</style>', '</style>\n<meta name="viewport" '
                        'content="width=device-width, initial-scale=1.0">', 1)
    output.write_text(text, encoding='utf-8')
