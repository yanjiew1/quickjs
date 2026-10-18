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

# Check runner diagnostics using a small local harness, without Test262.
set -eu
test262_runner=${MESON_EXE_WRAPPER:-}
test262_binary=${1:-./run-test262}
test262_temporal=${2:-n}
case "$test262_binary" in
    /*) ;;
    *) test262_binary="$(pwd)/$test262_binary" ;;
esac
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

for invalid_threads in -1 invalid 1025 999999999999999999999; do
    invoke_runner no -q -c "$test262_tmp/config" -T "$invalid_threads" \
        -d "$test262_tmp/failure"
    [ "$test262_status" -eq 2 ]
    grep -Fq 'invalid thread count' "$test262_tmp/err"
done

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
        object)
            printf '%s\n' 'throw { toString() { return "runner harness object sentinel"; } };' \
                > "$test262_tmp/harness/assert.js"
            test262_expected='Throw: runner harness object sentinel'
            ;;
        symbol)
            printf '%s\n' 'throw Symbol("runner harness symbol");' \
                > "$test262_tmp/harness/assert.js"
            test262_expected='Throw: [exception]'
            ;;
        coercion)
            printf '%s\n' 'throw { toString() { throw 42; } };' \
                > "$test262_tmp/harness/assert.js"
            test262_expected='Throw: [exception]'
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
for test262_kind in object symbol coercion; do
    check_harness_failure 1 "$test262_kind"
    check_harness_failure 2 "$test262_kind"
done

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
throw {constructor: TypeError, toString() { return "TypeError: ordinary throw sentinel"; }};
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

# Unexpected thrown values can themselves fail string conversion. Report the
# negative-test mismatch without passing NULL into the exception-name parser.
mkdir "$test262_tmp/unprintable"
for test262_kind in symbol coercion; do
    cat > "$test262_tmp/unprintable/$test262_kind.js" <<'JS'
/*---
flags: [noStrict]
negative:
  phase: runtime
  type: TypeError
---*/
JS
    if [ "$test262_kind" = symbol ]; then
        printf '%s\n' 'throw Symbol("unexpected negative exception");' \
            >> "$test262_tmp/unprintable/$test262_kind.js"
    else
        printf '%s\n' 'throw { toString() { throw 42; } };' \
            >> "$test262_tmp/unprintable/$test262_kind.js"
    fi
done
for test262_threads in 1 2; do
    invoke_runner no -q -v -c "$test262_tmp/config" -T "$test262_threads" \
        -d "$test262_tmp/unprintable"
    [ "$test262_status" -eq 1 ]
    grep -Fqx 'Result: 2/2 errors' "$test262_tmp/err"
done

# Diagnostic ToString must preserve useful messages, tolerate hostile Error
# properties and never count an agent's error text as an async completion.
mkdir "$test262_tmp/diagnostics"
cat > "$test262_tmp/diagnostics/object.js" <<'JS'
/*---
flags: [noStrict]
---*/
function Test262Error(message) { this.message = message; }
Test262Error.prototype.toString = function () { return "Test262Error: " + this.message; };
throw new Test262Error("runner object sentinel");
JS
cat > "$test262_tmp/diagnostics/undefined.js" <<'JS'
/*---
flags: [noStrict]
---*/
throw undefined;
JS
cat > "$test262_tmp/diagnostics/properties.js" <<'JS'
/*---
flags: [noStrict]
---*/
const error = new Error("runner original error");
Object.defineProperty(error, "name", { get() { throw 42; } });
Object.defineProperty(error, "stack", { get() { throw 43; } });
throw error;
JS
cat > "$test262_tmp/diagnostics/completion-lookalike.js" <<'JS'
/*---
flags: [noStrict, async]
---*/
$262.agent.start('throw { toString() { return "Test262:AsyncTestComplete"; } };');
// Broadcast waits for this agent's departure, after its diagnostic is written.
$262.agent.broadcast(new SharedArrayBuffer(4), 0);
JS
for test262_threads in 1 2; do
    rm -f "$test262_tmp/report"
    invoke_runner no -q -v -c "$test262_tmp/config" -T "$test262_threads" \
        -r "$test262_tmp/report" -d "$test262_tmp/diagnostics"
    [ "$test262_status" -eq 1 ]
    grep -Fqx 'Result: 4/4 errors' "$test262_tmp/err"
    grep -Fq 'Test262Error: runner object sentinel' "$test262_tmp/out"
    grep -Fq 'TypeError: $DONE() not called' "$test262_tmp/out"
    if [ "$test262_threads" -eq 1 ]; then
        grep -Fqx 'Throw: Test262Error: runner object sentinel' "$test262_tmp/report"
        grep -Fqx 'Throw: undefined' "$test262_tmp/report"
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
# Flushed execution records remain separate from the exact result streams.
invoke_runner no -q -c "$test262_tmp/config" -T 2 \
    --progress-log "$test262_tmp/execution.jsonl" -d "$test262_tmp/suite"
[ "$test262_status" -eq 0 ]
[ ! -s "$test262_tmp/out" ]
diff -u "$test262_tmp/expected-empty-features" "$test262_tmp/err"
python3 - "$test262_tmp/execution.jsonl" <<'PY'
import json, sys
with open(sys.argv[1]) as stream:
    records = [json.loads(line) for line in stream]
assert len(records) == 6
by_index = {}
for item in records:
    assert set(item) == {'phase', 'thread', 'index', 'file'}
    assert item['thread'] in (0, 1)
    assert item['file'].endswith('.js')
    by_index.setdefault(item['index'], []).append(item)
assert set(by_index) == {0, 1, 2}
for pair in by_index.values():
    assert [item['phase'] for item in pair] == ['start', 'finish']
    assert pair[0]['file'] == pair[1]['file']
    assert pair[0]['thread'] == pair[1]['thread']
PY

# Every executed variant has an independent wall-time record, even in parallel.
mkdir "$test262_tmp/timed"
cat > "$test262_tmp/timed/both.js" <<'JS'
/*---
---*/
if (1 + 1 !== 2) throw new Error('arithmetic');
JS
cat > "$test262_tmp/timed/strict.js" <<'JS'
/*---
flags: [onlyStrict]
---*/
if (function () { return this; }() !== undefined) throw new Error('strict');
JS
cat > "$test262_tmp/timed/module.js" <<'JS'
/*---
flags: [module]
---*/
export const value = 1;
JS
cat > "$test262_tmp/timed/negative.js" <<'JS'
/*---
negative:
  phase: runtime
  type: TypeError
---*/
throw new TypeError('expected');
JS
cat > "$test262_tmp/timed/failure.js" <<'JS'
/*---
---*/
throw new Error('timed failure');
JS
cat > "$test262_tmp/timed/skipped.js" <<'JS'
/*---
features: [timing-unsupported]
---*/
throw new Error('skipped body ran');
JS
# Deliberately configure a different suite: long options precede -d.
cat > "$test262_tmp/timing-config" <<'CONFIG'
[config]
style=new
harnessdir=harness
verbose=yes
testdir=failure
[features]
timing-unsupported=skip
CONFIG
for test262_threads in 1 2; do
    invoke_runner no --timing-log "$test262_tmp/timings-$test262_threads.jsonl" \
        --progress-log "$test262_tmp/progress-$test262_threads.jsonl" \
        -q -c "$test262_tmp/timing-config" -T "$test262_threads" -a \
        -d "$test262_tmp/timed"
    [ "$test262_status" -eq 1 ]
    grep -Fqx 'Result: 1/5 errors, 1 skipped' "$test262_tmp/err"
done
python3 - "$test262_tmp" <<'PY'
import json, math, os, sys
expected = {
    ('both.js', 'non-strict', 'pass'), ('both.js', 'strict', 'pass'),
    ('strict.js', 'strict', 'pass'), ('module.js', 'module', 'pass'),
    ('negative.js', 'non-strict', 'pass'), ('negative.js', 'strict', 'pass'),
    ('failure.js', 'non-strict', 'fail'), ('skipped.js', 'none', 'skipped'),
}
for workers in (1, 2):
    with open(os.path.join(sys.argv[1], 'timings-%d.jsonl' % workers)) as stream:
        rows = [json.loads(line) for line in stream]
    assert len(rows) == len(expected)
    assert {(os.path.basename(r['file']), r['mode'], r['status']) for r in rows} == expected
    for row in rows:
        assert set(row) == {'file', 'thread', 'index', 'mode', 'status', 'duration_ms'}
        assert 0 <= row['thread'] < workers
        assert 0 <= row['index'] < 6
        assert math.isfinite(row['duration_ms']) and row['duration_ms'] >= 0
        if row['status'] == 'skipped':
            assert row['duration_ms'] == 0
    with open(os.path.join(sys.argv[1], 'progress-%d.jsonl' % workers)) as stream:
        progress = [json.loads(line) for line in stream]
    assert len(progress) == 12
    assert {r['file'] for r in progress} == {r['file'] for r in rows}
PY

# Exclude the shared DST stress selection without executing expensive tests.
tr -d '\r' < test262-slow.txt > "$test262_tmp/slow.txt"
: > "$test262_tmp/harness/assert.js"
: > "$test262_tmp/harness/sta.js"
while IFS= read -r test262_slow_path; do
    mkdir -p "$test262_tmp/$(dirname "$test262_slow_path")"
    cat > "$test262_tmp/$test262_slow_path" <<'JS'
/*---
flags: [noStrict]
---*/
throw new Error("excluded DST stand-in must never execute");
JS
done < "$test262_tmp/slow.txt"
cat > "$test262_tmp/test262/test/pass.js" <<'JS'
/*---
flags: [noStrict]
---*/
true;
JS
cat > "$test262_tmp/dst-config" <<'CONFIG'
[config]
style=new
harnessdir=harness
excludefile=slow.txt
CONFIG
invoke_runner no -q -c "$test262_tmp/dst-config" -T 2 -a -d "$test262_tmp/test262/test"
test "$test262_status" -eq 0
grep -Fx 'Result: 0/1 error, 8 excluded' "$test262_tmp/err"

# Build-selected Temporal coverage must execute in enabled builds and skip
# only when the runner really lacks the feature.
cat > "$test262_tmp/temporal-config" <<'CONFIG'
[config]
style=new
harnessdir=harness
[features]
Temporal=auto
CONFIG
cat > "$test262_tmp/temporal.js" <<'JS'
/*---
features: [Temporal]
flags: [noStrict]
---*/
if (typeof Temporal !== "object") throw Error("Temporal was not installed");
JS
invoke_runner no -q -c "$test262_tmp/temporal-config" \
    --timing-log "$test262_tmp/temporal.jsonl" -f "$test262_tmp/temporal.js"
test "$test262_status" -eq 0
python3 - "$test262_tmp/temporal.jsonl" "$test262_temporal" <<'PYCODE'
import json, sys
with open(sys.argv[1]) as stream:
    rows = [json.loads(line) for line in stream]
assert len(rows) == 1
assert rows[0]['status'] == ('pass' if sys.argv[2] == 'y' else 'skipped')
PYCODE

# Path capabilities replace a broad rule at a narrower directory or exact
# file. Metadata requirements remain independent and conjunctive.
mkdir -p "$test262_tmp/requirements/narrow/format" "$test262_tmp/requirements/sibling" \
    "$test262_tmp/requirements/narrow-extra"
cat > "$test262_tmp/requirements-config" <<'CONFIG'
[config]
style=new
harnessdir=harness
[features]
available
unavailable=skip
[requirements]
requirements/ = unavailable
requirements/narrow/ = available
requirements/narrow/format/ = available unavailable
requirements/exact.js = available
CONFIG
for test262_path in narrow/pass.js exact.js; do
    cat > "$test262_tmp/requirements/$test262_path" <<'JS'
/*---
flags: [noStrict]
features: [available]
---*/
true;
JS
done
# A multi-capability rule must check its second feature even when metadata
# is supported. Exact file rules must not match a longer sibling filename.
for test262_path in narrow/format/skip.js exact.js-extra.js narrow-extra/skip.js; do
    cat > "$test262_tmp/requirements/$test262_path" <<'JS'
/*---
flags: [noStrict]
features: [available]
includes: [must-not-be-loaded.js]
---*/
throw Error("path capability or matching boundary was ignored");
JS
done
for test262_path in skip.js sibling/skip.js narrow/metadata.js; do
    cat > "$test262_tmp/requirements/$test262_path" <<'JS'
/*---
flags: [noStrict]
features: [unavailable]
includes: [must-not-be-loaded.js]
---*/
throw Error("missing path or metadata capability must skip before execution");
JS
done
# This one has no metadata feature, so only the broad path rule prevents it.
cat > "$test262_tmp/requirements/path-only.js" <<'JS'
/*---
flags: [noStrict]
includes: [must-not-be-loaded.js]
---*/
throw Error("path-only capability was ignored");
JS
for test262_threads in 1 2; do
    invoke_runner no -q -c "$test262_tmp/requirements-config" -T "$test262_threads" \
        --timing-log "$test262_tmp/requirements.jsonl" -d "$test262_tmp/requirements"
    test "$test262_status" -eq 0
    python3 - "$test262_tmp/requirements.jsonl" <<'PYCODE'
import json, sys
with open(sys.argv[1]) as stream:
    rows = [json.loads(line) for line in stream]
assert len(rows) == 9
assert sum(row['status'] == 'pass' for row in rows) == 2
assert sum(row['status'] == 'skipped' for row in rows) == 7
for row in rows:
    if row['status'] == 'skipped':
        assert row['duration_ms'] == 0
PYCODE
done
# Normalize config/test path spellings, including an absolute single test.
invoke_runner no -q -c "$test262_tmp/./requirements-config" -T 2 \
    --timing-log "$test262_tmp/requirements-absolute.jsonl" \
    -d "$(pwd)/$test262_tmp/requirements/../requirements"
test "$test262_status" -eq 0
python3 - "$test262_tmp/requirements-absolute.jsonl" <<'PYCODE'
import json, sys
with open(sys.argv[1]) as stream:
    rows = [json.loads(line) for line in stream]
assert len(rows) == 9
assert sum(row['status'] == 'pass' for row in rows) == 2
assert sum(row['status'] == 'skipped' for row in rows) == 7
PYCODE
invoke_runner no -q -c "$test262_tmp/requirements-config" \
    -f "$(pwd)/$test262_tmp/requirements/exact.js"
test "$test262_status" -eq 0
# An overlay in a child directory must match a root config's test spelling.
# This is how reduced-locale capabilities qualify untagged staging tests.
mkdir "$test262_tmp/overlays"
cat > "$test262_tmp/overlays/locale-config" <<'CONFIG'
[requirements]
../requirements/exact.js = unavailable
CONFIG
invoke_runner no -q -c "$test262_tmp/requirements-config" \
    -c "$test262_tmp/overlays/locale-config" \
    --timing-log "$test262_tmp/overlay.jsonl" \
    -f "$(pwd)/$test262_tmp/requirements/exact.js"
test "$test262_status" -eq 0
python3 - "$test262_tmp/overlay.jsonl" <<'PYCODE'
import json, sys
with open(sys.argv[1]) as stream:
    rows = [json.loads(line) for line in stream]
assert len(rows) == 1 and rows[0]['status'] == 'skipped'
assert rows[0]['duration_ms'] == 0
PYCODE
# A misspelled capability must report a config error rather than hiding tests.
cat >> "$test262_tmp/requirements-config" <<'CONFIG'
requirements/typo.js = missing-capability
CONFIG
invoke_runner no -q -c "$test262_tmp/requirements-config" -d "$test262_tmp/requirements"
test "$test262_status" -eq 2
grep -Fq 'unknown required feature missing-capability' "$test262_tmp/err"

# A raw script must execute once without even the default harness files.
cat > "$test262_tmp/harness/assert.js" <<'JS'
var injectedHarness = true;
JS
cat > "$test262_tmp/raw.js" <<'JS'
/*---
flags: [raw]
---*/
if (typeof injectedHarness !== "undefined")
    throw new Error("raw script received harness globals");
JS
invoke_runner no -q -v -c "$test262_tmp/config" -f "$test262_tmp/raw.js"
test "$test262_status" -eq 0

# The same error type in a different phase must not satisfy a negative test.
mkdir "$test262_tmp/phases"
cat > "$test262_tmp/phases/dep.js" <<'JS'
export const present = 1;
JS
cat > "$test262_tmp/phases/broken.js" <<'JS'
export const = 1;
JS
for test262_actual_phase in parse runtime resolution imported-parse module-runtime; do
    for test262_expected_phase in parse runtime resolution; do
        test262_flags=noStrict
        case "$test262_actual_phase" in
            parse) test262_source='function {'; test262_phase=parse ;;
            runtime) test262_source='throw new SyntaxError("runtime");'; test262_phase=runtime ;;
            resolution) test262_source='import { missing } from "./dep.js";'; test262_flags=module; test262_phase=resolution ;;
            imported-parse) test262_source='import "./broken.js";'; test262_flags=module; test262_phase=resolution ;;
            module-runtime) test262_source='throw new SyntaxError("runtime");'; test262_flags=module; test262_phase=runtime ;;
        esac
        cat > "$test262_tmp/phases/case.js" <<JS
/*---
flags: [$test262_flags]
negative:
  phase: $test262_expected_phase
  type: SyntaxError
---*/
$test262_source
JS
        invoke_runner no -q -v -c "$test262_tmp/config" -f "$test262_tmp/phases/case.js"
        if [ "$test262_phase" = "$test262_expected_phase" ]; then
            test "$test262_status" -eq 0
        else
            test "$test262_status" -eq 1
        fi
    done
done

# Harness failures must not pass as a test's expected TypeError.
for test262_flags in async module; do
    if [ "$test262_flags" = async ]; then
        test262_source='void 0;'
    else
        test262_source='await new Promise(() => {});'
    fi
    cat > "$test262_tmp/phases/incomplete.js" <<JS
/*---
flags: [$test262_flags]
negative:
  phase: runtime
  type: TypeError
---*/
$test262_source
JS
    invoke_runner no -q -v -c "$test262_tmp/config" -f "$test262_tmp/phases/incomplete.js"
    test "$test262_status" -eq 1
done
cat > "$test262_tmp/phases/async-module.js" <<'JS'
/*---
flags: [module, async]
---*/
$DONE();
throw new Error("module evaluation must also succeed");
JS
invoke_runner no -q -v -c "$test262_tmp/config" -f "$test262_tmp/phases/async-module.js"
test "$test262_status" -eq 1

# The expected type is the thrown value's constructor, not its text/name.
for test262_source in 'throw "TypeError: spoofed";' \
    'throw Object.assign(new Error("wrong constructor"), {name:"TypeError"});'; do
    cat > "$test262_tmp/phases/wrong-constructor.js" <<JS
/*---
negative:
  phase: runtime
  type: TypeError
---*/
$test262_source
JS
    invoke_runner no -q -v -c "$test262_tmp/config" -f "$test262_tmp/phases/wrong-constructor.js"
    test "$test262_status" -eq 1
done

# Quiet output still needs a failing process status and baseline accounting.
cat > "$test262_tmp/phases/quiet-failure.js" <<'JS'
/*---
flags: [noStrict]
---*/
throw new Error("quiet failure");
JS
invoke_runner no -q -c "$test262_tmp/config" -f "$test262_tmp/phases/quiet-failure.js"
test "$test262_status" -eq 1

# A resolution-negative test may link, but must never run its module body.
cat > "$test262_tmp/phases/no-evaluation.js" <<'JS'
/*---
flags: [module]
negative:
  phase: resolution
  type: SyntaxError
---*/
print("RESOLUTION BODY RAN");
JS
invoke_runner no -q -v -c "$test262_tmp/config" -f "$test262_tmp/phases/no-evaluation.js"
test "$test262_status" -eq 1
! grep -Fq 'RESOLUTION BODY RAN' "$test262_tmp/out"

# Fixture names may contain a suffix after the marker. A directory name
# containing the marker must not hide ordinary tests inside that directory.
mkdir "$test262_tmp/fixture-cases" "$test262_tmp/fixture-cases/dir_FIXTURE"
for test262_fixture in example_FIXTURE.js example_FIXTURE_more.js; do
    printf '%s\n' 'throw new Error("fixture executed");' > "$test262_tmp/fixture-cases/$test262_fixture"
done
cat > "$test262_tmp/fixture-cases/dir_FIXTURE/ordinary.js" <<'JS'
/*---
flags: [noStrict]
---*/
print("ordinary fixture-directory test ran");
JS
invoke_runner no -q -v -c "$test262_tmp/config" --timing-log "$test262_tmp/fixture-dir-timing.jsonl" \
    -d "$test262_tmp/fixture-cases"
test "$test262_status" -eq 0
python3 - "$test262_tmp/fixture-dir-timing.jsonl" <<'PYCODE'
import json, sys
with open(sys.argv[1]) as stream:
    rows = [json.loads(line) for line in stream]
assert len(rows) == 1 and rows[0]['status'] == 'pass'
assert rows[0]['file'].replace('\\', '/').endswith('/dir_FIXTURE/ordinary.js')
PYCODE

# Invalid metadata proves explicit fixture selection does not even parse it.
cat > "$test262_tmp/fixture-cases/example_FIXTURE_more.js" <<'JS'
/*---
negative:
  phase: unknown-phase
  type: Error
---*/
throw new Error("fixture executed");
JS
# Explicit file selection must not execute fixtures either.
invoke_runner no -q -v -c "$test262_tmp/config" --timing-log "$test262_tmp/fixture-timing.jsonl" \
    -f "$test262_tmp/fixture-cases/example_FIXTURE_more.js"
test "$test262_status" -eq 0
python3 - "$test262_tmp/fixture-timing.jsonl" <<'PYCODE'
import json, sys
with open(sys.argv[1]) as stream:
    rows = [json.loads(line) for line in stream]
assert len(rows) == 1 and rows[0]['status'] == 'skipped'
PYCODE
