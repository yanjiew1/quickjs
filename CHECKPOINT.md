# QuickJS modularization checkpoint

## Current status

Planning, independent plan review, and the pristine pre-refactor baseline are
complete. The multi-TU build foundation and first independent extraction are
implemented: binary object/bytecode serialization now compiles as
`src/quickjs/bytecode.c`. Correctness is validated, but this milestone remains
`[~]` because GCC non-LTO code-layout observations are deferred to the final
engine layout. Module lifecycle/resolution/evaluation is also now independently
compiled and validated. The next structural stage is frontend extraction.

Authoritative task: `task.md`. Living roadmap: `PLAN.md`.

## Baseline provenance

- Git commit: `04be246001599f5995fa2f2d8c91a0f198d3f34c`
  (`04be246`, 2026-06-16).
- Initial tree state: clean except untracked user-supplied `task.md`; `PLAN.md`
  and this checkpoint were then added before implementation.
- Platform: Linux x86_64, kernel 4.18.0-553.82.1.el8_10, AMD Ryzen 5 1600
  (6 cores/12 threads), 32 GiB RAM.
- Reference compiler: `/home/yanjie/opt/gcc-16.2.0/bin/gcc`, GCC 16.2.0,
  `-O2`, normal non-LTO, `CONFIG_WERROR=y`.
- Portability compiler: `/home/yanjie/opt/clang-23.1.1/bin/clang`, Clang
  23.1.1, `-O2`, normal non-LTO, `CONFIG_CLANG=y CONFIG_DEFAULT_AR=y
  CONFIG_WERROR=y`.
- `CC`, `HOST_CC`, and `AR` were explicitly selected from `/home/yanjie/opt`.
- Unicode input version is the repository's Unicode 17.0 corpus. Before and
  after baseline regeneration, `libunicode-table.h` SHA-256 was
  `cf782bc7a07549e976f606bd3cb8555858482b279574554dcb8d46412986006c`.
- Test262 checkout is pinned to `5c8206929d81b2d3d727ca6aac56c18358c8d790`
  and contains the repository patch. `test262_errors.txt` has 58 expected
  failure records.

Raw baseline logs and preserved GCC binaries are under
`/tmp/quickjs-baseline-04be246` for this environment. Reproducible summaries are
recorded here because `/tmp` is not a durable project artifact.

## Baseline build and correctness

GCC 16 WERROR clean parallel `all`: PASS, including `quickjs.check.o`, qjs,
qjsc, run-test262, static library, generated examples, shared examples, host
Unicode generator, and Unicode table regeneration.

Clang 23 WERROR clean parallel `all`: PASS for the same targets.

Repository `make test`: PASS with both GCC 16 and Clang 23, including:

- closure, language, builtin, loop, bigint and cyclic-module tests;
- workers, std module, OS/read-write handler tests;
- bjson shared-module and point shared-library examples.

Full GCC Test262 command:

```sh
timeout 20m ./run-test262 -t -m -c test262.conf -a
```

Result: PASS against the tracked exact expected set: `58/83558 errors`, `3356
excluded`, `6000 skipped`, six runner threads. Exit status was zero. Total
reported user time was 130.038 s. Because every actual mismatch is compared to
the named expected-error file by the runner, this establishes equality of the
actual and tracked failure sets, not merely equality of totals.

## Baseline size

GCC 16 non-LTO, unstripped files:

| File | Bytes |
|---|---:|
| `qjs` | 5,311,120 |
| `qjsc` | 5,299,312 |
| `run-test262` | 5,413,280 |
| `libquickjs.a` | 9,648,240 |

LLVM section-size report under the same build:

| File | text | data | bss | total |
|---|---:|---:|---:|---:|
| `qjs` | 1,080,048 | 1,912 | 336 | 1,082,296 |
| `qjsc` | 1,054,203 | 1,928 | 400 | 1,056,531 |
| `run-test262` | 1,079,847 | 2,016 | 1,744 | 1,083,607 |
| `.obj/quickjs.o` | 852,600 | 20 | 104 | 852,724 |

The baseline archive external-defined symbol listing is saved in the raw log
directory and will be compared after internal APIs are introduced.

## Baseline non-LTO performance

Five complete `tests/microbench.js` runs were executed serially with GCC 16 qjs
pinned to CPU 2 using `taskset -c 2`. Values below are medians in nanoseconds per
operation from the harness JSON; full per-test/per-run JSON is retained in the
raw baseline directory.

| Workload | Median ns |
|---|---:|
| prop_read | 14.12 |
| prop_write | 11.46 |
| array_read | 11.73 |
| array_write | 10.41 |
| typed_array_read | 19.94 |
| typed_array_write | 19.98 |
| global_read | 10.81 |
| global_write | 11.56 |
| func_call | 29.42 |
| global_func_call | 30.99 |
| int_arith | 14.88 |
| float_arith | 27.05 |
| regexp_ascii | 256.94 |
| regexp_utf16 | 269.63 |
| regexp_replace | 998.51 |
| string_build2 | 63.59 |
| string_to_int | 134.74 |
| sort_bench | 21.03 |

Performance comparisons use the preserved baseline qjs and the full median JSON,
not just this representative table. Suspicious results will be repeated in
isolation before classification.

## Known baseline limitations and pre-existing issues

- `tests/bench-v8` and `../quickjs-benchmarks` are absent, so the repository's
  broader V8/legacy benchmark targets cannot currently run. This is an external
  corpus availability limitation, not a source failure.
- `test262o` is absent; its obsolete ES5 suite cannot run.
- The custom GCC installation reports no 32-bit multilib beyond the default;
  CONFIG_M32 will be probed during final configuration validation and any actual
  link/runtime limitation recorded.
- The custom Clang installation has no bundled compiler-rt directory; MSan may
  be unavailable. ASan/UBSan and other supported local configurations will be
  attempted explicitly.
- The Makefile shares `.obj` across several configurations and compiler choices.
  Every configuration transition must start with `make clean` until unique
  object-directory support is implemented.
- No pre-existing correctness failure outside the tracked Test262 set was seen.
  No incidental improvement has yet occurred.

## Architecture decisions from reviewed analysis

- Preserve `JS_CallInternal` and its complete direct-threaded opcode dispatch in
  one TU. Keep shape/property/object/value code coarse until measured boundaries
  are stable.
- Parser/compiler owns two noncontiguous source regions around module runtime;
  module lifecycle/resolution is a separate owner with narrow mark/free/create
  hooks used by GC, compiler and serialization.
- The first extraction is binary serialization because its internal state is
  local; its explicit crossings include module creation and ArrayBuffer/SAB
  construction.
- Private layouts will be stratified; behavior declarations stay in owner
  headers. A monolithic quickjs-internal declaration header is not acceptable.
- Runtime class callback ordering and `JS_NewContext` intrinsic composition are
  resolved before hot-core extraction.
- RegExp has a strong compiler/executor bytecode boundary. Unicode has generic
  range, case, normalization and property owners, but generated table storage
  must have one deliberate owner. quickjs-libc host/event/worker state remains
  cohesive initially.

## Milestone 1 implementation and validation

Implemented files and build changes:

- Added guarded private headers `internal-config.h`, `internal-types.h`,
  `internal-opcode.h`, `internal-runtime.h`, `internal-object.h`,
  `internal-module.h`, and `internal-array.h` under `src/quickjs/`.
- Moved the original configuration/private-layout/opcode preamble losslessly
  into those headers. Behavior declarations are grouped by owner; the bytecode
  module sees only the owner APIs it actually consumes.
- Mechanically moved the original object-list, binary writer, and binary reader
  implementation into independently compiled `src/quickjs/bytecode.c`.
- Kept hot atom-tag, shape-layout, ref-header, stack-check and endian helpers as
  private `static inline` functions. Kept the original forced-inline property
  lookup in `quickjs.c`; bytecode uses a cold adapter rather than de-inlining the
  hot owner path.
- Added hidden `qjs_*` cold adapters for allocation, atom/string, object/bigint,
  GC, module, and ArrayBuffer construction. Existing implementation functions
  retain static linkage. Shared opcode metadata and typed-array size metadata
  have hidden internal linkage visibility and remain single-copy.
- The Makefile now has an explicit engine-object list, builds all engine TUs in
  check/normal/non-LTO/LTO/host/debug/fuzz variants through nested-safe rules,
  creates nested object directories, emits target-specific dependency files in
  compile recipes, and includes nested dependency files.

Validation after extraction:

- GCC 16 WERROR normal clean/parallel `all`: PASS.
- GCC 16 WERROR `make test`: PASS.
- Full Test262: PASS with unchanged exact result `58/83558 errors`, `3356`
  excluded, `6000` skipped. No new or missing expected failure.
- Clang 23 WERROR normal clean/parallel `all` and `make test`: PASS.
- GCC 16 WERROR LTO clean/parallel `all` and `make test`: PASS. Both
  `libquickjs.lto.a` and separately compiled non-LTO `libquickjs.a` were built.
- CONFIG_CHECK_JSVALUE independently compiled both engine TUs under GCC and
  Clang. qjsc optional-intrinsic examples, modules, bjson round trips,
  ArrayBuffer/typed-array serialization paths, workers, std/os and shared
  examples all passed through `all`/`test`/Test262.
- Unicode regeneration remained byte-for-byte stable.

GCC non-LTO size at the initial working implementation was 1,083,128 bytes of
linked qjs text versus baseline 1,082,296 (+832, +0.077%); final size for this
milestone must be re-recorded after later layout stabilization.

### Deferred non-LTO performance issue

`array_read` is a confirmed meaningful slowdown after the first extraction.
Interleaved, CPU-2-pinned ten-run comparisons gave a median 11.920 ns baseline
versus 13.025 ns current (+9.27%). `string_build2` was 64.380 versus 66.385 ns
(+3.11%, suspicious and retained for final retest). Other representative calls,
arithmetic, RegExp, sorting, property and typed-array workloads were neutral or
within small/noisy differences after retesting; `typed_array_write` retested at
-0.80%.

`perf stat -r 5` for `array_read` showed virtually unchanged work (baseline
4,278,715,390 instructions/733,071,298 branches; current 4,279,017,878/
733,136,241) but branch misses rose from 15,847,025 (2.16%) to 23,990,699
(3.27%), with cycles rising from 1.768B to 1.982B. `JS_CallInternal` and relevant
function sizes/disassembly sizes are unchanged; its linked address moved because
GCC reordered/placed functions differently after extraction.

Focused remedies attempted:

1. Replaced broadly promoted formerly-static functions with end-of-file cold
   `qjs_*` adapters, restoring static owner linkage and avoiding lost inlining.
   This reduced but did not eliminate the slowdown.
2. Moved all cold adapters after existing engine implementation to reduce their
   influence on hot source order.
3. Linked the bytecode object before rather than after the core object. A ten-run
   screen worsened `array_read` to +11.27%, so the ordering was rejected.

No extra instructions, missed inline, or localized semantic overhead remains.
Padding/alignment guesses would be compiler-specific and disproportionate before
the remaining planned TU boundaries settle. Per task policy, deeper cache/
branch-predictor layout work is deferred to final performance stabilization;
this keeps the milestone `[~]` but does not block later structural work.

## Milestone 2: module subsystem extraction

Implemented `src/quickjs/module.c` as the owner of module cleanup and lifecycle,
module namespace exotic behavior, normalization/loading, export resolution,
linking/evaluation, dynamic import, and the existing public module APIs. Parser
and import/export syntax remain in `quickjs.c`; duplicate-export parser errors
remain parser-owned, with unchecked mutation performed only after the parser's
existing duplicate check.

`internal-function.h` now defines the narrow module-to-function/promise bridge.
The module header exposes explicit lifecycle, namespace autoinit, parser mutation,
resolve, and one link-and-evaluate entry point. Resolver/evaluator state and the
namespace exotic table remain static in `module.c`. Core hot helpers—including
the forced-inline property lookup—remain static; module uses cold adapters.

Validation:

- GCC 16 WERROR clean parallel `all` and full `make test`: PASS.
- Full Test262 exact set: PASS, unchanged `58/83558`, 3356 excluded and 6000
  skipped.
- Clang 23 WERROR clean parallel `all` and full `make test`: PASS.
- CONFIG_CHECK_JSVALUE compiles all three engine TUs independently.
- `DUMP_MODULE_RESOLVE` and `DUMP_MODULE_EXEC` syntax builds: PASS.
- Cyclic imports, dynamic/module loading via the full suites, workers and bjson
  module serialization all pass.

GCC non-LTO linked qjs text is 1,083,712 bytes versus baseline 1,080,048
(+3,664, +0.34%). The three engine objects contain 812,405 bytes core text,
24,011 module text, and 19,384 bytecode text.

The module boundary changed final layout again and resolved the earlier deferred
hot results: an interleaved ten-run screen measured `array_read` 11.515 ns versus
baseline 11.875 (-3.03%, incidental improvement) and `string_build2` 64.565
versus 63.985 (+0.91%, neutral). `func_call` was +0.43% (neutral). This confirms
the prior array/string differences were layout-dependent rather than extra work.

An isolated interleaved ten-run RegExp retest observed `regexp_ascii` 270.865 ns
versus 258.395 (+4.83%) and `regexp_utf16` 277.460 versus 269.310 (+3.03%);
`regexp_replace` was +0.47%. Module code is not on these paths and no RegExp call
boundary changed, so the evidence again indicates final link/code placement.
Object ordering already failed as a remedy in the preceding milestone. This
observation stays deferred until the planned RegExp and final engine object
layout exists; the milestone remains `[~]` under the task's performance policy.

## Exact next steps

Array conversion/equality ownership prerequisite completed:

- Rehomed Array-needed integer/length conversion, locale-string conversion,
  property-value get/set, equality, OrdinaryIsInstanceOf, and realm APIs into
  number/string/property/function headers with bounded core implementations.
- GCC 16 WERROR clean build/tests and exact Test262 `58/83558` pass. No hot
  forced-inline helper changed.
- A compile-only Array trial was removed: its nominal range crosses JSON and a
  TypedArray species helper. Source extraction is deferred until those owners
  are separated; no broken or partial file remains.

Iterator/fast-array ownership prerequisite completed:

- Added `internal-iterator.h`; generic iterator traversal, fast-array views, and
  CopyDataProperties now have an owner-neutral API. `internal-array.h` owns fast-
  array allocation/growth plus a four-operation TypedArray bridge.
- `can_extend_fast_array` remains `static force_inline` in the owner header.
  GCC 16 and Clang 23 WERROR clean builds/tests pass; Test262 remains `58/83558`.
- Array source extraction awaits rehoming hot conversion/equality/property APIs;
  this avoids approximately 45 adapter calls on array/iteration paths.

Construction/property ownership prerequisite completed:

- Generic function-list instantiation, constructor linking, and module export-
  list APIs moved to `builtin.c`; `internal-property.h` now centralizes the
  object -> property -> function layer and rehomes scattered Proxy/RegExp APIs.
- Hot find-own-property remains core-local `force_inline`; only three bounded
  allocation/cycle/C-function adapters were added for generic construction.
- GCC 16 and Clang 23 WERROR clean builds/tests pass; Test262 remains exactly
  `58/83558`. Base extraction is intentionally deferred until iterator/error/
  realm bootstrap seams are owned, avoiding another 30 temporary adapters.

Numeric ownership and primitive milestone completed:

- Added `internal-number.h` and `builtin-primitive.c`/`internal-primitive.h`.
  Number, Boolean, String, Symbol and BigInt now share one owner with three exact-
  order install phases. String exotic behavior stays private; hot string/atom
  access remains header-inline.
- GCC 16 and Clang 23 WERROR clean builds/tests pass; Test262 remains exactly
  `58/83558`. Seven-run focused results include array_read +0.34%, float/int
  arithmetic -10.85%/-5.86%, string_to_int -6.41%, but int_to_string +7.59%,
  string_build2 +7.62%, RegExp ASCII +7.70%, replace +7.96%; unresolved layout
  regressions remain for final stabilization.

String ownership prerequisite completed:

- Added `internal-string.h` as the acyclic runtime -> string -> object layer.
  It owns atom-tag representation, StringBuffer layout, hot character/empty
  inlines, allocation/conversion/buffer APIs, debug hooks, invalid-codepoint and
  RegExp-facing string bridges formerly scattered in object/RegExp headers.
- GCC 16 and Clang 23 WERROR clean builds/tests pass. This is an ownership-only
  header change; no executable path or forced-inline policy changed.
- Primitive extraction remains gated on a genuine numeric/BigInt internal owner;
  forcing it now would introduce roughly 30 non-string trampolines.

Proxy milestone completed:

- `builtin-proxy.c` owns proxy state callbacks, traps, exotic vtable, descriptor/
  key checks, call/construct dispatch, revocation, tables, and installer. The
  class table remains static and core exposes one bounded registration adapter.
- GCC 16 and Clang 23 WERROR clean builds/tests pass; Test262 remains exactly
  `58/83558`. Seven-run property/call screening retained the known layout pattern:
  prop_write neutral, prop_read +1.92%, func_call +5.79%, array_read -3.38%,
  string_build2 +7.98%, RegExp ASCII +13.77%, replace +5.06%.

Global/URI and Date milestone completed:

- `builtin-global.c` owns eval/isNaN/isFinite, parseInt/parseFloat, URI and legacy
  escape functions plus the global table; its install still precedes Number
  aliases. `builtin-date.c` owns timezone handling, Date arithmetic/parser/
  formatting, tables, `JS_NewDate`, and `JS_AddIntrinsicDate`.
- GCC 16 and Clang 23 WERROR clean builds/tests pass; Test262 remains exactly
  `58/83558`. Date microbenchmarks are neutral/improved (date_parse -0.08%,
  date_now -3.61%). Current layout observations remain string_build2 +4.94%,
  RegExp ASCII +11.44%, replace +9.14%; array_read is neutral (-0.72%).

Builtin composition and Math milestone completed:

- `src/quickjs/builtin.c` owns the exact ordered intrinsic-install sequence;
  runtime/context creation now calls its two narrow phase entry points.
- `src/quickjs/builtin-math.c` owns Math functions, sumPrecise iterator handling,
  random-state initialization, method/constants tables, and installation.
  VM-shared power remains core-owned; only three iterator adapters cross inward.
- GCC 16 and Clang 23 WERROR clean builds/tests pass; Test262 remains exactly
  `58/83558`. Math initialization order and qjsc feature selection are preserved.
- Ten-run screening: math_min +4.59%, int/float arithmetic -9.98%/-13.42%,
  array_read -3.14%, string_build2 +8.54%, RegExp ASCII +11.59%, replace +7.55%.
  These shifting cross-workload results remain classified as intermediate code-
  layout effects pending the final engine layout, not cancellation by aggregate.

RegExp builtin milestone completed:

- `src/quickjs/builtin-regexp.c` owns RegExp compilation/setup, class and string-
  iterator callbacks/state, execution and string protocol methods, tables, and
  intrinsic installers. `internal-regexp.h` contains its scoped bridge.
- Both lastIndex helpers remain TU-local `force_inline`; no new forced inline was
  added. GCC 16 and Clang 23 WERROR clean parallel builds and `make test` pass;
  all five engine TUs pass CONFIG_CHECK_JSVALUE. Full Test262 remains exactly
  `58/83558`.
- An interleaved ten-run GCC screen currently shows broad layout sensitivity:
  RegExp ASCII +8.05%, UTF16 +6.44%, replace +6.49%, string_build2 +8.42%,
  array_read +7.85%, and func_call +2.58%. The boundary does not add work to
  array/string/call paths, and earlier milestone results changed direction as
  layout evolved. These remain explicit unresolved non-LTO issues for final
  stabilization after the remaining core/builtin layout is fixed.
- The standalone `regexp_test` TEST main has a pre-existing type mismatch:
  `uint8_t[]` is passed where current `lre_exec` requires `uint8_t **`. It fails
  unchanged under GCC 16 and is deferred rather than fixed during refactoring.

Frontend milestone completed after this section was first written:

- `src/quickjs/frontend.c` now owns lexer/parser/import-export syntax, scopes,
  lowering/optimization, eval bridges, bytecode metadata/freeing, and JSON token
  parsing; `JSParseState` and `JSToken` remain TU-private.
- `internal-frontend.h` exposes only the compact JSON reviver record and five
  cold adapters needed by the builtin JSON reviver remaining in `quickjs.c`.
- GCC 16 and Clang 23 WERROR clean parallel builds and `make test` pass;
  CONFIG_CHECK_JSVALUE compiles all four engine TUs. `DUMP_BYTECODE=127` syntax
  validation passes. Full Test262 remains exactly `58/83558`.
- GCC non-LTO qjs text is 1,079,736 bytes versus baseline 1,080,048 (-312).
  Ten-run interleaved screening: `array_read` -4.54%, `func_call` +1.16%,
  RegExp ASCII/UTF16 -0.95%/-1.58%, but `string_build2` +9.05% and
  `regexp_replace` +7.45%. Neither slower path calls frontend code; these are
  recorded layout regressions for final stabilization after builtin/core splits.

1. Commit the coherent buildable frontend-extraction milestone with current
   non-LTO layout observations recorded.
2. Split builtin clusters before hot
   core ownership work. Revisit the recorded branch-miss regression only after
   those boundaries establish the final code layout.

Useful baseline command prefix:

```sh
make CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  HOST_CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  AR=/home/yanjie/opt/gcc-16.2.0/bin/gcc-ar CONFIG_WERROR=y
```
