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

# Check capture offsets and unmatched groups in the RegExp test tool.
set -eu
regexp_runner=${MESON_EXE_WRAPPER:-}
regexp_binary=${1:-./regexp_test}
regexp_tmp=$(mktemp -d)
trap 'rm -rf "$regexp_tmp"' EXIT HUP INT TERM

check_output() {
    expected=$1
    shift
    $regexp_runner "$regexp_binary" "$@" > "$regexp_tmp/output"
    # TEST builds dump bytecode before the result; Windows uses CRLF.
    tr -d '\r' < "$regexp_tmp/output" | sed -n '/^ret=/,$p' \
        > "$regexp_tmp/actual"
    printf '%s\n' "$expected" > "$regexp_tmp/expected"
    diff -u "$regexp_tmp/expected" "$regexp_tmp/actual"
}

check_output 'ret=1
0: 0
1: 3
2: 0
3: 3' '(a+)' 0 aaa

check_output 'ret=1
0: 0
1: 2
2: 0
3: 1
4: <nil>
5: <nil>
6: 1
7: 2' '(a)(b)?(c)' 0 ac

check_output 'ret=0' '(a+)' 0 bbb

if $regexp_runner "$regexp_binary" '(' 0 '' \
        > "$regexp_tmp/output" 2> "$regexp_tmp/error"; then
    echo 'invalid RegExp unexpectedly compiled' >&2
    exit 1
fi
