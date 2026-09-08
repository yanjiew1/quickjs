Refactor this QuickJS fork so that the monolithic `quickjs.c` can eventually be split into multiple logical, independently compilable C translation units.

For now, perform the PLANNING PHASE ONLY.

Do not modify source files, build files, generated files, or repository configuration yet, except for creating or updating `PLAN.md`.

The goal of this phase is to design a maintainable modular architecture while preserving QuickJS behavior, portability, public API, and interpreter performance.

## Primary goal

The eventual refactoring should:

* split `quickjs.c` into logical modules/subsystems
* make each resulting `.c` file independently compilable
* preserve existing JavaScript behavior and public APIs
* preserve performance as closely as practical
* keep internal interfaces narrow
* make future interpreter/runtime optimization work easier

This is intended to be a structural refactoring, not an engine redesign.

## Analyze the current implementation first

Inspect the repository and `quickjs.c` in detail.

Identify:

* major logical regions
* shared data structures
* ownership relationships
* call dependencies
* static functions and variables
* hot execution paths
* compiler/interpreter boundaries
* parser/compiler dependencies
* runtime/object/string/atom dependencies
* GC/reference-counting dependencies
* build-system assumptions caused by `quickjs.c` being a single translation unit
* places where current performance may rely on same-translation-unit visibility or compiler inlining

Do not assume that conceptual areas listed below should necessarily become separate modules.

## Module decomposition

Propose logical module/subsystem boundaries based on the actual code.

Possible conceptual areas include:

* runtime / context
* allocator / GC
* values / conversions
* strings / ropes
* atoms
* objects
* properties
* shapes
* arrays / typed arrays
* functions / calls
* promises / jobs
* modules
* bytecode
* interpreter
* lexer / parser
* compiler / scopes
* builtins

This list is illustrative only.

Do not mechanically create one module for every item.

You may:

* merge tightly coupled areas
* split large areas into multiple modules
* choose different module names
* keep functionality together when splitting it would create excessive internal APIs
* use a flatter or more hierarchical structure when justified

Prefer boundaries based on:

1. data ownership
2. dependency direction
3. ability to preserve local `static` implementation details
4. minimizing circular dependencies
5. keeping hot paths simple
6. maintainability
7. independent compilation
8. future extensibility

For each major proposed module, explain why that boundary is preferable.

## Directory structure

Propose an appropriate directory layout based on the chosen module architecture.

The directory structure must follow the module boundaries, not determine them.

Prefer a shallow subsystem-oriented hierarchy.

For example:

```text
src/
    runtime/
    string/
    object/
    vm/
    compiler/
    builtins/
```

may be reasonable, but this is only an example.

Use subdirectories when they materially improve ownership and navigation.

Avoid:

* unnecessary nesting
* one-directory-per-file organization
* deeply nested trees
* vague dumping grounds such as `misc`, `common`, or `core` unless clearly justified
* directory boundaries that create excessive cross-module dependencies

In general, prefer no deeper than:

```text
src/<subsystem>/<file>
```

unless deeper nesting is clearly useful.

Provide a proposed final source tree.

## Independent compilation

The eventual implementation must make every resulting `.c` file independently compilable.

The normal build must compile separate translation units and link them together.

Do NOT plan a fake split where a central source file simply does:

```c
#include "runtime.c"
#include "object.c"
#include "interpreter.c"
```

Identify any current implementation patterns that depend on single-translation-unit visibility and explain how they should be handled.

Correctness and maintainability must not depend on combining all modules into one translation unit.

## Public and internal APIs

Preserve the existing public API in `quickjs.h`.

Do not expose implementation details publicly merely to make modularization easier.

Plan private/internal headers where necessary.

Avoid replacing one monolithic `quickjs.c` with one monolithic `quickjs-internal.h`.

Prefer subsystem-specific internal headers when appropriate.

For each proposed internal header, describe:

* what types it owns
* what functions it exposes
* which modules may include it
* what should remain private to the `.c` file

Identify important currently-`static` symbols that would need cross-translation-unit visibility.

For each such symbol, consider whether:

* it should become an internal API
* the module boundary should be adjusted instead
* a small shared abstraction should be extracted
* the functionality should remain colocated

Preserve `static` visibility whenever practical.

## Inline functions and hot helpers

Because splitting translation units may prevent normal compiler inlining across module boundaries, analyze performance-sensitive helper functions carefully.

Identify tiny helpers that are:

* currently `static`
* heavily used by interpreter/runtime hot paths
* likely to benefit significantly from inlining
* reasonable to place in an internal header as `static inline`

Examples may include helpers used by:

* JSValue tag/type operations
* fast conversions
* property lookup
* shape lookup
* atom lookup
* array access
* bytecode decoding
* interpreter dispatch support
* call setup

Do NOT simply move large functions into headers.

For each important proposed inline helper, explain why header inlining is justified.

Also identify functions that should remain normal out-of-line functions even if cross-module calls are introduced.

Avoid overusing `static inline`; use it primarily where there is a credible hot-path or code-generation reason.

## Performance-sensitive paths

Explicitly analyze the effect of modularization on:

* bytecode dispatch
* interpreter loop
* JSValue operations
* property get/set
* shape lookup
* atom lookup
* string operations
* numeric conversions
* function calls
* array access
* exception paths where relevant

Highlight any module boundary that could introduce meaningful hot-path call overhead.

If a theoretically clean module split would likely hurt a critical hot path, discuss whether keeping those parts together would be better.

Performance-sensitive design takes priority over artificially maximizing modular separation.

## Cross-translation-unit optimization and LTO

Splitting `quickjs.c` changes the compiler optimization model.

Analyze where the current monolithic translation unit may allow:

* inlining
* constant propagation
* dead-code elimination
* interprocedural analysis
* specialization based on locally visible implementation details

and where those optimizations could be weakened after the split.

Evaluate whether the currently supported build systems and toolchains can reasonably support Link Time Optimization for release/performance builds.

Plan how LTO could be used to recover useful cross-translation-unit optimization where appropriate.

However:

* correctness must not depend on LTO
* normal non-LTO builds must work correctly
* module boundaries must remain sensible without LTO
* LTO should not be used to justify poor or excessively chatty module interfaces

Describe:

* which existing build systems could expose LTO support
* whether LTO should be optional or recommended for performance-oriented release builds
* relevant GCC/Clang/toolchain portability concerns
* whether current QuickJS build behavior already uses or supports LTO in some configurations
* which hot paths are most likely to benefit from LTO after modularization

Do not implement LTO changes during this planning phase.

## Unity / amalgamated builds

A unity or amalgamated build is NOT a requirement for this refactoring.

The canonical architecture should consist of clean, independently compilable translation units.

Do not weaken module boundaries or introduce source-level coupling merely to make a future unity build easier.

If, after analyzing the codebase, there is a compelling future use case for:

* a unity build for performance experiments, or
* a generated amalgamated source file for easy embedding/distribution,

briefly note that possibility in the plan.

However:

* it is optional
* it should not influence the core module architecture
* it does not need to be designed in detail during this task
* it should not be implemented as part of this refactoring unless explicitly requested later

The normal modular build remains the primary architecture.

## Circular dependencies

Identify likely circular dependencies.

Do not plan to solve them by putting all internal types and functions into a global header.

Consider instead:

* changing module boundaries
* extracting narrowly scoped shared interfaces
* moving data ownership
* keeping tightly coupled code in one module
* using forward declarations where appropriate

Document questionable or difficult boundaries explicitly.

## Interpreter organization

Keep the bytecode interpreter and bytecode representation clearly identifiable and reasonably separated from unrelated parser/compiler/runtime implementation where practical.

Future work may include:

* dispatch optimization
* superinstructions
* quickening
* inline caches
* property-access fast paths
* call fast paths
* bytecode redesign
* template interpreter work

Do NOT design or implement those features now.

However, the proposed modular structure should avoid making such future work unnecessarily difficult.

Explain how the proposed VM/interpreter boundaries support future optimization work.

## Behavior that must remain unchanged

The eventual refactoring must not intentionally change:

* JavaScript semantics
* parser behavior
* compiler behavior
* bytecode format
* opcode semantics
* JSValue representation
* object representation
* shape semantics
* property lookup behavior
* string behavior
* atom behavior
* GC behavior
* reference-counting behavior
* cycle-removal behavior
* module semantics
* public API
* optimization strategy

The plan should avoid requiring semantic changes merely to achieve cleaner module boundaries.

## Avoid unrelated cleanup

The implementation plan should minimize unrelated changes.

Avoid planning:

* large-scale renaming
* whole-tree reformatting
* speculative abstractions
* data-structure redesign
* IC implementation
* shape redesign
* bytecode redesign
* interpreter redesign
* parser/compiler redesign

Prefer mechanical movement of existing code where practical.

Keep the eventual diff reasonably reviewable against upstream QuickJS.

## Build systems

Inspect the currently supported build systems.

Plan the changes needed so that normal builds compile all new translation units independently.

Preserve:

* C implementation
* supported compilers
* supported platforms
* existing portability
* existing external dependency model

Do not convert the project to C++.

Do not introduce new runtime dependencies.

Identify any build-system-specific risks.

## Baseline and validation plan

Before implementation begins, the eventual implementation phase must establish a pre-refactor baseline.

Plan exactly what should be recorded.

At minimum:

* successful build configurations
* existing unit/project tests
* Test262 result/failure set using the repository's existing configuration
* existing benchmarks
* binary size if useful
* relevant build-time or compile-time observations if useful

For Test262, the implementation phase should compare the before/after failure sets, not only process exit codes.

For benchmarks:

* use identical build and benchmark configurations before and after
* compare both non-LTO builds and LTO builds where practical
* report meaningful regressions
* do not hide regressions by modifying the benchmark configuration

If practical, recommend representative hot-path microbenchmarks for detecting regressions specifically caused by translation-unit splitting.

Pay particular attention to distinguishing:

* regressions caused by the structural refactoring itself
* regressions caused by loss of cross-TU optimization
* regressions recovered by appropriate `static inline` helpers
* regressions recovered by LTO

## Incremental migration plan

Propose an implementation order that keeps the repository buildable after major steps whenever practical.

Identify which modules are safest to extract first and which highly coupled areas should be delayed.

The exact order should come from the actual dependency analysis.

For each stage, indicate:

* what code moves
* what new internal interfaces appear
* what build changes are required
* what tests should run
* major risks

Avoid a giant all-at-once rewrite unless there is a strong technical reason.

## PLAN.md deliverable

Write the complete planning result to:

```text
PLAN.md
```

`PLAN.md` should be the authoritative planning document for the later implementation phase.

It should contain:

1. proposed module/subsystem boundaries
2. proposed final source tree
3. responsibility of each module
4. dependency direction between major modules
5. expected internal headers
6. important currently-static symbols that may become internal APIs
7. tiny hot helpers that should potentially become `static inline`
8. hot-path module-boundary risks
9. likely cross-TU optimization losses
10. circular dependencies and proposed solutions
11. build-system changes
12. LTO strategy
13. baseline and validation strategy
14. incremental migration sequence
15. areas that should intentionally remain together
16. architectural tradeoffs or uncertain boundaries
17. any major blocker or risk discovered
18. optionally, whether a future generated amalgamated distribution would be useful, without making it part of the current architecture

For meaningful architectural tradeoffs, briefly describe the alternatives and state which approach you recommend.

Make `PLAN.md` detailed enough that a later coding-agent session can implement the refactoring by reading it together with the repository, without relying on transient conversation context.

Do not implement the refactoring during this phase.

After writing `PLAN.md`, provide only a concise summary of the proposed architecture and any major risks or decisions that I should review.

Stop after that.

Do NOT begin implementation until I review `PLAN.md` and explicitly ask you to proceed.
