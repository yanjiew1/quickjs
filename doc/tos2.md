# Always-valid local TOS with an undefined sentinel

Base: `nan_boxing` at `afa15a061d8ddff822d61082c36bd7c209b7f225`.

This is a full interpreter conversion, not an opcode-by-opcode cache with
validity or dirty bits. The JSValue encoding, public API, bytecode format and
logical maximum stack depth remain unchanged. TOS applies to all value
representations. Compact NaN boxing remains opt-in: configure
`-Dvalue_representation=compact`.

**Performance status:** correctness tests passed, but the selected measured
microbenchmarks do not establish a speedup. Their speed-ratio geometric means
were 0.974x with GCC and 0.902x with Clang (larger is better). Treat this as an
experimental implementation, not a demonstrated performance improvement.

## Layout and ownership

Each regular or heap-allocated async/generator frame reserves one extra
JSValue between its locals and logical operand slots. `stack_buf` points to
the first logical operand slot; `stack_buf[-1]` is an undefined sentinel.
`sp` retains its existing next-free-slot meaning. Depth is `sp - stack_buf`.

| Depth | Local `tos` | Authoritative memory operands |
| --- | --- | --- |
| 0 | undefined sentinel | none; `sp == stack_buf` |
| 1 | first operand | none |
| N > 1 | operand N | `stack_buf[0]` through `sp[-2]` |

While running, the physical top slot `sp[-1]` may hold stale bits. Reading it
requires an explicit materialization boundary. At depth zero this address
names the allocated sentinel, so no pointer before the allocated array is
formed. A real undefined operand differs from an empty stack by depth, not tag.

Each logical operand owns exactly one reference. Local values and stale memory
bits are not independent references. Spill/reload operations neither increment
nor decrement reference counts. The undefined sentinel owns no heap allocation.

## Operations

The macros inside `JS_CallInternal` implement this protocol:

```c
/* PUSH: evaluate once before overwriting the old top. */
JSValue value = expression;
sp[-1] = tos;
tos = value;
sp++;

/* POP into an owning caller-local result. */
result = tos;
sp--;
tos = sp[-1];

/* Binary result, after consuming the operands. */
tos = result;
sp--;
```

There is no `has_top`, `valid` or `dirty` field, and no occupancy branch in
push/pop. Bytecode stack validation must still prevent underflow: a sentinel
does not make invalid bytecode safe.

`TOS_REPLACE` keeps results local without a store/reload. Integer, floating-point,
comparison and bitwise fast paths use it. `dup1` and `insert2/3/4` retain the
surviving top locally while shifting lower operands. Genuine duplication
opcodes still call `JS_DupValue`.

`TOS_DROP(n)` requires that removed values have already been freed or consumed.
Its sole possibly-zero use is `array_from`, after materializing the old top.
That helper consumes every supplied element even on failure; the interpreter
drops them before testing the result and cannot spill freed elements back.

`TOS_GROW(n)` adopts initialized slots written above the old top by stack-shaped
helpers or multi-value operations. These macros require their stated ownership
and initialized-slot preconditions; they are not public APIs.

## Helpers, exceptions and suspended frames

Stack-shaped helpers retain their existing `sp[-1]` interface. `TOS_CALL`
materializes, calls an int-returning helper, and reloads before testing status,
including on failure. `tos_status` is only a helper-result temporary, not cache
metadata. No helper receives `&tos`.

Contiguous argument-vector calls materialize the top. Borrowed argument arrays
may be modified by a bytecode callee, so these paths reload afterwards. Helpers
that take only by-value JSValues do not inherently require materialization.

Exception handling materializes before the existing memory-based unwind.
Catch restart reloads. Uncaught exits reload after unwinding so final cleanup
cannot spill a freed former top. Normal returns pop ownership into the return
value before frame cleanup.

Yield, await, initial yield and other generator exits materialize before
publishing `sf->cur_sp`. Suspended frames are fully memory-resident, preserving
GC scans and all `cur_sp[-1]` next/throw/return interfaces. Resume loads the top
before checking the throw flag. Generator allocation, undefined initialization
and var-ref placement all include the extra sentinel slot.

The extra storage is one JSValue per frame: 8 bytes in compact mode and 16 in
64-bit wide mode. Another live interpreter variable also creates register
pressure. A C local is not guaranteed a physical register; generated spills
and instruction layout still need measurement.

## Completed local validation

All builds used `-Dwerror=true -Ddocumentation=disabled` on Linux x86-64.
Final source blobs were hash-checked before being placed in Git.

| Build | Repository suite |
| --- | --- |
| Unmodified base, GCC, compact | 81/81 passed |
| TOS, GCC 14.2, compact, ASan + UBSan | 83/83 passed |
| TOS, Clang 17, compact, ASan + UBSan | 83/83 passed |
| TOS, GCC 14.2, wide, switch dispatch | 83/83 passed |

Sanitizer runs set `ASAN_OPTIONS=halt_on_error=1:detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

`test_vm_tos.js` covers empty-stack transitions, actual undefined values,
overflow, signed zero, BigInt and coercion slow paths, permutations, mutable
arguments, eval/with, classes, iterator close, exceptions/finally, generator
next/throw/return/delegation, async generators, disposal and GC.

`test_vm_tos_ownership.c` uses native class finalizers to verify that all tracked
objects are finalized exactly once after callback reentry, abrupt completion,
suspended/abandoned generators and an uncatchable interrupt.

`DIRECT_DISPATCH` has an override guard to exercise the existing portable switch
path with `-Dc_args=-DDIRECT_DISPATCH=0`, without pretending to target Emscripten.

CI includes `tos2` in the existing NaN-boxing representation/Test262 matrix.
The dedicated `ci-tos2` adds sanitized compact/wide switch builds and forced-GC
tests. Those hosted jobs and full Test262 are separate from the completed local
results above; this document does not claim that they passed.

## Selected microbenchmark measurements

Base and candidate executed the identical frozen
`tools/benchmarks/fixtures/fresh-microbench.js`, one workload per fresh process
and separate working directories. Three paired rounds alternated engine order
by workload and round. Both compilers built compact values at `-O2`, without
LTO or sanitizers, in separate source/build directories. Other common options:
`-Dtests=false -Dexamples=false -Ddocumentation=disabled`.

Environment: AMD EPYC 9V74 80-Core Processor, Linux x86-64, pinned to allowed CPU 0
but not exclusive. Frequency, turbo and host scheduling were not controlled.
Compilers: Debian GCC 14.2.0-19 and Clang 17.0.0. All 120 workload processes exited
successfully. Cells list three raw ns/op samples. Speed ratio is
median(base) / median(TOS); above 1 favors TOS.

| Compiler | Case | Base samples | TOS samples | Speed ratio |
| --- | --- | --- | --- | --- |
| GCC | empty_loop | 6.54, 6.50, 6.50 | 6.83, 6.77, 6.77 | 0.960x |
| GCC | prop_read | 7.83, 7.38, 7.39 | 7.07, 7.10, 7.14 | 1.041x |
| GCC | prop_write | 5.62, 5.62, 5.62 | 5.58, 5.58, 5.72 | 1.007x |
| GCC | array_read | 6.12, 6.19, 6.34 | 6.17, 6.24, 6.18 | 1.002x |
| GCC | array_update | 7.25, 7.33, 7.53 | 7.10, 7.44, 7.15 | 1.025x |
| GCC | func_call | 16.74, 16.75, 16.75 | 17.63, 17.68, 17.69 | 0.947x |
| GCC | func_closure_call | 16.82, 16.79, 16.81 | 18.27, 18.29, 18.57 | 0.919x |
| GCC | int_arith | 10.82, 10.80, 10.69 | 11.97, 11.96, 12.78 | 0.902x |
| GCC | float_arith | 15.99, 15.76, 15.78 | 16.34, 16.30, 16.51 | 0.966x |
| GCC | array_for_of | 20.18, 20.25, 20.25 | 22.09, 20.65, 20.64 | 0.981x |
| Clang | empty_loop | 6.23, 6.23, 6.23 | 7.04, 7.04, 7.04 | 0.885x |
| Clang | prop_read | 8.03, 8.02, 8.03 | 8.37, 8.33, 8.37 | 0.959x |
| Clang | prop_write | 5.80, 5.80, 5.86 | 6.24, 6.25, 6.25 | 0.928x |
| Clang | array_read | 6.26, 6.27, 6.26 | 7.29, 7.29, 7.29 | 0.859x |
| Clang | array_update | 7.67, 7.75, 7.67 | 9.02, 9.09, 9.02 | 0.850x |
| Clang | func_call | 17.43, 17.38, 17.39 | 19.22, 19.21, 19.45 | 0.905x |
| Clang | func_closure_call | 17.56, 17.59, 17.56 | 19.68, 19.99, 19.66 | 0.892x |
| Clang | int_arith | 10.92, 10.77, 10.96 | 11.98, 12.34, 12.58 | 0.885x |
| Clang | float_arith | 15.97, 16.04, 15.81 | 17.66, 17.65, 17.70 | 0.904x |
| Clang | array_for_of | 21.63, 21.08, 21.07 | 21.88, 21.85, 21.86 | 0.964x |

Geometric means over these ten cases: GCC 0.974x; Clang 0.902x. This is a selected
smoke comparison, not an overall engine score or a full V8/JetStream/Zoo run.
Register pressure, spill/reload traffic and code layout are possible subjects
for assembly-level investigation, not established causes from timing alone.

To reproduce a selected measurement with each engine in a fresh working directory:

```sh
/path/to/qjs --std /same/fixture/fresh-microbench.js int_arith -s result.json
```

Alternate engines, retain raw results, and use the regular full benchmark driver
for broader conclusions.

### Exact inputs

Frozen fixture SHA-256:
`9e6f7eda99cd0fa680d4d5d085bd21785c805956254aaaa417965b47b862f552`.

Candidate interpreter Git blob: `518b87d1d6fb146a959e9a141706d6b8aaaf82af`.
Candidate generator Git blob: `5f22ea77766f894c2c8db97d0463a43f103bf28a`.

Binary SHA-256 values, unchanged before and after measurements:

- GCC base: `1e0ad34b9e12a6e9ac4b1ae06b80b3ed8778656466d3047f33754031962ac4f3`
- GCC TOS: `746a60effc7433520340618b8d86ef002e6139f13332c296b66bc638dcbec464`
- Clang base: `6515efd75b8a901cae6a98fd1581d631a563118f7f3434a2dedccd1ede55527e`
- Clang TOS: `7961380656c80b646e5e8c9a0c9e329cffb7772d91f243e5b95105da525bcc8f`
