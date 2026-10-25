#!/usr/bin/env python3
"""Exercise qjsc's default executable output and its selected target and sanitizer flags."""
from pathlib import Path
import shlex
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

    # Supply argv[0] separately so this also covers hosts with short path limits.
    source.write_text('print(42);\n')
    for name in ['x' * 1100, '/' + 'x' * 1015 + '/qjsc']:
        rejected = subprocess.run([name, '-o', str(executable), str(source)],
                                  executable=qjsc, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=30)
        assert rejected.returncode == 1, (rejected.returncode, rejected.stderr)
        assert b'Compiler include or library path is too long' in rejected.stderr, rejected.stderr
