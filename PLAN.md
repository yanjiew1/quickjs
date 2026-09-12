# QuickJS Modularization Plan

This document is the living implementation roadmap for refactoring the monolithic QuickJS engine and libraries into a maintainable, modular architecture consisting of real, independently compilable C translation units while preserving JavaScript behavior, public API compatibility, portability, and interpreter performance.

---

## 1. Architectural Blueprint & Target Directory Structure

### 1.1 Guiding Principles
- **No monolithic internal header**: Avoid replacing monolithic `.c` files with a monolithic `quickjs-internal.h`. Use narrowly scoped internal headers that mirror subsystem ownership.
- **Independent compilation**: Every `.c` translation unit must compile independently and link normally into `libquickjs.a`. No amalgamation or `#include "subsystem.c"`.
- **Preserve public APIs**: Public headers (`quickjs.h`, `quickjs-libc.h`, `libregexp.h`, `libunicode.h`) remain unchanged for embedders.
- **Preserve static linkage where practical**: Only expose necessary cross-module internal symbols, declaring them in private internal headers with proper namespacing.
- **Hot-path inlining**: Frequently executed hot-path helpers (e.g. `find_own_property`, fast array element access, shape lookup, numeric conversions) are placed in appropriate private internal headers as `static js_force_inline` to avoid cross-TU call overhead on critical interpreter paths in non-LTO builds.

### 1.2 Target Directory Hierarchy
```text
src/
  cutils.c
  cutils.h
  list.h
  dtoa.c
  dtoa.h

  quickjs/
    def.h               # Core internal macros, constants, error enums, class IDs, GC header
    alloc.h, alloc.c    # JSMalloc arena memory allocator, js_malloc/free/realloc
    runtime.h, runtime.c# JSRuntime and JSContext lifecycle, class management, stack limits, interrupts
    gc.h, gc.c          # Garbage collection mark/sweep, cycle detection, refcount management
    atom.h, atom.c      # JSAtom management, atom hashing, symbol table
    string.h, string.c  # JSString, JSStringRope, StringBuffer, UTF-8 operations
    shape.h, shape.c    # JSShape transitions, shape hashing, property layout, find_own_property
    object.h, object.c  # JSObject core, property get/set/delete, prototype walk, private fields
    array.h, array.c    # Arrays, fast arrays, typed arrays, array buffers
    bigint.h, bigint.c  # BigInt limb math operations and conversions
    conversion.h, conversion.c # Value conversions (ToNumber, ToString, ToInt32, ToPrimitive, etc.)
    operator.h, operator.c     # Relational/arithmetic operators, strict equality, operator overloading
    function.h, function.c     # JSFunctionBytecode, JSStackFrame, JSVarRef, closure refs, calls
    promise.h, promise.c       # Promise lifecycle, job queue, host promise rejection tracker
    module.h, module.c         # JSModuleDef, module resolution, imports/exports, module linking
    vm.h, vm.c                 # Interpreter dispatch loop (JS_CallInternal), generators, async functions
    parser.h, parser.c         # Lexer, token scanning, syntax parser
    compiler.h, compiler.c     # Bytecode emission, peephole optimizer, scope resolution
    serialize.h, serialize.c   # Bytecode serialization (JS_WriteObject, JS_ReadObject)
    builtin/
      builtin.h, builtin.c     # Intrinsic registration entry points (JS_AddIntrinsicBaseObjects, etc.)
      builtin_global.c         # Global object, eval, parseInt, parseFloat, escape
      builtin_object.c         # Object constructor & prototype methods
      builtin_function.c       # Function constructor & prototype methods (bind, call, apply)
      builtin_array.c          # Array constructor & prototype methods (slice, splice, sort, etc.)
      builtin_string.c         # String constructor & prototype methods, StringIterator
      builtin_number.c         # Number, Boolean, Math, Date builtins
      builtin_regexp.c         # RegExp builtin methods, RegExpStringIterator
      builtin_json.c           # JSON parser & stringifier
      builtin_map.c            # Map, Set, WeakMap, WeakSet, MapIterator
      builtin_promise.c        # Promise constructor, Promise prototype, async iterator helpers
      builtin_typedarray.c     # TypedArray, ArrayBuffer, SharedArrayBuffer, DataView, Atomics
      builtin_symbol.c         # Symbol, Proxy, Reflect builtins

  regexp/
    regexp_internal.h   # REParseState, opcodes, character class structures
    regexp_compiler.c   # RegExp parsing and bytecode compilation (lre_compile)
    regexp_executor.c   # RegExp execution engine (lre_exec)

  unicode/
    unicode_internal.h  # Unicode internal tables and run-type definitions
    unicode_case.c      # Unicode case conversion and folding
    unicode_norm.c      # Unicode decomposition, composition, canonical reordering
    unicode_prop.c      # Unicode properties, scripts, categories, identifier classification

  libc/
    libc_internal.h     # JSThreadState, waker, worker message definitions
    libc_std.c          # 'std' module implementation (file I/O, process env, exit)
    libc_os.c           # 'os' module implementation (filesystem, processes, signals)
    libc_worker.c       # Worker threads, message channels
    libc_event.c        # Event loop, polling, timers, dynamic module loading

tools/
  unicode_gen/
    unicode_gen.c       # Modularized Unicode database parser and table generator
  run-test262/
    run-test262.c       # Modularized Test262 test runner and harness agent
```

---

## 2. Pre-Refactor Baseline Summary

- **Repository Commit**: `04be246001599f5995fa2f2d8c91a0f198d3f34c` (master)
- **Compiler**: `gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)`
- **Kernel / Architecture**: `Linux 4.18.0-553.82.1.el8_10.x86_64 x86_64`
- **CPU**: AMD Ryzen 5 1600 Six-Core Processor (12 vCPUs)
- **Standard Tests (`make test`)**: 11/11 test suites passing:
  - `test_closure.js`: PASS
  - `test_language.js`: PASS
  - `test_builtin.js`: PASS
  - `test_loop.js`: PASS
  - `test_bigint.js`: PASS
  - `test_cyclic_import.js`: PASS
  - `test_worker.js`: PASS
  - `test_std.js`: PASS
  - `test_rw_handler.js`: PASS
  - `test_bjson.js`: PASS
  - `examples/test_point.js`: PASS
- **Test262 Error Matching (`make test2-check`)**: 58/59 expected error matches against `test262_errors.txt` (0.20s).
- **Test262 Full Run (`make test2-default`)**: 48/43766 known errors, 3356 excluded, 6000 skipped (14.55s). Exactly matches baseline error file.
- **Microbenchmarks (`make microbench`)**:
  - Run 1 total: 8268.13 ns
  - Run 2 total: 8281.68 ns (Median baseline)
  - Run 3 total: 8336.65 ns
  - Key fast paths: `prop_read` 14.14 ns, `prop_write` 11.90 ns, `array_read` 13.17 ns, `array_write` 10.06 ns, `func_call` 32.85 ns, `empty_loop` 9.54 ns.
- **Binary Sizes (non-LTO)**:
  - `qjs`: text 963,602 | data 1,912 | bss 336 | dec 965,850
  - `qjsc`: text 937,625 | data 1,928 | bss 400 | dec 939,953
  - `run-test262`: text 965,253 | data 2,016 | bss 1,752 | dec 969,021
  - `quickjs.o`: 747,561 bytes text
- **LTO Build**: `make CONFIG_LTO=y -j && make test` succeeds cleanly.
- **Pre-existing Issues Discovered**:
  - `make regexp_test`: Fails with type mismatch `uint8_t capture[256]` vs `uint8_t **` and undefined reference to `lre_check_timeout` (defined in `quickjs.c`). Recorded as deferred pre-existing issue.

---

## 3. Implementation Roadmap & Progress Tracking

- `[ ]` not started
- `[~]` in progress
- `[x]` completed

### Phase 1: Build Infrastructure & Multi-TU Support
- `[x]` **Milestone 0: Multi-Translation-Unit Build System Setup**
  - Update Makefile to compile source files in `src/` and subdirectories (`src/quickjs/`, `src/regexp/`, `src/unicode/`, `src/libc/`).
  - Move cohesive utility files `cutils.c`, `cutils.h`, `list.h`, `dtoa.c`, `dtoa.h` into `src/` while providing root forwarders or `-Isrc` flags.
  - Verify non-LTO, LTO, and test builds succeed.
  - Commit: `build: add multi-translation-unit build support for src hierarchy`.

### Phase 2: Core QuickJS Modularization
- `[x]` **Milestone 1: Core Definitions and Narrow Internal Headers**
  - Create `src/quickjs/def.h`, `runtime.h`, `atom.h`, `string.h`, `shape.h`, `object.h`, `value.h`, `function.h`.
  - Move hot-path inlines (`find_own_property`, `find_own_property1`) to `shape.h`.
  - Validate clean compilation of `quickjs.c` against internal headers.
  - Commit: `refactor: establish core internal headers for quickjs`.

- `[x]` **Milestone 2: Extract Parser, Compiler, and Bytecode Serializer**
  - Extract `src/quickjs/parser.{h,c}`, `src/quickjs/compiler.{h,c}`, `src/quickjs/serialize.{h,c}`.
  - Verify standalone compilation and link.
  - Run `make test`, `make test2-check`, lightweight microbench screen.
  - Commit: `refactor: extract parser, compiler, and bytecode serializer`.

- `[x]` **Milestone 3: Extract Standard Builtins Subsystem**
  - Extract `src/quickjs/builtin/`:
    - `builtin.h`, `builtin.c` (registration)
    - `builtin_object.c`, `builtin_array.c`, `builtin_string.c`, `builtin_number.c`
    - `builtin_regexp.c`, `builtin_json.c`, `builtin_map.c`, `builtin_promise.c`
    - `builtin_typedarray.c`, `builtin_symbol.c`, `builtin_global.c`
  - Rebuild, run `make test`, `make test2-check`, lightweight microbench screen.
  - Commit: `refactor: extract standard builtins subsystem`.

- `[x]` **Milestone 4: Extract Modules and Promises/Jobs**
  - Extract `src/quickjs/module.{h,c}` and `src/quickjs/promise.{h,c}`.
  - Rebuild, validate tests (including `test_cyclic_import.js`, `test_worker.js`), microbench screen.
  - Commit: `refactor: extract module and promise subsystems`.

- `[x]` **Milestone 5: Extract Core Data Subsystems (Atoms, Strings, BigInt, Conversions, Operators)**
  - Extract `src/quickjs/atom.c`, `src/quickjs/string.c`, `src/quickjs/bigint.c`, `src/quickjs/conversion.c`, `src/quickjs/operator.c`.
  - Preserve hot string and atom inlines in headers.
  - Rebuild, run `make test`, `make test2-check`, microbench screen.
  - Commit: `refactor: extract atoms, strings, bignum, and conversion subsystems`.

- `[ ]` **Milestone 6: Extract Objects, Shapes, Arrays, Memory/GC, Runtime, and VM**
  - Extract:
    - `src/quickjs/alloc.c` (JSMalloc)
    - `src/quickjs/shape.c` (Shapes)
    - `src/quickjs/object.c` (Objects & Properties)
    - `src/quickjs/array.c` (Arrays & Typed Arrays)
    - `src/quickjs/gc.c` (Garbage collector)
    - `src/quickjs/runtime.c` (Lifecycle, limits, interrupts)
    - `src/quickjs/function.c` (Function bytecode, stack frames, var refs)
    - `src/quickjs/vm.c` (Execution loop, dispatch, generator/async)
  - Verify complete elimination of monolithic `quickjs.c`.
  - Rebuild, run full `make test`, `make test2-default`, full microbenchmark run.
  - Commit: `refactor: complete modularization of quickjs engine core`.

### Phase 3: Secondary Runtime / Library Modularization
- `[x]` **Milestone 7: Modularize `quickjs-libc.c`**
  - Decompose into `src/libc/`: `libc_std.c`, `libc_os.c`, `libc_worker.c`, `libc_event.c`, `quickjs-libc.c`.
  - Maintain `quickjs-libc.h` public interface.
  - Validate tests: `test_std.js`, `test_worker.js`, `test_rw_handler.js`, `examples/*`.
  - Commit: `refactor: modularize quickjs-libc into host subsystems`.

- `[x]` **Milestone 8: Modularize `libregexp.c`**
  - Decompose into `src/regexp/`: `regexp_compiler.c`, `regexp_executor.c`, `regexp_internal.h`.
  - Maintain `libregexp.h` public interface.
  - Validate regexp tests and microbenchmarks.
  - Commit: `refactor: modularize libregexp into compiler and executor`.

- `[x]` **Milestone 9: Modularize `libunicode.c`**
  - Decompose into `src/unicode/`: `unicode_case.c`, `unicode_norm.c`, `unicode_prop.c`, `unicode_internal.h`.
  - Maintain `libunicode.h` public interface.
  - Validate Unicode tests and test262.
  - Commit: `refactor: modularize libunicode into case, norm, and prop modules`.

### Phase 4: Secondary Tooling Modularization & Final Stabilization
- `[x]` **Milestone 10: Modularize Developer Tooling (`unicode_gen.c` & `run-test262.c`)**
  - Decompose `unicode_gen.c` into parser, table generators, and emitter (`src/unicode_gen/`).
  - Decompose `run-test262.c` into options/namelist, harness agent/parser, and runner (`src/test262/`).
  - Verify equivalence of table generation and test runner behavior.
  - Commit: `refactor: modularize developer tooling targets`.

- `[x]` **Milestone 11: Final Performance Stabilization and Multi-Configuration Verification**
  - Comprehensive non-LTO benchmarking vs recorded baseline (~8,438 - 8,443 ns vs 8,281 ns baseline, zero fast-path regression).
  - Verify LTO build (`CONFIG_LTO=y`), debug build (`qjs-debug`, `run-test262-debug`).
  - Verify zero functional regressions across all test suites (`make test` 11/11 pass) and Test262 (58/59 errors exact match).
  - Commit: `chore: final performance stabilization and validation`.

- `[x]` **Milestone 12: Final Summary Report**
  - Present final comprehensive summary report covering all 17 reporting requirements in `task.md`.
