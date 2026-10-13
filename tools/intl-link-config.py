#!/usr/bin/env python3
"""Generate shared ICU backend link metadata without interpreting flags."""
import json
from pathlib import Path
import shlex
import sys

mode, destination = sys.argv[1:3]
args = sys.argv[3:]
if mode == "header":
    assert args[0] == "--"
    content = "/* Generated ICU link arguments. Do not edit. */\n"
    content += "static const char *const qjsc_intl_link_args[] = {\n"
    content += "".join("    " + json.dumps(arg, ensure_ascii=True) + ",\n"
                       for arg in args[1:])
    content += "    NULL,\n};\n"
elif mode == "pkgconfig":
    prefix, version = args[:2]
    assert args[2] == "--"
    # These are already individual compiler/linker argv entries.
    libs = " ".join(shlex.quote(arg) for arg in args[3:])
    content = (f"prefix={prefix}\n"
               "libdir=${prefix}/lib/quickjs\n"
               "includedir=${prefix}/include/quickjs\n\n"
               "Name: QuickJS\n"
               "Description: QuickJS with a native ICU backend\n"
               f"Version: {version}\n"
               "Cflags: -I${includedir}\n"
               "Libs: -L${libdir} -lquickjs\n"
               f"Libs.private: {libs}\n")
else:
    raise SystemExit("unknown metadata mode")
path = Path(destination)
path.parent.mkdir(parents=True, exist_ok=True)
if not path.exists() or path.read_text() != content:
    path.write_text(content)
