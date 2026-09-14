# Future QuickJS optimization experiments

This document preserves optimization ideas explored during Final Performance
Stabilization of the modularization refactor.  They are **not** part of the
structural refactor's performance justification and must not be counted as
final pristine-versus-modular evidence.

The exact pre-cleanup tracked diff is archived outside the repository at:

```
/home/yanjie/src/quickjs-final-perf-precleanup.patch
SHA-256: 68d4d8959e664f0b7ef83ab7c2f3cc9b315bf48f026927deb1c9b78aec33c01f
```

The 1,315-line patch was captured from `a5ec7f4` plus the complete uncommitted
stabilization diff.  It excludes the untracked user-supplied `task.md`.  It also
contains legitimate restorative work, so a future optimization task should
extract individual hunks rather than applying the patch wholesale.

Raw measurements, profiles, candidate binaries, and disassembly remain under
`/tmp/qjs-final-perf/` on the original test host.  `/tmp` is not durable; the
important conclusions and measurements are therefore summarized below.

## Measurement context

- Pristine source: commit `04be246`.
- Modular source base: `a5ec7f4`.
- GCC: 16.2.0, normal `-O2`, non-LTO.
- Clang: 23.1.1, normal `-O2`, non-LTO.
- Focused measurements used CPU 2, alternating pristine/candidate order,
  usually five or seven fixed-count pairs, and `perf stat` task-clock, cycles,
  instructions, branches, and branch misses.
- Repository microbenchmark adaptive-loop results were useful for discovery but
  fixed-count counters were preferred for confirmation.
- Candidate numbers identify combined experimental states, not isolated
  upstream-quality patches.  Improvements below must be re-established in a
  future optimization-only task.

## 1. Direct integer conversion in `JS_ConcatString`

**Location:** `src/quickjs/atom-string.c`, `JS_ConcatString`.

**Motivation:** intermediate modular builds showed `int_to_string` and string
construction regressions.  The experiment detects `JS_TAG_INT` operands,
formats them with `i32toa`, and creates the string locally with
`js_new_string8_len`; other values retain `JS_ToStringFree`.

**Why this is a new optimization:** pristine `JS_ConcatString` calls
`JS_ToStringFree` for non-string operands.  Pristine disassembly likewise shows
calls to `JS_ToStringFree`, whereas the experiment adds direct calls to
`i32toa` and the string allocator.  The shortcut would independently optimize
monolithic QuickJS and is not merely removal of a modular wrapper.

**Evidence:** combined GCC candidate 33 executed about 8.28% fewer instructions
and 8.23% fewer branches in fixed `int_to_string`, with cycles about 2.61% below
pristine.  Clang candidate 26 was cycle-neutral while still executing 3.54%
more instructions and 2.35% more branches.  A later placement experiment moved
Clang candidate 34 from roughly +10.8% cycles to candidate 35 at roughly -7.8%
cycles without changing those instruction/branch deltas.  This demonstrates a
strong interaction with layout rather than an isolated algorithmic result.

**Status:** promising but entangled.  Risks include exception/allocation
equivalence, duplicated conversion policy, and compiler/layout sensitivity.
Only GCC/Clang WERROR candidate builds and benchmark execution were completed;
the combined experimental state was not accepted through the final correctness
matrix.

## 2. Direct int32/uint32 typed-array store

**Location:** `src/quickjs/internal-property.h`,
`qjs_set_property_value_fast`; selected by `src/quickjs/function-vm.c`.

**Motivation:** investigate property-set overhead after property ownership moved
out of the interpreter TU.  The helper recognizes an object, integer property,
integer value, `JS_CLASS_INT32_ARRAY` or `JS_CLASS_UINT32_ARRAY`, and an in-range
index, then writes `u.uint32_ptr[index]` directly.  All other cases call the
normal property owner.

**Why this is a new optimization:** it bypasses the pristine property setter
algorithm and adds a speculative type/class special case absent from monolithic
QuickJS.  It would be independently useful upstream.

**Evidence:** the original formal comparison already had `typed_array_write`
substantially faster than pristine (about -43.5% under GCC and -25.3% under
Clang), so there was no confirmed regression requiring this path.  An early
Clang profile attributed about 12.6% of samples to `JS_SetPropertyValue`, which
suggested optimization opportunity but not refactor damage.  No clean isolated
speedup was established.

**Status:** out of scope and insufficiently isolated.  Important risks include
detached/out-of-bounds buffers, exotic property semantics, and duplicating
typed-array ownership invariants.

## 3. Short-BigInt `SameValueZero` specialization

**Location:** `src/quickjs/builtin-collection.c`, `map_same_value_zero`, used by
`map_find_record` and `map_delete_record`.

**Motivation:** reduce collection equality call overhead.  When both operands
are `JS_TAG_SHORT_BIG_INT`, the helper compares their embedded values directly;
otherwise it calls `qjs_same_value_zero`.

**Why this is a new optimization:** pristine map lookup calls its general
`js_same_value_zero`/`js_strict_eq2` path.  Pristine GCC disassembly confirms
that it does not contain this direct short-BigInt comparison.  The shortcut is
useful independently of modularization.

**Evidence:** the combined formal GCC candidate reported `map_set_bigint` about
8.7% faster, while the earlier Clang formal result was about 1.8% slower.  The
specialization was not isolated from allocator, layout, and other collection
changes.

**Status:** promising only as future general optimization work.  Validate mixed
short/heap BigInt equality, hashing consistency, deletion, weak collections,
and all equality modes before reconsidering it.

## 4. Direct ordinary fast-array `pop`

**Location:** `src/quickjs/builtin-array.c`, `js_array_pop`.

**Motivation:** fixed-count GCC measurements of the restorative candidate showed
`array_pop` about +7% cycles, +2.27% instructions, and +4.16% branches versus
pristine.  The experiment recognizes a writable ordinary dense array whose
integer `length` equals its fast-array count, returns the last element directly,
and updates count and length without executing the generic property algorithm.

**Why this is a new optimization:** pristine always enters the generic
`ToObject`/length/property/delete path.  This is a new algorithmic fast path
that would strongly improve pristine QuickJS too.

**Evidence:** GCC candidate 47 measured approximately -44.4% cycles, -38.1%
instructions, and -36.7% branches.  Clang candidate 47 measured approximately
-46.7% cycles, -33.7% instructions, and -33.2% branches.  The magnitude confirms
that the change does much more than restore refactor-added work.

**Status:** highly promising for an optimization project.  Risks include array
holes, prototype/accessor effects, writable/configurable length invariants,
shift sharing the same implementation, GC/value ownership, and exact error
behavior.  Candidate builds succeeded, but no final semantic validation was
performed for the experimental state.

## 5. Direct fast-array iterator length and value access

**Location:** `src/quickjs/builtin-array.c`, `js_array_iterator_next`.

**Motivation:** Clang candidate 25 showed `array_for_of` near +20.6% cycles with
+7.23% instructions and +9.45% branches.  Profiles exposed modular property and
conversion functions on the path.  The experiment reads `p->u.array.count`
for dense ordinary arrays and duplicates `p->u.array.u.values[idx]` directly;
typed arrays and all fallback cases retain the generic code.

**Why this is a new optimization:** pristine obtains length and elements through
normal property access.  Direct representation access is an algorithmic array
iterator optimization, not merely removal of one newly added forwarding call.

**Evidence:** Clang candidate 35 measured `array_for_of` around -11% to -13%
cycles, -20.35% instructions, and -12.85% branches across seven alternating
pairs.  GCC candidate 46 was roughly -24% cycles with about -27.3% instructions
and -26.9% branches.  Branch-miss percentages were noisy because the absolute
miss count was small.

**Status:** highly promising but out of scope.  A future task must validate
mutation during iteration, holes, prototype properties, exotic arrays, length
changes, iterator kinds, and exception/GC behavior.

## 6. Direct C/non-bytecode method dispatch in the interpreter

**Location:** `src/quickjs/function-vm.c`, `OP_call_method` and
`OP_tail_call_method`.

**Motivation:** GCC `array_push` remained approximately +15% to +17% cycles with
only +0.02% instructions and +0.11% branches after structural remedies.  The
first experiment inspected the callee class in the opcode and invoked a
non-bytecode class call hook directly.  A second specialization called
`js_call_c_function` directly for `JS_CLASS_C_FUNCTION`, polling interrupts
before the call and falling back to `JS_CallInternal` otherwise.

**Why this is a new optimization:** pristine always calls `JS_CallInternal` at
this opcode and lets it dispatch the callee.  Bypassing that interpreter entry
is a new dispatch shortcut useful in monolithic QuickJS.

**Evidence:** the generic hook version (GCC candidate 46) reduced executed
instructions about 7.51% and branches about 2.98%, but `array_push` still showed
about +7.6% cycles.  The direct C-function version (candidate 47) measured about
-12.5% cycles, -8.35% instructions, and -2.98% branches.  Under Clang, compiling
the direct specialization enlarged/relocated `JS_CallInternal` and caused
unrelated `int_to_string`, large-string, and RegExp regressions; the experiment
was subsequently GCC-guarded.

**Status:** promising GCC optimization, harmful/fragile as a compiler-neutral
change.  Validate interrupt timing, realm/stack-frame setup, argument padding,
tail calls, constructors, exotic class hooks, recursion limits, backtraces, and
exception handling in any future attempt.

## 7. Forced-inline `qjs_get_length32`

**Location:** `src/quickjs/internal-object.h`; the former out-of-line definition
was in `src/quickjs/builtin-base.c`.

**Motivation:** remove a visible modular call on length-heavy paths by placing
the full property/get-and-convert helper in a private header.

**Why this is a new optimization:** pristine GCC and Clang both emit and call an
out-of-line `js_get_length32`.  Forcing all modular consumers to inline it goes
beyond restoration and could optimize pristine independently.

**Evidence:** combined candidate 33 still measured GCC `array_write` about
+0.16% cycles and +1.55% instructions.  Clang candidate 26 measured about
+0.27% cycles and +0.32% instructions.  No isolated useful improvement was
established.

**Status:** inconclusive and outside scope.  A future task should first identify
specific call sites where specialization is semantically and measurably useful.

## 8. Copied/inlined allocator arena fast path

**Location:** `src/quickjs/internal-allocator.h`,
`qjs_malloc_block_size_index`, `qjs_malloc_raw_fast`, and
`qjs_free_raw_fast`; `src/quickjs/allocator.c` exported
`qjs_malloc_block_sizes` to support it.

**Motivation:** Clang profiles showed modular allocation layers in hot paths.
For example, the unstabilized weak-map profile attributed about 5.45% to
`qjs_malloc_raw`, 2.64% to `js_free_rt`, and 0.74% to `js_malloc`; the pristine
profile instead showed the allocator implementation directly.  The experiment
copied the common free-arena pop/push algorithm into a force-inlined private
header and fell back to the allocator owner for arena creation, destruction,
zero-size, and large-block cases.

**Why this is a new optimization:** pristine still executes out-of-line calls to
`__js_malloc` and `__js_free`.  Inlining their arena algorithm into each caller
would independently optimize the monolithic implementation.  Only a thin
adapter that removes refactor-added wrapper calls is restorative.

**Evidence:** later combined candidates removed allocator symbols from important
hot profiles and helped weak-map, RegExp, argument, and string screens, but a
clean isolated delta was not recorded.  Candidate 35 had `regexp_ascii` about
-4.5% cycles and large-string construction neutral; these results also include
other changes.  The header duplicates roughly 80 lines of allocator invariants
across consumers and broadens access to the block-size table.

**Status:** potentially useful, but maintenance-heavy and not isolated.  A
future task should compare thin raw-owner calls, selective caller inlining,
compiler IPA options, and the copied algorithm while testing custom allocation,
large-block-only, iterator bitmap, OOM, arena create/destroy, and sanitizer
configurations.

## 9. Compiler placement and alignment experiments

**Locations:** `dtoa.c` (`i32toa`), `src/quickjs/atom-string.c`
(`JS_ConcatString`), and experimental compiler attributes on
`JS_CallInternal` in `src/quickjs/function-vm.c`.

Several bounded experiments investigated cycle-only results:

- Clang `i32toa` `hot, aligned(64)` moved the symbol from approximately
  `0x0a5f00` to `0x01b300`.  Candidate 34 to 35 changed fixed
  `int_to_string` from roughly +10.8% to -7.8% cycles while instructions stayed
  +3.54% and branches +2.35%.  This is strong placement sensitivity, but it was
  measured with the new integer-concat shortcut active.
- Making Clang `JS_CallInternal` plain moved it from about `0x011c00` to
  `0x0375c0` with identical function size and caused `int_to_string` and RegExp
  losses.  Restoring its prior hot placement recovered the earlier symbol
  addresses.  This particular interpreter attribute already predates cleanup;
  the experiment is recorded to warn future work about whole-engine layout
  coupling.
- GCC `optimize("align-labels=16")` largely recovered `array_push`, but enlarged
  `JS_CallInternal` to about `0xad0b` bytes and regressed `regexp_replace` around
  13%; it was rejected.  Label alignment 4/8 reduced the push difference only
  partially and worsened pop.  Selective `.p2align` before candidate opcode
  labels had no material effect and was removed.
- GCC opcode offsets differed substantially despite near-identical executed
  work: pristine `OP_call_method`, `OP_call`, `OP_drop`, `OP_push_i32`, and
  `OP_get_field2` were approximately `+0xcf2`, `+0xe02`, `+0x1224`, `+0x129c`,
  and `+0x5275` from `JS_CallInternal`; one modular candidate placed them near
  `+0xc20`, `+0xd23`, `+0x13a5`, `+0x141f`, and `+0x3e63`.

**Why these are not automatically restorative:** an accidental address or cache
line in one pristine link is not an architectural property.  Explicit alignment
must be justified by repeatable generated-code evidence and must not merely tune
the modular binary around one favorable address.

**Status:** diagnostic and compiler-specific.  The broad GCC experiments were
harmful or inconclusive.  The Clang `i32toa` result is promising but entangled.
Future work should record I-cache/iTLB and branch data where supported, vary
link order/address placement deliberately, and validate across machines before
retaining fragile alignment directives.

## Reproduction notes

The archival patch can be inspected without applying it:

```sh
git apply --stat /home/yanjie/src/quickjs-final-perf-precleanup.patch
git apply --numstat /home/yanjie/src/quickjs-final-perf-precleanup.patch
```

Useful raw locations on the original host include:

- `/tmp/qjs-final-perf/perf/candidate33-gcc/`
- `/tmp/qjs-final-perf/perf/candidate35-repeat-clang/`
- `/tmp/qjs-final-perf/perf/candidate46-gcc/`
- `/tmp/qjs-final-perf/perf/candidate47-gcc/`
- `/tmp/qjs-final-perf/perf/candidate47-clang/`
- `/tmp/qjs-final-perf/perf/candidate49-clang/`
- `/tmp/qjs-final-perf/profiles/`
- `/tmp/qjs-final-perf/function-vm-labels-gcc.o`
- `/tmp/qjs-final-perf/quickjs-baseline-labels-gcc.o`

Reimplementation should begin from the final structural-refactor commit, apply
one optimization family at a time, run complete semantic validation, and compare
against both the structural state and pristine QuickJS.  None of the numbers in
this document substitutes for the structural refactor's final performance
acceptance evidence.
