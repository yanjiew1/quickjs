# Informational CI benchmarks

The `benchmarks` workflow compares the exact candidate commit with its actual
immediate first parent. The runner independently verifies this relationship.
GCC and Clang each build both
revisions using their own checked out Makefile with `-O2`, no LTO, and no
sanitizers. Complete build commands, compiler flags, binary checksums, and
compiler/linker versions are retained with the measurements. Compiler
versions come from the selected Ubuntu runner image and are reported.

On `main`, routine benchmarks run when a push crosses a multiple of 25
first-parent commits, and earlier when changed runtime, build, or workload
paths anywhere in `github.event.before..candidate` indicate likely performance
impact. Only an exact, available before revision on the candidate's first-parent
history with a lower positive commit count permits skipping. New branches,
missing revisions, nonancestor revisions, and rewritten history conservatively
run. This push range selects work; the measured baseline remains candidate
`HEAD^1`. Other pushes create a lightweight selection job. Selected pushes run
one paired round of all 10 V8 v7 cases and all 72 frozen QuickJS microbenchmarks.
Manual dispatch always runs and offers 1 or 3 to 6 rounds, Zoo alone,
all three historical suites, or the separate optional native suite. Zoo runs exactly
10 paired rounds of 14 workloads producing 16 scores; zlib is excluded.
This means 10 samples per engine per Zoo score, rather than 10 engine
invocations total. The hosted default is intentionally a noisy reference.
Local measurements continue to use their separate controlled policy.

Each workload invocation starts a fresh JavaScript process. Parent and
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
  --parent /path/to/parent --parent-revision EXACT_PARENT_HASH \
  --compiler gcc --suite core --rounds 3 \
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

## Optional Intl and Temporal catalog v1

`optional-intl-temporal-v1.json` describes six service workloads in
`tests/microbench.js`. Select them explicitly with `--optional-v1`; an
invocation without this flag retains the legacy workload list. The optional
mode runs available services only and fails if none are available. Explicit
case names match exactly and fail if unavailable. Constructors and bound
method getters run before timing. Segmenter reports nanoseconds per complete
traversal of its fixed text; the other cases report nanoseconds per call.

Run each service in a fresh JavaScript process and a separate working
directory, for example with absolute paths:

```sh
/path/to/native/qjs --std /path/to/source/tests/microbench.js \
  --optional-v1 intl_number_format -s /path/to/native-number.json
/path/to/icu/qjs --std /path/to/source/tests/microbench.js \
  --optional-v1 intl_number_format -s /path/to/icu-number.json
```

For a separate native-versus-ICU experiment, use matching compiler, inputs,
rounds, and clock, and retain raw JSON and process logs. The hosted optional
suite instead compares two exact revisions of the same enabled native profile.
Optional mode reads reference scores only when `-r` is supplied and writes
`microbench-optional-v1-new.txt` by default. A selected optional case can
save its score with `-s`; JSON keys and table columns retain the existing
harness format.
These workloads are separate from the frozen 72-case manifest and its
historical aggregate. Core, Zoo, and all modes continue to use the historical fixtures. The Intl backend does not imply a distinct
implementation for every Temporal path; label Temporal results by the
actual build configuration.

## Hosted enabled native comparison

Dispatch `benchmarks` with `suite=optional` and an optional exact 40-character
`candidate_revision`. Blank selects the workflow commit. The workflow checks
out its own tooling revision separately, so current tooling can measure an
earlier correction commit against that correction's actual parent. The
parent input is derived automatically and cannot be overridden. Pushing a
branch under `scratch/benchmarks/native/` selects this suite automatically;
other `scratch/benchmarks/` branches follow the core selection policy.

The same runner builds both pristine checkouts with explicit
`CONFIG_INTL=y CONFIG_TEMPORAL=y CONFIG_INTL_BACKEND=native CONFIG_ICU=n`,
`-O2`, and no LTO. Ordinary historical modes explicitly set both frontends
and ICU to `n`. Command-line make assignments override inherited MAKEFLAGS;
there is no global disabled-feature workflow environment. Native profile
checks reject frontend compilation without the expected enabled defines or
with ICU enabled. No ICU dependency installation is needed for this scope.
Builds use the checkouts' committed data; the tool does not invoke source-data
generators or overwrite tracked data. The existing post-build tracked-tree
check rejects unexpected modifications. Timezone build products remain under
`.obj`; use fresh checkouts, never a source tree carrying earlier products.

Both engines execute the same copied and hashed optional script from the
exact tooling checkout. All six v1 cases are required; missing services or
selected results fail instead of becoming a smaller aggregate. Each case and
round has a fresh process and working directory, and pair order alternates.
The era-free Gregorian, latn, UTC `intl_date_time_format` case measures format
calls over four dates, with formatter setup outside timing; it is the existing
focused format-bank workload. It does not cover all calendars, era requests,
or constructor cost. The six cases retain a separate geometric mean and
individual medians, raw JSON/CSV samples and logs, feature flags, exact parent,
candidate and tooling identities, and the same observed hardware/software
reports as the historical suites. Hosted noise still applies; no speed result
is a correctness gate. Prepared source or an unrun workflow establishes no
performance result.

For root-owned execution from a complete source checkout:

```sh
python3 tools/benchmarks/run.py \
  --candidate /path/to/correction --candidate-revision EXACT_CORRECTION_HASH \
  --parent /path/to/immediate-parent --parent-revision EXACT_PARENT_HASH \
  --compiler gcc --suite optional --rounds 3 \
  --cpu auto --output /path/to/new-native-report
```

The manifest's `upstream_revision` remains historical fixture provenance and
no longer selects a comparison engine. The mock unit suite checks actual-parent
rejection, cadence/impact selection, explicit enabled profiles, missing compile
defines, shared optional workload bytes, individual scores, and separate
optional aggregates without timing an engine.
