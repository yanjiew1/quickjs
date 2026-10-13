#!/bin/sh
# Check virtual CLI modules and real file import.meta with the actual qjs.
set -eu
qjs_runner=${1:-}
qjs_binary=${2:-./qjs}
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
