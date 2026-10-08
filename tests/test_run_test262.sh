#!/bin/sh
# Check runner diagnostics using a small local harness, without Test262.
set -eu
test262_runner=${1:-}
test262_binary=${2:-./run-test262}
# Relative paths also work when the runner is a Windows binary under Wine.
test262_tmp=$(mktemp -d ./test262-runner.XXXXXX)
trap 'rm -rf "$test262_tmp"' EXIT HUP INT TERM
mkdir "$test262_tmp/harness" "$test262_tmp/failure" "$test262_tmp/suite"
: > "$test262_tmp/harness/sta.js"
cat > "$test262_tmp/config" <<'CONFIG'
[config]
style=new
harnessdir=harness
async=yes
CONFIG
cat > "$test262_tmp/failure/case.js" <<'JS'
/*---
flags: [noStrict]
---*/
throw new Error("test body must not run after a harness failure");
JS

invoke_runner() {
    test262_fatal=$1
    shift
    test262_status=0
    if [ "$test262_fatal" = yes ]; then
        # Including a broken harness intentionally calls fatal() before the
        # runtime is freed. Disable only this child's LSan exit scan; keep
        # inherited ASan memory checks and UBSan settings in effect.
        if LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}leak_check_at_exit=0" \
                $test262_runner "$test262_binary" "$@" \
                > "$test262_tmp/stdout" 2> "$test262_tmp/stderr"; then
            :
        else
            test262_status=$?
        fi
    else
        if $test262_runner "$test262_binary" "$@" \
                > "$test262_tmp/stdout" 2> "$test262_tmp/stderr"; then
            :
        else
            test262_status=$?
        fi
    fi
    # Windows output can use CRLF. Preserve all other bytes for comparisons.
    tr -d '\r' < "$test262_tmp/stdout" > "$test262_tmp/out"
    tr -d '\r' < "$test262_tmp/stderr" > "$test262_tmp/err"
    if grep -Eq 'AddressSanitizer|LeakSanitizer|MemorySanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer|runtime error:' \
            "$test262_tmp/out" "$test262_tmp/err"; then
        cat "$test262_tmp/stderr" >&2
        echo 'runner regression encountered a sanitizer diagnostic' >&2
        exit 1
    fi
}

check_harness_failure() {
    test262_threads=$1
    test262_kind=$2
    case "$test262_kind" in
        Error)
            printf '%s\n' 'throw new Error("runner harness Error sentinel");' \
                > "$test262_tmp/harness/assert.js"
            test262_expected='Error: runner harness Error sentinel'
            ;;
        string)
            printf '%s\n' 'throw "runner harness string sentinel";' \
                > "$test262_tmp/harness/assert.js"
            test262_expected='Throw: runner harness string sentinel'
            ;;
    esac
    rm -f "$test262_tmp/report"
    invoke_runner yes -q -c "$test262_tmp/config" -T "$test262_threads" \
        -r "$test262_tmp/report" -d "$test262_tmp/failure"
    [ "$test262_status" -eq 1 ]
    grep -Fqx "$test262_expected" "$test262_tmp/err"
    grep -Fq "error evaluating $test262_tmp/harness/assert.js" "$test262_tmp/err"
    grep -Fq "error including assert.js for $test262_tmp/failure/case.js" "$test262_tmp/err"
    [ ! -s "$test262_tmp/out" ]
    if [ "$test262_kind" = Error ]; then
        grep -Fq "$test262_tmp/harness/assert.js:1:" "$test262_tmp/err"
    fi
    if [ "$test262_threads" -eq 1 ]; then
        # Harness diagnostics belong on stderr, even with a configured report.
        printf '0: %s  @noStrict\n' "$test262_tmp/failure/case.js" \
            > "$test262_tmp/expected"
        tr -d '\r' < "$test262_tmp/report" > "$test262_tmp/actual"
        diff -u "$test262_tmp/expected" "$test262_tmp/actual"
    else
        [ ! -e "$test262_tmp/report" ]
    fi
}

# Directory mode really creates two workers; only one test is needed to
# exercise the NULL global report stream without racing two fatal exits.
check_harness_failure 2 Error
check_harness_failure 2 string
check_harness_failure 1 Error
check_harness_failure 1 string

: > "$test262_tmp/harness/assert.js"
cat > "$test262_tmp/harness/doneprintHandle.js" <<'JS'
function $DONE(error) {
    if (error === undefined)
        print("Test262:AsyncTestComplete");
    else
        print("Test262:AsyncTestFailure: " + error);
}
JS
cat > "$test262_tmp/suite/async.js" <<'JS'
/*---
flags: [noStrict, async]
---*/
Promise.resolve().then(function () { $DONE(); });
JS
cat > "$test262_tmp/suite/negative.js" <<'JS'
/*---
flags: [noStrict]
negative:
  phase: runtime
  type: TypeError
---*/
throw "TypeError: ordinary throw sentinel";
JS
cat > "$test262_tmp/suite/ordinary.js" <<'JS'
/*---
flags: [noStrict]
---*/
print("runner", 42, true, null);
print("ordinary", "two");
JS
rm -f "$test262_tmp/report"
invoke_runner no -q -c "$test262_tmp/config" -T 1 \
    -r "$test262_tmp/report" -d "$test262_tmp/suite"
[ "$test262_status" -eq 0 ]
[ ! -s "$test262_tmp/out" ]
grep -Fqx 'Result: 0/3 errors' "$test262_tmp/err"
{
    printf '0: %s  @noStrict  async\n' "$test262_tmp/suite/async.js"
    printf '%s\n' 'Test262:AsyncTestComplete'
    printf '1: %s  @noStrict  @negative\n' "$test262_tmp/suite/negative.js"
    printf '%s\n' 'Throw: TypeError: ordinary throw sentinel'
    printf '2: %s  @noStrict\n' "$test262_tmp/suite/ordinary.js"
    printf '%s\n' 'runner 42 true null' 'ordinary two'
} > "$test262_tmp/expected"
tr -d '\r' < "$test262_tmp/report" > "$test262_tmp/actual"
diff -u "$test262_tmp/expected" "$test262_tmp/actual"

# Direct file mode still sends ordinary JavaScript print to stdout.
rm -f "$test262_tmp/report"
invoke_runner no -q -c "$test262_tmp/config" -T 1 \
    -r "$test262_tmp/report" -f "$test262_tmp/suite/ordinary.js"
[ "$test262_status" -eq 0 ]
[ ! -e "$test262_tmp/report" ]
printf '%s\n' 'runner 42 true null' 'ordinary two' > "$test262_tmp/expected"
diff -u "$test262_tmp/expected" "$test262_tmp/out"

# Async completion still counts while ordinary JavaScript print is suppressed.
rm -f "$test262_tmp/report"
invoke_runner no -q -c "$test262_tmp/config" -T 2 \
    -r "$test262_tmp/report" -d "$test262_tmp/suite"
[ "$test262_status" -eq 0 ]
[ ! -s "$test262_tmp/out" ]
[ ! -e "$test262_tmp/report" ]
grep -Fqx 'Result: 0/3 errors' "$test262_tmp/err"

# An async failure marker must still prevent a passing result.
cat > "$test262_tmp/failure/case.js" <<'JS'
/*---
flags: [noStrict, async]
---*/
Promise.resolve().then(function () { $DONE("runner async failure sentinel"); });
JS
for test262_threads in 1 2; do
    rm -f "$test262_tmp/report"
    invoke_runner no -q -v -c "$test262_tmp/config" -T "$test262_threads" \
        -r "$test262_tmp/report" -d "$test262_tmp/failure"
    [ "$test262_status" -eq 1 ]
    grep -Fq 'TypeError: $DONE() not called' "$test262_tmp/out"
    grep -Fqx 'Result: 1/1 error' "$test262_tmp/err"
    if [ "$test262_threads" -eq 1 ]; then
        tr -d '\r' < "$test262_tmp/report" > "$test262_tmp/actual"
        grep -Fqx 'Test262:AsyncTestFailure: runner async failure sentinel' "$test262_tmp/actual"
        grep -Fqx 'TypeError: $DONE() not called' "$test262_tmp/actual"
        grep -Fqx '  FAILED' "$test262_tmp/actual"
    else
        [ ! -e "$test262_tmp/report" ]
    fi
done
