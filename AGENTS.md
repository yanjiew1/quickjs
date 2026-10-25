<!--
Copyright (c) 2017-2021 Fabrice Bellard
Copyright (c) 2017-2021 Charlie Gordon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

# Development guide for agents

This is a modular C QuickJS fork. It builds the `qjs` interpreter, `qjsc`
compiler, `libquickjs.a` and development/test tools.

Current user instructions take precedence over this guide. Read any nested
`AGENTS.md` applicable to the files being changed for more specific guidance.

## Establish the working context

- Inspect the current branch, working tree and relevant history before editing.
  Preserve existing work and confirm which branch the task concerns; experiments
  on another branch are not automatically part of the current implementation.
- Read the affected source, callers, tests and build configuration on the current
  branch. `doc/quickjs.texi` is the embedding manual; verify its claims against
  the implementation. The optional [Code Guide](https://github.com/yanjiew1/quickjs/wiki/Code-Guide)
  describes a source snapshot and may be outdated; use it for navigation, not
  as development policy or evidence of current behavior.
- Treat local `PLAN.md`, `CHECKPOINT.md` and audit files as working notes, not
  permanent policy or proof that their recorded work is present on this branch.

## Build and test commands

Use Meson 0.58 or newer, Ninja, Python 3 and a C toolchain. Read
`BUILDING.md`, `meson_options.txt` and the matching CI job for dependencies and
compiler options. Select the compiler at setup with `CC` or a Meson native/cross
file. Keep each compiler/configuration in a separate build directory; the source
and build directories may be on different paths or filesystems.

Default local validation for implementation changes is a combined ASan+UBSan
build and repository suite, using a fresh build directory:

```sh
meson setup build-sanitize -Dwerror=true -Db_sanitize=address,undefined
meson compile -C build-sanitize -j4
meson test -C build-sanitize --print-errorlogs --num-processes 4
```

Generated files, objects and runtime fixtures belong in the build directory.
For focused checks, use `meson test -C build-sanitize language` or the matching
native test name. Test executables are under the build directory's `tests/`.
For documentation-only tasks, check Markdown, references and the file-change scope.

## Architecture and ownership

| Location | Owner |
| --- | --- |
| `include/` | Public engine and host APIs; root header aliases are build conveniences |
| `src/quickjs/` | Runtime, allocation, GC, values, objects, functions, modules and VM |
| `src/quickjs/builtins/` | JavaScript built-ins and their owned state |
| `src/quickjs/compiler/` | Lexing, parsing, binding resolution, bytecode generation and analysis |
| `src/quickjs/serialization/` | Binary object/bytecode reading and writing |
| `src/quickjs-libc/` | Host services: std, OS, module loading, events and workers |
| Other `src/` components | C utilities, numeric conversion, Unicode, RegExp and native waits |
| `tools/`, `tests/`, `fuzz/` | Applications, validation and development tooling |

- Group code by semantic responsibility, state ownership and lifetime. Review
  functions, file-scope variables and tables together. File size, broad category
  names and a common registration table alone do not justify a boundary.
- Share genuinely common declarations in the owning private header; keep local
  types, macros and helpers in their `.c` file. Keep engine-independent support
  libraries independent of JS objects, and host services behind the public API.
- Compile engine implementation files as real translation units. Preserve source-path
  identity in object names so duplicate basenames need no artificial renaming.
  Keep generated tables with one owner; splitting Unicode must not duplicate
  large table definitions across translation units.
- Only `include/` is the public header surface. Preserve API/ABI compatibility
  when extending it. Private interfaces may evolve; `JS_`/`js_` spelling alone
  does not establish whether a function is public. Keep shared-library symbols
  hidden unless declared on that public surface. Mark public callable
  declarations with `JS_EXTERN` and check the exact export list.

## C conventions and correctness

- Keep complete MIT notices in project-owned source, scripts and build files;
  preserve the applicable licenses and attribution of imported code and data.
- Follow surrounding C style, align continued parameters with the first parameter,
  and preserve upstream comments beside the behavior they explain. Keep source
  moves mechanical and independently reviewable from behavioral changes.
- Keep TU-local functions `static`, including inline/force-inline helpers. Put
  private inline bodies in headers only for actual cross-TU consumers. Check
  macros, conditional builds and header callers before changing linkage.
- Prefer portable C99 inline semantics over GNU extern-inline conventions.
  For public API inline optimization, follow the allocator's private
  `name_inline` implementation and scoped macro aliases while retaining the
  callable `(name)` symbol and function-address behavior. Callbacks normally
  need one out-of-line definition.
- Trace ownership on success, exceptions and allocation failure: values, atoms,
  buffers, varrefs, contexts and cached callbacks. New reference-bearing state
  needs appropriate GC marking and teardown, including suspended frames.
- Preserve coercion order, reentrancy, realm identity and abrupt completion.
  Compiler/VM changes must account for eval/with, closures, scope cleanup,
  strict state and yield/await. Coordinate opcode definitions, stack analysis,
  diagnostics, serialization, endian handling and format version when affected.
- Check slow/error paths before replacing `JSContext *` with `JSRuntime *`;
  allocation failure or realm-dependent helpers can still need the context.
- Implement standard features natively in C, using the latest ECMA-262/ECMA-402
  living drafts. Check QuickJS-NG for reusable work before starting; review any
  upstream/NG port against the specification and this fork's ownership contracts.
  Credit directly reused code and preserve its copyright/license attribution.

## Validation

- Add focused repository regression tests for correctness fixes and features.
  Prefer JavaScript when it can reproduce the issue, including `std.createRealm()`
  and `std.gc()` where useful. Use C for embedding contracts, allocation failure
  and inaccessible internal state. Group tests with their subsystem and keep
  runtime bounded; Test262 coverage does not replace quick repository tests.
- Native allocation failure tests must account for allocator pools so the
  intended failure occurs.
- Run full Test262 and its sanitizer/platform variants in GitHub Actions. Use
  `python3 tools/build/test262.py bootstrap` for the repository pin and patches; it resets an existing
  Test262 checkout. Target failing cases during diagnosis and use available runner
  threads. Reuse passing exact-revision CI evidence instead of repeating it locally.
- Inspect `.github/workflows/ci.yml`, `ci-sanitizers.yml` and `ci-qemu.yml`
  in that directory for current coverage.
  Track exact SHAs and attempts; queued or running jobs have not passed. Inspect
  failures in tests and fixtures as well as production code.
- Investigate Test262 failures before changing tests: trace the latest applicable
  ECMA algorithms, relevant CLDR/Unicode rules, source-data selection and the
  implementation. If other engines pass, inspect why; distinguish required
  behavior from compatibility policy and optional support. Attempt relevant
  optional behavior rather than immediately treating its coverage as invalid.
  Modify a test only after establishing that it contradicts the specification
  or is outdated, and record the exact specification revision, reason and
  supporting evidence. A differing output alone is insufficient evidence.
- Remove skips made obsolete by fixes/features. Keep intentional long-running
  exclusions explicit. Do not hide correctness failures or performance costs by
  adding skips or updating expected-error baselines; check suspect tests against
  the standard and report genuine discrepancies.

## Performance work

- Compare a change with its parent or agreed baseline. Follow
  `tools/benchmarks/README.md`: identical workloads, fresh JS processes, alternating
  paired runs, individual scores and aggregates, and recorded compiler/hardware
  configuration. Use unsanitized GCC and Clang non-LTO builds for throughput.
- Measure likely performance changes directly; follow the benchmark tooling's
  current cadence for routine broader checks. Include repository microbenchmarks
  as well as V8 v7; Zoo excludes zlib. Add workloads when new behavior lacks coverage.
- Disable C assertions in benchmark builds for both compared revisions
  (`-Db_ndebug=true` with Meson, or `-DNDEBUG`). Keep assertions enabled for
  correctness validation; record the setting with benchmark results.
- Prefer measured inline/no-inline/force-inline and fast/slow-path adjustments
  before rewriting algorithm bodies. Record body changes and their rationale.
  Investigate regressions using workload behavior, assembly and code layout;
  shared-runner noise and one sample cannot establish a regression. Keep trials
  bounded and record rejected approaches rather than repeating them indefinitely.

## History, collaboration and handoff

- Follow the current task's permission to commit or publish. When requested,
  make small coherent, buildable commits. Use meaningful
  prefixes such as `fix:`, `feat:`, `refactor:` or `perf:` when appropriate.
  Separate independently meaningful correctness, performance and build changes.
- Commit messages explain the original problem, final change and regression
  coverage. Include actual corresponding Test262 paths when available. For
  specification-related changes, record specification commit hashes, source/review
  dates and stable algorithm anchors; prefer ECMA-262 multipage links. For
  unmerged Stage 4 work, use the Stage 4 PR specification as primary, with proposal
  provenance. GitHub PR links use `https://redirect.github.com/` to avoid
  cross-reference notifications.
- Keep execution logs and discarded designs in working records, not commit
  messages. During requested history cleanup, fold superseded/partial fixes into
  their owning changes when clearer, while retaining useful independent steps.
- Before rewriting published history, cancel unfinished CI for superseded commits
  and affected descendants. Push a backup branch under `history/` before force
  pushing the rewritten branch. Target `yanjiew1/quickjs` explicitly in GitHub
  tooling; repository defaults may point at Bellard's upstream.
- Use subagents for independent read-only review unless the user explicitly
  delegates implementation. Keep one writer/integrator and use default agent
  settings. If a message reaches an already completed agent, use a minimal
  follow-up when needed to consume pending messages and let it finish cleanly.
- For long-running tasks, resolve recoverable problems autonomously and keep
  working. Stop only when the goal and all tasks are complete, at the user's
  direction, or when a genuine blocker prevents further progress without
  external input. Continue independent authorized work when a change is blocked,
  and defer its dependents too. Keep long tool calls, commands, and agent work
  observable and interruptible, using timeouts or watchdog checks so a stall
  cannot prevent inspection or recovery. Long execution alone is not a reason
  to stop useful work. Use completion notifications when available; otherwise,
  use bounded sleeps between checks instead of frequent polling.
- Maintain concise handoff notes for long tasks, including exact revisions,
  evidence, unresolved issues and next steps. Keep these notes out of
  commits. Choose a temporary location with sufficient free space for large build,
  test and benchmark artifacts; follow any workspace-specific storage instructions.
- Use English for development notes and progress, and Traditional Chinese for
  final responses unless requested otherwise. Report material findings clearly;
  follow the user's requested pause and reporting cadence.
