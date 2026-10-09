#!/usr/bin/env python3
"""Exercise qjsc's default executable output and its selected sanitizer flags."""
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

qjsc = str(Path(sys.argv[1]).resolve())
prefix = shlex.split(sys.argv[2])
if prefix and "/" in prefix[0] and not Path(prefix[0]).is_absolute():
    prefix[0] = str(Path(prefix[0]).resolve())
expected_flags = shlex.split(sys.argv[3])
intl_enabled, temporal_enabled = (value == "y" for value in sys.argv[4:6])
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
    compiled = subprocess.run(prefix + [qjsc, "-v", "-o", str(executable),
                                       str(source)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
    assert compiled.returncode == 0, (compiled.returncode, compiled.stderr)
    arguments = compiled.stdout.decode().split()
    actual_flags = [arg for arg in arguments if arg.startswith("-fsanitize=")]
    assert actual_flags == expected_flags, (actual_flags, expected_flags,
                                             compiled.stdout, compiled.stderr)
    if expected_flags:
        assert "-fno-omit-frame-pointer" in arguments, compiled.stdout
    source.unlink()
    completed = subprocess.run(prefix + [str(executable)], cwd=str(scratch),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert completed.returncode == 0, (completed.returncode, completed.stderr)
    assert b"qjsc-executable-ok" in completed.stdout.splitlines(), (
        completed.stdout, completed.stderr)
