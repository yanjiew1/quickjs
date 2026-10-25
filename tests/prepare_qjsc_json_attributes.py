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

"""Exercise the real qjsc CLI; generated executables use no JSON source files.

Only -e output is requested. Meson supplies a native qjsc in cross builds,
then compiles the emitted C separately with the selected target compiler.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

qjsc = [str(Path(sys.argv[1]).resolve())]
destination = Path(sys.argv[2]).resolve()
fixtures = Path(__file__).resolve().parent / "qjsc-json-attributes"
with tempfile.TemporaryDirectory(prefix="qjsc-json-attributes-") as directory:
    scratch = Path(directory)
    for name in ["main.js", "invalid-main.js", "native-std.js", "payload.so",
                 "invalid.so", "std", "extended.so", "plain.json"]:
        shutil.copy2(str(fixtures / name), str(scratch / name))

    def compile_source(source, output, options=()):
        return subprocess.run(qjsc + ["-e", "-o", str(scratch / output)] +
                              list(options) + [source], cwd=str(scratch),
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    invalid = compile_source("invalid-main.js", "invalid.c")
    assert invalid.returncode != 0, "invalid JSON .so compiled as a native dummy"
    assert b"SyntaxError" in invalid.stderr, invalid.stderr
    assert b"binary module" not in invalid.stderr, invalid.stderr

    plain = compile_source("main.js", "plain.c")
    assert plain.returncode == 0, plain.stderr
    assert b"binary module" not in plain.stderr, plain.stderr
    declared = compile_source("main.js", "declared.c",
                              ["-M", "payload.so,qjsc_json_declared"])
    assert declared.returncode == 0, declared.stderr
    assert b"binary module" not in declared.stderr, declared.stderr
    emitted = (scratch / "declared.c").read_text()
    assert "js_std_eval_binary_json_module2" in emitted
    assert "js_std_eval_binary_json_module(ctx," not in emitted
    assert "_attributes_size" in emitted and "_attributes[" in emitted
    assert "js_init_module_qjsc_json_declared" not in emitted
    assert "js_init_module_std" not in emitted

    native = compile_source("native-std.js", "native.c")
    assert native.returncode == 0, native.stderr
    assert "js_init_module_std" in (scratch / "native.c").read_text()

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(emitted)
# Every source, including the JSON file named std, is now gone. The separately
# compiled executable must use the serialized JSON modules, never file I/O or
# binary module loading. The declared C name must not require a native stub.
