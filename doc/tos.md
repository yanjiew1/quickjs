# Always-valid interpreter top of stack

The `tos3` branch implements a local top-of-stack value on top of
`nan_boxing` commit `afa15a061d8ddff822d61082c36bd7c209b7f225`.
`JS_CallInternal` keeps its top operand in the C local `JSValue tos` across
opcode dispatch. A bottom sentinel makes the local valid at logical depth
zero. There is no presence flag, dirty flag, per-opcode cache-state lookup,
or store/reload in the dispatch macro.

The implementation applies to compact, legacy and wide values. Select
`-Dvalue_representation=compact` explicitly to use the 8-byte representation
on a 64-bit host; the branch retains the existing default value mode.

## Frame layout and ownership

Each ordinary or suspended function frame reserves one additional `JSValue`:

| Region | Physical placement |
| --- | --- |
| Copied arguments, when required | Unchanged |
| Local variables | Unchanged |
| Bottom sentinel | `var_buf[b->var_count]`, initialized to `JS_UNDEFINED` |
| Operand stack | Begins at `stack_buf = var_buf + b->var_count + 1` |
| Local variable references | Begins after `stack_buf + b->stack_size` |

`sp` retains its conventional meaning: one past the logical operand stack.
The logical depth is still `sp - stack_buf`. The sentinel is not a JavaScript
operand and does not change the compiler's stack analysis, maximum logical
stack size, opcode definitions, or serialized bytecode.

At every opcode boundary:

- At positive depth, `tos` owns the top operand. The lower operands are owned
  by the memory slots in `[stack_buf, sp - 1)`.
- The memory slot `sp[-1]` is a non-owning mirror and may be stale. Code must
  not read it as the current top without an explicit materialization boundary.
- At depth zero, `tos` and `stack_buf[-1]` are both `JS_UNDEFINED`.

The sentinel uses an ordinary non-reference-bearing value. Its value is
never tested to determine whether the logical stack is empty; a real
`undefined` operand behaves normally. Empty-stack decisions use `sp` and
`stack_buf` only where control flow actually needs the depth, such as catch
unwinding. The invariant assumes the same valid bytecode as the original VM.

## Stack operations

`TOS_PUSH(value)` first evaluates `value` into a temporary, then performs:

```c
sp[-1] = tos;
++sp;
tos = push_value;
```

Evaluation must occur first because the expression can read `tos`, use a
relative stack operand, advance `pc`, allocate, or call another function.
At depth zero, the store simply writes the sentinel. At positive depth, the
previous top becomes an ordinary lower stack operand.

After the caller transfers or frees the consumed value, `TOS_DROP(count)`
performs:

```c
sp -= count;
tos = sp[-1];
```

Dropping the last operand loads the sentinel. There is no conditional load
and no `has_top` test. The macro itself does not free values: opcode handlers
retain their original ownership and consumption contracts.

Unary operations update `tos` directly. A binary fast path reads its left
operand from `sp[-2]`, reads its right operand from `tos`, writes the result
to `tos`, and decrements `sp`. It does not write the result to the memory
stack and immediately load it again. Stack shuffles write only the operands
that must remain below the cached top. For example, `nip` frees `sp[-2]`
and decrements `sp` while retaining `tos`.

## Explicit materialization boundaries

`TOS_SPILL()` writes `tos` to `sp[-1]`; `TOS_RELOAD()` loads it back. Neither
operation changes a reference count. The mirror is never a second owner.
The address of `tos` is not passed to helpers, leaving it eligible for
register allocation.

Existing helpers that consume or produce an operand-stack range use
`TOS_SLOW(call, delta)`:

1. Materialize the top before evaluating the helper call.
2. If it fails, reload at the original depth and enter exception handling.
   The helper's original error contract leaves retained operands or cleared
   consumed slots at that depth.
3. If it succeeds, apply its logical stack delta and reload the new top.

Reloading after the success delta avoids treating an already-consumed old
top as the result. Reloading on failure is essential: iterator setup,
post-increment and arithmetic helpers can replace or clear the top before
failing. Common fast paths do not use this boundary. Calls that receive
values individually can use `tos` directly without materializing it.

### Calls and argument mutation

Bytecode calls can lend their argument slots to the callee. Parameter
assignment and mapped `arguments` can therefore overwrite the caller's
materialized top, freeing the object that its old cached value referenced.
Direct calls, method calls, constructor calls and fallback eval calls:

1. Materialize the argument range before the call.
2. Reload immediately after the call, before checking the return value or
   taking a tail-call exit.
3. Retain the original argument cleanup and returned-value ownership.

This reload is required on exceptional returns as well as successful ones.
Iterator calls through `JS_Call`/`JS_CallFree` use the public copy-arguments
contract: they materialize their argument but do not need to reload it.
Array construction materializes its input range, consumes the range, and
restores the top at the reduced depth even if allocation fails.

### Exceptions, return, yield and await

Exception unwinding pops the cached top and reloads the next value using the
same invariant. Catch offsets and iterator-close markers retain their
original interpretation; the sentinel lies below the unwind boundary.
Uncatchable exceptions reach normal bulk cleanup with the current cached top.

Both ordinary cleanup and generator suspension materialize the top before
the existing memory-based owner takes over. A suspended frame publishes
`cur_sp` only after this store. Generator resume reloads `tos` before testing
the throw flag, so `.throw()` and rejected awaits enter exception handling
with a valid top.

`async_func_init` reserves and initializes the same sentinel as an ordinary
frame. Existing suspended-frame GC and teardown traverse the full
`arg_buf .. cur_sp` range, including the harmless undefined sentinel.
Running generator frames continue to have `cur_sp == NULL` and retain the existing
reference-count/GC treatment of active C roots; no new GC root field is added.

## Costs and limits

- Physical frame storage grows by one `JSValue`: 8 bytes in compact mode,
  or the representation's value size in legacy/wide mode. Allocation and
  stack-overflow accounting include this slot.
- The C local is eligible for a register, but the compiler decides its
  actual location. Compact needs one 64-bit value; wide has more register
  pressure. A C local alone does not establish a throughput improvement.
- Slow helpers and argument-array calls still need explicit stack stores.
  The gain depends on the executed opcode mix, compiler and architecture.
- The public value ABI and bytecode encoding are unchanged by this change.
  Selecting a different value representation still carries the existing
  rebuild requirement documented in `BUILDING.md`.

## Validation

`tests/test_vm_stack.js` covers empty and deep operand stacks, value kinds,
borrowed argument replacement, call variants, shuffles, coercion failures,
iterator failures, catch/finally, and generator/async suspension.
`tests/test_vm_stack.c` uses class finalizers to verify both survival and
exact release of temporary objects, including cyclic objects held only by
operand stacks. Both tests are part of the repository and GC stress suites.

The usual primary check is:

```sh
CC=gcc meson setup build-tos-sanitize \
  -Dwerror=true -Dvalue_representation=compact \
  -Db_sanitize=address,undefined
meson compile -C build-tos-sanitize -j4
meson test -C build-tos-sanitize --print-errorlogs --num-processes 4
```

Use separate build directories for Clang, wide values and GC stress.
The default dispatch selection can be overridden with
`CFLAGS=-DDIRECT_DISPATCH=0` for native switch-dispatch validation without
changing platform features. `ci-nan-boxing.yml` runs on `tos3` and covers
compact sanitizers, the three 32-bit representations, compact without
`__int128`, and compact/wide switch dispatch, including full Test262.

For performance comparisons, use the exact `nan_boxing` base above and the
paired fresh-process policy in `tools/benchmarks/README.md`. Select compact
explicitly in both builds; the benchmark driver's default Meson invocation
otherwise measures legacy mode. Record individual V8 v7 and microbenchmark
results, compiler flags and the environment before drawing conclusions.
