# QuickJS modularization implementation plan

## Purpose and constraints

This plan decomposes the current QuickJS implementation into real, normally
linked C translation units while preserving the behavior, ABI/API, portability,
bytecode formats, object/value representation, garbage-collection semantics,
and non-LTO performance of commit `04be246`.  `task.md` is authoritative; this
document records the implementation path and will be updated as dependency and
validation evidence is collected.

The work is an extraction, not an engine redesign.  Public declarations remain
in `quickjs.h`, `quickjs-libc.h`, `libregexp.h`, and `libunicode.h`.  Internal
interfaces will be private to `src/`, narrowly grouped by owner, and hidden from
the public archive interface where the platform permits.  Mechanical moves must
retain the original copyright/license notice and should not reformat moved code.

Progress markers are `[ ]` not started, `[~]` in progress or structurally done
with an unresolved non-LTO performance issue, and `[x]` completed and validated.

## Paused implementation status (2026-09-13)

Implementation is intentionally paused at commit `f8fa5bc` after completing the
frontend, module, bytecode, builtin-composition, and all planned builtin-family
translation units. The residual `quickjs.c` is 21,553 lines and still owns the
allocator/runtime, atoms/strings, objects/properties/GC/conversions, opcode slow
paths, the complete interpreter, and generator/async execution. A bounded
function/VM extraction was attempted after `f8fa5bc` and fully rolled back
because substantial private-boundary integration remained; there is no partial
source or header from that attempt in the tree.

The current engine is coherent and correctness-validated, but the overall task
is not complete. Meaningful non-LTO RegExp/layout observations remain open, so
the affected engine/builtin milestones stay `[~]`. No secondary runtime/library
or developer-tooling source split has begun.

## Inspected starting architecture

The starting tree has one 61,424-line `quickjs.c`, one-object build rules in the
root `Makefile`, and no `src/` implementation hierarchy.  Its important natural
source regions are:

- lines 1-1415: configuration, engine-private representations, atoms/opcodes,
  forward declarations, and class callbacks;
- lines 1416-2867: allocator and runtime/context/job lifecycle;
- lines 2868-5118: atoms, strings, ropes, string buffers and class registry;
- lines 5119-16199: shapes/properties/objects, GC, exceptions, conversions and
  operators;
- lines 16200-21782: iterators, calls, the complete direct-threaded interpreter
  dispatch (`JS_CallInternal` alone spans roughly lines 17746-20711), generators,
  and async execution;
- lines 21783-29850 and 31664-37376: lexer/parser, scope and variable resolution,
  bytecode emission and optimization. Import/export parsing resumes after the
  intervening module-runtime region, so this owner is mechanically noncontiguous;
- lines 29664-31663: module allocation/lifecycle, resolution, linking and
  evaluation runtime (overlapping the apparent parser marker at its front edge);
- lines 37376-39507: object-list helpers and bytecode/object serialization;
- lines 39509-61424: a mixed composition/core-object prefix (including object
  creation, constructor/function-list installation and `JS_ToObject`) followed by
  ECMAScript builtins, with clear large regions for objects/functions/errors,
  arrays/iterators, primitives and strings, RegExp, JSON/Reflect/Proxy/Symbol,
  collections, promises/async, globals/date, typed arrays/ArrayBuffer/Atomics,
  and weak references. The 39509 marker is not itself a safe builtin boundary.

The apparent sequential seams are not independent interfaces.  Runtime class
tables refer to builtin-owned finalizers and exotic-method tables; the frontend
and serializer consume runtime representations; builtins call core property,
conversion, string, call, iterator, and collection helpers; parser/compiler hot
paths share bytecode structures with the interpreter.  Extraction therefore
starts with coarse translation units and promotes only symbols proven to cross a
chosen boundary.  Static linkage remains the default.

The secondary targets have distinct architectures:

- `quickjs-libc.c` combines file/module loading (395-735), the `std` module and
  its cohesive FILE/formatting implementation (167-1731), synchronous `os`
  services, event/worker machinery (1732-4060), and host helper/loop/rejection
  handling (4061-end). Event polling, timers, signals, handlers, workers,
  rejected promises, and host lifecycle share `JSThreadState`, so that state and
  its poll/teardown owner should remain cohesive initially.
- `libregexp.c` has a compiler/parser region through bytecode register analysis
  and emission (roughly lines 1-2615) and a VM region (roughly 2616-end).  The
  public bytecode is the natural dependency boundary; parsing and compilation
  stay together.
- `libunicode.c` has case conversion/canonicalization (1-378 and 547-743), generic
  character-range storage/operations (379-546), normalization (761-1243), and
  identifier/property/range/sequence lookup (746-759 and 1245-end). Generated
  table data needs a deliberate single owner so it is not duplicated by each TU.
- `unicode_gen.c` and `run-test262.c` are developer tools.  They are lower
  priority and will be split only after runtime libraries stabilize, and only if
  actual state ownership permits a small internal interface.

## Intended directory and dependency architecture

The likely end state, subject to compile/benchmark evidence at each finer split,
is:

```text
src/
  quickjs/
    internal-config.h       build/platform policy used by engine TUs
    internal-types.h        minimal foundational runtime representations
    internal-object-types.h object/string/shape concrete layouts
    internal-bytecode.h     bytecode/function concrete layouts
    internal-module.h       module concrete layout and narrow API
    internal-runtime.h      allocator/runtime/GC/value internal API and hot inline helpers
    internal-object.h       atom/string/shape/property/object internal API
    internal-function.h     call/interpreter/function internal API
    internal-frontend.h     parser/compiler-only representations and API
    allocator.c             allocator backend and public allocation API
    runtime.c               runtime/context, jobs, GC/lifetime where separable
    atom-string.c           atoms, strings, ropes and string buffers
    object.c                shapes, properties, objects, exceptions
    number.c                value conversions, BigInt, numeric slow operators
    function-vm.c           calls, closures, bytecode VM, generators/async state
    frontend.c              lexer, parser, scopes and bytecode compiler
    module.c                module resolution, linking and evaluation
    bytecode.c              binary object/bytecode read and write
    builtin-object.c        Object/Function/Error and intrinsic setup
    builtin-array.c         Array and synchronous iterator builtins
    builtin-primitive.c     Number/Boolean/String/Math
    builtin-regexp.c        JS RegExp integration
    builtin-json.c          JSON/Reflect/Proxy/Symbol
    builtin-collection.c    Map/Set and related iterators
    builtin-async.c         Promise and async builtins
    builtin-global.c        global functions, URI and Date
    builtin-typed-array.c   ArrayBuffer, typed arrays, Atomics and weak refs
  libc/
    internal.h
    std.c
    os.c
    host.c
  regexp/
    bytecode-internal.h
    compile.c
    exec.c
  unicode/
    table-internal.h
    char-range.c
    case.c
    normalize.c
    property.c
  unicode-gen/              only if the assessed split is low-coupling
  test262/                  only if the assessed split is low-coupling
```

The exact number of QuickJS runtime and builtin files is intentionally a target,
not a mandate.  A file remains coarser when splitting it would create a broad
internal API or impair a critical path.  `dtoa.c`, `cutils.c`, `qjs.c`, and
`qjsc.c` remain cohesive.  Generated/declarative headers remain at project level
unless include-path evidence makes a move clearly beneficial.

Dependency direction is public headers and utility libraries -> private shared
representations -> owning engine subsystems -> initialization/composition.
Frontend, module, and bytecode modules may use runtime/object/function internal
APIs; the runtime must not depend on parser representations. Builtin modules may
use the core internal APIs but must not depend on frontend implementation details.
RegExp execution consumes compiler-produced bytecode and shared opcode/layout
definitions, not parser state. Unicode normalization and property modules may
use the range owner through a narrow range API, never the reverse. Unicode table
storage has one implementation owner, with generated private declarations or
narrow accessors used by consumers; object/archive size checks must prove the
split did not duplicate the large generated `static const` tables.

## Validation and measurement policy

All primary builds use explicit toolchains under `/home/yanjie/opt`:

- GCC `/home/yanjie/opt/gcc-16.2.0/bin/gcc` for the intermediate reference
  non-LTO build and one final non-LTO performance comparison;
- Clang `/home/yanjie/opt/clang-23.1.1/bin/clang` for compiler portability,
  diagnostics, and an independent matched final non-LTO performance comparison;
- their matching `ar`, `nm`, `size`, and `objdump` tools where applicable.

Before structural edits, record commit/tree status, compiler versions, platform
and CPU, full build and repository test results, actual Test262 failure set,
`qjs` and library sizes, and repeated non-LTO microbenchmarks.  If the supplied
Test262 checkout/config makes a full run practical, save its exact failure list;
otherwise record the concrete blocker and run the broadest bounded subset that
is reproducible.  The repository lacks an installed `tests/bench-v8` corpus at
inspection time, so final broader-workload validation will use it only if it can
be obtained without substantial porting; absence is documented rather than
masked.

Each structural milestone gets, in order: independent compilation of every new
`.c`; clean and parallel normal builds; focused tests; the normal `make test`
suite; archive/link/API checks; and a lightweight, isolated, repeated non-LTO
microbenchmark screen.  Suspect performance changes are rerun and compared by
median.  Confirmed cross-TU losses are investigated using compiler optimization
reports and disassembly; a boundary adjustment or a small `static inline` helper
in the narrow owner header is preferred over blanket LTO or large forced-inline
functions.  Existing `force_inline`/`js_force_inline` behavior is retained.  A
new forced inline requires evidence recorded in `CHECKPOINT.md`.

Final configurations include GCC and Clang optimized builds, debug/check builds,
supported GCC and Clang LTO build/correctness, supported sanitizer builds (ASan
and UBSan where the repository/toolchain supports them), clean and parallel
builds, library/examples, and the complete available functional suite. Matched
pristine-versus-final non-LTO performance comparisons are required separately
for GCC and Clang with the same affinity, isolation, repetition, and statistical
methodology. LTO performance is informational; non-LTO correctness and
performance are completion gates. Test262 is compared by exact failure names,
not totals.

Binary-size comparisons use unstripped `qjs`, text/data/bss section sizes, and
`libquickjs.a` member/total size under identical flags.  New public exported
symbols are checked with `nm`; internal cross-TU APIs are not added to public
headers and should use hidden visibility where portable without breaking static
archives or supported shared consumers.

Because the existing Makefile reuses `.obj` across compiler choice, LTO, M32,
TSan, and WERROR changes, every configuration transition starts with `make
clean` (or later uses a configuration-unique `OBJDIR`). This avoids stale
objects silently invalidating results. The LTO configuration must continue to
produce both `libquickjs.lto.a` from LTO objects and the separately compiled
non-LTO `libquickjs.a`. Architecture-sensitive M32 and the CI sanitizer/cross
variants are explicitly attempted when local toolchain/runtime support exists;
concrete unavailability is recorded rather than silently omitted.

## Milestones and commit structure

### 0. Planning and baseline

- [x] Complete source/build/test inspection; review this plan against `task.md`,
  subagent findings, concrete symbol ownership, and the current tree.  Revise
  unsupported boundaries before implementation.
- [x] Establish the reproducible pristine baseline and store raw results under
  a clearly named ignored or documented baseline directory as appropriate.
- [x] Create `CHECKPOINT.md` with provenance, results, pre-existing failures,
  benchmark variance, and exact reproduction commands.

No implementation commit is made for baseline-only generated artifacts.  The
reviewed plan and checkpoint accompany the first coherent structural commit.

### 1. Multi-TU engine foundation and low-risk outer extractions

Status: [~] structurally complete through the function/VM extraction; residual
core ownership and deferred final performance stabilization remain.

- [x] Create `src/quickjs/` and private configuration/type headers by moving the
  existing preamble and concrete private representations losslessly. Start with
  base macros/class IDs and opaque forwards; separately own runtime/context,
  object/string/shape, bytecode/module, and frontend-only layouts. Record which
  modules require each concrete layout, and keep behavior declarations in owner
  headers rather than a catch-all declaration dump.
- [x] Teach every Makefile object mode (`.o`, `.host.o`, `.pic.o`, `.nolto.o`,
  `.debug.o`, `.fuzz.o`, `.check.o`) to compile nested sources and create nested
  object directories.  Replace the single engine object in every consumer with
  an ordered engine-object list and extend dependency inclusion recursively.
- [ ] Replace special monolithic compile/link recipes for `regexp_test` and
  `unicode_gen` with the corresponding modular object sets, and preserve the
  generated Unicode-table dependencies in every applicable object variant.
- [x] First extract binary object/bytecode I/O as one coherent commit. Its narrow
  cross-owner API includes module creation and ArrayBuffer/SAB construction;
  validate module, typed-array and shared-buffer serialization explicitly.
- [x] Next extract frontend/compiler and module runtime as distinct owners, then
  builtin clusters one coherent commit at a time. Leave the hot runtime/
  interpreter together initially. Parser/compiler extraction mechanically gathers
  its noncontiguous source ranges around module runtime. Determine crossing
  symbols from actual compiler/link diagnostics and owner-specific declarations.
  Module runtime and the coarse frontend are complete and correctness-validated.
  Intermediate code-layout observations keep the item `[~]` until stabilization.
- [x] Verify public header/API and bytecode round-trip behavior, qjsc-generated
  examples, modules, fuzz archive compilation, CONFIG_CHECK_JSVALUE, GCC/Clang,
  clean/parallel builds, tests, Test262 comparison, and microbenchmark screen.

First commits: `build: support modular QuickJS sources`, then
`refactor: extract QuickJS bytecode serialization`, followed by separately
validated frontend/module and builtin-cluster commits.

This coarse step is the rollback/bisect anchor.  It proves real separate
compilation before risky hot-core or builtin subdivisions.

### 2. Core ownership split

- [~] Extract allocator/runtime/context/job ownership. The allocator backend,
  default malloc implementation, and public runtime/context allocation API now
  have an independent owner with a two-function private lifecycle seam. Runtime/
  context/jobs remain in the residual core pending the atom-string and object
  ownership passes. Keep GC release/marking,
  weak-reference hooks, runtime class registration, and context teardown with the
  object/function lifetime owner until module/bytecode/class callback ownership
  is explicit.
- [x] Extract atoms, strings, ropes, and string-buffer operations.  Preserve hot
  atom/string/value accessors as scoped `static inline` helpers when already
  inline or when non-LTO evidence shows the call boundary is material.
  `atom-string.c` now owns atom tables/lifetime, raw strings, C-string conversion,
  string buffers, comparison, ropes, and concatenation. Atom kind, tagged/index
  classification, numeric-index rejection, string reads/equality, and zero-ref
  decrement shells remain scoped inline; runtime teardown and accounting cross
  through owner-level hooks rather than exposing atom storage.
- [~] Extract shapes/properties/objects and separate conversions only where the
  cross-surface remains narrow. Evidence from the established VM/builtin seams
  supported extracting `number.c` first: it now owns primitive/number/string
  conversion, BigInt arithmetic, public numeric conversion APIs, equality, and
  all numeric/operator slow paths. BigInt and operators remain together because
  separating them would export the private multiprecision arithmetic layer.
  The float conversion tagged fast path and Uint32 alias remain scoped inline.
  Shapes/properties/objects/GC/exceptions remain in the residual core for the
  next ownership pass. Property lookup/set, fast arrays, exception paths, and
  the recorded typed-array cycle-only observation still require final
  disassembly and benchmark attention.
- [x] Keep calls, bytecode dispatch, closures, var refs, generators, and async
  execution in `function-vm.c` unless evidence supports a call/runtime split.
  The complete direct-threaded interpreter, opcode-adjacent iterator support,
  closures/var refs, calls, generators, and async resume paths now have one
  independent owner. A post-extraction dependency review found that splitting
  iterator opcode support or generator/async execution would expose additional
  hot interpreter or call/resume state, so no finer natural boundary is retained
  at this milestone. Interpreter dispatch and opcode handlers remain within one
  translation unit; there is no per-opcode modularization.
- [x] Resolve `JS_NewContext` -> `JS_AddIntrinsicBasicObjects` and runtime class
  callback dependencies through a small builtin composition entry point and
  narrow lifecycle APIs, not a speculative global registry or dozens of exported
  finalizers.
- [~] At each extraction, compile/test/measure before the next. The function/VM
  state is correctness-validated and its initially measured array/string/RegExp
  losses received localized hot-placement and direct-owner remediation. Existing
  pristine-baseline string/RegExp layout regressions remain recorded for final
  stabilization. The allocator extraction is also correctness-validated; keeping
  the core before the cold allocator object recovered its isolated string loss,
  while an isolated RegExp ASCII layout observation remains recorded. The
  atom/string milestone is correctness-validated and isolated property/array/
  RegExp behavior is neutral, but its confirmed `string_build2` loss remains
  `[~]` performance work for final stabilization.

Likely commits, adjusted to coherent buildable boundaries:

- `refactor: extract QuickJS allocator subsystem`
- `refactor: extract QuickJS runtime lifecycle`
- `refactor: extract QuickJS atom and string subsystem`
- `refactor: extract QuickJS object subsystem`
- `refactor: isolate QuickJS function and VM subsystem`

### 3. Frontend and bytecode refinement

- [x] Review the coarse frontend with the compiled dependency graph.  Separate
  module resolution/evaluation only if it does not require exposing parser-local
  state; otherwise keep it with frontend and document why.
- [x] Separate lexer/parser from scope/bytecode lowering only if their shared
  structures can live in `internal-frontend.h` without becoming a second engine
  representation header.  Parsing plus compilation may remain one sizeable but
  cohesive module when that is the more maintainable ownership boundary.
- [x] Keep serialization/object-list helpers together unless the object-list API
  demonstrably serves another owner.  Validate byte-for-byte serialized output
  for deterministic fixtures and read/write compatibility in both directions.

Commit: `refactor: define QuickJS frontend and bytecode boundaries`

### 4. Builtin decomposition

- [~] Split builtins in dependency-aware batches: base object/function/error;
  arrays and synchronous iterators; primitive/string/math; RegExp; JSON/Reflect/
  Proxy/Symbol; collections; promise/async; global/date; typed-array/Atomics/weak
  references.  Merge adjacent batches when class tables, finalizers, or helper
  traffic show tighter ownership than the conceptual label.
  All listed builtin clusters are extracted and correctness-validated. Current
  non-LTO RegExp/layout regressions keep this item `[~]`.
- [x] Assign the mixed post-serialization core helpers (`JS_NewObjectProtoList`,
  constructor/function-list setup, `JS_ToObject`, and related functions) to the
  object/composition owner before mechanically slicing builtin source regions.
- [x] Central intrinsic registration remains a small composition layer.  Each
  builtin module owns its method/property tables, class callbacks, and state.
- [x] Re-run configuration-sensitive qjsc `-fno-*` examples so optional intrinsic
  installation and dead-code behavior remain correct.  Track archive and final
  executable size after each batch; avoid many thin TUs that add layout/size cost.

Commits group only related validated batches, for example:

- `refactor: split foundational QuickJS builtins`
- `refactor: split collection and async builtins`
- `refactor: split RegExp and typed-array builtins`

### 5. Secondary runtime/library targets

- [ ] `quickjs-libc.c`: first extract cohesive `std`/FILE formatting and file/
  module loading behind narrow loader hooks; keep `os`, event polling, timers,
  signals, rejected promises, workers, and thread state together in the host/
  event owner.
  Then split host helper/loop lifecycle if its thread-state API is small.  Split
  worker/event code only if the resulting state owner and wake/message interface
  are clear.  Exercise std/os, loaders, dynamic modules, workers, rw handlers,
  promises, and event-loop shutdown.
- [ ] `libregexp.c`: share only opcode enum/order and bytecode-header layout in a
  regexp-only internal header; keep `REParseState` and `REExecContext` TU-private,
  extract compiler/parser and executor TUs, and keep register analysis
  with compilation.  Validate public metadata accessors, fuzz targets, regexp
  behavior through QuickJS and standalone `regexp_test`; benchmark compile-heavy
  and execute-heavy cases separately.
- [ ] `libunicode.c`: extract generic character ranges, case/canonicalization,
  normalization, and property/sequence modules where table ownership stays
  unambiguous. Keep `cr_regexp_canonicalize` with case implementation and
  sequence properties with property decoding. Keep normalization recursion/
  composition together, and place only compressed table-index helpers in the
  small table-private header. Validate case folding,
  identifier checks, normalization forms, property/category lookup, regexp
  canonicalization, and sequence properties.

Likely commits:

- `refactor: split QuickJS host library modules`
- `refactor: split RegExp compiler and executor`
- `refactor: split Unicode runtime subsystems`

### 6. Developer-tooling assessment and extraction

- [ ] `unicode_gen.c`: map generator-wide database state and call graph after the
  runtime Unicode split.  If clean, separate input/database construction, case
  and normalization generation, property/sequence generation, and emission/self
  tests into a small number of TUs.  Otherwise retain the cohesive file and
  document the coupling.  Compare generated `libunicode-table.h` byte for byte
  from identical Unicode inputs and run generator self-tests.
- [ ] Give generator-wide Unicode DB, emoji stores, conversion tables, and size
  counters an explicit private context/state owner rather than a broad extern
  header. Replace `USE_TEST`'s inclusion of `libunicode.c` with normal linkage to
  modular Unicode objects and public-behavior tests or narrowly gated private
  test hooks; no `.c` inclusion or public runtime API exposure is acceptable.
- [ ] `run-test262.c`: map config, metadata, test discovery, agent/worker,
  execution, expected-failure, and reporting state.  Extract metadata/config and
  reporting only if runner state need not become global or broadly exposed.
  Validate serial and threaded runs, filtering, exclusions, expected-failure
  comparison, update ordering, and statistics on a bounded deterministic corpus
  before full Test262 comparison.
- [ ] Prefer the low-risk name/path list utility and coherent agent/$262 harness
  boundaries. Keep metadata parsing, evaluation, and expected-failure comparison
  together until a real `TestMetadata`/runner-state object replaces the current
  shared globals; otherwise leave the runner cohesive and document the decision.

Commits are made only for actual beneficial splits:

- `refactor: modularize Unicode table generator`
- `refactor: modularize Test262 runner`

An intentionally cohesive target receives a documented decision but no churn.

### 7. Final performance stabilization and completion

- [ ] Rebuild pristine baseline and final states separately with identical GCC
  non-LTO flags and identical Clang non-LTO flags. For each compiler, run the
  same CPU-pinned, isolated, repeated microbenchmark methodology and the
  available broader ECMAScript-only V8 v7 corpus; evaluate aggregate and
  individual workloads and record compiler-specific differences.
- [ ] Revisit every deferred meaningful non-LTO regression.  Inspect missed
  inlining, constants, dead code, linker ordering, code layout, and generated
  instructions; attempt proportionate boundary/header/ordering remedies and
  rerun correctness after every retained change.  No confirmed meaningful
  refactor-induced non-LTO regression may remain at completion.
- [ ] Run the full supported configuration matrix, exact Test262 failure-set
  comparison, host/worker, RegExp, Unicode, serialization/generator, qjsc/example,
  archive/API/export, size, and sanitizer validation.
- [ ] Perform a fresh read-only architectural/diff review for accidental semantic
  edits, over-broad internal APIs, license loss, unsupported public exposure,
  dependency cycles, artificial modules, and stale build rules.
- [ ] Update this plan and `CHECKPOINT.md` to fully complete, with no implementation
  work remaining, and produce the final report required by `task.md`.

Commit: `refactor: finalize modular QuickJS architecture`

## Risks and decision points

1. **Private representation breadth.**  Nearly every subsystem needs `JSObject`,
   `JSString`, `JSShape`, `JSFunctionBytecode`, runtime/context, and module layouts.
   These concrete representations must be stratified across foundational,
   runtime/context, object, bytecode/module, and frontend-only layout headers;
   behavior declarations stay in owner headers to avoid replacing `quickjs.c`
   with a monolithic internal API.
2. **Class callback inversion.**  Runtime startup currently references callbacks
   implemented far later in builtins.  Prefer builtin-supplied class descriptors
   or narrow extern callbacks; do not make runtime depend on entire builtin
   implementation headers. Preserve the fixed class-ID ordering of
   `js_std_class_def` when ownership changes.
3. **Hot cross-TU calls.**  Property lookup/set, value conversion, string/atom,
   call dispatch, iterator and fast-array helpers are the greatest non-LTO risk.
   Co-locate first, then split with measurement.  LTO is never used to conceal a
   normal-build regression. Preserve the existing forced inlines for
   `find_own_property1`/`find_own_property`, `can_extend_fast_array`, and RegExp
   lastIndex get/set, and the current narrow atom/shape/value/stack inline helpers.
4. **Frontend entanglement.**  parser/module/compiler source order is not proof of
   separability.  A single frontend TU is acceptable and preferable to a broad
   parser API if compile evidence confirms dense sharing.
5. **Builtin tables and static constants.**  Registration tables often contain
   function pointers that naturally cross an initialization boundary.  The
   owning module should expose one descriptor/init entry point, not every table.
6. **Nested Makefile objects.**  basename-only dependency-file naming currently
   risks collisions.  Preserve all build variants and recursively include `.d`
   files; create parent object directories deterministically under parallel make.
7. **Archive/public symbols.**  Static archives can resolve private externs, but
   consumers may observe their names.  Use a consistent internal prefix and
   hidden visibility when supported, and verify shared-library examples and
   cross-compilation assumptions before relying on visibility attributes.
8. **Test262 duration and baseline drift.**  Pin the supplied checkout/commit and
   save exact failures.  Bound long runs and preserve logs so a timeout is
   distinguishable from a semantic failure.
9. **Performance noise.**  CPU frequency and background load can swamp small
   differences.  Run isolated repeated samples, retain raw output, and only alter
   architecture for reproduced practically meaningful results.
10. **Secondary over-splitting.**  Host event/worker state, Unicode generated
    tables, and generator/test-runner state may justify fewer files than their
    conceptual responsibilities suggest.  A documented cohesive decision meets
    the maintainability goal better than artificial accessors or global state.

## Plan self-review checklist

- [x] Confirm every requirement and target in `task.md` has an implementation or
  explicit evidence-based evaluation path.
- [x] Confirm the module boundaries match actual source regions and ownership,
  not merely the examples in the task.
- [x] Confirm milestone order keeps the tree buildable and the core stable before
  secondary work.
- [x] Confirm public APIs, static linkage, license retention, hot paths, LTO,
  exact Test262 failures, binary size, portability, tools, and incidental/pre-
  existing issue recording are covered.
- [x] Reconcile independent source/build reviews and revise this plan before the
  baseline or implementation begins.
