#!/bin/sh
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

# Check virtual CLI modules and real file import.meta with the actual qjs.
set -eu
qjs_runner=${MESON_EXE_WRAPPER:-}
qjs_binary=${1:-./qjs}
qjs_root=$(pwd)
qjs_fixtures=$(CDPATH= cd "$(dirname "$0")" && pwd)
case "$qjs_binary" in
    /*) ;;
    *) qjs_binary="$qjs_root/$qjs_binary" ;;
esac
# Preserve the repository's runner word-list convention under a new cwd.
if [ -n "$qjs_runner" ]; then
    set -- $qjs_runner
    qjs_launcher=$1
    shift
    case "$qjs_launcher" in
        /*) ;;
        */*) qjs_launcher="$qjs_root/$qjs_launcher" ;;
    esac
    qjs_runner="$qjs_launcher${*:+ $*}"
fi
qjs_tmp=$(mktemp -d ./qjs-cli.XXXXXX)
qjs_tmp="$qjs_root/$qjs_tmp"
trap 'rm -rf "$qjs_tmp"' EXIT HUP INT TERM
cp "$qjs_fixtures/test_qjs_std.js" "$qjs_tmp/script.js"
cp "$qjs_fixtures/test_qjs_import_meta.js" "$qjs_tmp/module.js"
cp "$qjs_fixtures/fixture_qjs_import_meta.js" "$qjs_tmp/fixture_qjs_import_meta.js"
ln -s module.js "$qjs_tmp/entry.js"
cd "$qjs_tmp"

invoke_qjs() {
    qjs_status=0
    if $qjs_runner "$qjs_binary" "$@" > stdout 2> stderr; then
        :
    else
        qjs_status=$?
    fi
    tr -d '\r' < stdout > out
    tr -d '\r' < stderr > err
    if grep -Eq 'AddressSanitizer|LeakSanitizer|MemorySanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer|runtime error:' out err; then
        cat stderr >&2
        echo 'qjs CLI regression encountered a sanitizer diagnostic' >&2
        exit 1
    fi
}

check_success() {
    qjs_marker=$1
    shift
    invoke_qjs "$@"
    if [ "$qjs_status" -ne 0 ] || [ -s err ]; then
        cat stderr >&2
        echo 'qjs CLI invocation unexpectedly failed' >&2
        exit 1
    fi
    printf '%s\n' "$qjs_marker" > expected
    diff -u expected out
}

# No physical <input> or <cmdline> file can hide virtual-name resolution.
check_success qjs-cli-std-ok --std script.js
check_success qjs-cli-file-meta-ok --std -m module.js
# Canonical main metadata must still refer to module.js through this alias.
check_success qjs-cli-file-meta-ok --std -m entry.js
check_success qjs-cli-virtual-meta-ok --std -m -e '
if (typeof std.sprintf !== "function" || typeof os.getcwd !== "function" ||
    import.meta.url !== "file://<cmdline>" || import.meta.main !== true)
    throw Error("virtual command-line module metadata failed");
print("qjs-cli-virtual-meta-ok");'

# Module execution errors still use the normal failing CLI cleanup path.
invoke_qjs --std -m -e 'throw Error("qjs-cli-abrupt");'
if [ "$qjs_status" -ne 1 ] || [ -s out ] ||
   ! grep -q '^Error: qjs-cli-abrupt$' err; then
    cat stderr >&2
    echo 'qjs CLI failed to propagate an abrupt module completion' >&2
    exit 1
fi

invoke_qjs --std -e 'os.ttySetRaw(-1)'
if [ "$qjs_status" -ne 1 ] ||
   ! grep -q 'could not set raw terminal mode' err; then
    cat stderr >&2
    echo 'ttySetRaw accepted an invalid terminal descriptor' >&2
    exit 1
fi

# A PTY lets us check restoration of a nonzero descriptor without a real TTY.
if [ -z "$qjs_runner" ]; then
    python3 - "$qjs_binary" <<'PY'
import os
import subprocess
import sys
try:
    import termios
except ImportError:
    sys.exit(0)

master, slave = os.openpty()
original = termios.tcgetattr(slave)
try:
    script = ('os.ttySetRaw(%d); let rejected = false; '
              'try { os.ttySetRaw(%d); } catch (e) { rejected = true; } '
              'if (!rejected) throw Error("duplicate raw mode accepted");') % (slave, slave)
    result = subprocess.run(
        [sys.argv[1], '--std', '-e', script],
        pass_fds=(slave,), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=10)
    assert result.returncode == 0, result.stderr
    assert termios.tcgetattr(slave) == original
finally:
    termios.tcsetattr(slave, termios.TCSANOW, original)
    os.close(master)
    os.close(slave)
PY
fi

invoke_qjs -T -e 'print("qjs-cli-trace-ok")'
if [ "$qjs_status" -ne 0 ] || [ -s err ] ||
   ! grep -q '^qjs-cli-trace-ok$' out ||
   ! grep -Eq '^A [0-9]+ -> H[+-][0-9]+\.[0-9]+' out; then
    cat stdout stderr >&2
    echo 'qjs CLI allocation trace was malformed' >&2
    exit 1
fi
