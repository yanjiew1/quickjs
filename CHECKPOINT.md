# QuickJS modularization checkpoint

## Current status

The QuickJS modularization task is complete in the current working tree. All
planned structural migrations, restorative-only Final Performance
Stabilization, supported-configuration validation, the fresh adversarial
review, and final reporting preparation are complete. No implementation work
remains.

The final architecture is:

- the primary QuickJS engine has no root `quickjs.c` build input. Normally
  linked owners under `src/quickjs/` cover allocation, runtime/context/jobs,
  atoms/strings, object/value/property/GC, numeric/operators, function/VM
  (including the complete interpreter and generator/async execution), frontend,
  module runtime, bytecode serialization, builtin composition, and builtin
  families;
- secondary runtime/library modularization is complete for `quickjs-libc`, the
  RegExp compiler/executor, and Unicode case, character-range, normalization,
  and property/sequence domains;
- developer-tooling modularization is complete for the Unicode table generator
  and Test262 runner.

The latest completed structural milestones are `aeb7c25` (Unicode runtime),
`c2be331` (Unicode table generator), and `2436025` (Test262 runner), followed by
`a5ec7f4` (structural-completion handoff). The final stabilization source and
documentation are included in the normal descriptive completion milestone.
The final working tree is clean except for the user-supplied authoritative
`task.md`, which remains untracked and uncommitted.

Authoritative task: `task.md`. Living roadmap: `PLAN.md`.

## Canonical-owner naming cleanup follow-up (2026-09-16)

This phase follows commit `fa7aa5f` and resolves the wrapper/linkage cases that
the preceding identifier-only cleanup intentionally could not touch. It does
not reopen module ownership, algorithms, optimization policy, data
representation, inline policy, public API/ABI, or build configuration.

### Resulting call topology

- Original upstream implementations are now the canonical cross-TU functions.
  Where extraction requires another owner to call an upstream-local function,
  its linkage changes minimally from `static` to `QJS_INTERNAL` and the exact
  upstream declaration is exposed through the existing narrow owner header.
- Consumer-specific `qjs_date_*`, `qjs_proxy_*`, `qjs_primitive_*`,
  `qjs_regexp_*`, `qjs_typed_*`, `qjs_async_*`, `qjs_base_*`, collection,
  object/property, numeric, string, allocator, VM, iterator, and quickjs-libc
  forwarding bridges are gone. Callers use the canonical owner function
  directly; no forwarding wrapper replaces them.
- Existing public/private inline pairs are normalized to the real public API
  name plus a private `_inline` implementation and a function-like internal
  macro. Parenthesized public declarations/definitions preserve the exported
  function, function-pointer use, and ABI. No new inline twin was created and
  no inline/force-inline/no-inline policy changed. Each narrow private header
  now orders the exact forward declarations first, then the public-name remaps,
  then the inline definitions. Inline bodies retain public API call spelling so
  calls to another existing twin follow its internal inline path automatically.
  Seven allocator-backend callback invocations use the equivalent parenthesized
  member-function form to prevent the public-name macro from matching an
  indirect callback call; the callback targets and control flow are unchanged.
- The exact upstream `find_own_property`, `find_own_property1`, `to_digit`, and
  `JS_IteratorNext` inline bodies are shared through their narrow private owner
  headers where cross-TU optimizer visibility is required. Their bodies and
  attributes are unchanged; the former copied/forwarding entries are removed.
- All meaningless self-aliases left by the strict token-only pass are removed.
  The only upstream-style macros with a `qjs_*` right-hand side are the two
  VM-local mappings to `qjs_add_gc_object_fast` and
  `qjs_remove_gc_object_fast`; these deliberately select the existing private
  direct GC-list boundary, not a forwarding function.

The inventory changed from 383 unique `qjs_*` identifiers (2,035 occurrences)
after the strict identifier-only phase to 36 unique identifiers (111 source
occurrences). There are no exact-upstream-name collisions in the resulting
tree, no self-aliases, and no surviving `qjs_*` whose sole purpose is to forward
to an existing upstream-named implementation. The exhaustive 36-name
classification is at the end of this file.

### Validation

- `git diff --check`: PASS.
- GCC 16.2 normal non-LTO clean WERROR `all` and full `make test`: PASS.
- Clang 23.1 normal non-LTO clean WERROR `all` and full `make test`: PASS.
- Exact full Test262 on the GCC build: unchanged at 58/83,558 errors, 3,356
  excluded, and 6,000 skipped.
- Exported dynamic symbols: exact match to the accepted completed-tree sets,
  292 names for GCC and 286 for Clang.
- No compiler-specific inline/no-inline policy, alignment/section placement,
  link-order, compiler-flag, or build-system change was introduced. No new
  optimization or algorithmic path was introduced.
- A bounded three-pair non-LTO sanity screen was run on CPU 2 against matched
  `fa7aa5f` GCC and Clang builds after ASLR was disabled and the CPU governor
  was set to performance. No tuning campaign followed. Median GCC deltas were
  `func_call` -0.93%, `prop_read` +1.57%, `array_read` -2.04%,
  `string_build2` -3.83%, `regexp_replace` -1.33%, and aggregate -1.51%.
  Median Clang deltas were +6.44%, +0.07%, +0.27%, +2.85%, +0.07%, and
  aggregate +0.58%, respectively.
- A single bounded fixed-work check investigated the Clang-only `func_call`
  result. Three pairs measured task-clock +6.85%, cycles +4.80%, instructions
  +1.77%, branches -0.99%, and branch misses -1.12%. `JS_CallInternal` has
  fewer static call instructions (349 versus 356), and call-target inspection
  found renamed/direct canonical targets rather than an added wrapper or
  trampoline. The pre-header-layout canonical-owner candidate had essentially
  the baseline instruction and branch counts but was slower in cycles, further
  indicating placement/compiler sensitivity. Because implementation logic,
  semantics, build flags, and inline attributes are unchanged, call topology is
  strictly simpler, and the user-directed header remap ordering is intentional,
  this isolated result is recorded rather than compensated with layout tuning
  or a new optimization.

Logs are `/tmp/qjs-wrapper-main-{gcc,clang}-{build,test}.log`, exact Test262 is
`/tmp/qjs-wrapper-main-gcc-test262.log`, and exported-symbol comparisons are
`/tmp/qjs-wrapper-main-{gcc,clang}-exports.diff`. Raw bounded performance
samples and summaries are under `/tmp/qjs-wrapper-perf/`.

## Upstream-symbol naming cleanup follow-up (2026-09-16)

This follow-up uses inline-policy completion commit `b3da94f` as its immediate
baseline and upstream commit `04be246` as the naming authority. It does not
reopen modular architecture, ownership, inline policy, optimization, or final
performance acceptance.

### Audit and strict source-equivalence result

The pre-edit audit enumerated 486 unique `qjs_*` identifiers (2,892 source
occurrences) under `src/` and `quickjs-libc.c`, including all QuickJS engine,
QuickJS libc, RegExp, Unicode, Unicode-generator, and Test262-runner owners. It
also found 365 upstream-style-to-`qjs_*` alias definitions. Comparison with
upstream classified 296 identifiers as exact or extracted upstream symbols, 50
as genuinely new modular lifecycle/composition/test glue, and 140 as real
split adapters, collision-avoiding entries, or private fast-boundary helpers.

Only 103 of the 296 exact/extracted names can be restored without changing
anything except identifier spelling. Applying the other 193 exact mappings
produces concrete same-TU redefinitions, static/non-static declaration
conflicts, or duplicate owner entries because the modular tree deliberately
contains both an upstream-local implementation and a cross-TU adapter. They
remain `qjs_*`: changing `static`, `QJS_INTERNAL`, wrappers, macros, header
placement, ownership, or call topology to force those names is prohibited by
the strict identifier-only scope.

The accepted source change consists of 103 global whole-token substitutions in
32 files, with 852 added and 852 removed lines. For every changed file, applying
the same token map to `git show b3da94f:<path>` produces a byte-for-byte match
with the working file. Therefore function bodies, control flow, `static`,
`inline`, `force_inline`, `no_inline`, `QJS_INTERNAL`, visibility, attributes,
types, qualifiers, parameter order, macro structure, TU/header placement, call
topology, compiler flags, and build configuration are unchanged. That
strict-phase tree had 383 unique `qjs_*` identifiers (2,035 occurrences), 205
remaining upstream-style-to-`qjs_*` aliases, and 158 harmless self-aliases
created by the identifier substitution. Those figures describe the historical
`fa7aa5f` state and are superseded by the canonical-owner cleanup above, whose
broader scope permits the minimal linkage and call-topology cleanup that was
then prohibited.

The exhaustive surviving-name classification appears at the end of this file.
Raw audit maps and rejected-collision compiler logs are under
`/tmp/qjs-naming-*`; the strict source-equivalence proof directory is
`/tmp/qjs-naming-proof.h87Z2Z`.

### Correctness, interface, and performance disposition

- `git diff --check`: PASS.
- GCC 16.2 normal non-LTO clean WERROR `all` and full `make test`: PASS.
- Clang 23.1 normal non-LTO clean WERROR `all` and full `make test`: PASS.
- Exact full Test262: unchanged at 58/83,558 errors, 3,356 excluded, and
  6,000 skipped.
- Exported dynamic symbols: byte-identical name sets to the accepted baseline,
  292 names for GCC and 286 for Clang.
- No compiler-specific inline/no-inline branch, annotation, alignment, section,
  link-order, flag, or build change was introduced.
- No new performance campaign or tuning was performed. The completed branch's
  accepted performance record remains authoritative; for this strict naming
  cleanup, the byte-for-byte identifier-substitution proof establishes that any
  small timing movement cannot reflect changed semantic work or call topology
  and is not a blocker.

Validation logs are `/tmp/qjs-naming-main-{gcc,clang}-{build,test}.log`, the
Test262 log is `/tmp/qjs-naming-main-gcc-test262.log`, and exact exported-symbol
comparisons are `/tmp/qjs-naming-main-{gcc,clang}-exports.diff`.

## Inline-policy cleanup follow-up (2026-09-15)

This separate follow-up uses completed modularization commit `af558f1` as its
immediate baseline. It does not reopen architecture or alter JavaScript
semantics, public API/ABI, algorithms, representations, or module ownership.

### Audit and resulting policy

The audit found all compiler-identity inline policy introduced during final
stabilization:

- `JS_ConcatString1` was `no_inline` only for Clang. Clang also used a forced
  `JS_ConcatString2Inline` at selected sites plus a no-inline
  `JS_ConcatString2` forwarder, while GCC used one ordinary implementation.
- `JS_IteratorNext` and `js_for_of_next` were ordinary under Clang and forced
  under GCC.
- `qjs_free_value` was ordinary inline under Clang and forced under GCC.
- `qjs_to_int32_free` exposed its implementation only to Clang and used an
  out-of-line owner entry for GCC.
- `object.c`, `function-vm.c`, and `builtin-regexp.c` selected the direct
  private atom-free helper only outside Clang. `function-vm.c` similarly
  selected its private allocator adapters only outside Clang.

All of those compiler-name choices are gone. The sole compiler test remaining
under `src/quickjs/` is the centralized attribute-portability implementation in
`internal-config.h`; it expresses the same policy portably rather than choosing
a different policy or call topology.

The canonical source now has ordinary `JS_ConcatString1` and one ordinary
`JS_ConcatString2`; `JS_ConcatString2Inline`, its forwarding wrapper, and its
macro alias are gone. The concat algorithm is unchanged. Both iterator helpers
are normal `static inline`. `qjs_to_int32_free` has one normal static-inline
definition in its narrow number header, which lets both optimizers see the
owner helper without forcing an outcome. Atom-free and allocator calls use the
same private owner interfaces under both compilers.

The final-performance force annotations on atom dup/free helpers, SameValue
wrappers, GC-list helpers, and the RegExp C-function predicate were reduced to
normal static inline because this follow-up found no evidence requiring a
forced result. No new `no_inline` remains from the completed task.

The one follow-up-owned explicit annotation retained is compiler-independent
`force_inline` on `qjs_free_value`. In a rotated seven-pair fixed-work
comparison, the otherwise-equivalent ordinary-inline build emitted 23 GCC
calls/local copies and measured `int_to_string` +5.86% cycles and
`string_build2` +8.51% cycles versus `af558f1`; forcing the helper reduced those
to +3.01% and +2.46%, respectively, and restored the direct zero-ref path.
Clang `func_call` was -4.24% ordinary and -4.51% forced, and Clang typed-array
read was +5.51% ordinary and +4.39% forced, so forcing introduced no meaningful
Clang regression. The source-level justification is therefore that failing to
inline materially harms hot GCC value-release paths, not that a particular
compiler historically chose to inline.

Other explicit annotations still visible in the repository were already in
the completed baseline and are compiler-independent. The force-inlined shape
lookup, fast-array extension predicate, and RegExp `lastIndex` accessors are the
same tiny per-operation hot fast paths present in the pristine monolithic
source; their failure to inline would add a call at each lookup/mutation/match.
The inherited no-inline routines are cold allocation, growth/conversion, slow
operator, interrupt, or recursion-sensitive paths deliberately kept out of hot
callers. They were reviewed for compiler-name selection and left unchanged;
none selects a different policy by compiler.

### Generated code and performance

The final GCC binary uses the same 0x35f-byte `JS_ConcatString2` body as
`af558f1`. Clang naturally emits one 0x4c3-byte `JS_ConcatString2` and no
`JS_ConcatString1` symbol instead of the former compiler-selected two-entry
topology. This difference is accepted because the source and semantics are
canonical and neither compiler needs its former heuristic shape reproduced.
Generated-code captures are under `/tmp/qjs-inline-cleanup/generated/`.

Seven alternating complete 72-workload runs, pinned serially to CPU 2 after
warmup, give a median-ratio geometric mean of **+0.093% for GCC 16.2** and
**-0.872% for Clang 23.1** against exact `af558f1` normal non-LTO binaries. Raw
samples and median tables are under:

- `/tmp/qjs-inline-cleanup/perf/final-gcc-full/`
- `/tmp/qjs-inline-cleanup/perf/final-clang-full/`

Fixed-work counter checks resolved the adaptive outliers. GCC
`string_build2` is +1.95% cycles with identical instructions/branches;
`string_to_float` is +4.09% with unchanged work. GCC `string_build3` remains
+5.51% cycles with identical instructions and branches, consistent with layout
rather than an inline-policy work change. Clang `prop_update` is +0.40%,
`array_write` -0.02%, and typed-array read +4.86% cycles (+1.28%
instructions); unrelated `float_toPrecision` and `math_min` cycle outliers have
identical instruction and branch counts. Fixed logs are under
`/tmp/qjs-inline-cleanup/perf/final-fixed-confirm/`.

The natural iterator policy has one explicit cross-compiler tradeoff. Normal
static inline leaves GCC `array_for_of` at +6.15% cycles, +2.93%
instructions, and +1.49% branches. Forcing both iterator helpers removes the
GCC work but makes Clang fixed `func_call` +21.75% cycles and +4.19%
instructions; forcing only the inner helper still makes it +22.56%/+4.19%.
Ordinary functions were worse for both compilers. Per the follow-up's conflict
rule, the source retains the simpler normal static-inline form instead of
encoding opposite compiler policies. Evidence is under
`/tmp/qjs-inline-cleanup/perf/candidate{1,2,3,5}-*`.

A bounded canonical public-atom-entry variant was also checked. It restored
Clang's former typed-read generated shape, but caused GCC fixed
`string_build2` +7.52% cycles with identical executed instructions/branches.
The final uniform private static-inline owner interface avoids that regression;
the rejected samples are under `/tmp/qjs-inline-cleanup/perf/atom-topology/`,
`candidate6-focused/`, and `candidate6-fixed/`.

No general optimization or layout/alignment tuning was introduced. The only
remaining above-threshold observations are the GCC-only natural-inline
`array_for_of` work difference and cycle-only GCC `string_build3` layout
sensitivity; neither is compensated by compiler-specific policy or a new fast
path.

### Follow-up correctness and interface validation

- GCC 16.2 normal non-LTO clean WERROR `all` and full `make test`: PASS.
- Clang 23.1 normal non-LTO clean WERROR `all` and full `make test`: PASS.
- Final measured and final validated binaries are byte-identical for each
  compiler.
- Exported dynamic symbol names match `af558f1` exactly: 292 GCC names and 286
  Clang names.
- `git diff --check` passes; no stale concat helper or compiler-specific inline
  branch remains.

Validation logs and symbol lists are under
`/tmp/qjs-inline-cleanup/final-validation/`.

## Final restorative-only acceptance state

### Performance-scope audit and preserved future work

Final Performance Stabilization briefly explored general optimization paths.
The audit classified and removed all such work from the structural-refactor
tree. The implementation details and numerical evidence are preserved in
`FUTURE_OPTIMIZATIONS.md`; the exact 1,315-line tracked pre-cleanup diff is:

```text
/home/yanjie/src/quickjs-final-perf-precleanup.patch
SHA-256 68d4d8959e664f0b7ef83ab7c2f3cc9b315bf48f026927deb1c9b78aec33c01f
```

The archive excludes `task.md`, contains both experiments and restorative
work, and is not applied to the final tree. The removed NEW OPTIMIZATION
families are direct integer conversion in concat, direct int32/uint32
typed-array stores, short-BigInt collection equality, direct fast-array pop,
direct fast-array iteration, interpreter-side direct C-method dispatch, forced
inline `qjs_get_length32`, and the copied allocator arena algorithm/exported
block-size table. The unproven Clang `i32toa`/concat placement experiment was
also removed. Experimental measurements are future-work evidence only and are
not used for final refactor acceptance.

The retained performance source changes are RESTORATIVE:

- hidden allocator-owner raw entries plus tiny inline allocation adapters
  restore the monolithic direct adapter shape without copying the allocator
  algorithm;
- atom duplicate/free, zero-ref value free, GC-list add/remove, fast-array
  representation access, and other tiny owner-private helpers restore direct
  representation operations that were visible in the monolithic TU. GCC's
  zero-ref value path remains forced inline where generated-code evidence
  requires it; Clang retains its ordinary pristine inline choice;
- `free_var_ref` is again static in the VM owner, and owner-local calls replace
  extraction-added forwarding layers;
- string allocation/buffer/get/put helpers live in the narrow string private
  header where this restores pristine caller visibility; allocator arena policy
  remains solely in `allocator.c`;
- RegExp callers now use actual owner interfaces rather than RegExp-specific
  forwarding trampolines. Clang atom-free calls retain the pristine out-of-line
  shape while GCC retains its pristine direct zero-ref behavior;
- GCC iterator handling and Clang concat inline/no-inline choices restore their
  respective pristine compiler shapes. In particular Clang's final concat
  routine is exactly 0x419 bytes with 20 calls, and normalized disassembly is
  instruction-identical to pristine;
- no explicit concat/interpreter alignment or hot-section tuning remains.

The fresh read-only adversarial review found no semantic change, forbidden new
optimization, license loss, dependency cycle, stale build rule, or public API
expansion. Its valid concerns were resolved by restoring Clang's out-of-line
atom-free choice, removing the last four RegExp forwarding trampolines, and
making Clang's zero-ref value helper an ordinary inline. Its proposed rejection
of selective Clang concat inlining was not accepted: pristine generated code
inlines exactly the same two sites and leaves the same third call, so this is
compiler-specific restoration rather than a new optimization.

### Final normal non-LTO performance

Pristine `04be246` and final binaries were built with identical `-O2`, WERROR,
normal non-LTO configuration separately with GCC 16.2 and Clang 23.1. Seven
alternating full `tests/microbench.js` pairs ran serially on CPU 2 after a
separate warmup. Raw samples and median tables are under:

- `/tmp/qjs-final-perf/runs/final-postaudit-gcc/`
- `/tmp/qjs-final-perf/runs/final-postaudit-clang/`

Across all 72 workloads the median-ratio geometric mean is **-1.643% with GCC**
and **-0.254% with Clang** (negative is faster). Representative final deltas
are:

| Workload | GCC | Clang |
|---|---:|---:|
| `prop_read` | -0.99% | -4.52% |
| `prop_write` | -1.78% | +0.87% |
| `func_call` | -1.77% | +8.08% adaptive, -0.28% fixed |
| `array_read` | -3.83% | -9.15% |
| `array_write` | +2.12% | +5.92% adaptive, +0.07% fixed cycles |
| `array_push` | -1.26% | -25.55% |
| `array_pop` | +7.70% | +1.28% |
| `typed_array_read` | -1.56% | -9.08% |
| `typed_array_write` | -8.49% | +1.15% |
| `string_build_large1` | -9.48% | +11.79% |
| `regexp_ascii` | +4.41% | +0.02% |
| `regexp_replace` | +1.42% | -3.48% |
| `int_to_string` | -0.66% | +6.68% adaptive, +0.82% fixed cycles |
| `arguments_read` | -4.89% | +4.49% adaptive, +3.07% fixed cycles |

Fixed-count confirmation with task-clock, cycles, instructions, branches, and
branch misses is in `/tmp/qjs-final-perf/perf/final-postaudit-confirm/`.
Adaptive-only Clang call/write/conversion candidates either disappeared or
fell below the meaningful threshold. The narrow Clang argument result is
synthetic and below 5%; a bounded allocator/object owner merge reduced it but
caused broader regressions under both compilers, so the natural allocator
owner remains separate. The full rejected merge campaigns are under
`/tmp/qjs-final-perf/runs/restorative13-final-{gcc,clang}/`.

Two remaining results satisfy the strict irreducible layout/boundary exception:

1. **GCC `array_pop` only.** Seven fixed pairs measured task-clock +9.69%
   (845 ms to 926 ms), cycles +10.14% (2.947B to 3.245B), instructions +0.35%,
   branches +0.59%, branch misses +5.02%, and L1-I misses 0.403M to 21.351M.
   The source performs the same semantic algorithm, the compiled routine has
   the same 12-call shape, and four additional static instructions account for
   only the +0.35% executed-work difference. There is no Clang analogue;
   representative array read/write/push and aggregate GCC performance are not
   systematically regressed. Direct owner calls, private inline representation
   access, link-layout changes across the stabilization candidates, and bounded
   owner-merge placements were investigated. The result varied from +2.79% to
   +10.24% fixed cycles as unrelated layout changed. Eliminating it would
   require fragile placement/alignment tuning or the archived new direct-pop
   algorithm, so no actionable structural defect remains.
2. **Clang `string_build_large1` only.** Seven fixed pairs measured task-clock
   +5.22% (991 ms to 1043 ms), cycles +5.85% (3.425B to 3.625B), instructions
   +2.14%, branches +9.54%, branch misses -3.37%, cache misses -10.00%, L1-I
   misses -32.57%, and iTLB misses -27.05%. The complete concat routine has
   identical normalized instruction text, size (0x419), and 20-call shape to
   pristine. An earlier binary with essentially the same +2.29% instructions
   and +9.69% branches was 1.90% faster than pristine, demonstrating placement
   sensitivity. GCC improves the workload by 9.48%. Thin allocator adapters
   contain no forwarding layer, and the bounded allocator/object merge reduced
   this local result but caused meaningful broader regressions. Copying the
   arena algorithm into callers would be the explicitly removed NEW
   OPTIMIZATION. Further compensation would therefore be disproportionate or
   out of scope.

The GCC RegExp ASCII fixed screen was +3.94% cycles with -0.23% instructions;
although branch-miss percentage doubled from a small absolute base, it remains
below the task's meaningful threshold and Clang is neutral. Historical Unicode
normalization and typed-array observations are neutral/improved in the final
compiler-specific evidence. The broader V8 corpus is absent, so no broader run
was available; aggregate and representative individual repository workloads
provide the practical cross-check. No similar meaningful regression remains
under both compilers.

Detailed layout counters and disassembly are under
`/tmp/qjs-final-perf/perf/final-postaudit-layout/` and
`/tmp/qjs-final-perf/perf/final-postaudit-confirm/`. These `/tmp` artifacts are
host-local; the numerical conclusions above are durable here.

### Final correctness and supported configurations

- GCC 16.2 and Clang 23.1 normal non-LTO clean parallel WERROR `all` builds and
  full repository `make test`: PASS on the final post-audit source. Logs:
  `/tmp/qjs-final-perf/final-validation-postaudit/{gcc,clang}-{all,test}.log`.
- Exact final GCC Test262: PASS, unchanged `58/83558` errors, `3356` excluded,
  `6000` skipped, exit zero. Log:
  `/tmp/qjs-final-perf/final-validation-postaudit/gcc-final-test262.log`.
- Standalone final GCC RegExp compile/execute smoke: PASS. Generated Unicode
  table remains byte-identical to the validated structural milestone.
- GCC 16.2 LTO WERROR `all`/`make test`: PASS. Clang 23.1 LTO WERROR
  `all`/`make test`: PASS with lld and llvm-ar. The ordinary GNU linker path
  cannot load this Clang installation's missing `LLVMgold.so`; the successful
  qjsc compiler wrapper is `/tmp/qjs-final-perf/clang-lld-cc`. LTO performance
  is informational and was not used for acceptance.
- GCC ASan and UBSan WERROR `all`/`make test`: PASS with the custom GCC runtime
  directory in `LD_LIBRARY_PATH`. Clang ASan/MSan cannot link because this local
  Clang installation lacks the compiler-rt archives.
- GCC and Clang debug targets and core/worker/std/read-write smoke tests: PASS.
  `CONFIG_CHECK_JSVALUE` compilation covers every engine TU in normal `all`.
- Final exported dynamic symbol names equal pristine exactly (292 GCC names,
  286 Clang names). Final GCC sizes are 1,065,966 text bytes for `qjs`,
  1,039,894 for `qjsc`, and 1,063,439 for `run-test262`; pristine `qjs` was
  1,080,048 text bytes.
- GCC `CONFIG_M32` WERROR compilation stops at the same `number.c` maybe-
  uninitialized diagnostic in pristine and final, so this is a pre-existing
  toolchain/configuration issue rather than a refactor regression.
- `unicode_gen_test` retains the exact pre-existing U+1FD3 case-folding failure
  under both compilers. `test262o`, `tests/bench-v8`, and the external
  `quickjs-benchmarks` corpus remain absent. These issues were not changed.

Useful final reproduction commands remain in the historical pause section near
the end of this file. There are no exact next implementation steps: clean the
generated standalone `regexp_test`, review the final diff/status, create the
normal completion commit, and report completion.

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

## Host library modularization milestone (authoritative current state)

### Architecture and boundary decision

The former 4,403-line `quickjs-libc.c` is now four normally compiled owners:

- `src/quickjs-libc/loader.c` owns file loading, shared-library loading,
  import-meta handling, import attributes, JSON modules, and the public module
  loader;
- `src/quickjs-libc/std.c` owns formatting, environment/process helpers exposed
  by `std`, the FILE class and methods, `urlGet`, and std module composition;
- `src/quickjs-libc/os.c` owns synchronous descriptor, terminal, filesystem,
  process, and path services plus public `os` module composition;
- the residual 1,727-line `quickjs-libc.c` owns handlers, signals, timers,
  polling, rejected promises, worker/SAB/message transport, runtime thread
  state, shell helpers, loop/await, and binary evaluation.

`evalScript` remains event/host-owned because it directly manages the runtime
thread state's worker distinction, interrupt recursion, and pending signal bit.
The loader boundary exposes only load-script and JSON-module-construction hooks;
the std owner exposes only its value-printer callback; and the event owner
exposes two table-level `os` module composition hooks. All are hidden from the
dynamic interface. Two genuinely shared, small error/option helpers are scoped
static inline in `internal-base.h`.

A finer event split was rejected after dependency review: rw/signal/timer lists,
poll descriptor indexes, wakers, message queues, worker ports, promise rejection
checks, job-loop sleep decisions, and teardown all share `JSThreadState` and
call bidirectionally. Splitting shell lifecycle or workers would expose that
state or numerous callbacks. The synchronous OS owner is separate because it
has no dependency on that representation and composes the event API through two
narrow hooks.

### Validation and performance

Acceptance validation on this source state:

- independent GCC 16.2 WERROR and Clang 23.1 WERROR compilation of all four
  affected TUs, both normal and `CONFIG_CHECK_JSVALUE`: PASS;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all`: PASS;
- full repository `make test`: PASS, including std/os, cyclic imports, dynamic
  modules, workers, rw handlers, binary JSON, and generated examples;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- function inventory matches exact parent `d417176` except for the two intended
  inline helpers; normal `qjs` dynamic exports remain the exact parent 292-name
  set; `git diff --check`: PASS.

Current GCC 16 non-LTO sizes are: qjs 5,218,296 bytes with 1,057,998 text
bytes; qjsc 5,206,456/1,032,022 text; run-test262 5,320,432/1,057,719 text;
and libquickjs.a 10,035,898 bytes. File-size growth is principally separate-TU
debug metadata; qjs text is 632 bytes above exact parent.

Five alternating parent/current runs of the focused GCC 16 non-LTO screen were
executed serially on CPU 2. Like-for-like median changes were: `prop_read`
+0.58%, `prop_write` +0.35%, `prop_create` +0.35%, `array_push` +0.23%,
`array_read` -0.17%, `array_slice` +1.89%, `func_call` +0.18%, `float_arith`
-0.13%, `typed_array_read` +0.16%, `typed_array_write` -0.21%,
`string_to_int` +0.00%, `string_build2` -0.20%, `regexp_ascii` +2.18%,
`regexp_replace` +0.04%, and `sort_bench` -0.46%. `int_arith` is excluded from
the comparison because the harness selected 100 parent versus 200 current
iterations, producing a non-comparable apparent -40.22%; unchanged engine
objects and all surrounding arithmetic workloads provide no regression signal.
No confirmed meaningful slowdown exists. Logs are under
`/tmp/qjs-libc-build/`; the exact parent checkout is `/tmp/quickjs-d417176`.

### Exact next steps

1. Map and implement the natural `libregexp.c` compiler/executor boundary,
   keeping parse/compiler state and executor state private and sharing only the
   bytecode contract. Validate standalone RegExp, QuickJS behavior, fuzz
   compilation, exact Test262, and compile-/execute-focused performance.
2. Assess Unicode runtime ownership next, preserving unambiguous table and
   recursion/composition ownership and validating generator compatibility.
3. Evaluate developer-tooling targets last, then perform the full final
   correctness matrix and matched GCC/Clang pristine-versus-final normal
   non-LTO performance stabilization.

## RegExp compiler/executor milestone (authoritative current state)

### Architecture and boundary decision

`libregexp.c` is now the 2,684-line compiler/bytecode owner: regexp parsing,
class/string-set construction, bytecode emission and dumping, capture/name
analysis, register allocation, `lre_compile`, public bytecode metadata
accessors, and the standalone test harness. `src/libregexp/exec.c` is the
799-line executor owner: character traversal, private `REExecContext`, timeout
polling, stack growth, capture rollback, the full backtracking dispatch loop,
and `lre_exec`.

The only shared representation is `src/libregexp/internal.h`, containing the
opcode enum generated from the canonical `libregexp-opcode.h`, the eight-byte
bytecode header offsets, and a small inline flags read. The compiler's opcode
size/name table remains private; `DUMP_EXEC` generates its own conditional name
table. Parser/emitter and executor state, macros, allocators, and helpers do not
cross the boundary. The inline flags read specifically avoids turning the
former same-TU `lre_get_flags` use into a normal non-LTO call on every match.

The explicit archive/fuzz/standalone build rules include the executor object,
and release packaging now copies the modular engine, host-library, and RegExp
source trees rather than the removed root `quickjs.c`.

### Incidental pre-existing test repair

The parent standalone `regexp_test` target failed GCC WERROR before the split:
its test-only capture buffer was declared `uint8_t *` even though `lre_exec`
requires `uint8_t **`, causing incompatible accesses and underallocation, and
the harness omitted the required `lre_check_timeout` callback. These
`#ifdef TEST`-only defects directly blocked required validation, so the capture
declaration and no-timeout callback were corrected. Production library behavior
and public headers are unchanged.

### Validation and performance

Acceptance validation on this source state:

- independent GCC 16.2 and Clang 23.1 WERROR compilation of compiler and
  executor, including `CONFIG_CHECK_JSVALUE`, `DUMP_REOP`, and `DUMP_EXEC`:
  PASS;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all` and full `make test`: PASS;
- standalone GCC WERROR `regexp_test` plus basic capture, named backreference,
  and Unicode-sets cases: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- five representative compiled regexp bytecode buffers are byte-for-byte
  identical to exact parent `0c24560`;
- Clang 23 compiles all fuzzer objects; its installation lacks libFuzzer runtime
  archives, while the available Clang 21 builds the full target and completes a
  1,000-run `fuzz_regexp` smoke without failure;
- normal `qjs` dynamic exports remain the exact parent 292-name set;
  `release.sh` syntax and `git diff --check`: PASS.

Current GCC 16 non-LTO qjs size is 5,218,408 bytes with 1,058,030 text bytes,
32 text bytes above exact parent. The compiler and executor object text sizes
are 26,710 and 7,448 bytes respectively.

Seven alternating parent/current CPU-2 runs show execution medians of
`regexp_ascii` -1.35%, `regexp_utf16` -0.60%, and `regexp_replace` -0.22%.
An eleven-pair changing-pattern compile benchmark is +0.06%. No confirmed
meaningful regression exists. Logs and bytecode comparisons are under
`/tmp/qjs-regexp-build/`; the exact parent checkout is
`/tmp/quickjs-0c24560`.

### Exact next steps

1. Map `libunicode.c` into natural case/canonicalization, generic-range,
   normalization, and property/sequence ownership domains. Share only compact
   table-index helpers where needed and keep recursion/composition state
   cohesive.
2. Validate runtime Unicode behavior, RegExp canonicalization, exact Test262,
   generator/test integration, generated-table stability, and focused normal
   non-LTO performance before committing.
3. Evaluate `unicode_gen.c` and `run-test262.c` developer-tooling ownership last,
   then perform final configuration and dual-compiler performance stabilization.

## Unicode runtime modularization milestone (authoritative current state)

### Architecture and boundary decision

The former 2,124-line `libunicode.c` is now four normally compiled owners:

- the 486-line root `libunicode.c` owns case conversion/folding, RegExp
  canonicalization, the case-derived range builder, and the hot cased predicate;
- `src/libunicode/char-range.c` owns the generic `CharRange` allocator and set
  operations;
- `src/libunicode/normalize.c` owns canonical combining-class lookup,
  decomposition recursion, composition, and all normalization forms;
- `src/libunicode/property.c` owns identifier predicates, Unicode scripts,
  categories, binary properties, property-stack decoding, code-point
  categorization, and sequence properties.

The generated `libunicode-table.h` now places case, normalization, and property
data behind owner selectors, so no large compressed table is emitted into more
than one object. `Cased1` remains case-owned because `lre_is_cased` directly
searches the case-conversion table on the final-sigma path. `Case_Ignorable`
remains property-owned because the generated property pointer table refers to
it. The only cross-owner behavior interface is the hidden case-range builder
used by property expressions; compressed-table lookup primitives are static
inline in a 104-line table-private header. Generic range storage and all larger
functions remain out of headers.

`unicode_gen.c` emits the owner selectors itself. Regeneration from the pinned
Unicode inputs is reproducible, with all numeric table data byte-for-byte
unchanged; only the selector guards differ from exact parent `cff9213`.
Archive, debug, non-LTO, fuzz, check, and cross-host object variants all depend
on the generated table. Standalone RegExp and release-packaging rules include
the new Unicode source directory.

### Validation and performance

Acceptance validation on this source state:

- independent GCC 16.2 and Clang 23.1 WERROR compilation of all four affected
  Unicode TUs, plus GCC `CONFIG_CHECK_JSVALUE` compilation: PASS;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all` and full repository `make test`:
  PASS;
- focused NFC/NFD, case/final-sigma, script/property, Unicode-set, and emoji
  sequence behavior plus standalone RegExp capture, named-reference, and
  Unicode-set cases: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- Clang 23 WERROR compilation of all RegExp/Unicode fuzzer objects and a full
  Clang 21 libFuzzer build with a 1,000-run smoke: PASS;
- public Unicode global-symbol inventory matches exact parent except for the
  one intended hidden case-range bridge; normal `qjs` dynamic exports remain
  the exact parent 292-name set; generated-table reproducibility,
  `release.sh` syntax, and `git diff --check`: PASS.

Current GCC 16 non-LTO qjs size is 5,211,112 bytes with 1,056,566 text bytes,
1,464 text bytes below exact parent. The case, character-range, normalization,
and property objects contain 6,640, 1,638, 20,706, and 33,310 text/data bytes;
object-symbol inspection confirms each selected compressed table has one owner.

Seven alternating exact-parent/current CPU-2 runs of the general focused screen
are neutral: `prop_read` -0.14%, `prop_write` +0.70%, `func_call` +0.36%,
`array_read` +0.17%, `sort_bench` +0.05%, `string_build2` +0.50%,
`regexp_ascii` +0.04%, `regexp_utf16` -0.85%, and `regexp_replace` +0.46%.
Unicode-specific medians are case conversion -1.49%, property execution -2.08%,
and changing-pattern property compilation -0.24%.

Normalization is reproducibly +5.30% across eleven alternating isolated pairs.
A fixed-count five-repeat `perf stat` check shows cycles +5.08% but instructions
-0.167%; the parent and current `unicode_normalize` have identical call sites,
the current function has fewer static branch instructions, and no extra
semantic work, missed important inline, or trampoline was found. Branches are
+0.98% and branch misses +17.22%, identifying a likely code-placement/branch-
prediction effect while layout is still changing. This is deferred under the
intermediate-milestone policy and keeps the Unicode PLAN item `[~]`; it must be
revisited during final dual-compiler non-LTO stabilization. Raw logs are under
`/tmp/qjs-unicode-*` and `/tmp/qjs-unicode-perf/`; the exact parent checkout is
`/tmp/quickjs-cff9213`.

### Exact next steps

1. Assess `unicode_gen.c` developer-tooling ownership, replace `USE_TEST` source
   inclusion with normal modular linkage/narrow gated hooks, and split only
   natural generator owners that do not expose its database globally. Validate
   self-tests and byte-for-byte generated output.
2. Assess `run-test262.c` ownership and implement only clean runner boundaries;
   validate bounded serial/threaded/filter/exclusion/reporting behavior before
   the exact full comparison.
3. Perform final configuration/correctness validation and matched pristine-vs-
   final normal non-LTO performance stabilization with both GCC and Clang.
   Revisit every deferred meaningful regression; LTO performance is diagnostic
   only, while supported LTO correctness remains required.

## Unicode table generator milestone (authoritative current state)

### Architecture and boundary decision

The former 3,792-line `unicode_gen.c`, which included `libunicode.c` in its
test configuration, is now six normally compiled generator owners:

- the 1,247-line root driver owns Unicode input parsing, database construction,
  immutable category/script/property metadata, and process lifetime;
- `src/unicode-gen/case.c` owns case-run analysis and compressed case-table
  generation;
- `src/unicode-gen/normalize.c` owns combining-class, decomposition, and
  composition table generation;
- `src/unicode-gen/property.c` owns derived flags, category/script/property,
  and emoji-sequence table generation;
- `src/unicode-gen/emission.c` owns the two compressed byte/index emitters
  shared by normalization and property generation;
- `src/unicode-gen/selftest.c` owns generator/runtime comparison tests and is
  linked only into the explicit `unicode_gen_test` target.

`UnicodeGenState` contains the database and emoji stores and is call-local to
the driver; parser and generation owners receive it explicitly. Output size
accounting is carried by an explicit `UnicodeGenOutput`. Case conversion's
temporary compressed tables are call-local to the case owner, including on the
self-test path; there are no mutable cross-TU or file-global generator states.
The private header exposes immutable metadata and only the narrow helpers used
across actual owner boundaries. Optional diagnostic switches live in that
shared generator header so their definitions reach every consuming TU.

The production generator links only its owners and `cutils`. The test target
normally links the modular Unicode runtime and enables three hidden,
`CONFIG_UNICODE_TEST`-gated normalization inspection hooks. Production runtime
objects contain none of those symbols, and no source file includes another
`.c` file. Release packaging includes both new private source directories.

### Validation and pre-existing self-test issue

Acceptance validation on this source state:

- independent GCC 16.2 and Clang 23.1 WERROR builds of `unicode_gen` and
  `unicode_gen_test`: PASS;
- GCC 16.2 and Clang 23.1 WERROR syntax with all generator diagnostic/profile
  switches enabled: PASS;
- generation from the pinned `unicode/` inputs under both compilers: PASS,
  byte-for-byte identical to tracked `libunicode-table.h`;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all` and full repository
  `make test`: PASS;
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- generator global-symbol inspection finds only the narrow declared private
  interface and immutable metadata, with no mutable generator globals;
  `release.sh` syntax and `git diff --check`: PASS.

The generator self-test exits 1 at the existing case-folding comparison for
U+1FD3:

```text
ERROR: F
01fd3: U: 00399 00308 00301 L: 01fd3 F: 00390
```

The exact parent `cff9213` source-inclusion self-test produces the same two
lines and exit status from the same Unicode inputs. This is therefore a
pre-existing self-test result, not a modularization regression; unrelated
case-folding behavior was not changed. The modular test still constructs and
checks the compressed case tables before performing the same runtime
comparison sequence as the parent.

Normal qjs text remains exactly 1,056,566 bytes, matching the preceding Unicode
runtime milestone. This developer-tooling-only extraction adds no production
runtime call boundary or generated-data change, so the intermediate runtime
performance screen was not repeated. Raw validation logs are under
`/tmp/qjs-ugen-*`; the exact parent test binary and comparison logs are under
`/tmp/qjs-unicode-perf/`.

### Exact next steps

1. Assess `run-test262.c` ownership. Extract only natural runner utilities or
   state owners with narrow dependency direction; keep metadata, evaluation,
   expected-failure comparison, and agent execution together where separating
   them would expose broad runner globals.
2. Validate bounded deterministic serial/threaded runs, filtering, exclusions,
   expected-failure reporting/update ordering, and statistics, then repeat the
   exact full Test262 comparison before committing any retained split.
3. Perform final supported-configuration correctness validation and matched
   pristine/final normal non-LTO performance stabilization separately with GCC
   16.2 and Clang 23.1. Revisit every deferred meaningful regression; supported
   LTO correctness remains required and LTO performance remains diagnostic.

## Test262 runner milestone (authoritative current state)

### Architecture and boundary decision

The former 2,555-line `run-test262.c` is now three normally compiled owners:

- the 1,875-line root runner owns config loading, `ftw` discovery, metadata and
  feature interpretation, module loading/evaluation, expected-error matching,
  statistics, progress/work scheduling, reporting, and process lifetime;
- `src/run-test262/namelist.c` owns string/path helpers plus numeric-aware name
  list allocation, loading, sorting, duplicate removal, and lookup;
- `src/run-test262/harness.c` owns opaque per-runner agent/report synchronization
  state, `print`, `$262`, realms, SharedArrayBuffer agent broadcast/reporting,
  async-completion state, and harness installation/cleanup.

The name-list owner has no runner-global dependency. `ftw` discovery remains in
the runner because the portable callback has no opaque argument and consumes
the runner's selected-test list. Parsing test names back out of the expected-
error text also remains with error/reporting ownership. Numeric subrange sort is
provided as one list operation rather than exporting its comparator.

The harness representation is opaque. Runner threads allocate it through a
narrow lifecycle interface, pass it as the QuickJS runtime opaque, and access
only output selection, async completion, helper installation, exceptional-value
printing, and agent cleanup. The previous global `outfile` dependency is now an
explicit per-harness field. All agent structs, callbacks, synchronization, and
realm composition stay private. The root runner's remaining functions and
state have static linkage; both private owner interfaces are hidden.

Config, metadata, evaluation, expected-error comparison, statistics, progress,
and reporting were deliberately not split further. They share most of the
current mode, feature, output, counter, error-list, and mutex globals; a finer
split would require a broad `RunnerState` API or artificial callback plumbing.
A future redesign around explicit runner and metadata values could revisit that
coupling, but creating it solely for file separation is outside this refactor.

### Validation

Acceptance validation on this source state:

- independent GCC 16.2 and Clang 23.1 WERROR compilation of the root, name-list,
  and harness TUs: PASS;
- normal and debug runner links under GCC 16.2 and Clang 23.1: PASS;
- clean parallel GCC 16.2 `CONFIG_WERROR=y all` and full repository
  `make test`: PASS;
- matched exact-parent `c2be331` and current focused execution covering
  `createRealm`, async completion/print output, and Atomics agent start,
  broadcast, SharedArrayBuffer, and report handling: byte-identical stdout and
  stderr, PASS;
- matched parent/current bounded config runs with one and four worker threads:
  identical at `0/474` errors, `3356` excluded, and `49292` index-skipped;
- matched stdout reporting, memory statistics, skipped-feature counts, and
  selection order: identical after separately removing expected wall-time-only
  report lines;
- expected-error-only execution: identical at `58/59`; update mode returns the
  same status and emits byte-identical, deterministically sorted error files
  (`cf0b47e5...d1955275` SHA-256);
- exact full Test262 comparison: PASS, unchanged at `58/83558` errors, `3356`
  excluded, and `6000` skipped;
- release packaging syntax, private symbol inspection, and `git diff --check`:
  PASS.

The normal GCC 16 run-test262 text size is 1,054,167 bytes, 2,120 bytes below
exact parent `c2be331`. Normal qjs remains byte-identical with SHA-256
`e14a2d2c...482db994d` and 1,056,566 text bytes. This tooling-only extraction
adds no production engine boundary or data change, so the engine performance
screen was not repeated. Raw logs are under `/tmp/qjs-runner-*`; the exact
parent checkout is `/tmp/quickjs-c2be331`.

### Exact next steps

1. Begin final supported-configuration validation from this structurally
   complete state, including GCC and Clang normal/debug/LTO correctness,
   checked-value, sanitizer, shared-library, host/generator, RegExp, Unicode,
   bytecode/module, worker, and Test262 paths required by `task.md` where the
   local toolchain supports them.
2. Perform formal Final Performance Stabilization against pristine `04be246`
   with matched normal non-LTO builds separately for GCC 16.2 and Clang 23.1,
   identical CPU affinity/isolation/repetition/statistics, the repository
   microbenchmarks, and the broader ECMAScript-only corpus when locally
   available. Record compiler-specific results and revisit all deferred
   meaningful regressions.
3. Request a fresh adversarial final review, correct and revalidate any retained
   issues, update PLAN/CHECKPOINT with final evidence, and complete the task only
   when no confirmed meaningful refactor-induced non-LTO regression remains.

## Historical structural pause handoff (2026-09-14; superseded)

This section records the pre-stabilization handoff for provenance. Its status
and next steps are superseded by the top-level final acceptance state.

All planned source and build-system migrations are complete. No implementation,
architecture cleanup, validation matrix, or benchmark run is in progress. The
repository is deliberately paused before Final Performance Stabilization; the
historical handoffs above remain useful milestone evidence but their old exact
next steps are superseded by this section.

### Deferred non-LTO observations

The following are formal-retest candidates, not claims that every workload is
still regressed in the final layout. Several moved substantially or became
neutral after later link-layout changes:

- the original pristine comparisons identified `prop_read`, `array_read`/
  `array_push`, `string_build2`, `regexp_ascii`, `regexp_utf16`, and
  `regexp_replace` as layout-sensitive at different engine milestones;
- the numeric-owner milestone isolated `typed_array_read` at +4.01% versus its
  parent. The fast-path owner disassembly was byte-identical, normalized
  instructions and branches decreased about 2.4%, branch misses were flat, and
  normalized cycles increased about 3.8%; no extra call/trampoline or semantic
  work was found;
- the Unicode-owner milestone isolated normalization at +5.30% across eleven
  alternating pairs versus its parent. Fixed-count counters were cycles +5.08%,
  instructions -0.167%, branches +0.98%, and branch misses +17.22%; call sites
  were identical and no extra semantic work, missed important inline, or
  trampoline was found;
- the last general screen after Unicode modularization was neutral versus its
  exact parent (`prop_read` -0.14%, `prop_write` +0.70%, `func_call` +0.36%,
  `array_read` +0.17%, `sort_bench` +0.05%, `string_build2` +0.50%, RegExp
  ASCII +0.04%, UTF-16 -0.85%, and replace +0.46%). This does not replace the
  required matched final-versus-pristine comparisons.

Under `task.md`, the cycle-only intermediate cases remain `[~]` likely
layout/microarchitectural observations after their focused structural checks;
they did not block later migrations. Final Performance Stabilization must retest
them with both compilers. Every remaining confirmed meaningful
refactor-induced regression in a normal non-LTO final build must be resolved
before completion; compiler-specific results must not be generalized from one
compiler to the other.

Relevant historical raw logs are under `/tmp/qjs-vm-*`,
`/tmp/qjs-allocator-*`, `/tmp/qjs-atom-*`, `/tmp/qjs-number-build/`,
`/tmp/qjs-regexp-*`, and `/tmp/qjs-unicode-*` (including
`/tmp/qjs-unicode-perf/`). These `/tmp` paths are useful on the current host but
are not durable repository artifacts.

### Pre-existing issues and environmental limits

- Preserve the tracked exact Test262 failure set: 58 failures. Do not fix an
  unrelated expected failure as part of performance work.
- `unicode_gen_test` has the exact pre-existing U+1FD3 case-folding failure
  documented in its milestone section (`ERROR: F` followed by the U+1FD3
  mapping line). The exact parent produces the same output and exit status; do
  not change Unicode semantics incidentally.
- `test262o`, `tests/bench-v8`, and the external `quickjs-benchmarks` corpus are
  absent. Record continued absence rather than manufacturing substitutes. If a
  suitable broader ECMAScript-only corpus becomes available, run it for both
  compilers.
- GCC 32-bit multilib and the Clang MSan runtime may be unavailable locally.
  Probe them during final configuration validation and record genuine toolchain
  limitations. Clean between configuration/compiler changes because the
  Makefile shares `.obj` paths.

### Preparation completed immediately before this pause

A bounded build-only preparation step rebuilt pristine `04be246` qjs with GCC
16.2 and Clang 23.1 normal non-LTO settings. No benchmark was run. The temporary
pristine worktree is `/tmp/quickjs-final-04be246`; preserved binaries are
`/tmp/qjs-final-perf/bin/baseline-gcc-qjs` and
`/tmp/qjs-final-perf/bin/baseline-clang-qjs`. Build logs are:

- `/tmp/qjs-final-perf/logs/baseline-gcc-clean.log`
- `/tmp/qjs-final-perf/logs/baseline-gcc-build.log`
- `/tmp/qjs-final-perf/logs/baseline-clang-clean.log`
- `/tmp/qjs-final-perf/logs/baseline-clang-build.log`

These binaries may be reused only after verifying their compiler versions,
optimization flags, non-LTO status, and configuration exactly match the final
comparison builds. The pristine text sizes are 1,080,048 bytes with GCC and
1,103,434 bytes with Clang.

### Exact first actions on resume

1. Verify `HEAD`, `git status`, this handoff, and the pristine-build logs. Do not
   change architecture unless performance evidence identifies a concrete
   structural defect.
2. Build the final tree separately with GCC 16.2 and Clang 23.1 using the exact
   compiler-specific pristine flags and normal non-LTO configuration. Preserve
   each binary before cleaning for the other compiler.
3. For each compiler independently, run alternating pristine/final repository
   microbenchmarks pinned to CPU 2 with the same isolation, workload calibration,
   repetitions, and statistical method. Cover the aggregate and important call,
   property, array/typed-array, string, RegExp, arithmetic, sort, and Unicode
   normalization workloads; run the broader ECMAScript-only corpus if available.
4. Confirm any meaningful result with focused alternating runs and counters.
   Check semantic work, generated code, inlining, calls/trampolines, and linkage
   before classifying a cycle-only difference as layout-related. Apply only
   reasonably scoped remedies and rerun affected correctness validation after
   each retained change. No confirmed meaningful normal non-LTO regression may
   remain when stabilization closes.
5. After performance stabilization, run the final supported-configuration and
   sanitizer correctness matrix (including supported LTO builds), exact Test262,
   fresh adversarial review, and final documentation/reporting. LTO performance
   is informational only.

Core reproduction commands begin with clean, compiler-matched builds:

```sh
make clean
make -j12 CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  HOST_CC=/home/yanjie/opt/gcc-16.2.0/bin/gcc \
  AR=/home/yanjie/opt/gcc-16.2.0/bin/gcc-ar CONFIG_WERROR=y qjs
make clean
make -j12 CC=/home/yanjie/opt/clang-23.1.1/bin/clang \
  HOST_CC=/home/yanjie/opt/clang-23.1.1/bin/clang \
  AR=ar CONFIG_CLANG=y CONFIG_DEFAULT_AR=y CONFIG_WERROR=y qjs
taskset -c 2 ./qjs --std tests/microbench.js \
  prop_read prop_write func_call array_read array_push typed_array_read \
  string_build2 regexp_ascii regexp_utf16 regexp_replace sort_bench
```

## Historical strict-phase qjs identifier audit (`fa7aa5f`)

This historical appendix lists every unique `qjs_*` identifier after the strict
identifier-only cleanup. It is retained as evidence of the input to the
canonical-owner phase and does not describe the current source tree. Entries in
each table share the precise disposition stated above that table.

### Exact upstream counterparts blocked by identifier-only collisions (193)

Each entry has an exact upstream counterpart, but direct token replacement was
compiler-tested and rejected because it creates a same-TU redefinition, a
static/non-static declaration conflict, or a duplicate owner entry. Resolving
one would require a prohibited linkage, wrapper, macro, placement, ownership, or
call-topology change.

| Surviving modular name | Exact upstream name |
|---|---|
| `qjs_add_brand` | `JS_AddBrand` |
| `qjs_add_intrinsic_bigint` | `JS_AddIntrinsicBigInt` |
| `qjs_add_property` | `add_property` |
| `qjs_allocate_fast_array` | `js_allocate_fast_array` |
| `qjs_array_buffer_finalizer` | `js_array_buffer_finalizer` |
| `qjs_array_buffer_is_resizable` | `array_buffer_is_resizable` |
| `qjs_array_every` | `js_array_every` |
| `qjs_array_finalizer` | `js_array_finalizer` |
| `qjs_array_iterator_finalizer` | `js_array_iterator_finalizer` |
| `qjs_array_iterator_mark` | `js_array_iterator_mark` |
| `qjs_array_mark` | `js_array_mark` |
| `qjs_array_push` | `js_array_push` |
| `qjs_array_reduce` | `js_array_reduce` |
| `qjs_atof` | `js_atof` |
| `qjs_atom_from_uint32` | `__JS_AtomFromUInt32` |
| `qjs_atom_is_tagged_int` | `__JS_AtomIsTaggedInt` |
| `qjs_atom_to_uint32` | `__JS_AtomToUInt32` |
| `qjs_auto_init_property` | `JS_AutoInitProperty` |
| `qjs_bigint_from_float64` | `js_bigint_from_float64` |
| `qjs_bigint_new` | `js_bigint_new` |
| `qjs_bigint_normalize` | `js_bigint_normalize` |
| `qjs_bigint_set_short` | `js_bigint_set_short` |
| `qjs_bigint_sign` | `js_bigint_sign` |
| `qjs_bigint_to_float64` | `js_bigint_to_float64` |
| `qjs_bigint_to_string` | `js_bigint_to_string1` |
| `qjs_build_backtrace` | `build_backtrace` |
| `qjs_call_free` | `JS_CallFree` |
| `qjs_check_brand` | `JS_CheckBrand` |
| `qjs_check_define_global_var` | `JS_CheckDefineGlobalVar` |
| `qjs_check_exception_free` | `check_exception_free` |
| `qjs_check_function` | `check_function` |
| `qjs_class_has_bytecode` | `js_class_has_bytecode` |
| `qjs_closure` | `js_closure` |
| `qjs_closure2` | `js_closure2` |
| `qjs_compact_bigint` | `JS_CompactBigInt` |
| `qjs_convert_fast_array_to_array` | `convert_fast_array_to_array` |
| `qjs_copy_data_properties` | `JS_CopyDataProperties` |
| `qjs_create_array` | `js_create_array` |
| `qjs_create_array_free` | `js_create_array_free` |
| `qjs_create_array_iterator` | `js_create_array_iterator` |
| `qjs_create_from_ctor` | `js_create_from_ctor` |
| `qjs_create_iterator_result` | `js_create_iterator_result` |
| `qjs_create_var_ref` | `js_create_var_ref` |
| `qjs_dbuf_bytecode_init` | `js_dbuf_bytecode_init` |
| `qjs_dbuf_put_leb128` | `dbuf_put_leb128` |
| `qjs_dbuf_put_sleb128` | `dbuf_put_sleb128` |
| `qjs_define_auto_init_property` | `JS_DefineAutoInitProperty` |
| `qjs_define_object_name` | `JS_DefineObjectName` |
| `qjs_define_object_name_computed` | `JS_DefineObjectNameComputed` |
| `qjs_define_private_field` | `JS_DefinePrivateField` |
| `qjs_define_property_value_int64` | `JS_DefinePropertyValueInt64` |
| `qjs_define_property_value_value` | `JS_DefinePropertyValueValue` |
| `qjs_delete_global_var` | `JS_DeleteGlobalVar` |
| `qjs_delete_property` | `delete_property` |
| `qjs_delete_property_int64` | `JS_DeletePropertyInt64` |
| `qjs_dtoa2` | `js_dtoa2` |
| `qjs_dump_atoms` | `JS_DumpAtoms` |
| `qjs_dump_value_write` | `js_dump_value_write` |
| `qjs_dup_shape` | `js_dup_shape` |
| `qjs_dynamic_import` | `js_dynamic_import` |
| `qjs_enqueue_job2` | `JS_EnqueueJob2` |
| `qjs_eval_internal` | `JS_EvalInternal` |
| `qjs_eval_object` | `JS_EvalObject` |
| `qjs_find_line_num` | `find_line_num` |
| `qjs_finrec_delete` | `finrec_delete_weakref` |
| `qjs_free_function_bytecode` | `free_function_bytecode` |
| `qjs_free_module_def` | `js_free_module_def` |
| `qjs_free_property` | `free_property` |
| `qjs_free_zero_refcount` | `free_zero_refcount` |
| `qjs_function_set_properties` | `js_function_set_properties` |
| `qjs_get_array_buffer` | `JS_GetArrayBuffer` |
| `qjs_get_function_realm` | `JS_GetFunctionRealm` |
| `qjs_get_global_var_ref` | `JS_GetGlobalVarRef` |
| `qjs_get_iterator` | `JS_GetIterator` |
| `qjs_get_iterator2` | `JS_GetIterator2` |
| `qjs_get_leb128` | `get_leb128` |
| `qjs_get_own_property_internal` | `JS_GetOwnPropertyInternal` |
| `qjs_get_own_property_names_internal` | `JS_GetOwnPropertyNamesInternal` |
| `qjs_get_private_field` | `JS_GetPrivateField` |
| `qjs_get_property_int64` | `JS_GetPropertyInt64` |
| `qjs_get_prototype_free` | `JS_GetPrototypeFree` |
| `qjs_get_shape_prop` | `get_shape_prop` |
| `qjs_get_sleb128` | `get_sleb128` |
| `qjs_get_var_ref` | `get_var_ref` |
| `qjs_global_is_finite` | `js_global_isFinite` |
| `qjs_global_is_nan` | `js_global_isNaN` |
| `qjs_global_object_find_uninitialized_var` | `js_global_object_find_uninitialized_var` |
| `qjs_hash_string` | `hash_string` |
| `qjs_import_meta` | `js_import_meta` |
| `qjs_init_class_range` | `init_class_range` |
| `qjs_instantiate_function_list_item` | `JS_InstantiateFunctionListItem2` |
| `qjs_invoke_free` | `JS_InvokeFree` |
| `qjs_is_backtrace_needed` | `is_backtrace_needed` |
| `qjs_is_c_function` | `JS_IsCFunction` |
| `qjs_is_digit` | `is_digit` |
| `qjs_is_regexp` | `js_is_regexp` |
| `qjs_is_safe_integer` | `is_safe_integer` |
| `qjs_iterator_close` | `JS_IteratorClose` |
| `qjs_iterator_concat_finalizer` | `js_iterator_concat_finalizer` |
| `qjs_iterator_concat_mark` | `js_iterator_concat_mark` |
| `qjs_iterator_get_complete_value` | `JS_IteratorGetCompleteValue` |
| `qjs_iterator_helper_finalizer` | `js_iterator_helper_finalizer` |
| `qjs_iterator_helper_mark` | `js_iterator_helper_mark` |
| `qjs_iterator_next` | `JS_IteratorNext` |
| `qjs_iterator_next2` | `JS_IteratorNext2` |
| `qjs_iterator_proto_iterator` | `js_iterator_proto_iterator` |
| `qjs_iterator_wrap_finalizer` | `js_iterator_wrap_finalizer` |
| `qjs_iterator_wrap_mark` | `js_iterator_wrap_mark` |
| `qjs_json_free_parse_record` | `json_free_parse_record` |
| `qjs_json_parse_record_add` | `json_parse_record_add` |
| `qjs_json_parse_record_find` | `json_parse_record_find` |
| `qjs_json_parse_record_init_obj` | `json_parse_record_init_obj` |
| `qjs_map_delete_weakrefs` | `map_delete_weakrefs` |
| `qjs_mark_module_def` | `js_mark_module_def` |
| `qjs_math_pow` | `js_pow` |
| `qjs_method_set_home_object` | `js_method_set_home_object` |
| `qjs_method_set_properties` | `js_method_set_properties` |
| `qjs_module_ns_autoinit` | `js_module_ns_autoinit` |
| `qjs_new_c_constructor` | `JS_NewCConstructor` |
| `qjs_new_c_function3` | `JS_NewCFunction3` |
| `qjs_new_module_def` | `js_new_module_def` |
| `qjs_new_module_value` | `JS_NewModuleValue` |
| `qjs_new_object_proto_list` | `JS_NewObjectProtoList` |
| `qjs_new_regexp` | `JS_NewRegexp` |
| `qjs_number_is_integer` | `JS_NumberIsInteger` |
| `qjs_number_is_negative_or_minus_zero` | `JS_NumberIsNegativeOrMinusZero` |
| `qjs_object_group_by` | `js_object_groupBy` |
| `qjs_ordinary_is_instance_of` | `JS_OrdinaryIsInstanceOf` |
| `qjs_parse_json3` | `JS_ParseJSON3` |
| `qjs_poll_interrupts` | `js_poll_interrupts` |
| `qjs_print_atom` | `js_print_atom` |
| `qjs_proxy_free_desc` | `js_free_desc` |
| `qjs_regexp_finalizer` | `js_regexp_finalizer` |
| `qjs_regexp_string_iterator_finalizer` | `js_regexp_string_iterator_finalizer` |
| `qjs_regexp_string_iterator_mark` | `js_regexp_string_iterator_mark` |
| `qjs_resize_array` | `js_resize_array` |
| `qjs_resolve_module` | `JS_ResolveModule` |
| `qjs_resolve_proxy` | `js_resolve_proxy` |
| `qjs_same_value` | `js_same_value` |
| `qjs_same_value_zero` | `js_same_value_zero` |
| `qjs_set_constructor2` | `JS_SetConstructor2` |
| `qjs_set_cycle_flag` | `set_cycle_flag` |
| `qjs_set_object_data` | `JS_SetObjectData` |
| `qjs_set_private_field` | `JS_SetPrivateField` |
| `qjs_set_prototype_internal` | `JS_SetPrototypeInternal` |
| `qjs_set_value` | `set_value` |
| `qjs_string_equal` | `js_string_eq` |
| `qjs_string_find_invalid_codepoint` | `js_string_find_invalid_codepoint` |
| `qjs_string_to_bigint_error` | `JS_StringToBigIntErr` |
| `qjs_throw_error2` | `JS_ThrowError2` |
| `qjs_throw_reference_error_not_defined` | `JS_ThrowReferenceErrorNotDefined` |
| `qjs_throw_reference_error_uninitialized` | `JS_ThrowReferenceErrorUninitialized` |
| `qjs_throw_reference_error_uninitialized2` | `JS_ThrowReferenceErrorUninitialized2` |
| `qjs_throw_stack_overflow` | `JS_ThrowStackOverflow` |
| `qjs_throw_syntax_error_atom` | `JS_ThrowSyntaxErrorAtom` |
| `qjs_throw_syntax_error_var_redeclaration` | `JS_ThrowSyntaxErrorVarRedeclaration` |
| `qjs_throw_type_error_not_constructor` | `JS_ThrowTypeErrorNotAConstructor` |
| `qjs_throw_type_error_not_object` | `JS_ThrowTypeErrorNotAnObject` |
| `qjs_throw_type_error_read_only` | `JS_ThrowTypeErrorReadOnly` |
| `qjs_to_array_length_free` | `JS_ToArrayLengthFree` |
| `qjs_to_bigint` | `JS_ToBigInt` |
| `qjs_to_bigint64_free` | `JS_ToBigInt64Free` |
| `qjs_to_bigint_free` | `JS_ToBigIntFree` |
| `qjs_to_bool_free` | `JS_ToBoolFree` |
| `qjs_to_digit` | `to_digit` |
| `qjs_to_float64_free` | `JS_ToFloat64Free` |
| `qjs_to_int32_clamp` | `JS_ToInt32Clamp` |
| `qjs_to_int32_free` | `JS_ToInt32Free` |
| `qjs_to_int32_sat` | `JS_ToInt32Sat` |
| `qjs_to_int64_clamp` | `JS_ToInt64Clamp` |
| `qjs_to_int64_free` | `JS_ToInt64Free` |
| `qjs_to_int64_sat` | `JS_ToInt64Sat` |
| `qjs_to_integer_free` | `JS_ToIntegerFree` |
| `qjs_to_length_free` | `JS_ToLengthFree` |
| `qjs_to_locale_string_free` | `JS_ToLocaleStringFree` |
| `qjs_to_number` | `JS_ToNumber` |
| `qjs_to_number_free` | `JS_ToNumberFree` |
| `qjs_to_numeric` | `JS_ToNumeric` |
| `qjs_to_primitive` | `JS_ToPrimitive` |
| `qjs_to_primitive_free` | `JS_ToPrimitiveFree` |
| `qjs_to_string_check_object` | `JS_ToStringCheckObject` |
| `qjs_to_string_internal` | `JS_ToStringInternal` |
| `qjs_to_uint32_free` | `JS_ToUint32Free` |
| `qjs_to_uint8_clamp_free` | `JS_ToUint8ClampFree` |
| `qjs_try_get_property_int64` | `JS_TryGetPropertyInt64` |
| `qjs_typed_array_constructor` | `js_typed_array_constructor` |
| `qjs_typed_array_finalizer` | `js_typed_array_finalizer` |
| `qjs_typed_array_get_length_unsafe` | `js_typed_array_get_length_unsafe` |
| `qjs_typed_array_is_oob` | `typed_array_is_oob` |
| `qjs_typed_array_mark` | `js_typed_array_mark` |
| `qjs_typed_array_species_create` | `js_typed_array___speciesCreate` |
| `qjs_update_property_flags` | `js_update_property_flags` |
| `qjs_weakref_delete` | `weakref_delete_weakref` |

### Genuinely new modularization glue (50)

No identifier with the same conceptual lifecycle, composition, split-helper, or
test-interface purpose exists in upstream `04be246`; these names are genuinely
introduced by the multi-TU architecture.

| Surviving name | Disposition |
|---|---|
| `qjs_add_intrinsic_array_basic` | New modularization-only glue; no upstream counterpart. |
| `qjs_add_intrinsic_generator` | New modularization-only glue; no upstream counterpart. |
| `qjs_add_intrinsic_global` | New modularization-only glue; no upstream counterpart. |
| `qjs_add_intrinsic_iterators` | New modularization-only glue; no upstream counterpart. |
| `qjs_add_intrinsic_math` | New modularization-only glue; no upstream counterpart. |
| `qjs_add_intrinsic_number_boolean_string` | New modularization-only glue; no upstream counterpart. |
| `qjs_add_intrinsic_symbol` | New modularization-only glue; no upstream counterpart. |
| `qjs_add_intrinsics` | New modularization-only glue; no upstream counterpart. |
| `qjs_allocator_init` | New modularization-only glue; no upstream counterpart. |
| `qjs_array_get_length32` | New modularization-only glue; no upstream counterpart. |
| `qjs_array_get_opaque2` | New modularization-only glue; no upstream counterpart. |
| `qjs_array_get_this` | New modularization-only glue; no upstream counterpart. |
| `qjs_array_object_to_string` | New modularization-only glue; no upstream counterpart. |
| `qjs_atom_string_compute_memory_usage` | New modularization-only glue; no upstream counterpart. |
| `qjs_atom_string_free_runtime` | New modularization-only glue; no upstream counterpart. |
| `qjs_atom_string_free_value_rt` | New modularization-only glue; no upstream counterpart. |
| `qjs_atom_string_init_runtime` | New modularization-only glue; no upstream counterpart. |
| `qjs_default_malloc_functions` | New modularization-only glue; no upstream counterpart. |
| `qjs_function_class_id` | New modularization-only glue; no upstream counterpart. |
| `qjs_function_vm_init_runtime` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_add_event_module_exports` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_create_json_module` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_eval_script` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_get_bool_option` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_get_errno` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_init_event_module` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_load_script` | New modularization-only glue; no upstream counterpart. |
| `qjs_libc_print_value_write` | New modularization-only glue; no upstream counterpart. |
| `qjs_module_add_export_unchecked` | New modularization-only glue; no upstream counterpart. |
| `qjs_module_add_request` | New modularization-only glue; no upstream counterpart. |
| `qjs_module_add_star_export` | New modularization-only glue; no upstream counterpart. |
| `qjs_module_find_export` | New modularization-only glue; no upstream counterpart. |
| `qjs_module_free_all` | New modularization-only glue; no upstream counterpart. |
| `qjs_module_init_class` | New modularization-only glue; no upstream counterpart. |
| `qjs_module_link_and_evaluate` | New modularization-only glue; no upstream counterpart. |
| `qjs_new_atom_rt_ascii` | New modularization-only glue; no upstream counterpart. |
| `qjs_object_dump_context` | New modularization-only glue; no upstream counterpart. |
| `qjs_object_free_context_shapes` | New modularization-only glue; no upstream counterpart. |
| `qjs_object_free_shape_hash` | New modularization-only glue; no upstream counterpart. |
| `qjs_object_gc_shutdown` | New modularization-only glue; no upstream counterpart. |
| `qjs_object_init_classes` | New modularization-only glue; no upstream counterpart. |
| `qjs_object_init_shapes` | New modularization-only glue; no upstream counterpart. |
| `qjs_object_proto_class_alloc` | New modularization-only glue; no upstream counterpart. |
| `qjs_string_object_length` | New modularization-only glue; no upstream counterpart. |
| `qjs_throw_array_buffer_oob` | New modularization-only glue; no upstream counterpart. |
| `qjs_throw_detached_array_buffer` | New modularization-only glue; no upstream counterpart. |
| `qjs_throw_duplicate_export` | New modularization-only glue; no upstream counterpart. |
| `qjs_unicode_test_compose_pair` | New modularization-only glue; no upstream counterpart. |
| `qjs_unicode_test_decomp_char` | New modularization-only glue; no upstream counterpart. |
| `qjs_unicode_test_get_cc` | New modularization-only glue; no upstream counterpart. |

### Real adapters, collision entries, and private split boundaries (140)

Each entry performs actual ownership adaptation, preserves a public/local-name
collision, or exposes a distinct private fast boundary; it is not merely an
exact upstream implementation renamed on extraction. Removing or renaming these
entries would require a non-identifier structural change.

| Surviving name | Disposition |
|---|---|
| `qjs_add_gc_object` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_add_gc_object_fast` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_array_buffer_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_array_buffer_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_aggregate_error_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_bytecode_finalizer` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_bytecode_mark` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_c_function_data` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_create_from_ctor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_create_from_sync_iterator` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_dump_value` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_function_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_invoke_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_perform_promise_then` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_promise_resolve` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_resolve_call` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_resolve_finalizer` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_resolve_mark` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_species_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_async_to_int32_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_atom_is_array_index_slow` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_atom_is_numeric_index_slow` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_aggregate_error` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_build_arg_list` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_create_data_property_uint32` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_create_from_ctor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_define_property_value` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_error_to_string` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_free_arg_list` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_free_desc` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_function_apply` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_function_class_id` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_function_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_get_active_function` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_get_own_property_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_get_prototype_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_obj_to_desc` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_poll_interrupts` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_set_immutable_prototype` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_set_prototype_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_species_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_base_throw_not_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_collection_create_array` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_collection_create_from_ctor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_collection_throw_not_object` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_create_from_ctor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_get_string` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_new_c_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_new_string8` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_string_get` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_throw_type_error_not_object` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_to_float64_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_date_to_primitive` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_dup_atom` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_find_own_property` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_find_own_property1` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_find_own_property_fast` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_atom` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_atom_rt` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_raw` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_rt_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_string_zero_ref` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_value` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_free_var_ref` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_get_ref_header` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_global_atof` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_global_skip_spaces` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_global_string_buffer_putc16` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_global_string_buffer_write8` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_global_string_get` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_global_throw_error` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_json_array_includes` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_json_array_pop` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_json_array_push` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_json_object_keys` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_malloc_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_malloc_raw` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_malloc_rt_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_math_get_iterator` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_math_iterator_close` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_math_iterator_next` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_poll_interrupts_slow` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_check_define_flags` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_create_array_iterator` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_create_data_property_uint32` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_create_from_ctor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_get_property_int64` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_get_property_value` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_init_classes` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_invoke_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_new_c_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_new_object_proto_list` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_throw_not_configurable` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_throw_not_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_to_object_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_to_primitive_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_primitive_to_string_check_object` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_check_define_prop_flags` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_create_array` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_get_own_property_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_is_strict_mode` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_new_c_function3` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_obj_to_desc` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_register_class` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_same_value` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_set_prototype_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_throw_revoked` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_throw_stack_overflow` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_throw_type_error_not_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_throw_type_error_not_object` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_proxy_to_bool_free` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_realloc2_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_realloc_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_realloc_raw` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_realloc_rt_internal` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_add_shape_property` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_create_from_ctor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_get_proto_obj` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_get_this` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_is_c_function` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_is_empty_string` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_new_c_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_new_object_proto_list` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_new_shape2` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_species_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_string_advance_index` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_string_get` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_string_indexof_char` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_throw_interrupted` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_throw_type_error_invalid_class` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_regexp_throw_type_error_not_object` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_remove_gc_object` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_remove_gc_object_fast` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_to_float64_free_slow` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_typed_bigint_sign` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_typed_create_from_ctor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_typed_species_constructor` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_typed_throw_invalid_class` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |
| `qjs_typed_to_primitive` | Real split adapter, collision entry, or private boundary; no safe exact-name substitution. |

## Exhaustive current qjs identifier audit (canonical-owner phase)

Every current survivor has no equivalent upstream implementation to expose.
The first group is new lifecycle, composition, owner-registration, or test glue
created by the multi-TU architecture. The second group is a deliberately small
private boundary that preserves direct access to owner-private representation or
a hot operation that the monolithic TU performed directly.

### Genuinely new modularization glue (28)

| Surviving name | Modular-only responsibility |
|---|---|
| `qjs_add_intrinsic_array_basic` | Installs the array owner's basic intrinsic subset during cross-owner composition. |
| `qjs_add_intrinsic_generator` | Installs generator/async-owner intrinsics during composition. |
| `qjs_add_intrinsic_global` | Installs the global owner's private function table. |
| `qjs_add_intrinsic_iterators` | Installs array/iterator-owner iterator intrinsics. |
| `qjs_add_intrinsic_math` | Installs the math owner's intrinsic table. |
| `qjs_add_intrinsic_number_boolean_string` | Installs primitive-owner intrinsic families. |
| `qjs_add_intrinsic_symbol` | Installs the primitive owner's Symbol family. |
| `qjs_add_intrinsics` | Coordinates intrinsic composition across builtin owners. |
| `qjs_atom_string_compute_memory_usage` | Contributes atom/string-owner data to the cross-owner memory report. |
| `qjs_atom_string_free_runtime` | Atom/string-owner runtime teardown entry. |
| `qjs_atom_string_init_runtime` | Atom/string-owner runtime initialization entry. |
| `qjs_function_vm_init_runtime` | Function/VM-owner runtime class initialization entry. |
| `qjs_libc_add_event_module_exports` | Event-owner export composition for quickjs-libc. |
| `qjs_libc_init_event_module` | Event-owner module initialization for quickjs-libc. |
| `qjs_module_init_class` | Module-owner runtime class initialization entry. |
| `qjs_module_link_and_evaluate` | Cross-owner frontend entry coordinating module link and evaluation. |
| `qjs_new_atom_rt_ascii` | Runtime-only ASCII atom construction needed by split runtime/class setup. |
| `qjs_object_dump_context` | Object-owner contribution to cross-owner context diagnostics. |
| `qjs_object_free_context_shapes` | Object-owner context-shape teardown entry. |
| `qjs_object_free_shape_hash` | Object-owner runtime shape-hash teardown entry. |
| `qjs_object_gc_shutdown` | Object/GC-owner runtime shutdown entry. |
| `qjs_object_init_classes` | Registers the object owner's private class-definition range. |
| `qjs_object_init_shapes` | Object-owner initial shape setup entry. |
| `qjs_primitive_init_classes` | Primitive-owner runtime class initialization entry. |
| `qjs_proxy_register_class` | Registers the proxy owner's class callbacks with the runtime owner. |
| `qjs_unicode_test_compose_pair` | Test-only bridge to normalization-owner static composition data. |
| `qjs_unicode_test_decomp_char` | Test-only bridge to normalization-owner static decomposition data. |
| `qjs_unicode_test_get_cc` | Test-only bridge to normalization-owner combining-class data. |

### Genuinely new private fast-boundary helpers (8)

| Surviving name | Required boundary |
|---|---|
| `qjs_add_gc_object_fast` | Header-local direct GC-list insertion used by the VM owner without an out-of-line boundary. |
| `qjs_async_c_function_data` | Read-only accessor for object-owner private C-function representation used by the async owner. |
| `qjs_atom_is_array_index_slow` | Slow owner entry behind the header-local tagged-integer/atom fast path. |
| `qjs_atom_is_numeric_index_slow` | Slow owner entry behind the header-local numeric-index fast path. |
| `qjs_atom_string_free_value_rt` | Header-local value teardown spanning string/rope owner-private representation. |
| `qjs_free_string_zero_ref` | Zero-reference slow owner entry behind direct string refcount handling. |
| `qjs_function_class_id` | Maps split VM-private function kinds to runtime class IDs without exposing the private table. |
| `qjs_remove_gc_object_fast` | Header-local direct GC-list removal used by the VM owner without an out-of-line boundary. |
