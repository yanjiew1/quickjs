<!--
Copyright (c) 2017-2021 Fabrice Bellard
Copyright (c) 2017-2021 Charlie Gordon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

# Informational CI benchmarks

The benchmark tooling requires Python 3.8 or newer. Meson and Ninja are needed
when either revision uses Meson; historical Make revisions still require Make.

The `benchmarks` workflow compares the exact candidate commit separately with
both its actual immediate first parent and original Bellard QuickJS. The default
Bellard reference is the fixed fork upstream base
`535a7c250ff4a577ec36c3e103daab6dadeea650` from
`https://github.com/bellard/quickjs.git` (2026-09-29); it is not a claim about
current upstream HEAD. Manual dispatch can select a different exact Bellard SHA.
The runner verifies the first-parent relationship for parent comparisons and the
original repository origin for Bellard comparisons. Reports record the baseline
kind, source repository and exact SHA.

The compiler/baseline matrix runs four independent paired comparisons. GCC and
Clang each build candidate and baseline using their own checked out build
definitions (Meson, or Make for historical parents and the pinned Bellard base)
with `-O2`, assertions disabled (`-DNDEBUG`), no LTO, and no sanitizers. Meson uses `-Db_ndebug=true`; historical Make builds append
`-DNDEBUG` to the environment's `CFLAGS`, preserving the Makefile's defaults.
The driver verifies that every produced runtime object's compile command
contains `-DNDEBUG` and does not undefine it. Complete build commands,
compiler flags, binary checksums, and
compiler/linker versions are retained with the measurements. Compiler
versions come from the selected Ubuntu runner image and are reported.
Feature defines are reported separately for baseline and candidate from their
actual compile commands. Meson builds retain each revision's defaults; the
legacy Make feature request is recorded separately and is not evidence of a
Meson build's configuration.

Pushes to the latest formal branch tips (`main`, `main-new`, `temporal`, `intl`)
always run both comparisons. Every `scratch-verify-*` push runs the lightweight
selection job. Scratch measurements run when a push crosses a multiple of 25
first-parent commits, and earlier when changed runtime, build, or workload
paths anywhere in `github.event.before..candidate` indicate likely performance
impact. A newly published scratch branch has a zero before SHA; selection then
uses the candidate's immediate-parent tree difference and an exact multiple of
25, rather than measuring merely because the branch is new. For other uncertain,
missing, nonancestor or rewritten push histories, selection remains conservative.
This push range selects work; the measured parent baseline remains candidate
`HEAD^1`. Selected pushes run
one paired round of all 10 V8 v7 cases and all 72 frozen QuickJS microbenchmarks.
Manual dispatch always runs and offers 1 or 3 to 6 rounds, Zoo alone,
all three historical suites. Zoo runs exactly
10 paired rounds of 14 workloads producing 16 scores; zlib is excluded.
This means 10 samples per engine per Zoo score, rather than 10 engine
invocations total. The hosted default is intentionally a noisy reference.
Local measurements continue to use their separate controlled policy.

Each workload invocation starts a fresh JavaScript process. Baseline and
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

The live `string_normalize_short` and `string_normalize_long` cases contrast
ordinary short NFC input with a disordered nonstarter NFD run. Each caches its
source string on first use and reports normalization operations. Run the same
updated script on both engines; these cases do not change the frozen 72-case
catalog. Already ordered and ASCII inputs are useful additional controls when
evaluating changes to canonical ordering.

The live repository microbenchmark has `intl_case_default`, `intl_case_lower`
and `intl_case_upper` for validated locale negotiation and Turkish/Lithuanian
Unicode contexts. These require an Intl-enabled build. Run each exact name in
a fresh process, for example `build/qjs --std tests/microbench.js intl_case_lower`,
using an unsanitized build with assertions disabled. They do not modify the
frozen comparison catalog.

The live repository microbenchmark additionally offers the `intl_number`
prefix for constructor, bound numeric formatting, exact decimal/BigInt inputs,
currency parts, ranges and compact division-unit formatting. These optional
cases require NumberFormat and do not modify the frozen 72-case comparison
fixture. For example, run `build/qjs --std tests/microbench.js intl_number` from
the source directory using an unsanitized build with assertions disabled.

The live `intl_collation` cases separate constructor cost, cached bound
comparison, numeric runs and canonical equivalence from explicit/default-locale
`String.prototype.localeCompare` calls. Select each exact name in a fresh
process with an unsanitized, non-LTO, assertion-disabled Intl build. The bound
cases require Collator; the two string entry points can also run on older
Intl builds, whose code-point fallback has different semantics. Such a ratio
measures added functionality cost, not an equivalent collation algorithm.
Bound callbacks initialize during their first timed invocation; the adaptive
minimum reports warmed steady-state costs rather than cold service startup.
These cases leave the frozen 72-case comparison fixture unchanged.

Zoo's self-contained files are fetched only when requested, from the
exact `ivankra/javascript-zoo` revision in the manifest. Every download
has a SHA-256 check before execution. The original source notices remain
untouched, and the downloaded upstream MIT license is preserved alongside
them. `fixtures/ZOO-LICENSE.txt` also preserves Ivan Krasilnikov's license
for the Zoo harness conventions. The driver uses Python's standard library
and does not download or import Zoo's Python dependencies. It reproduces
the pinned harness's named-score parsing and median convention rather
than presenting its report as an official Zoo harness invocation.

The `intl_relative` prefix similarly selects RelativeTimeFormat construction,
numeric formatting, automatic literals and parts. These guarded live workloads
are available only in builds with the service enabled and also leave the frozen
historical catalog unchanged.

The guarded `intl_duration` cases cover DurationFormat construction, text units,
exact fractional digital output and typed parts. They also use live feature
detection and leave the frozen historical comparison catalog unchanged.

The guarded live `intl_date` cases cover DateTimeFormat construction, bound
formatting, parts, cross-day ranges and source parts, localized offset zone
names, Date.toLocaleString and optional Temporal.PlainDateTime formatting.
Inputs use fixed UTC dates; constructor and Date locale cases reuse their
option objects. Run each exact case in a fresh process using the same enabled
Intl/calendar/zone profile for both revisions. Formatting/parts/range cases
cache their formatter on the benchmark function, and the Temporal case also
caches its fixed input. The first timed callback prepares that state; later
minimum-of-100 trials measure repeated reuse. Construction and Date locale
workloads intentionally keep their constructor cost. Apply the same workload
to both binaries: older live date scores included recurring formatter setup
and are not directly comparable to these steady-state cases.
These cases do not extend the
frozen 72-case catalog, and the default hosted core benchmark does not measure
enabled Intl services.

## Running locally

Use two clean, distinct source checkouts and a new output directory:

```sh
python3 tools/benchmarks/run.py \
  --candidate /path/to/candidate --candidate-revision EXACT_COMMIT_HASH \
  --parent /path/to/parent --parent-revision EXACT_PARENT_HASH \
  --compiler gcc --suite core --rounds 3 \
  --cpu auto --output /path/to/new-report
```

For an original Bellard comparison, use a clean checkout whose `origin` is the
original `bellard/quickjs` repository and select the exact revision explicitly:

```sh
python3 tools/benchmarks/run.py \
  --candidate /path/to/candidate --candidate-revision EXACT_COMMIT_HASH \
  --baseline /path/to/bellard --baseline-revision 535a7c250ff4a577ec36c3e103daab6dadeea650 \
  --baseline-kind bellard --compiler gcc --suite core --rounds 3 \
  --cpu auto --output /path/to/new-bellard-report
```

`--baseline-kind parent` is the default and enforces the actual first parent.
`--parent` and `--parent-revision` remain aliases for `--baseline` and
`--baseline-revision`. The version 2 report schema consistently uses `baseline`
for raw samples, build directories, feature defines and individual scores; the
state records `baseline_kind`, `baseline_repository` and `baseline_revision`.
Workflow artifacts include the compiler and baseline kind in their names. No
benchmark run is cancelled by a later push.

The tool checks both revisions and hashes the resulting binaries before
and after measurement. It also refuses existing `.obj`, engine/compiler
binaries, generated `repl.c`, root objects/archives/dependencies, and
untracked compatibility headers before invoking a build. Meson builds use a fresh directory in the report
folder, and their compiler database is checked against the objects produced. A clean tracked
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
cleanup, exact parent/Bellard provenance, scratch cadence, latest formal tips,
speed direction, geometric mean, and the distinct Zoo median contract without timing an engine:

```sh
python3 -m unittest discover -s tools/benchmarks -p 'test_*.py'
```

The fixture manifest upstream revision records historical provenance; it
does not select the comparison baseline. The comparison baseline is selected
explicitly: either the candidate's actual immediate first parent or the exact original Bellard revision. Fixture provenance
is independent of both baseline sources.

The pinned Bellard Make build retains compiler warnings without treating them
as errors: disabling assertions makes some upstream assertion-only locals unused.
The source remains unmodified. Both compared engines still use `-O2`, non-LTO
code and `-DNDEBUG`; each build report records its actual warning policy. Candidate
and parent builds retain their usual warnings-as-errors checks.
