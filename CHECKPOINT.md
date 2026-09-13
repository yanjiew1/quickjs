# QuickJS modularization checkpoint

## Current status

Implementation is intentionally paused at commit `f8fa5bc` in a coherent,
buildable, correctness-validated state. The cold engine and builtin decomposition
is complete: frontend/compiler, modules, bytecode serialization, builtin
composition, and every planned builtin family are independent translation units.

The residual 21,553-line `quickjs.c` still owns allocator/runtime/context/jobs,
atoms/strings, shapes/objects/properties/GC/conversions, opcode slow paths, the
complete `JS_CallInternal` interpreter, and generator/async execution. A bounded
function/VM extraction was the only work in progress when pause was requested;
it was fully rolled back because substantial boundary work remained. There is no
partial VM file, header, Makefile edit, or tracked working-tree diff.

The overall task is not complete. Current non-LTO RegExp/layout regressions are
deferred under `task.md`, hot-core extraction remains, and no secondary library
or tooling target has begun. The only untracked file is the user-supplied
authoritative `task.md`.

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

## Milestone history after frontend extraction

Foundational builtin milestone completed:

- `builtin-base.c` owns Object, Function, Error/AggregateError, Reflect,
  descriptor helpers, debug accessors and both raw/full bootstrap phases. All
  delegated phase ordering remains identical.
- GCC 16 WERROR clean builds/tests, targeted base smoke, Clang 23 syntax and
  Test262 `58/83558` pass. Seven-run screen: prop_write/func/array/sort neutral
  or improved; prop_read +7.35%, string_build2 +5.33%, RegExp ASCII +5.32%,
  replace +9.70% remain layout observations for final stabilization.
- Removed now-unreferenced private `JS_IsEmptyString`, detected by Clang after
  its last consumer moved; this is extraction cleanup, not semantic change.

Promise/async milestone completed:

- `builtin-async.c` owns Promise data/jobs/rejection/public APIs, Generator
  installation, and AsyncIterator/AsyncFunction/AsyncGenerator tables/classes;
  VM execution/resume stays core-owned behind typed bridges.
- GCC 16 WERROR clean builds/tests, DUMP_PROMISE syntax, targeted async/generator
  smoke, Clang 23 syntax and Test262 `58/83558` pass.
- Seven-run screen: func_call +0.07%, array_read -1.95%, sort -0.62%,
  string_build2 +1.36%; only RegExp layout remains materially slow (+9.14% ASCII,
  +9.35% replace) before final stabilization.

Collections/weak-reference milestone completed:

- `builtin-collection.c` owns Map/Set/WeakMap/WeakSet, iterators/algebra, groupBy,
  weak bookkeeping, WeakRef and FinalizationRegistry jobs/state/tables/installers.
  Hot map hash/lookup remains TU-local; string/rope hashing is small owner-inline.
- GCC 16 WERROR clean builds/tests, Clang 23 syntax, and Test262 `58/83558` pass.
  Seven-run map/weak screens are neutral or improved (-0.64% to -5.47%); sort is
  -2.39%. Existing RegExp layout observations remain (+9.34%/+6.05%).

Atomic Array/TypedArray milestone completed:

- `builtin-array.c` owns Array, synchronous iterators/wrap/concat/helpers and two
  exact-order bootstrap phases. `builtin-typed-array.c` owns ArrayBuffer/SAB,
  TypedArray/DataView, codecs, Atomics, species, callbacks/tables/public APIs and
  the sole typed-size table. `internal-array-algorithm.h` is their neutral seam.
- GCC 16 and Clang 23 WERROR clean builds/tests pass; Test262 remains `58/83558`.
  Fast array/view screen is neutral: read -1.61%, write -0.34%, slice +1.27%,
  typed read +1.81%, typed write -3.10%.
- Initial sort +8.94% was a confirmed extra-work regression (instructions
  27.237B -> 29.030B). A focused remedy inlined only the small string comparison/
  length logic in `internal-string.h`, leaving one hidden memcmp primitive. Sort
  retested -1.81%; instructions fell to 26.653B. Retain this validated incidental
  improvement; no new always_inline was added.

JSON milestone completed:

- `builtin-json.c` owns reviver/source records, parse integration, raw JSON,
  quoting/stringification, tables and public JSON APIs. Parser token state stays
  frontend-private. Four narrow Array stack/property-list bridges are used.
- GCC 16 WERROR clean build/tests, Clang 23 WERROR syntax, and exact Test262
  `58/83558` pass. JSON/bjson focused repository tests pass.

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

## Historical pause handoff snapshot (superseded by the milestone below)

### Completed architecture

The normal engine build links these real independent translation units under
`src/quickjs/`:

- `frontend.c`: lexer/parser, import/export parsing, scopes, lowering,
  optimization, eval bridge, bytecode metadata/freeing, JSON token parsing;
- `module.c`: module lifecycle, namespace behavior, loading, resolution,
  linking/evaluation and dynamic import;
- `bytecode.c`: binary object/bytecode writer and reader;
- `builtin.c`: generic construction/function-list support and ordered intrinsic
  composition;
- `builtin-base.c`: Object, Function, Error/AggregateError, Reflect and both
  foundational bootstrap phases;
- `builtin-array.c`: Array and synchronous iterator/wrap/concat/helper builtins;
- `builtin-typed-array.c`: ArrayBuffer/SAB, TypedArray, DataView, codecs, Atomics;
- `builtin-primitive.c`: Number, Boolean, String, Symbol and BigInt;
- `builtin-json.c`, `builtin-regexp.c`, `builtin-proxy.c`;
- `builtin-collection.c`: Map/Set/WeakMap/WeakSet, WeakRef and finalization;
- `builtin-async.c`: Promise and async/generator builtin tables/jobs;
- `builtin-math.c`, `builtin-global.c`, and `builtin-date.c`.

Private headers are layered by owner: config/types/opcodes -> runtime -> string
-> number -> object -> property -> function -> iterator -> modules/frontend,
with scoped builtin headers. Hot helpers retained inline include atom tags,
string character access, ref headers, stack/poll checks, shape-property access,
`qjs_can_extend_fast_array`, RegExp lastIndex access, and small string comparison
logic. No speculative `always_inline` annotation was added.

The residual `quickjs.c` is 21,553 lines. It deliberately still contains the
hot/core implementation: allocation/runtime/context/jobs, atoms/strings,
objects/shapes/properties/GC/conversions, opcode slow paths, the complete
interpreter, and generator/async execution. `JS_CallInternal` remains whole.

### Current validation

Stopping-state validation on commit `f8fa5bc` plus the documentation edits:

- clean parallel GCC 16.2 `CONFIG_WERROR=y all`: PASS;
- all engine TUs independently compiled with `CONFIG_CHECK_JSVALUE`: PASS;
- full GCC `make test`: PASS, including modules, workers, std/os/rw handlers,
  bjson, shared modules and generated qjsc examples;
- exact full Test262: PASS against the tracked set, `58/83558` errors, `3356`
  excluded, `6000` skipped; no new or missing failure;
- latest changed foundational TUs passed Clang 23 WERROR syntax; full Clang 23
  WERROR builds/tests passed at the preceding major Array/TypedArray state and
  repeatedly throughout the builtin sequence;
- Unicode table SHA-256 remains the baseline
  `cf782bc7a07549e976f606bd3cb8555858482b279574554dcb8d46412986006c`.

Current GCC non-LTO sizes: qjs 5,199,264 bytes and 1,061,294 text bytes; qjsc
5,187,368/1,035,243 text; run-test262 5,301,320/1,061,047 text;
libquickjs.a 9,816,096 bytes. Versus baseline, linked qjs text improved by
18,754 bytes (-1.74%) and unstripped qjs by 111,856 bytes (-2.11%); the archive
grew 167,856 bytes (+1.74%) due to separate object/debug metadata. This retained
size improvement is incidental and correctness-validated.

### Current performance and deferred issues

The latest seven-run GCC non-LTO screen at `f8fa5bc` versus baseline measured:
prop_read +7.35%, prop_write -1.69%, func_call -0.92%, array_read -2.88%,
sort_bench -0.81%, string_build2 +5.33%, regexp_ascii +5.32%, and
regexp_replace +9.70%. The RegExp replace slowdown and prop/string observations
are unresolved meaningful non-LTO issues, so engine/builtin milestones remain
`[~]`; they must be resolved in final stabilization before completion.

Prior diagnostics showed shifting results as object layout changed, usually with
near-identical instruction counts and differing branch misses. Remedies already
attempted include cold-adapter placement, restoring static owner linkage, and
linking extracted bytecode before core (rejected because it worsened results).
One direct sort regression was different: +8.94% time and +6.58% instructions.
Inlining only the small string comparison/length logic in `internal-string.h`
removed a double cross-TU hop; sort recovered to -1.81% and instructions improved
from baseline 27.237B to 26.653B. Retain this evidence-backed fix.

The last attempted function/VM extraction mapped current `quickjs.c` lines
13,414-20,405 as one coherent region, including all opcode slow paths,
`JS_CallInternal`, calls/closures/iterators and generator/async execution. It was
fully rolled back on pause because private-boundary integration remained
substantial. No `function-vm.c`, temporary header, or partial Makefile change is
present.

Pre-existing issues not to fix incidentally: standalone `regexp_test` has a TEST
main type mismatch (`uint8_t[]` passed where `lre_exec` expects `uint8_t **`);
the tracked 58 Test262 failures; absent `test262o`, `tests/bench-v8`, and external
benchmark corpus; likely unavailable local GCC M32 multilib and Clang MSan
runtime. Record rather than repair these unless they directly block the refactor.

## Exact next steps

1. Resume the bounded function/VM extraction from current `quickjs.c` lines
   13,414-20,405. Keep the entire interpreter together, preserve stack/interrupt
   checks inline, and expose only the rare interrupt slow path. Validate GCC
   WERROR `all`/`test`, exact Test262, and focused non-LTO call/property/array/
   string/RegExp screens before committing.
2. Reassess the residual core after the VM move. Prefer runtime, atom-string and
   object-value owners only if class/GC callbacks remain group-level and the
   internal API shrinks; otherwise retain the reviewed minimum defensible
   coarse `core.c` plus `function-vm.c` architecture.
3. Complete core correctness and non-LTO stabilization, resolving every confirmed
   meaningful regression (especially RegExp replace and prop/string observations)
   before declaring the engine stable.
4. Only then begin secondary targets in priority order: RegExp compiler/executor,
   Unicode runtime owners, quickjs-libc host/std/loader split; update special
   Makefile rules and validate each. Evaluate unicode_gen and run-test262 last,
   retaining either cohesive file when state ownership makes splitting harmful.
5. Run final GCC/Clang/LTO/sanitizer/configuration, exact Test262, host, RegExp,
   Unicode/generator, size/export and broader available benchmark validation;
   finish PLAN/CHECKPOINT and final report.

Useful commands:

```sh
make clean
make -j12 CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  HOST_CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  AR=/home/yanjie/opt/gcc-16.2.0/bin/gcc-ar CONFIG_WERROR=y all
make CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  HOST_CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  AR=/home/yanjie/opt/gcc-16.2.0/bin/gcc-ar CONFIG_WERROR=y test
timeout 20m ./run-test262 -t -m -c test262.conf -a
taskset -c 2 ./qjs --std tests/microbench.js \
  prop_read prop_write func_call array_read sort_bench \
  string_build2 regexp_ascii regexp_replace
```

## Function/VM extraction milestone (authoritative current state)

### Architecture and boundary decision

The next hot-core boundary from the historical pause has been implemented
afresh. `src/quickjs/function-vm.c` is a real 5,964-line translation unit owning
arguments/iterator opcode support, closures and variable references, class and
method setup adjacent to closures, C/bound/bytecode call dispatch, the complete
direct-threaded `JS_CallInternal`, constructor/invoke paths, generator execution,
and async function/generator resume machinery. The interpreter and all computed
goto labels remain together. The residual `quickjs.c` is 15,903 lines and still
owns allocator/runtime/context/jobs, atoms/strings, values/objects/shapes/
properties/GC/conversions, and the operator slow layer.

The implemented seam begins after the operator slow layer rather than moving the
entire historical orientation range. Moving those operators would require
exporting a broad set of BigInt, string, conversion, and property internals;
leaving them with their data owners requires only the narrow
`internal-operator.h` entry points. Hot property lookup, stack overflow and
interrupt checks, ref-header/value updates, shape-property access, and flat
string equality remain scoped inline helpers. Only the rare interrupt slow path
crosses the runtime boundary.

Runtime class callback ownership is installed through one
`qjs_function_vm_init_runtime()` entry point. VM-owned callbacks and call slots
remain static. Direct hidden owner entries replace avoidable property and string
wrapper trampolines. No public API was added: the normal `qjs` dynamic export set
is exactly the same 292 names as the pristine baseline.

A finer-boundary review was performed after the extraction. Iterator setup is
interleaved with for-in/for-of opcode helpers; separating it would add several
hot interpreter-facing interfaces. Generator and async execution share private
frame/resume state and `JS_CallInternal`; separating them would expose that state
or the internal call dispatcher. These are not clean independent ownership
domains at this point, so the cohesive VM owner is retained. This is an
ownership/coupling decision, not a file-size preference.

### Validation

Acceptance validation on the exact recorded source state:

- independent GCC 16 checked-value compilation of every engine TU: PASS as part
  of clean `CONFIG_WERROR=y all`;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all`: PASS;
- full GCC repository `make test`: PASS, including closures, language/builtins,
  loops, BigInt, cyclic modules, workers, std/os/rw handlers, bjson, shared
  modules, and generated examples;
- Clang 23 WERROR syntax for `quickjs.c` and `function-vm.c`, normal and
  `CONFIG_CHECK_JSVALUE`: PASS;
- exact full Test262 expected-failure comparison: PASS, unchanged at
  `58/83558` errors, `3356` excluded, and `6000` skipped; no new or missing
  expected failure;
- `git diff --check`: PASS; dynamic exported-symbol comparison: exact match.

Current GCC non-LTO sizes are: qjs 5,200,512 bytes with 1,061,910 text bytes;
qjsc 5,188,616/1,035,923 text; run-test262 5,302,584/1,061,727 text; and
libquickjs.a 9,871,788 bytes. Relative to `f8fa5bc`, qjs grew 1,248 bytes and
616 text bytes; the archive grew 55,692 bytes from the additional object/debug
metadata. Relative to pristine, qjs remains 110,608 bytes smaller and its text
is 18,138 bytes smaller. The size change is understood and non-blocking.

### Non-LTO performance investigation

The first seven-run interleaved screen against pristine measured `array_read`
+10.06%, `string_build2` +10.54%, `regexp_ascii` +13.01%, and
`regexp_replace` +7.99%. A direct freshly built `f8fa5bc` comparison isolated
VM-boundary deltas of +12.40%, +5.07%, +5.36%, and -1.15% respectively. For
array access, five `perf stat` repeats showed 4.299B versus 4.281B instructions
(+0.43%) but 1.915B versus 1.722B cycles (+11.2%), with branches and misses
nearly unchanged. This established compiler placement/scheduling rather than
semantic extra work as the main cause.

Scoped remedies were measured rather than assumed. A direct property owner entry
removed a wrapper but did not change the in-range array fast path. Linking the VM
object before the core did not recover array access and worsened string/RegExp,
so that ordering was reverted. Direct ownership of `qjs_concat_string` reduced
the isolated string loss. Aligning the interpreter improved but did not fully
recover the array path. The retained evidence-backed result marks the central
interpreter and concatenation routine hot and aligns `JS_CallInternal` to 128
bytes; this places the two demonstrated hot owners together without LTO or
moving large functions into headers. The final isolated seven-run comparison
to `f8fa5bc` is `array_read` -0.78%, `string_build2` +1.63%, and
`regexp_ascii` +1.73%. `regexp_replace` also improved relative to the handoff
state in the final pristine comparison.

The final seven-run pristine comparison is: `prop_read` -0.71%, `prop_write`
-0.09%, `func_call` -4.07%, `array_read` -2.38%, `sort_bench` +0.19%,
`string_build2` +6.67%, `regexp_ascii` +8.75%, and `regexp_replace` +6.06%.
Thus the VM extraction has no remaining confirmed meaningful isolated loss, but
the reproducible string/RegExp pristine-baseline layout group remains deferred.
The affected engine/builtin performance milestones stay `[~]`; these issues do
not block subsequent structural work and must be revisited during Final
Performance Stabilization.

Raw logs for this milestone are under `/tmp/qjs-vm-*.log`, with perf counter,
annotation, build, test, and Test262 summaries in the corresponding
`/tmp/qjs-vm-*` files. The freshly rebuilt comparison checkout is
`/tmp/quickjs-f8fa5bc`.

### Exact next steps

1. Reassess the residual 15,903-line core from actual static-symbol and callback
   ownership. Split runtime/context/jobs, atom-string, and object/value owners
   only at natural seams with narrow dependency direction; retain coupled areas
   when a split would materially broaden APIs, create cycles, or harm hot paths.
2. Implement and validate any justified residual-core owner one bounded milestone
   at a time. If the cleanest reviewed result is fewer owners, document that
   decision rather than splitting for file count.
3. Once the primary core decomposition is structurally stable and
   correctness-validated, proceed to secondary targets even if the recorded
   string/RegExp non-LTO issues remain `[~]`: RegExp compiler/executor, Unicode
   runtime ownership, and quickjs-libc host/std/loader, followed by assessment of
   unicode_gen and run-test262.
4. After all structural migrations, perform Final Performance Stabilization and
   resolve every remaining confirmed meaningful refactor-induced non-LTO
   regression, then run the full supported configuration/sanitizer/correctness/
   export/size review and fresh adversarial audit required by `task.md`.

## Allocator extraction milestone (authoritative current state)

### Architecture and boundary decision

`src/quickjs/allocator.c` is now an independent 582-line owner for the arena and
large-block allocator, the default system allocator adapter, and the public
runtime/context allocation API. `quickjs.c` is 15,353 lines. The only new private
lifecycle surface is `qjs_allocator_init()` plus
`qjs_default_malloc_functions()` in `internal-allocator.h`; no allocator data
representation or backend helper was exported.

The context allocation functions moved with the backend after generated-code
inspection showed that leaving `js_malloc()` in the core introduced an extra
cross-TU tail-call through `js_malloc_rt()` on every allocation. With one owner,
GCC again emits a direct call from `js_malloc()` to the private `__js_malloc()`
backend. GC threshold policy remains in the core because it owns object/runtime
lifetime rather than raw allocation. Runtime/context/jobs are not yet claimed by
this milestone.

The residual hot core stays before the cold allocator object in link order. A
seven-run comparison showed this maintainable ordering recovered the extraction's
isolated `string_build2` loss; the alternative allocator-first order did not.

### Validation and performance

Acceptance validation on the documented source state:

- clean parallel GCC 16.2 `CONFIG_WERROR=y all`, including normal and
  `CONFIG_CHECK_JSVALUE` compilation of every engine TU: PASS;
- full GCC repository `make test`: PASS;
- Clang 21.1 WERROR syntax for `quickjs.c` and `allocator.c`, normal and
  `CONFIG_CHECK_JSVALUE`: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- `git diff --check`: PASS; normal `qjs` dynamic exports exactly match the
  baseline 292-name set.

Current GCC non-LTO sizes are: qjs 5,177,376 bytes with 1,059,462 text bytes;
qjsc 5,165,472/1,033,475 text; run-test262 5,279,432/1,059,279 text; and
libquickjs.a 9,828,472 bytes.

The final seven-run CPU-pinned comparison against the exact `cbfac5a` VM
milestone is: `prop_create` -1.15%, `array_slice` +2.26%, `array_push` -11.51%,
`func_call` -0.01%, `array_read` +0.17%, `string_build2` +0.00%,
`regexp_ascii` +7.19%, and `regexp_replace` +2.14%. The direct-allocation remedy
removed confirmed extra work. Both allocator/core link orders were measured;
the retained core-first order recovered string construction and improved replace,
but RegExp ASCII remained layout-sensitive. That unresolved non-LTO observation
is recorded for Final Performance Stabilization and does not block subsequent
structural work under the task performance policy. Raw build, test, Test262, and
benchmark logs are `/tmp/qjs-allocator-*` and `/tmp/qjs-allocator-order-*`; the
fresh comparison checkout is `/tmp/quickjs-cbfac5a`.

### Exact next steps

1. Extract the atom/string owner afresh from the current residual core. Preserve
   the zero-ref string release fast path, atom-kind and numeric-index checks, and
   rope/string character hot paths inline where the current dependency and
   generated-code evidence require them.
2. Reassess runtime/context/jobs and object/shape/property/GC ownership after the
   atom-string seam is established. Split only natural ownership domains with
   narrow dependency direction; keep coupled lifetime and class callbacks
   together.
3. When the primary core is structurally stable and correctness-validated,
   continue to the secondary runtime/library targets even if recorded non-LTO
   issues remain `[~]`; resolve all confirmed meaningful refactor-induced losses
   during Final Performance Stabilization.

## Atom/string extraction milestone (authoritative current state)

### Architecture and boundary decision

`src/quickjs/atom-string.c` is now a separate 2,355-line owner for predefined
atom data, atom table/hash allocation and lifetime, raw strings, StringBuffer,
C-string conversion, plain/rope comparison, rope construction/rebalancing, and
string concatenation. The residual `quickjs.c` is 12,952 lines.

Atoms and strings remain one owner because atoms are string-backed and their
allocation, comparison, refcounts, predefined empty strings, and zero-ref
release paths are bidirectionally coupled. Splitting them would expose atom-table
and string-lifetime details without creating a clean dependency direction.
Class registration stays in the core and uses cold runtime atom create/dup
entries; shape/object GC stays in the core.

The owner initializes and tears down its runtime state, finalizes zero-ref
string/rope/symbol values, and computes atom memory usage through narrow
lifecycle APIs. The residual core no longer iterates or frees atom storage.
Tagged-atom conversion, atom kind, complete array-index classification, common
nonnumeric-index rejection, character reads, hashing, string equality/compare,
and the zero-ref string decrement remain scoped inline in `internal-string.h`.
The canonical numeric-index conversion is an owner slow path. The complete rope
and hot concatenation implementation stays together in the owner.

### Validation and performance

Acceptance validation on the documented source state:

- clean parallel GCC 16.2 `CONFIG_WERROR=y all`, including normal and
  `CONFIG_CHECK_JSVALUE` compilation of every engine TU: PASS;
- independent GCC 16 WERROR compilation of `quickjs.c` and `atom-string.c`:
  PASS; leak/atom-dump and combined checked-value syntax variants: PASS;
- full GCC repository `make test`: PASS;
- Clang 21.1 WERROR syntax for both affected TUs, normal and
  `CONFIG_CHECK_JSVALUE`: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- normal `qjs` dynamic exports exactly match the baseline 292-name set;
  `git diff --check`: PASS.

Current GCC non-LTO sizes are: qjs 5,176,136 bytes with 1,058,430 text bytes;
qjsc 5,164,296/1,032,390 text; run-test262 5,278,200/1,058,119 text; and
libquickjs.a 9,870,054 bytes.

The final seven-run CPU-pinned comparison against the exact freshly built
`b58d08a` parent is: `prop_read` +0.70%, `prop_write` +0.66%, `prop_create`
+2.86%, `array_push` +7.04%, `array_read` -0.22%, `func_call` +0.61%,
`sort_bench` -8.79%, `regexp_ascii` +4.95%, `regexp_replace` -1.80%, and
`string_build2` +4.62%. `array_slice` had one 2.96 ns parent outlier; a separate
seven-run confirmation had stable current samples at 1.59-1.60 ns and one
reciprocal 2.92 ns parent outlier, establishing neutral normal behavior.

Generated-code review found and removed an extra context-to-runtime release
adapter in owner-local string code. Atom-kind and numeric/index rejection shells
were restored inline. Both natural object orders and explicit alignment of
`JS_ConcatString2` were benchmarked; the retained core-first order was best for
string construction, and explicit alignment was reverted because it worsened
the result. The final small array-index header shell avoids moving the complete
parser into a header; its layout leaves confirmed `array_push`, RegExp ASCII,
and `string_build2` observations. A final five-repeat `perf stat` comparison for
`string_build2` measured +0.58% instructions, +0.65% branches, and +1.69%
cycles. These remaining results are deferred as `[~]` to Final Performance
Stabilization; they do not block subsequent structural work. Logs are under
`/tmp/qjs-atom-*`; the exact parent checkout is `/tmp/quickjs-b58d08a`.

### Exact next steps

1. Reassess the 12,952-line residual core for natural runtime/context/jobs and
   object/shape/property/GC/value ownership after the atom/string seam. Keep
   context teardown and class/GC callbacks with the lifetime owner when moving
   them would create broad callback APIs or cycles.
2. Implement only the defensible residual owner boundaries, validating each as
   an independent milestone. Large coupled object/value/conversion regions may
   remain together when a finer split would broaden hot interfaces.
3. Once primary core decomposition is structurally stable and correctness-
   validated, proceed to secondary targets despite recorded `[~]` performance
   items, then resolve all confirmed meaningful refactor-induced non-LTO losses
   during Final Performance Stabilization.

## Number/value and operator extraction milestone (authoritative current state)

### Architecture and boundary decision

The numeric/value conversion domain is now an independent 4,449-line
`src/quickjs/number.c`; the residual `quickjs.c` is 8,695 lines. The new owner
contains primitive coercion and boolean conversion, numeric parsing, complete
BigInt/multiprecision arithmetic, number/integer/string conversion, the public
numeric conversion APIs, equality, and every numeric/operator slow path used by
the VM. BigInt and the slow operators deliberately remain together because the
operators directly consume the private multiprecision add/multiply/divide,
logic, shift, power, normalize, and comparison implementation; splitting them
would expose that implementation rather than create a natural owner boundary.

The cross-TU interface reuses the already established `internal-number.h` and
`internal-operator.h` surfaces. `qjs_to_primitive[_free]`, the full Int32 slow
conversion, BigInt64 conversion, and the small value predicates are the only
new generic owner entries required by residual object code. The original
`JS_ToFloat64Free` tagged immediate/float fast path remains a scoped inline shell
whose uncommon coercion path enters `qjs_to_float64_free_slow`; the Uint32
conversion remains an inline alias to the Int32 owner entry. The tiny BigInt
sign layout accessor also remains inline for residual debug printing. No large
function moved to a header. Public API names and behavior are unchanged.

This finer conversion boundary was retained before moving shapes/properties
because compilation showed a narrow interface already existed from the VM and
builtin extractions. Shape/property/object allocation, free-value/GC,
exceptions/backtraces, and value diagnostics remain cohesive in the residual
core. The current audit recommends extracting runtime/context/jobs/class
registry next through cold object-lifecycle hooks, then moving the remaining
object/value owner without separating its mutually dependent shape, property,
GC, and exception paths.

### Validation and performance

Acceptance validation on the documented source state:

- independent GCC 16.2 WERROR compilation of `quickjs.c` and `number.c`: PASS;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all`, including normal and
  `CONFIG_CHECK_JSVALUE` compilation of both affected engine TUs: PASS;
- full GCC repository `make test`: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- Clang 23.1 WERROR syntax for both affected TUs, normal and
  `CONFIG_CHECK_JSVALUE`: PASS; GCC leak/atom-dump syntax variants: PASS;
- normal `qjs` dynamic exports exactly match the parent 292-name set;
  `git diff --check`: PASS.

Current GCC 16 non-LTO sizes are: qjs 5,191,120 bytes with 1,057,598 text
bytes; qjsc 5,179,288/1,031,526 text; run-test262 5,293,192/1,057,255 text;
and libquickjs.a 9,932,248 bytes. Relative to exact parent `cb0b0ef`, qjs text
decreased by 832 bytes; file growth is debug/object metadata for the extra TU.

The seven-run CPU-pinned comparison against an exact freshly built `cb0b0ef`
parent is: `prop_read` -0.14%, `prop_write` +1.32%, `prop_create` -0.52%,
`array_push` -5.73%, `array_read` -0.09%, `array_slice` 0.00%, `func_call`
-0.18%, `int_arith` +0.35%, `float_arith` -0.13%, `typed_array_read` +7.30%,
`string_to_int` -0.86%, `string_build2` -1.92%, `regexp_ascii` -1.24%,
`regexp_replace` +3.13%, and `sort_bench` -2.36%.

The only initially meaningful isolated loss was `typed_array_read`. A separate
nine-run read/write confirmation narrowed it to +4.01% read while write improved
6.29%. The typed-array owner disassembly is byte-for-byte unchanged and no new
conversion, call, or trampoline occurs in the fast path. Five-repeat counters
used a parent calibration count twice the current count; normalized per count,
current instructions and branches decreased about 2.4%, branch misses were
flat, and cycles increased about 3.8%. This is recorded as an intermediate
placement/scheduling-sensitive `[~]` observation rather than prompting code
alignment work while the object/runtime layout is still changing. It remains
for Final Performance Stabilization. Logs are under `/tmp/qjs-number-build/`;
the exact parent checkout is `/tmp/quickjs-cb0b0ef`.

### Exact next steps

1. Extract runtime/context/jobs and class-registry ownership without exporting
   the standard-class callback table. Keep that table with object/GC and use
   cold owner-level initialization, GC-shutdown, context mark/release, and
   shape-hash teardown hooks while preserving teardown order exactly.
2. Move the remaining natural object/value owner: shapes, properties, object
   allocation, free-value/GC, exceptions/backtraces, and value diagnostics.
   Preserve the hot lookup, mutation, array, and free-value paths together.
3. Validate the stable primary-core decomposition, then proceed to secondary
   targets despite recorded `[~]` performance issues. Resolve all remaining
   confirmed meaningful refactor-induced non-LTO losses during Final
   Performance Stabilization after structural migrations finish. Final formal
   performance validation uses matched pristine/final normal non-LTO builds and
   the same isolated repeated methodology separately for GCC 16.2 and Clang
   23.1; supported LTO builds remain correctness requirements and their
   performance is diagnostic only.

## Runtime/context/jobs extraction milestone (authoritative current state)

### Architecture and boundary decision

`src/quickjs/runtime.c` is now an independent 732-line owner for runtime and
context construction/destruction, job queues, stack policy, runtime/context
opaque APIs, class-ID allocation and class registration, runtime exception
state, and cold bytecode-buffer allocation helpers. The residual `quickjs.c` is
8,018 lines and now consists almost entirely of the cohesive object/value core:
the standard-class callback table, shapes, object allocation, properties, fast
arrays, free-value/GC, exceptions/backtraces, value diagnostics, and remaining
object adapters.

The standard-class table stayed object-owned because it directly names the
object, function, array, typed-array, collection, and RegExp finalizer/mark
callbacks. Runtime initialization calls `qjs_object_init_classes()` before the
existing VM/primitive/module class setup, then `qjs_object_init_shapes()` to
bind C-function-data call behavior and initialize the shape hash. Runtime
teardown preserves the exact order: jobs, object GC/leak shutdown, class array,
atom/string owner, shape hash, allocator report, runtime allocation. Context
teardown similarly delegates only debug dumping and the five cached-shape
releases. GC enters the runtime-owned context marker through one cold hook.

Strict-mode property checks continue to use the existing scoped inline helper.
The interrupt counter fast path also remains inline, while its rare slow path
and exception construction remain object-owned; this avoids adding an error
construction interface merely to make the cold runtime file larger. The
boundary therefore exposes owner-level lifetime operations rather than the
callback table or individual shape internals.

### Validation and performance

Acceptance validation on the documented source state:

- independent GCC 16.2 WERROR compilation of `quickjs.c` and `runtime.c`: PASS;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all`, including normal and checked
  compilation of the affected TUs: PASS;
- full GCC repository `make test`: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- Clang 23.1 WERROR syntax for both affected TUs, normal and
  `CONFIG_CHECK_JSVALUE`: PASS; combined GCC leak/atom/shape/object/memory dump
  syntax: PASS;
- normal `qjs` dynamic exports exactly match the parent 292-name set;
  `git diff --check`: PASS.

Current GCC 16 non-LTO sizes are: qjs 5,202,648 bytes with 1,057,334 text
bytes; qjsc 5,190,808/1,031,390 text; run-test262 5,304,664/1,057,119 text;
and libquickjs.a 9,981,912 bytes. Relative to exact parent `e035255`, qjs text
decreased by 264 bytes; file growth is object/debug metadata for the extra TU.

The five-run CPU-pinned comparison against an exact freshly built `e035255`
parent is: `prop_read` +0.86%, `prop_write` +0.44%, `prop_create` -0.26%,
`array_push` -0.38%, `array_read` +0.78%, `array_slice` +2.53%, `func_call`
+1.00%, `int_arith` -0.42%, `float_arith` +0.22%, `typed_array_read` -5.45%,
`typed_array_write` -4.87%, `string_to_int` -1.05%, `string_build2` +2.55%,
`regexp_ascii` +4.27%, `regexp_replace` +0.02%, and `sort_bench` -0.83%.
No runtime-boundary workload has a confirmed meaningful regression, so no
layout correction was attempted. Logs are under `/tmp/qjs-runtime-build/`; the
exact parent checkout is `/tmp/quickjs-e035255`.

### Exact next steps

1. Move the residual object/value core into `src/quickjs/object.c`, retaining
   shapes, properties, object allocation, fast arrays, free-value/GC,
   exceptions/backtraces, value diagnostics, the standard-class callbacks, and
   object adapters together. Remove now-unnecessary lifecycle bridges that
   become owner-local, but keep the runtime-facing cold hooks.
2. Independently compile and validate the final primary-core owner, run exact
   Test262 and the focused GCC non-LTO screen, and investigate only confirmed
   structural defects before recording layout-only issues for final
   stabilization.
3. Once primary-core correctness is stable, proceed to secondary runtime/library
   targets despite recorded `[~]` performance observations, then developer
   tooling. Final formal non-LTO performance validation remains required for
   matched GCC and Clang pristine/final builds.

## Object core ownership milestone (authoritative current state)

### Architecture and boundary decision

The primary engine decomposition is structurally complete. The 8,018-line
residual root `quickjs.c` was moved mechanically to `src/quickjs/object.c`; its
local private-header includes and the Makefile object path were adjusted, with
no semantic source changes. This final owner retains values, shapes,
properties, object allocation, fast arrays, free-value/GC, exceptions and
backtraces, diagnostics, the standard-class callback table, and object adapters.

Keeping these areas together preserves the private object/value representation,
hot lookup and mutation paths, free-value/GC coupling, and direct class callback
references. Further splitting at this point would export representation-heavy
interfaces or insert boundaries into important hot paths rather than establish
a distinct natural owner. Runtime-facing initialization, teardown, marking, and
diagnostic hooks remain the narrow cold interfaces established by the preceding
runtime milestone. There is no remaining root `quickjs.c` build input.

### Validation and performance

Acceptance validation on this source state:

- independent GCC 16.2 WERROR and Clang 23.1 WERROR compilation of
  `src/quickjs/object.c`: PASS;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all`, including normal and checked
  compilation of the object owner: PASS;
- full GCC repository `make test`: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- normal `qjs` dynamic exports remain exactly 292 names.

Current GCC 16 non-LTO sizes are: qjs 5,202,656 bytes with 1,057,366 text
bytes; qjsc 5,190,816/1,031,422 text; run-test262 5,304,712/1,057,151 text;
and libquickjs.a 9,981,984 bytes. Relative to exact parent `37c2d7f`, each
executable gains 32 text bytes; the path-only source move changes diagnostic
source strings but introduces no new call or semantic boundary.

Five alternating parent/current runs of the focused GCC 16 non-LTO screen were
executed serially on CPU 2. Median changes were: `prop_read` +0.36%,
`prop_write` -0.18%, `prop_create` +0.09%, `array_push` -0.17%, `array_read`
-0.43%, `array_slice` +1.89%, `func_call` +1.47%, `int_arith` +0.00%,
`float_arith` +1.17%, `typed_array_read` +0.64%, `typed_array_write` +0.21%,
`string_to_int` +0.03%, `string_build2` -0.24%, `regexp_ascii` +0.92%,
`regexp_replace` -1.21%, and `sort_bench` -0.42%. No workload has a confirmed
meaningful regression, so no layout correction was attempted. Logs are under
`/tmp/qjs-object-build/`; the exact parent checkout is
`/tmp/quickjs-37c2d7f`.

### Exact next steps

1. Begin secondary runtime/library work by remapping the current
   `quickjs-libc.c` ownership and dependency graph. Extract only natural
   std/file-loader and host/event boundaries with narrow state interfaces;
   validate std/os, loaders, dynamic modules, workers, handlers, promises, and
   event-loop shutdown.
2. Assess and, where clean, implement the RegExp compiler/executor and Unicode
   runtime ownership splits with their target-specific correctness and focused
   performance validation.
3. Evaluate developer-tooling targets last, then run final supported-
   configuration correctness and matched pristine/final GCC and Clang normal
   non-LTO performance stabilization. LTO performance remains diagnostic only.
