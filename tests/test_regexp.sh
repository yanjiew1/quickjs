#!/bin/sh
# Check capture offsets and unmatched groups in the RegExp test tool.
set -eu
regexp_runner=${1:-}
regexp_binary=${2:-./regexp_test}
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
