#!/usr/bin/env python3
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
