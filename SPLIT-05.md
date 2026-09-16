# split-05: correctness-first engine split

Starting tree: `b63ab025d402e46c647a8a239bffe72a58f73a35` (`main`).
Authoritative upstream: `04be246001599f5995fa2f2d8c91a0f198d3f34c`.

This branch supersedes the performance-first instructions in the older `PLAN.md`
and `CHECKPOINT.md` for this experiment. It deliberately does not benchmark,
add fast paths, change algorithms, or tune compiler optimization. `main` is not
modified. Existing supporting-library/tool splits outside `src/quickjs` are not
redesigned in this pass.

## Source contract

- Every upstream `quickjs.c` function is retained once per original conditional
  definition. Function bodies are byte-for-byte upstream, not equivalent rewrites.
- Signatures are upstream, except `static` becomes hidden `QJS_INTERNAL` when
  needed across translation units. Cross-TU out-of-line definitions omit the
  original `inline`/`force_inline` attribute; ordinary TU-local implementations
  retain upstream attributes.
- There are no function definitions in `src/quickjs/internal-*.h`, no engine
  `qjs_*` identifiers, and no private inline twins or function-like performance
  remaps. The upstream public `quickjs.h`, including its original inline bodies,
  is preserved byte-for-byte.
- The full `JS_CallInternal()` interpreter body remains intact in one `.c` file.
- The temporary broad private interface in `internal-canonical.h` is intentional.
  It exposes canonical upstream functions/tables instead of inventing adapters.
  Necessary shared types are in private headers. Narrowing this interface is a
  separate later task, as is introducing measured header inlining.

`JS_GetOpaque2()` is the ordinary upstream implementation in `object.c`; there
is no `JS_GetOpaque2_inline` or custom opaque fast path. The same restoration
applies to allocator/atom/free helpers and GC helper twins.

## Additional ownership split

| File | Responsibility |
| --- | --- |
| `object.c` | Object/value lifetime, classes, GC, diagnostics and object APIs |
| `shape.c` | Shape lifetime, hashes, property metadata and shape lookup/update |
| `property.c` | Property/prototype algorithms, private fields, descriptors and copying |
| `number.c` | General conversion, numeric/operator semantics and equality |
| `bigint.c` | Limb arithmetic, BigInt lifetime, conversion and formatting |
| `function-vm.c` | Complete interpreter and VM-stack-specific helpers |
| `function.c` | Functions, closures/VarRefs, arguments, calls and async/generator execution |
| `iterator.c` | Iterator protocol, for-in enumeration setup and iterator lifetime |

The normal Makefile builds the new `.c` files independently. The migration
scripts are not part of compilation and do not implement an amalgamation.

## Verification and reproduction

`tools/check_split05.py` checks function/signature multiplicities, literal bodies,
all brace-initialized upstream data tables, public-header fidelity, the absence
of engine-private header bodies and engine `qjs_*`, and a source SHA-256 manifest.
The initial audit covers 1,563 definitions (1,557 unique function names) and 97
data initializers; conditional source definitions are included.

`SPLIT-05-VALIDATION.json` records the actual GitHub correctness comparison against
pinned `main`: clean GCC/Clang WERROR builds, built-in tests, dynamic exported-name
sets, and the complete accepted Test262 configuration. Test262 failure lists and
case counts must match the baseline exactly; known failures are not described as
passing tests. Detailed logs and source-audit records are workflow artifacts.
No benchmark or performance-regression test is run.

`git diff --check` can report upstream trailing whitespace restored with verbatim
bodies. This is intentionally not removed by rewriting those bodies. The
additional check with `core.whitespace=-blank-at-eol` must pass; all body bytes are
independently compared to upstream. The actual raw diff-check output is logged.

To repeat the source audit in a full clone:

```sh
python3 tools/check_split05.py --report split05-source-audit.json
```

The one-off `tools/split05.py` migration reads the two pinned Git objects and
**overwrites** engine source files, the public header, the Makefile, and its
inventory. Run it only in a disposable worktree, never over later development.
It is retained for provenance, not installed as a recurring auto-rewriter.
