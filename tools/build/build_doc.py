#!/usr/bin/env python3
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
