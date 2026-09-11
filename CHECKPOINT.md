# QuickJS Modularization Checkpoint & Handover Log

## Current Status
- **Phase**: Phase 2 — Core QuickJS Modularization
- **Active Milestone**: Milestone 3 (Extract Standard Builtins Subsystem)
- **Last Completed Milestone**: Milestone 2 (Extract Parser, Compiler, and Bytecode Serializer)

---

## Pre-Refactor Baseline
- **Baseline Git Commit**: `04be246001599f5995fa2f2d8c91a0f198d3f34c`
- **Compiler**: `gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)`
- **Host / OS**: `Linux master.idm.bze.me 4.18.0-553.82.1.el8_10.x86_64 x86_64`
- **CPU**: AMD Ryzen 5 1600 Six-Core Processor (12 threads)
- **Standard Tests (`make test`)**: 11/11 pass (100%):
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
- **Test262 Error Match (`make test2-check`)**: 58/59 errors matched `test262_errors.txt` (0.20s).
- **Test262 Full Run (`make test2-default`)**: 48/43766 errors, 3356 excluded, 6000 skipped (14.55s). Zero unexplained failures.
- **Microbenchmarks (`./qjs --std tests/microbench.js`)**:
  - Run 1: 8268.13 ns
  - Run 2: 8281.68 ns (Baseline median)
  - Run 3: 8336.65 ns
  - Detailed reference values (from Run 2):
    - `empty_loop`: 9.54 ns
    - `prop_read`: 14.14 ns
    - `prop_write`: 11.90 ns
    - `prop_update`: 16.93 ns
    - `array_read`: 13.17 ns
    - `array_write`: 10.06 ns
    - `global_read`: 11.24 ns
    - `global_write`: 11.65 ns
    - `global_func_call`: 35.84 ns
    - `func_call`: 32.85 ns
    - `func_closure_call`: 32.91 ns
    - `int_arith`: 14.68 ns
    - `float_arith`: 22.02 ns
    - `string_length`: 14.25 ns
    - `string_to_int`: 115.56 ns
- **Binary Sizes (non-LTO)**:
  - `qjs`: text 963602, data 1912, bss 336, dec 965850
  - `qjsc`: text 937625, data 1928, bss 400, dec 939953
  - `run-test262`: text 965253, data 2016, bss 1752, dec 969021
  - `quickjs.o` (in `libquickjs.a`): 747561 bytes text
- **LTO Validation**: `make CONFIG_LTO=y -j && make test` passes cleanly.
- **Pre-existing Issues Recorded**:
  - `make regexp_test`: Fails with type mismatch `uint8_t capture[256]` vs `uint8_t **` and undefined reference to `lre_check_timeout` (defined in `quickjs.c`). Deferred as an existing issue per `task.md`.

---

## Architectural & Design Decisions
1. **Source Layout**: All C implementation modules and narrow internal headers are placed under `src/` (`src/quickjs/`, `src/regexp/`, `src/unicode/`, `src/libc/`, etc.).
2. **Public API Preservation**: `quickjs.h`, `quickjs-libc.h`, `libregexp.h`, and `libunicode.h` remain accessible in the root for external consumers.
3. **No Monolithic Internal Header**: Internal headers are modularized: `def.h`, `runtime.h`, `atom.h`, `string.h`, `shape.h`, `object.h`, `value.h`, `function.h`, `vm.h`, etc.
4. **Hot-Path Preservation**: `find_own_property`, `find_own_property1`, fast array inline helpers, and numeric fast conversion inlines are kept in headers using `js_force_inline` to ensure zero performance degradation in non-LTO compilation.
5. **Incremental Migration Sequence**: Coarse-grained subsystems are extracted first (infrastructure, headers, compiler/parser, builtins, modules, data subsystems, core engine), validated with tests and microbenchmarks after every milestone.

---

## Milestone 1 Validation Results
- **Standard Tests (`make test`)**: 11/11 tests pass (100%).
- **Test262 Error Match (`make test2-check`)**: 58/59 errors matched `test262_errors.txt`.
- **Microbenchmarks (`./qjs --std tests/microbench.js`)**:
  - Total: 8280.73 ns (Baseline median: 8281.68 ns, diff: -0.01%)
  - `prop_read`: 14.16 ns (Baseline: 14.14 ns)
  - `prop_write`: 11.80 ns (Baseline: 11.90 ns)
  - `func_call`: 32.57 ns (Baseline: 32.85 ns)
  - Zero performance regression.

---

## Milestone 2 Validation Results
- **Extracted Modules**:
  - `src/quickjs/opcode.{h,c}`: bytecode opcode information, operand tables, and special opcode defines.
  - `src/quickjs/serialize.{h,c}`: bytecode serialization (`JS_WriteObject`, `JS_ReadObject`).
  - `src/quickjs/parser.{h,c}`: JavaScript syntax lexer, token scanner, parse state definitions.
  - `src/quickjs/compiler.{h,c}`: JavaScript AST-to-bytecode compiler, scope management, peephole optimizer, and evaluation entry points (`JS_Eval`, `JS_EvalThis`, `JS_EvalObject`, `JS_EvalInternal`).
- **Standard Tests (`make test`)**: 11/11 tests pass (100%).
- **Test262 Error Match (`make test2-check`)**: 58/59 errors matched `test262_errors.txt` (0.20s).
- **LTO Validation**: `make CONFIG_LTO=y -j && make test` passes 100%.
- **Microbenchmarks (`./qjs --std tests/microbench.js`)**:
  - Total: 8238.32 ns (Baseline median: 8281.68 ns, diff: -0.52%)
  - Zero performance regression.
- **Binary Sizes (non-LTO)**:
  - `qjs`: text 959922 (Baseline: 963602)
  - `qjsc`: text 929745 (Baseline: 937625)
  - `run-test262`: text 957373 (Baseline: 965253)

---

## Exact Next Steps
1. Milestone 3: Extract Standard Builtins Subsystem (`src/quickjs/builtin/`).
2. Milestone 3: Separate object, array, string, number, math, regexp, json, map, promise, typedarray, symbol, and global builtins into dedicated translation units.
3. Validate tests (`make test`, `make test2-check`) and microbenchmarks.
4. Commit Milestone 3 according to Milestone Git Commits rules.
