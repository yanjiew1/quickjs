# QuickJS Modularization Task

## Agent continuity

This file is the authoritative task specification.

If conversation context is compacted, summarized, truncated, or you become uncertain about the requirements, re-read `task.md` before continuing.

## Workflow

Read this file completely before making structural changes.

Inspect the repository and design the implementation based on the actual source, dependency graph, ownership boundaries, and hot paths.

Produce a complete, self-contained implementation plan and write it to `PLAN.md`. The plan must be detailed enough that another capable coding agent could continue from it without relying on hidden context.

Do not wait for user approval after writing `PLAN.md`.

After the plan is written:

1. establish and record the pre-refactor baseline
2. create the initial `CHECKPOINT.md`
3. proceed directly with implementation according to `task.md` and `PLAN.md`
4. validate incrementally
5. keep `PLAN.md` updated as the implementation roadmap evolves
6. keep `CHECKPOINT.md` updated at meaningful milestones, and create Git commits at coherent structural milestone boundaries according to the Milestone Git Commits rules below

Once implementation begins, do not restart or redo planning unnecessarily.

Only stop before implementation if there is a genuine major blocker or architectural conflict that makes the task unsafe or materially ambiguous.

# Technical Requirements

## Goal

Refactor the monolithic QuickJS implementation into a maintainable modular architecture made of real, independently compilable C translation units while preserving QuickJS behavior, public API compatibility, portability, and interpreter performance.

The primary target is `quickjs.c`.

After the core split is structurally stable and correctness-validated, with any deferred performance issues recorded, also evaluate and modularize the secondary runtime/library targets:

- `quickjs-libc.c`
- `libregexp.c`
- `libunicode.c`

Also evaluate the secondary developer-tooling targets:

- `unicode_gen.c`
- `run-test262.c`

These secondary targets should be modularized only where the actual source shows clear ownership boundaries and the split improves maintainability without introducing artificial cross-translation-unit coupling.

This is a structural refactor, not an engine redesign.

## Repository Analysis and Module Boundaries

Inspect the actual repository and all files in scope before deciding the final module layout.

Treat `quickjs.c` as the primary architectural task. For `quickjs-libc.c`, `libregexp.c`, and any optional tooling split, independently inspect their real data ownership, call graph, static implementation boundaries, and coupling rather than mechanically copying the module structure chosen for `quickjs.c`.

Determine module boundaries from the real architecture, including:

- data ownership
- dependency direction and call relationships
- existing static/internal implementation boundaries
- circular dependency risk
- hot-path performance
- maintainability
- independent compilation
- future extensibility where it follows naturally from clean ownership, without speculative abstractions

Conceptual areas that may be useful include, but are not mandatory:

- runtime and context lifecycle
- allocator and garbage collection
- values and conversions
- strings, atoms, and ropes
- objects, properties, and shapes
- arrays and typed arrays
- functions and calls
- promises and jobs
- modules
- bytecode and interpreter
- lexer, parser, compiler, and scopes
- builtins

These are examples only. Merge tightly coupled areas, split large areas, or use different names when the actual dependency graph justifies it.

Plan the likely final architecture up front, but implement the decomposition incrementally. Prefer extracting coherent coarse-grained subsystems first, validating correctness and performing lightweight performance screening at meaningful boundaries, then further splitting large subsystems where the observed architecture justifies it.

Do not maximize the number of source files prematurely.

## Additional Modularization Targets

After the `quickjs.c` decomposition is structurally stable and correctness-validated, with any deferred performance issues recorded, evaluate the following targets. Treat runtime/library architecture and developer tooling as distinct concerns.

### Secondary runtime/library targets

#### `quickjs-libc.c`

This is a strong secondary modularization target because it contains several conceptually distinct host/runtime-support responsibilities.

Potential areas may include, but are not mandatory:

- `std` module implementation
- `os` module implementation
- event loop and polling
- signals and timers
- workers and inter-thread messaging
- module loading and file loading
- shared host/runtime support

Determine the actual boundaries from the source and dependency graph. Do not force each conceptual area into its own translation unit if the implementation is tightly coupled.

Preserve the existing `quickjs-libc.h` public interface unless a change is genuinely unavoidable.

#### `libregexp.c`

Treat the RegExp implementation as a cohesive library with potentially separable compiler and execution responsibilities.

Potential areas may include, but are not mandatory:

- RegExp parsing and compilation
- RegExp bytecode emission
- RegExp bytecode execution
- shared RegExp internals

Prefer a small number of strong modules over many thin files. In particular, do not separate parsing from compilation if doing so creates a large artificial internal API or excessive cross-TU coupling.

Preserve the existing `libregexp.h` public interface and RegExp bytecode/behavior unless a change is genuinely unavoidable.

#### `libunicode.c`

This is also a legitimate modularization target because it contains multiple natural responsibility areas rather than one single cohesive algorithm.

Potential areas may include, but are not mandatory:

- character-range operations
- Unicode case conversion and case folding
- RegExp-related Unicode canonicalization
- Unicode normalization and decomposition/composition
- Unicode scripts, categories, binary properties, identifier properties, and sequence/emoji properties

Prefer a small number of cohesive modules. Keep tightly coupled normalization helpers together, and do not split hot or closely related Unicode operations merely for file-count symmetry.

Preserve the existing `libunicode.h` public interface unless a change is genuinely unavoidable.

### Secondary developer-tooling targets

#### `unicode_gen.c`

This is a strong tooling modularization target because it combines multiple distinct responsibilities, including:

- parsing Unicode database input files
- building the in-memory Unicode database
- generating and compressing case-conversion tables
- generating normalization/decomposition/composition tables
- generating script/category/property tables
- generating emoji/sequence-property tables
- emitting generated Unicode tables
- optional self-tests and validation

Organize these responsibilities into a small number of cohesive tooling modules when the dependency structure supports it.

Do not treat generator modularization as runtime architecture work, and do not let it destabilize or delay the core engine/library refactor.

#### `run-test262.c`

This is a lower-priority tooling target, but it is worth modularizing when clear ownership boundaries exist.

Potential areas may include, but are not mandatory:

- configuration and command-line handling
- test discovery and name lists
- Test262 metadata parsing
- harness and agent support
- execution and worker/thread management
- expected-failure comparison
- reporting and statistics

Do not let test-runner cleanup delay or destabilize the engine modularization.

### Files that are not automatic split targets

Do not split files merely because they are non-trivial in size.

In particular:

- `dtoa.c` should remain a cohesive numeric-conversion implementation unless repository evidence shows a strong architectural reason to split it.
- `cutils.c` should remain cohesive unless a clearly independent subsystem emerges.
- `qjs.c` and `qjsc.c` should normally remain single-file standalone tools; reorganize their location if useful, but do not decompose them into many small implementation files without a concrete need.
- generated or declarative headers such as `libunicode-table.h` and opcode-definition headers should not be split merely because they are large.
- public umbrella headers such as `quickjs.h` should remain convenient for embedders even if internal implementation headers become more modular.

## Directory Structure

Keep engine and library implementation source files under `src/` so the repository root remains clean and does not accumulate a large number of implementation files.

The repository root should primarily contain project-level files such as public headers when appropriate, build-system entry points, documentation, licenses, tests/tools directories, and other top-level project metadata.

Within `src/`, prefer an architecture-oriented hierarchy that remains easy to navigate. A flat layout is acceptable for small cohesive areas, but larger subsystems may and should use additional subdirectories when a single directory would become crowded or obscure ownership boundaries.

For example, a reasonable shape might be:

```text
src/
  quickjs/
    runtime/
    value/
    string/
    object/
    function/
    vm/
    compiler/
    builtin/
    module.c
    promise.c

  regexp/
    ...

  unicode/
    ...
```

This is an example, not a required layout. Choose subdirectories from the actual ownership, dependency structure, and source organization. Related implementation files and their narrowly scoped private internal headers should normally be colocated with the subsystem that owns them.

Large implementation areas such as `src/quickjs/` may use multiple levels of subsystem organization when that improves clarity. Smaller cohesive areas such as `src/regexp/` or `src/unicode/` may remain flat when another hierarchy level would not improve organization.

Do not create directories merely for naming symmetry, one directory per file, or tiny groups that are clearer kept together. Avoid unnecessary nesting and vague dumping grounds such as `misc`, `common`, or `core`.

Standalone executables and project utilities that are built on top of the engine, such as `qjs` and `qjsc`, may be moved to an appropriate top-level directory such as `tools/` when doing so clearly improves organization, but relocation is not required for this refactor.

## Independent Compilation

Every resulting `.c` file must be independently compilable.

The normal modular build must compile multiple translation units and link them normally.

Do not use a central source file whose primary architecture is:

```c
#include "runtime.c"
#include "object.c"
#include "compiler.c"
```

Correctness and maintainability must not depend on all modules being part of one translation unit.

A generated amalgamated distribution may be considered later, but it is not part of this refactor and must not dictate the architecture.

## Public and Internal APIs

Preserve the existing public `quickjs.h`, `quickjs-libc.h`, `libregexp.h`, and `libunicode.h` APIs unless a change is genuinely unavoidable.

Do not expose engine or library internals through public APIs merely to make the split compile.

Use private/internal headers with narrow responsibilities. Avoid replacing the monolithic `quickjs.c` with a monolithic `quickjs-internal.h`.

Preserve `static` linkage whenever practical.

When a currently static symbol must cross a translation-unit boundary, consider whether the module boundary should be adjusted, whether the code should remain colocated, whether a small shared internal abstraction is appropriate, or whether a narrow internal API is justified.

Do not indiscriminately make static functions global.

## Hot Paths and Inlining

Analyze optimization opportunities that may be lost when splitting the original translation unit.

Pay special attention to interpreter dispatch, `JSValue` manipulation, property get/set, shapes and property lookup, atoms, strings, numeric conversion, calls, and arrays.

Hot-path helpers may be moved to appropriately scoped private internal headers as `static inline` when inlining is justified by performance evidence or strong architectural evidence. Do not move large functions into headers simply to force inlining.

Preserve existing upstream `always_inline` usage unless there is a clear reason to change it.

New `always_inline` annotations should be considered for frequently executed helpers on clearly established performance-critical hot paths when inlining is important to the intended fast path. For such hot paths, reasonable code-size growth is acceptable when the non-LTO performance benefit justifies it; do not reject forced inlining solely because it increases code size. Prefer the project's existing `force_inline` or `js_force_inline` macros where applicable.

Do not force-inline functions when their hot-path importance or expected inlining benefit is unclear. Use compiler optimization reports, generated code, profiling, benchmarks, or strong architectural evidence from a clearly established critical hot path to justify forced inlining.

When evidence shows that failure to inline a hot function causes a meaningful performance regression, actively try to restore the inline. This may be done by adjusting the module boundary, moving the helper into an appropriately scoped private internal header, or using `always_inline` / the project's `force_inline` or `js_force_inline` macro. If forced inlining materially recovers the regression, retain it when the performance benefit justifies the resulting code-size cost and no other meaningful regression is introduced. Avoid only excessive code duplication or poorly justified code-size growth with small or unproven performance benefit.

Possible cross-TU losses include inlining, constant propagation, dead-code elimination, interprocedural analysis, and specialization.

If a clean-looking module boundary materially harms a critical interpreter path, prefer a better boundary over maximal modularity.

## LTO

Treat the normal non-LTO build as the performance target for this refactor.

Preserve supported LTO build and correctness behavior, but LTO performance is not a completion target. LTO benchmarks may be recorded when useful, and LTO-only performance regressions may be deferred without blocking milestones or final completion. Do not spend refactor time fixing LTO-only performance unless it helps diagnose or recover a non-LTO regression.


## No-Regression Policy

This is a structural refactor with a no-regression policy. Apply the policy across all relevant dimensions, including but not limited to functional behavior and correctness, performance, resource usage, binary size, portability, build/test behavior, tooling behavior, and maintainability. Do not intentionally redesign JavaScript semantics, parser/compiler behavior, QuickJS bytecode format or opcodes, `JSValue` representation, object representation, property/shape architecture, atom/string semantics, GC/reference-count/cycle-removal semantics, module semantics, RegExp semantics or RegExp bytecode behavior, Unicode behavior or generated Unicode table semantics, host-library behavior, Test262 runner behavior, public APIs, or optimization strategy.

If the refactor exposes a pre-existing issue in any relevant dimension, record it for later work rather than deliberately fixing or improving it during this task. The dimensions named above are examples, not an exhaustive list; apply the same rule to other relevant concerns discovered during the refactor. Avoid unrelated cleanup, redesign, optimization, broad renaming, and formatting churn.

Regressions introduced by the refactor in any relevant dimension must be investigated and fixed before the affected milestone is considered complete, except that the performance completion gate applies to the non-LTO build. A performance regression observed only under LTO may be recorded and deferred without blocking the milestone or final completion. This exception applies only to LTO performance; LTO build failures, correctness regressions, or other behavioral regressions introduced by the refactor remain subject to the normal no-regression policy.

If the refactor incidentally improves any relevant dimension, the improvement may be kept when it is understood, validated, and does not introduce an offsetting regression; do not undo a genuine improvement merely to preserve the old baseline. Record every such incidental improvement, including what improved, the relevant dimension, the likely cause when known, how it was validated, and the relevant milestone or commit. A pre-existing issue may otherwise be changed during this task only when it directly blocks the refactor or makes reliable validation impossible; document the exception and keep that change in a separate commit.

## Baseline and Validation

Before structural changes, establish a reproducible baseline when one has not already been established.

Record enough provenance to reproduce the baseline, including where available: baseline commit/working-tree state, compiler/version, relevant build flags/options, platform/CPU, build/test status, Test262 failure set, representative microbenchmarks, non-LTO performance, optional informational LTO measurements, and binary size.

For Test262, compare the actual failure set, not merely a total count. New failures are regressions and must be investigated. Existing failures may disappear incidentally; when that happens, verify that the change is a genuine improvement rather than a shifted or masked regression, and record the result. Do not intentionally chase existing failures during the structural refactor.

Treat unexplained negative differences from the recorded baseline in any relevant dimension as potential regressions. For performance and other measurement-sensitive metrics, distinguish an observation from an established regression: judge by reproducibility, normal variance, and practical significance rather than exact numerical parity. Small or noisy differences that have not been established as meaningful regressions may be recorded and deferred to final stabilization rather than investigated exhaustively during the current migration milestone. Incidental improvements in any relevant dimension are acceptable when validated and must not be reverted solely for the sake of exact parity with the old baseline.

When performing detailed performance validation or investigating a suspicious performance result:

- use the same configuration before and after
- run multiple measurements where practical
- prefer median or otherwise stable results over a single run
- repeat suspicious near-threshold regressions before changing architecture
- run performance measurements in isolation from other CPU-intensive background work that could distort the comparison

### Long-Running Commands

Do not run hang-prone tests, benchmarks, fuzzers, or expensive builds as unbounded blocking commands when a safer harness mechanism is available. Prefer background/yield/poll support, otherwise use a reasonable timeout or background process with incremental log inspection. If a command stops making meaningful progress, recover control and investigate rather than waiting indefinitely or blindly rerunning it.


### Performance Validation

Performance work in this task is non-LTO-first. LTO-only performance regressions are informational and do not block milestone or final completion; LTO build or correctness regressions still do.

At each performance-sensitive milestone:

1. run a lightweight non-LTO regression screen after correctness validation
2. if a meaningful, reproducible regression is found, investigate likely structural causes
3. make a reasonable attempt to fix or mitigate it. When a regression appears to result from a translation-unit boundary, actively consider adjusting the module boundary or moving hot helper functions into a narrowly scoped private internal header shared only by the modules that actually need it when header inlining is justified by the hot path and code-size cost. Prefer restoring a critical hot path over preserving an artificial module boundary; do not move large functions into headers or create broad internal headers merely to chase small or noisy differences. Also remove accidental extra work or correct clearly worse generated code when justified. When code layout is a plausible cause of a non-LTO regression, object-file or linker input ordering may also be adjusted and benchmarked. Retain such ordering changes only when the improvement is reproducible and the resulting build remains portable and maintainable
4. rebuild, rerun the relevant correctness validation, and remeasure after the attempted fix
5. if the regression remains difficult to resolve after reasonable effort, record it in `CHECKPOINT.md` and continue with later structural work rather than getting stuck

Do not defer a confirmed regression after investigation alone; at least one reasonable corrective attempt is required unless the investigation establishes that the measurement was not a real regression or that the cause is outside the refactor.

For deferred regressions, record enough information to resume later: affected workload, baseline/current measurements, reproducibility, instruction/cycle counts when available, suspected cause, diagnostics performed, fixes attempted and their results, and the relevant repository state or commit.

Near-identical instruction counts do not prove equal performance. If no clear extra work, missed inline, or structural defect remains after reasonable fix attempts, defer deeper investigation of code layout, branch prediction, cache effects, scheduling, register allocation, or compiler heuristics to Final Performance Stabilization.

A deferred meaningful non-LTO regression keeps the affected `PLAN.md` milestone `[~]`, but does not block later implementation. An LTO-only performance regression does not keep the milestone `[~]`.

#### Final Performance Stabilization

After the planned structural migrations are complete, perform comprehensive non-LTO validation against the recorded baseline. Revisit every deferred non-LTO regression and resolve remaining confirmed meaningful refactor-induced regressions before final completion.

Final stabilization may still change module boundaries, internal private headers, or hot-helper placement when those changes are justified by performance evidence. Do not treat the structural layout reached at the end of the migration phase as immutable. If a deferred regression is best fixed by revising a boundary or moving an appropriate hot helper into or out of a narrowly scoped private internal header shared only by the modules that actually need it, make that change and then rerun the affected correctness and performance validation.

Understanding or classifying a remaining non-LTO regression is not sufficient by itself. If it cannot be resolved, record the state clearly and do not claim the overall task is complete.

LTO performance remains deferred from this completion gate. LTO results may still be used as diagnostic evidence when useful.


After every meaningful migration stage, rebuild and run appropriate validation. Keep the repository buildable after meaningful stages whenever practical.

During final validation, exercise a representative set of supported build configurations discovered from the repository and CI, where available. Include relevant compiler variants, diagnostic or debug configurations, sanitizer builds, architecture-sensitive configurations, and clean or parallel builds when they are supported by the project. Do not invent unsupported configurations solely for this task.

When splitting `quickjs-libc.c`, run the relevant `std`, `os`, module-loader, event-loop, worker, and host-integration tests or examples that are available in the repository.

When splitting `libregexp.c`, include relevant RegExp correctness tests and benchmark compile/execute hot paths when the boundary could affect performance.

When splitting `libunicode.c`, validate case conversion/folding, identifier classification, normalization, Unicode property lookup, RegExp canonicalization, and sequence-property behavior as applicable.

If `unicode_gen.c` is modularized, verify that generated Unicode tables are semantically equivalent. Prefer byte-for-byte output comparison when deterministic generation makes that practical; otherwise compare the relevant generated data and runtime/self-test behavior.

If `run-test262.c` is modularized, verify that test discovery, metadata handling, harness behavior, execution, filtering/exclusions, expected-failure comparison, failure reporting, and statistics remain behaviorally equivalent.

## Migration Strategy

Perform the extraction incrementally based on the actual dependency graph.

For large mechanical code moves or extractions, prefer lossless file, shell, or script-based transformations where practical rather than regenerating large unchanged code blocks through model-written patches. Keep semantic edits around the move explicit and reviewable, and verify that mechanically moved code was not accidentally altered.

Use this overall priority unless repository evidence strongly justifies a different order:

1. modularize and stabilize `quickjs.c`
2. modularize the secondary runtime/library targets: `quickjs-libc.c`, `libregexp.c`, and `libunicode.c`
3. modularize the secondary developer-tooling targets: `unicode_gen.c` and `run-test262.c`

Within each group, choose the exact order from dependency structure, risk, validation cost, and architectural value rather than from file size alone.

Do not begin a secondary target merely to increase parallelism while the core `quickjs.c` architecture is still unstable.

Within each target, a good implementation sequence is generally:

1. inspect and understand the relevant region and dependencies
2. choose or confirm the target module boundary
3. extract a coherent subsystem or portion
4. update internal headers and build files
5. rebuild
6. run focused tests
7. run broader correctness validation when appropriate
8. run a lightweight performance regression screen if the boundary may affect performance
9. continue to finer decomposition after the boundary is structurally and functionally stable, subject to the performance-screening rules above

Minor implementation adjustments to the plan are acceptable when supported by repository evidence.

If a major architectural assumption proves wrong, update the plan and checkpoint clearly before proceeding with a substantially different architecture.

## Parallel Work and Subagents

Independent exploration, read-only analysis, and non-conflicting validation may be parallelized or delegated when supported by the harness. Do not parallelize overlapping source edits, dependent Git operations, commands mutating the same build tree, or performance measurements competing for shared CPU/cache resources. Keep conflicting code ownership and architectural decisions coordinated by the primary agent.


## Milestone Git Commits

Treat each meaningful planned implementation milestone as a Git commit boundary. The Git commit boundary and the `PLAN.md` completion marker are related but not identical: a structurally complete, buildable, correctness-validated state may be committed normally even when a deferred meaningful non-LTO performance regression keeps that milestone marked `[~]`. An LTO-only performance regression does not affect the milestone completion marker.

When the structural implementation for a milestone reaches a coherent commit point:

1. finish the structural implementation work belonging to that milestone
2. rebuild and run correctness validation appropriate for that milestone
3. for performance-sensitive milestones, run the lightweight performance regression screening defined above
4. resolve correctness failures; for a confirmed non-LTO performance regression, make a reasonable corrective attempt and remeasure before deferring it under the Performance Validation rules
5. update the corresponding `PLAN.md` progress marker when `PLAN.md` is in use: use `[x]` only when no established unresolved blocking regression prevents completion; an LTO-only performance regression is non-blocking
6. update `CHECKPOINT.md` whenever there is a deferred performance regression or the milestone materially changes the handoff or architectural state
7. review the diff for accidental or unrelated changes
8. create one normal, descriptive Git commit for the coherent structural state

The commit should include the implementation changes together with directly related build-system, test, `PLAN.md`, and `CHECKPOINT.md` updates for that milestone state.

Do not label a commit `WIP`, `checkpoint`, or similar solely because a performance regression has been deferred. If the structural work is coherent, buildable, and correctness-validated, use the normal descriptive commit message for the structural change. Record unresolved non-LTO performance status in `CHECKPOINT.md` and keep the affected `PLAN.md` milestone `[~]` until Final Performance Stabilization resolves it. Record LTO-only performance regressions as deferred informational issues without blocking `[x]` completion.

Do not deliberately mix fixes or improvements for deferred pre-existing issues into structural refactoring milestone commits. If an incidental improvement results naturally from the structural change, record it and validate that it introduces no regression. If an exceptional pre-existing issue must be changed because it blocks the refactor or reliable validation, keep that deliberate change in its own dedicated commit.

Use concise, descriptive commit messages, for example:

```text
refactor: extract atom and string subsystem
refactor: split parser and compiler modules
refactor: move qjs and qjsc into tools
build: add multi-translation-unit build support
```

Do not combine multiple structural milestones into one large commit merely for convenience.

Do not create a commit for every tiny edit or intermediate broken state. A milestone commit should represent a coherent, buildable, correctness-validated step with its validation state accurately recorded.

If work on a milestone stops in an actually broken or structurally incomplete state, record the partial state in `CHECKPOINT.md` and avoid presenting that state as a completed structural milestone.

## PLAN.md Progress Tracking

Treat `PLAN.md` as a living implementation roadmap, not an immutable document or merely a static planning artifact.

During implementation, update the plan when repository evidence, newly discovered dependencies, validation results, or implementation experience justify changing the order, decomposition, milestones, or implementation approach. Routine plan adjustments do not require additional user approval. Preserve completed work, in-progress work, and still-valid architectural decisions rather than rewriting the plan from scratch.

For significant architectural changes, record the rationale in `PLAN.md` and update `CHECKPOINT.md` with the resulting decision and implementation state. Do not stop solely to request approval for an implementation-plan adjustment unless it would change the authoritative task itself.

`task.md` remains the authoritative specification. Do not modify `PLAN.md` in a way that weakens, removes, or silently changes the task requirements, scope, no-regression policy, validation policy, or completion criteria. If a proposed change would alter the task rather than only the implementation path, do not make that change without explicit user direction.

Use explicit progress markers for implementation items, for example:

- `[ ]` not started
- `[~]` in progress
- `[x]` completed

Update these markers at meaningful implementation milestones so the current progress is visible directly in `PLAN.md`.

When an item is marked completed, it should correspond to work that is actually present in the repository and has received the appropriate validation for that stage.

`CHECKPOINT.md` remains the detailed handoff/status record; `PLAN.md` should provide the concise current roadmap and progress overview.

## Checkpoint / Handoff

Maintain `CHECKPOINT.md` during implementation.

Create or update it at meaningful milestones, especially after a meaningful subsystem extraction, major build-system changes, resolving an important dependency or architecture issue, important validation stages, before a large or risky migration stage, whenever implementation materially diverges from `PLAN.md`, and before ending a session with substantial work remaining.

Do not update it after every trivial edit.

`CHECKPOINT.md` should contain enough information for another agent to continue safely, including:

- current overall status
- completed work
- current in-progress stage
- files/modules added, moved, or significantly changed
- important implementation decisions
- deviations from `PLAN.md` and why
- build status
- tests run and results
- Test262 status
- benchmark status
- known failures, regressions, blockers, or suspicious behavior
- pre-existing issues discovered and deferred for later work, plus incidental improvements observed in any relevant dimension during the refactor, with enough detail to identify what improved, why when known, how it was validated, and where the change occurred
- exact next recommended steps
- temporary or partial states the next agent must understand
- useful commands/configurations for reproducing validation

When resuming interrupted work, read `task.md`, `PLAN.md`, and `CHECKPOINT.md` before making changes.

Before final completion, update `CHECKPOINT.md` with the final validation state and clearly indicate that no implementation work remains.


## Final Report

At completion, summarize:

1. final module/subsystem structure for `quickjs.c`
2. whether and how each secondary target (`quickjs-libc.c`, `libregexp.c`, `libunicode.c`, `unicode_gen.c`, and `run-test262.c`) was modularized, including the rationale for any target intentionally left cohesive
3. final directory tree
4. responsibility of each subsystem/module
5. important dependency directions
6. internal headers introduced
7. formerly-static symbols that became internal APIs
8. hot helpers kept or moved to `static inline`, including any new justified `always_inline` usage
9. build-system changes
10. LTO build/correctness support and any observed LTO performance differences deferred for future work
11. final supported-build/configuration validation performed
12. test and Test262 results
13. RegExp, Unicode, generator, and host-library validation results
14. non-LTO benchmark comparison and disposition of every deferred non-LTO performance issue, plus any recorded LTO-only performance regressions explicitly deferred for future work
15. files intentionally left cohesive and the rationale
16. remaining coupling, risks, or future cleanup opportunities
17. pre-existing issues discovered during the refactor and deferred for later work, incidental improvements retained in any relevant dimension and their validation/relevant commits, and any exceptional blocking issue that had to be changed

