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

"""Exercise qjsc's default executable output and its selected target and sanitizer flags."""
from pathlib import Path
import json
import shlex
import struct
import subprocess
import sys
import tempfile

qjsc = str(Path(sys.argv[1]).resolve())
expected_flags = shlex.split(sys.argv[2])
intl_enabled, temporal_enabled = (value == "y" for value in sys.argv[3:5])
lto_flag = sys.argv[5]
library_name = sys.argv[6]
with tempfile.TemporaryDirectory(prefix="qjsc-executable-") as directory:
    scratch = Path(directory)
    source = scratch / "main.js"
    executable = scratch / "check"
    source.write_text(
        'if (6 * 7 !== 42) throw Error("generated executable failed");\n'
        'if ((typeof Intl === "object") !== ' + str(intl_enabled).lower() +
        ') throw Error("qjsc Intl configuration mismatch");\n'
        'if ((typeof Temporal === "object") !== ' + str(temporal_enabled).lower() +
        ') throw Error("qjsc Temporal configuration mismatch");\n'
        'print("qjsc-executable-ok");\n')
    compiled = subprocess.run([qjsc, "-v", "-o", str(executable),
                                       str(source)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
    assert compiled.returncode == 0, (compiled.returncode, compiled.stderr)
    arguments = compiled.stdout.decode().split()
    actual_flags = [arg for arg in arguments if arg.startswith("-fsanitize=") or arg == "-m32"]
    assert actual_flags == expected_flags, (actual_flags, expected_flags,
                                             compiled.stdout, compiled.stderr)
    actual_lto = [arg for arg in arguments if arg.startswith("-flto")]
    assert actual_lto == ([lto_flag] if lto_flag else []), actual_lto
    libraries = [Path(arg).name for arg in arguments if Path(arg).name in
                 ("libquickjs.a", "libquickjs.so", "libquickjs.dylib",
                  "libquickjs.lto.a")]
    assert libraries == [library_name], libraries
    if any(arg.startswith("-fsanitize=") for arg in expected_flags):
        assert "-fno-omit-frame-pointer" in arguments, compiled.stdout
    source.unlink()
    completed = subprocess.run([str(executable)], cwd=str(scratch),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert completed.returncode == 0, (completed.returncode, completed.stderr)
    assert b"qjsc-executable-ok" in completed.stdout.splitlines(), (
        completed.stdout, completed.stderr)

    source.write_text('function readMeta() { return import.meta; }\n'
                      'if (typeof import.meta !== "object" || '
                      'readMeta() !== import.meta) '
                      'throw Error("missing import.meta");\n'
                      'print("qjsc-stripped-import-meta-ok");\n')
    compiled = subprocess.run([qjsc, "-m", "-s", "-o", str(executable),
                               str(source)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=30)
    assert compiled.returncode == 0, (compiled.returncode, compiled.stderr)
    completed = subprocess.run([str(executable)], cwd=str(scratch),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               timeout=30)
    assert completed.returncode == 0, (completed.returncode, completed.stderr)
    assert b"qjsc-stripped-import-meta-ok" in completed.stdout.splitlines(), (
        completed.stdout, completed.stderr)

    # Supply argv[0] separately so this also covers hosts with short path limits.
    source.write_text('print(42);\n')
    for name in ['x' * 1100, '/' + 'x' * 1015 + '/qjsc']:
        rejected = subprocess.run([name, '-o', str(executable), str(source)],
                                  executable=qjsc, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=30)
        assert rejected.returncode == 1, (rejected.returncode, rejected.stderr)
        assert b'Compiler include or library path is too long' in rejected.stderr, rejected.stderr

    if sys.platform.startswith('linux'):
        output_dir = scratch
        for _ in range(5):
            output_dir /= 'd' * 210
            output_dir.mkdir()
        generated = output_dir / 'generated.c'
        compiled = subprocess.run([qjsc, '-c', '-o', str(generated), str(source)],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  timeout=30)
        assert compiled.returncode == 0, (compiled.returncode, compiled.stderr)
        assert generated.is_file(), 'qjsc truncated the requested output path'

    module_name = 'mod' + 'x' * 1100
    source.write_text(f"import '{module_name}';\n")
    generated = scratch / 'long-module.c'
    compiled = subprocess.run([qjsc, '-c', '-m', '-M',
                               f'{module_name},longmod', '-o', str(generated),
                               str(source)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=30)
    assert compiled.returncode == 0, (compiled.returncode, compiled.stderr)

    # A module specifier is data, not part of the generated C syntax.
    for module_name, literal in [
        ('quote"and\\backslash', '"quote\\"and\\\\backslash"'),
        ('trigraph??/name', '"trigraph\\?\\?/name"'),
    ]:
        source.write_text('import ' + json.dumps(module_name) + ';\n')
        generated = scratch / 'escaped-module.c'
        compiled = subprocess.run([qjsc, '-e', '-M',
                                   f'{module_name},specialmod', '-o', str(generated),
                                   str(source)], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=30)
        assert compiled.returncode == 0, (compiled.returncode, compiled.stderr)
        expected = f'js_init_module_specialmod(ctx, {literal});'
        assert expected in generated.read_text(), generated.read_text()

    if Path('/dev/full').exists():
        source.write_text('print(42);\n')
        failed = subprocess.run([qjsc, '-c', '-o', '/dev/full', str(source)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=30)
        assert failed.returncode != 0, failed.stderr

    if struct.calcsize('P') >= 8 and '-m32' not in expected_flags:
        source.write_text('print(42);\n')
        generated = scratch / 'stack-size.c'
        compiled = subprocess.run([qjsc, '-e', '-S', '5G', '-o', str(generated),
                                   str(source)], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=30)
        assert compiled.returncode == 0, (compiled.returncode, compiled.stderr)
        assert 'JS_SetMaxStackSize(rt, 5368709120);' in generated.read_text()
