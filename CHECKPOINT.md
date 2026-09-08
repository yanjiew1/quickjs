# Implementation Checkpoint

Reconstructed on 2026-09-09 from `task.md`, `PLAN.md`, Git history, the live
worktree, and clean build/test verification of commit `0b41782`.

## Overall status

The modularization implementation is complete. Stages 0-4 and the first three
Stage 5 extractions (shapes, object core, and properties) are committed. The
completed iterator and VM extractions are present as uncommitted work. Stage 7
local validation, including the current Test262 suite, is complete; only
validation requiring absent legacy benchmark corpora, toolchains, or unsupported
sanitizer/ABI runtimes remains.

At committed `HEAD`, `quickjs.c` is 8,642 lines. In the live worktree it has been
fully decomposed and removed. The canonical build is a real multi-translation-
unit build: `Makefile` compiles each extracted source independently and links the
objects; there are no source-inclusion shortcuts.

## Verified completed work

- Stage 0: subsystem directory/build infrastructure is present. The source tree
  uses `src/{builtins,compiler,value,runtime,object}` and `src/quickjs-internal.h`.
- Stage 1: all eleven planned builtin implementation units and
  `src/builtins/js_builtins.h` are present and independently compiled.
- Stage 2: bytecode serialization, lexer, parser, and code generation are present
  under `src/compiler/` and independently compiled.
- Stage 3: BigInt, values/slow arithmetic, strings/ropes, and atoms are present
  under `src/value/` and independently compiled.
- Stage 4: allocator, GC, errors/backtraces, and runtime/context lifecycle are
  present under `src/runtime/` and independently compiled.
- Stage 5: commits `2207f65`, `7c0b6f1`, and `0b41782` contain the shape,
  object, and property extractions; the live worktree completes iterator and
  enumeration extraction under `src/object/`.

Verification performed during reconstruction:

- A clean archive of committed `HEAD` (`0b41782`) builds successfully with
  `make -j4 CONFIG_WERROR=y`.
- The same clean archive passes `make CONFIG_WERROR=y test`, including all eleven
  commands listed by the target: closure, language, builtin, loop, BigInt,
  cyclic-module, worker, std, read/write handler, bjson, and point tests.
- Each extracted module appears as its own object in `QJS_SRC_OBJS`.
- No extracted QuickJS module is pulled into `quickjs.c` via a source include.
  (`unicode_gen.c` still intentionally includes `libunicode.c`; that pre-existing
  generator arrangement is outside this refactor.)

## Current/incomplete stage

Stage 7 is the only incomplete stage, and its remaining work is external
validation. There is no known incomplete source extraction. The live worktree
contains the completed Stage 5 iterator milestone:

- modified `Makefile` adding `.obj/js_iterator.o`;
- deleted `quickjs.c` after assigning all of its remaining implementation;
- untracked `src/object/js_iterator.c` and `src/object/js_iterator.h`.

Stage 6 module milestone completed in the live worktree:

- `src/vm/js_module.c` and `src/vm/js_module.h` now own the ECMAScript module
  graph, linking, namespace, dynamic-import, and evaluation implementation.
- The module source is independently compiled as `.obj/js_module.o`.
- The formerly private module entry points needed by eval/interpreter code have
  narrow declarations in `js_module.h`.
- The post-extraction GCC `CONFIG_WERROR=y` build and all 11 `make test`
  commands pass.

Stage 6 VM and monolith-removal milestone completed in the live worktree:

- `src/vm/js_func.c` owns frames, closures, variable references, calls,
  constructors, and bytecode-function destruction.
- `src/vm/js_async.c` owns generators, async functions, and async generators.
- `src/vm/js_interp.c` owns the complete, unsplit `JS_CallInternal` dispatch
  loop.
- Residual eval functions moved to `src/compiler/js_codegen.c`, function-list
  initialization moved to `src/builtins/js_builtin_init.c`, and `JS_ToObject`
  moved to `src/object/js_object.c`.
- `quickjs.c` is removed from both the tree and `QJS_LIB_OBJS`.
- `CONFIG_CHECK_JSVALUE` now compiles every modular QuickJS source, not only the
  former monolith; all such check objects compile with `CONFIG_WERROR=y`.

During completion, the accidental movement of argument-object and variable-
reference helpers across the Stage 5/6 boundary was corrected. They now reside
in `src/vm/js_func.c`. The mismatched private for-in helper prototype was removed
and the helper remains `static` in `js_iterator.c`.

## Important changed files and module ownership

- `Makefile`: `vpath` discovers subsystem sources and `QJS_SRC_OBJS` is the
  authoritative modular object list.
- `src/quickjs-internal.h`: shared private layouts, forward declarations, common
  macros, and cross-subsystem contracts. It is intentionally broad at this
  intermediate stage, while public `quickjs.h` remains unchanged.
- `src/builtins/js_builtin_*.c`, `js_builtins.h`: standard builtins and intrinsic
  registration.
- `src/compiler/js_{bc,lexer,parser,codegen,opcode}.*`: bytecode IO, front end,
  compiler, and opcode definitions.
- `src/value/js_{bigint,value,string,atom}.*`: primitive/value subsystems.
- `src/runtime/js_{malloc,gc,error,runtime}.*`: allocator, collection, exception,
  and lifecycle subsystems.
- `src/object/js_{shape,object,property}.*`: hidden classes, object allocation,
  and property operations.
- `src/object/js_iterator.c`, `js_iterator.h`: iterator and enumeration protocols.
- `src/vm/js_module.c`, `js_module.h`: module graph, linking, namespace,
  dynamic import, and module evaluation.
- `src/vm/js_func.c`, `js_func.h`: function frames, closures, calls, constructors,
  and variable references.
- `src/vm/js_async.c`, `js_async.h`: generators and async state machines.
- `src/vm/js_interp.c`, `js_interp.h`: bytecode dispatch and interpreter loop.
- `quickjs.c`: removed; all former implementation now has a modular owner.

## Architectural decisions visible in the code

- The public API stays in the unchanged `quickjs.h`; new APIs are private.
- Extracted files are independently compiled, not source-included.
- Subsystem-specific headers coexist with `src/quickjs-internal.h`; narrow headers
  expose moved entry points while shared layouts remain available during the
  incremental migration.
- Hot shape lookup helpers (`get_shape_prop`, `find_own_property`, and
  `find_own_property1`) remain inline in private headers to protect property fast
  paths across translation-unit boundaries.
- Complex slow paths and lifecycle operations are ordinary out-of-line functions.
- Mechanical extraction is favored over renaming or redesign, preserving data
  layouts, bytecode behavior, ownership, and reference-count/GC behavior.
- The interpreter dispatch loop remains one intact function in its dedicated
  Stage 6 translation unit.

## Build and test status

- Committed `HEAD`: clean GCC `-Werror` build; all `make test` cases pass.
- Live worktree: a final clean GCC `CONFIG_WERROR=y` build succeeds; all 11
  `make test` commands pass.
- Every source in `QJS_SRC_OBJS` also compiles under `CONFIG_CHECK_JSVALUE` with
  warnings treated as errors.
- Stage 5 microbenchmark: 6735.81 ms total; `prop_read` 8.90 ns,
  `prop_write` 7.02 ns, `array_read` 6.66 ns, `array_write` 7.78 ns, and
  `func_call` 24.60 ns. These are consistent with the recorded baseline.
- Stage 6 non-LTO microbenchmark after monolith removal: 6721.55 ms total;
  `empty_loop` 7.21 ns, `prop_read` 8.77 ns, `prop_write` 7.32 ns,
  `array_read` 6.69 ns, `array_write` 7.76 ns, `func_call` 24.71 ns,
  `int_arith` 12.53 ns, and `float_arith` 17.37 ns. Total time is about
  0.5% above the 6689.59 ms baseline and within normal run variation.
- A clean-from-scratch `make -j4 CONFIG_WERROR=y` succeeds, including every
  modular `CONFIG_CHECK_JSVALUE` object, and the subsequent `make test` passes.
- A debug build (`qjs-debug`) succeeds with warnings as errors and passes
  `tests/test_language.js`.
- The LTO build succeeds and passes all 11 `make test` commands. Its microbenchmark
  total is 6615.80 ms (`empty_loop` 7.20 ns, `prop_read` 8.64 ns,
  `prop_write` 7.31 ns, `array_read` 6.58 ns, `array_write` 7.55 ns,
  `func_call` 23.90 ns, `int_arith` 12.36 ns, `float_arith` 17.52 ns).
- ASAN passes all 11 test commands when leak detection is disabled because
  LeakSanitizer cannot operate under the container's ptrace restrictions.
- UBSAN passes all 11 test commands.
- The 32-bit profile compiles and links all modular sources and tools. Execution
  cannot be tested here because the container kills the generated 32-bit `qjsc`
  with `SIGSYS` (`Bad system call`).
- TSAN compiles and links all modular sources and tools, but the TSAN runtime
  aborts at startup with an unsupported memory-mapping error in this environment.
- Clean archive symbol comparison against committed `HEAD` preserves all 734
  baseline-defined globals. The modular archive has 24 additional internal
  cross-translation-unit symbols; none is declared through public `quickjs.h`,
  which has no diff.
- The pinned Test262 revision (`5c8206929d81b2d3d727ca6aac56c18358c8d790`)
  was downloaded and the repository patch applied cleanly. `make test2` executed
  83,558 tests: 58 known failures, 3,356 exclusions, and 6,000 skips. There were
  zero new, changed, or fixed failures relative to the 58-line
  `test262_errors.txt`, so the exact failure set matches the baseline.
- The separate Test262-old corpus remains absent, so `make test2o` cannot verify
  its recorded zero-failure baseline.
- Kraken and Octane corpora are absent. Clang/MSAN, MinGW, and Cosmopolitan
  compilers are not installed.

## Known or suspected issues

- No locally reproducible correctness, build, or performance regression is
  known.
- Leak detection, TSAN execution, and 32-bit execution are unverified due to
  container runtime restrictions, not observed source failures.
- Current Test262 parity is verified. Test262-old parity and the non-local
  portability matrix remain unverified because their corpus/toolchains are absent.
- Independent compilation necessarily gives external linkage to 24 formerly
  translation-unit-local helpers. They are confined to private headers and do
  not alter the public API, but are visible as symbols in the static archive.
- The plan's projected intermediate `quickjs.c` sizes and original line ranges
  were stale because VM, async, and module regions were interleaved differently;
  extraction followed actual symbol ownership and the monolith is now removed.
- `task.md` and this checkpoint are untracked in the live worktree; do not lose
  them when preparing future commits.

## Exact next recommended steps

1. Obtain the separate Test262-old corpus, run `make test2o`, and confirm its
   recorded zero-failure baseline.
2. Run Kraken and Octane in the same controlled environment used for the original
   baseline if those benchmark suites are available.
3. Run Clang/MSAN, MinGW, and Cosmopolitan builds on hosts with those toolchains.
4. Rerun 32-bit and TSAN tests on hosts whose kernels/runtimes support them, and
   rerun ASAN with LeakSanitizer enabled outside the ptrace-restricted container.
5. Review and commit the Stage 5 iterator, Stage 6 VM, Makefile, `PLAN.md`, and
   checkpoint changes together or in dependency-ordered commits. Do not restore
   or rebuild `quickjs.c`.
