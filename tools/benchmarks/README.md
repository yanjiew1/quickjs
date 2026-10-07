# Informational CI benchmarks

The `benchmarks` workflow compares the exact pushed commit with upstream
`535a7c250ff4a577ec36c3e103daab6dadeea650`. GCC and Clang each build both
revisions using their own checked out Makefile with `-O2`, no LTO, and no
sanitizers. Complete build commands, compiler flags, binary checksums, and
compiler/linker versions are retained with the measurements. Compiler
versions come from the selected Ubuntu runner image and are reported.

Every push to `main` runs one paired round of all 10 V8 v7 cases and all
72 frozen QuickJS microbenchmarks. The manual workflow offers 3 to 6
rounds, and can select Zoo alone or all three suites. Zoo runs exactly
10 paired rounds of 14 workloads producing 16 scores; zlib is excluded.
This means 10 samples per engine per Zoo score, rather than 10 engine
invocations total. The hosted default is intentionally a noisy reference.
Local measurements continue to use their separate controlled policy.

Each workload invocation starts a fresh JavaScript process. Upstream and
candidate order alternates by case and round; each run has a separate
working directory. V8 preserves its original warmup/workload rules and
reports microseconds per iteration. The microbenchmarks report nanoseconds
per operation using `os.now`. Zoo reports the original named Octane
scores. V8 and microbenchmarks use statistical medians; Zoo uses the upper
middle sample, matching its pinned `harness/compare.py -m` convention.
Suite comparisons use the geometric mean of candidate speed ratios.
Positive percentages mean the candidate is faster.

The driver selects one CPU from its actual allowed affinity set when
supported. Pinning does not grant exclusive access to a CPU. Turbo,
ASLR, frequency governors, and host scheduling are unchanged. Reports
include observed CPU model/topology, allowed CPUs and affinity, frequency
and Turbo controls or their absence, virtualization, memory, OS/kernel,
Python, compiler/linker, exact revisions, build flags, clocks, sample
counts, raw values and units. Missing observations are marked unavailable.
The full environment is captured before the build, before measurement,
and after measurement.

Hosted runners can share physical CPUs with other work, vary between
machines, and change frequency or scheduling during a run. A single
hosted sample cannot establish a performance regression. There are no
speed thresholds or pass/fail performance gates. Build errors, workload
errors, invalid/missing scores, checksum mismatches, and timeouts fail
normally and preserve available diagnostics.

## Fixtures and attribution

`manifest.json` freezes the catalog, hashes, and provenance. The V8 v7
fixture is assembled from the checksum-verified upstream originals in
`tests/bench-v8`, followed by the small CI runner in
`fixtures/v8-runner.js`. Its complete SHA-256 must match the existing
local fresh-process harness exactly. The upstream extras import must
precede this tooling commit; a second copy of the original workload
data is unnecessary. Individual copyright and license notices remain
intact in the assembled JavaScript, and `tests/bench-v8/README.txt`
describes the suite. The upstream `run_harness.js` is unchanged. The CI
runner selects exactly one original case, prepares Crypto Decrypt
ciphertext outside measurement, and adds machine-readable results
without changing the measured workload.

The archive's prebuilt `combined.js` predates the `"use strip";` line in
its separate `base.js`. The driver deliberately assembles the canonical
separate originals, as the local recovery harness does, rather than
silently substituting that older generated artifact.

The microbenchmark fixture preserves the QuickJS MIT notices and the
existing local 72-case workload. Two runner changes select an exact case
name and permit saving that selected case's results. The original source
revision and checksum are recorded. Both engines execute this identical
fixture, even if a later repository commit adds new microbenchmarks.
New cases should be added to a separate versioned catalog so historical
comparisons remain meaningful.

Zoo's self-contained files are fetched only when requested, from the
exact `ivankra/javascript-zoo` revision in the manifest. Every download
has a SHA-256 check before execution. The original source notices remain
untouched, and the downloaded upstream MIT license is preserved alongside
them. `fixtures/ZOO-LICENSE.txt` also preserves Ivan Krasilnikov's license
for the Zoo harness conventions. The driver uses Python's standard library
and does not download or import Zoo's Python dependencies. It reproduces
the pinned harness's named-score parsing and median convention rather
than presenting its report as an official Zoo harness invocation.

## Running locally

Use two clean, distinct source checkouts and a new output directory:

```sh
python3 tools/benchmarks/run.py \
  --candidate /path/to/candidate --candidate-revision EXACT_COMMIT_HASH \
  --upstream /path/to/upstream --compiler gcc --suite core --rounds 3 \
  --cpu auto --output /path/to/new-report
```

The tool checks both revisions and hashes the resulting binaries before
and after measurement. It also refuses existing `.obj`, engine/compiler
binaries, generated `repl.c`, root objects/archives/dependencies, and
untracked compatibility headers before invoking make. A clean tracked
tree is insufficient when ignored products from another compiler remain.
The tool never deletes these files; use new pristine checkouts instead.
Existing output directories are rejected so a
partial or previous measurement cannot silently become a new result.
Fixture files, raw per-process logs, JSON/CSV samples, JSON environment,
build logs, and a Markdown summary are uploaded as workflow artifacts.

SIGTERM and interruption requests unwind through child process-group
cleanup on POSIX and save the available failure state and reports. A
printed sort ordering error is a workload failure even if the frozen
microbenchmark also saves a positive numeric score.

The small Python unit suite checks parsers, printed sort failures,
corruption rejection, catalog coverage, incomplete sample rejection,
stale product rejection without deletion, cancellation/timeout child
cleanup, speed direction, geometric mean, and the distinct Zoo median
contract without timing an engine:

```sh
python3 -m unittest discover -s tools/benchmarks -p 'test_*.py'
```
