#!/usr/bin/env python3
"""Prepare host-zic headers outside the pristine submodule."""
import argparse
from pathlib import Path
parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
parser.add_argument("--header", choices=("tzdir.h", "version.h"))
args = parser.parse_args()
args.directory.mkdir(parents=True, exist_ok=True)
headers = {
    "tzdir.h": '#ifndef TZDEFAULT\n#define TZDEFAULT "/etc/localtime"\n#endif\n#ifndef TZDIR\n#define TZDIR "/usr/share/zoneinfo"\n#endif\n',
    "version.h": '#define PKGVERSION "QuickJS "\n#define TZVERSION "2026e"\n#define REPORT_BUGS_TO "https://github.com/eggert/tz"\n',
}
for name, text in headers.items():
    if not args.header or name == args.header:
        (args.directory / name).write_text(text)
