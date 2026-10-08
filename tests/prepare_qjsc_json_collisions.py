#!/usr/bin/env python3
"""Emit collision witnesses through qjsc; leave no sources for execution."""
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

prefix = shlex.split(sys.argv[4]) if len(sys.argv) > 4 else []
qjsc = prefix + [str(Path(sys.argv[1]).resolve())]
destination = Path(sys.argv[2]).resolve()
case = sys.argv[3]
assert case in ("attributes-first", "attributes-last", "size-first", "size-last")
fixtures = Path(__file__).resolve().parent / "qjsc-json-collisions"
with tempfile.TemporaryDirectory(prefix="qjsc-json-collision-") as directory:
    scratch = Path(directory)
    for name in ("a.json", "a_attributes.js", "a_attributes_size.js",
                 "a_1_attributes.js", "a_1_attributes_size.js"):
        shutil.copy2(str(fixtures / name), str(scratch / name))
    shutil.copy2(str(fixtures / (case + ".js")), str(scratch / "main.js"))
    result = subprocess.run(qjsc + ["-e", "-o", str(scratch / "generated.c"),
                             "main.js"], cwd=str(scratch),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert result.returncode == 0, result.stderr
    emitted = (scratch / "generated.c").read_text()
    names = re.findall(r"^(?:static )?const uint(?:8|32)_t ([A-Za-z_]\w*)",
                       emitted, re.MULTILINE)
    assert names and len(names) == len(set(names)), names
    assert "js_std_eval_binary_json_module2" in emitted
    if case == "attributes-first":
        for symbol in ("qjsc_a_attributes", "qjsc_a_attributes_size"):
            rejected = subprocess.run(qjsc + ["-e", "-N", symbol, "-o",
                                       str(scratch / "rejected.c"), "main.js"],
                                      cwd=str(scratch), stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
            assert rejected.returncode != 0, symbol
            assert b"reserved JSON attribute identifier" in rejected.stderr, (
                symbol, rejected.stderr)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(emitted)
# Every source disappears here. Make separately compiles the C with its
# selected native target; the generated executable must pass the JS marker.
