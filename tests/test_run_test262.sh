#!/bin/sh
# Check runner diagnostics using a small local harness, without Test262.
set -eu
test262_runner=${1:-}
test262_binary=${2:-./run-test262}
# Relative paths also work when the runner is a Windows binary under Wine.
test262_tmp=$(mktemp -d ./test262-runner.XXXXXX)
cleanup_runner_fixture() {
    test262_exit=$?
    trap - EXIT HUP INT TERM
    if [ "$test262_exit" -ne 0 ]; then
        echo "runner regression failed with status $test262_exit" >&2
        echo "last invocation: ${test262_invocation:-not started}" >&2
        for test262_capture in stdout stderr out err expected actual; do
            if [ -f "$test262_tmp/$test262_capture" ]; then
                echo "--- $test262_capture ---" >&2
                sed -n '1,120p' "$test262_tmp/$test262_capture" >&2
            fi
        done
    fi
    rm -rf "$test262_tmp"
    exit "$test262_exit"
}
trap cleanup_runner_fixture EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
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
    test262_invocation="$test262_binary $*"
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

# Timings depend on host speed and do not describe test output. Match only
# the complete timing line emitted by run-test262 after a slow test.
normalize_report() {
    tr -d '\r' | sed '/^ time: [0-9][0-9]* ms$/d'
}

# Check normalization without relying on a test crossing the timing threshold.
cat > "$test262_tmp/normalization-expected" <<'REPORT'
runner 42 true null
 time: ms
 time: -100 ms
 time: +100 ms
 time: 100.0 ms
 time: 100 ms extra
time: 100 ms
  time: 100 ms
ordinary two
REPORT
{
    printf ' time: 100 ms\r\n'
    cat "$test262_tmp/normalization-expected"
    printf ' time: 271 ms\n'
} | normalize_report > "$test262_tmp/normalization-actual"
diff -u "$test262_tmp/normalization-expected" \
    "$test262_tmp/normalization-actual"

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
        normalize_report < "$test262_tmp/report" > "$test262_tmp/actual"
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
normalize_report < "$test262_tmp/report" > "$test262_tmp/actual"
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
        normalize_report < "$test262_tmp/report" > "$test262_tmp/actual"
        grep -Fqx 'Test262:AsyncTestFailure: runner async failure sentinel' "$test262_tmp/actual"
        grep -Fqx 'TypeError: $DONE() not called' "$test262_tmp/actual"
        grep -Fqx '  FAILED' "$test262_tmp/actual"
    else
        [ ! -e "$test262_tmp/report" ]
    fi
done

# Every fixture has the primary disabled feature; half also have the
# secondary feature. Each feature counts metadata occurrences, while the
# aggregate counts skipped files only once. Leave a third counter at zero.
mkdir "$test262_tmp/skipped"
cat > "$test262_tmp/skip-config" <<'CONFIG'
[config]
style=new
harnessdir=harness
[features]
runner-skip-primary=no
runner-skip-secondary=no
runner-skip-unused=no
CONFIG
test262_skip_cases=1024
test262_case=0
while [ "$test262_case" -lt "$test262_skip_cases" ]; do
    test262_features=runner-skip-primary
    if [ "$((test262_case % 2))" -eq 0 ]; then
        test262_features="$test262_features, runner-skip-secondary"
    fi
    printf '%s\n' '/*---' 'flags: [noStrict]' \
        "features: [$test262_features]" '---*/' \
        'throw new Error("disabled feature test body must not run");' \
        > "$test262_tmp/skipped/case-$test262_case.js"
    test262_case=$((test262_case + 1))
done
{
    printf '%-30s %7s\n' 'SKIPPED FEATURE' 'COUNT'
    printf '%-30s %7d\n' 'runner-skip-primary' "$test262_skip_cases"
    printf '%-30s %7d\n' 'runner-skip-secondary' "$((test262_skip_cases / 2))"
    printf '\n'
} > "$test262_tmp/expected-skipped-features"
printf 'Result: 0/0 errors, %d skipped\n' "$test262_skip_cases" \
    > "$test262_tmp/expected-skipped-result"

check_skipped_feature_counts() {
    invoke_runner no -q -c "$test262_tmp/skip-config" -T "$1" \
        --count_skipped_features -d "$test262_tmp/skipped"
    [ "$test262_status" -eq 0 ]
    diff -u "$test262_tmp/expected-skipped-features" "$test262_tmp/out"
    diff -u "$test262_tmp/expected-skipped-result" "$test262_tmp/err"
}

# Exact totals are required on every run. Repetition gives shared counters
# useful contention; failure of the former race depends on thread scheduling.
check_skipped_feature_counts 1
for test262_round in 1 2; do
    check_skipped_feature_counts 2
    check_skipped_feature_counts 8
done

# Disabling optional telemetry still skips the same files without a report.
invoke_runner no -q -c "$test262_tmp/skip-config" -T 8 \
    -d "$test262_tmp/skipped"
[ "$test262_status" -eq 0 ]
[ ! -s "$test262_tmp/out" ]
diff -u "$test262_tmp/expected-skipped-result" "$test262_tmp/err"

# Optional telemetry also works when no disabled features were configured.
# Reuse the ordinary suite and its feature-free config; no table is printed.
invoke_runner no -q -c "$test262_tmp/config" -T 2 \
    --count_skipped_features -d "$test262_tmp/suite"
[ "$test262_status" -eq 0 ]
[ ! -s "$test262_tmp/out" ]
printf '%s\n' 'Result: 0/3 errors' > "$test262_tmp/expected-empty-features"
diff -u "$test262_tmp/expected-empty-features" "$test262_tmp/err"

# Configured filenames are owned even when a later config or CLI overrides
# them. These successful exits also exercise their cleanup under LSan.
cat > "$test262_tmp/owned-config" <<'CONFIG'
[config]
style=new
harnessdir=harness
async=yes
errorfile=owned-errors
reportfile=owned-report
CONFIG
: > "$test262_tmp/owned-errors"
invoke_runner no -q -c "$test262_tmp/owned-config" -T 1 \
    -d "$test262_tmp/suite"
[ "$test262_status" -eq 0 ]
[ -f "$test262_tmp/owned-report" ]
grep -Fqx 'Result: 0/3 errors' "$test262_tmp/err"

rm -f "$test262_tmp/owned-report"
invoke_runner no -q -c "$test262_tmp/owned-config" \
    -c "$test262_tmp/owned-config" -e "$test262_tmp/owned-errors" \
    -e "$test262_tmp/owned-errors" -r "$test262_tmp/owned-report" \
    -r "$test262_tmp/owned-report" -T 1 -d "$test262_tmp/suite"
[ "$test262_status" -eq 0 ]
[ -f "$test262_tmp/owned-report" ]
grep -Fqx 'Result: 0/3 errors' "$test262_tmp/err"

# An empty initial directory must not pass a NULL list to qsort. Adding a
# real suite afterward still selects all cases and produces its summary.
mkdir "$test262_tmp/empty"
invoke_runner no -q -c "$test262_tmp/config" -d "$test262_tmp/empty" \
    -d "$test262_tmp/suite" -T 2
[ "$test262_status" -eq 0 ]
grep -Fqx 'Result: 0/3 errors' "$test262_tmp/err"
