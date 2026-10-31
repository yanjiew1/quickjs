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

"""Emit collision witnesses through qjsc; leave no sources for execution."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

qjsc = [str(Path(sys.argv[1]).resolve())]
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
            assert b"reserved generated identifier" in rejected.stderr, (
                symbol, rejected.stderr)

    # Every emitted array also owns a _size symbol; JSON owns _module_name.
    (scratch / "a_size.js").write_text("export default 75;\n")
    (scratch / "a_module_name.js").write_text("export default 76;\n")
    for name, sources in (
        ("array-size", ["a_attributes.js", "a_attributes_size.js"]),
        ("json-module-name", ["main-module-name.js"]),
    ):
        if name == "json-module-name":
            (scratch / sources[0]).write_text(
                'import payload from "./a.json" with { type: "json" };\n'
                'import peer from "./a_module_name.js";\n'
                'if (payload.value !== 73 || peer !== 76) throw Error();\n')
        probe = subprocess.run(qjsc + ["-c", "-m", "-o",
                                     str(scratch / (name + ".c"))] + sources,
                               cwd=str(scratch), stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
        assert probe.returncode == 0, (name, probe.stderr)
        text = (scratch / (name + ".c")).read_text()
        symbols = re.findall(r"^(?:static )?const uint(?:8|32)_t ([A-Za-z_]\w*)",
                             text, re.MULTILINE)
        assert symbols and len(symbols) == len(set(symbols)), (name, symbols)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(emitted)
# Every source disappears here. Meson separately compiles the C with its
# selected native target; the generated executable must pass the JS marker.
