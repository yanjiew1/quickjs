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

# Historical simplification review and accepted plan

Reviewed on 2026-10-24 against main `7e50e8aa`, Temporal `5a34524c`,
and Intl `72c47acf`. This records the accepted direction; it does not claim
that the proposed changes or their validation are complete. This historical
record is not an actionable checklist; inspect current code before repeating
any of its implementation steps.

The goal is a small, fast, maintainable engine. Remove unnecessary owners,
repeated work and maintenance machinery while preserving standards behavior,
required algorithms, error handling and useful tests.

## Findings

| Area | Finding | Direction |
| --- | --- | --- |
| Host modules and examples | JSON/Point initialization ignores fallible operations | Propagate failures and test cleanup; share the JSON constructor completion path |
| Intl data | Shipped collation diagnostics are unused by ranked runtime comparison | Move their 1,186,388 payload bytes per pack to independent test fixtures |
| Intl initialization | Services repeatedly admit the same immutable data | Reuse existing checked views; initialize typed state only when needed |
| Locale preferences | Validation scratch scales with all 391,196 strings instead of 329 preference rows | Group referenced list IDs/counts, retaining grammar and conflict checks |
| List and composed formatters | Redundant List wrapper/patterns; repeated Duration configurations and text copies | Remove proven redundant state first; measure broader composition changes |
| DateTimeFormat | Multiple intermediate programs and repeated endpoint preparation | Establish semantic equality, then reduce copies and materialize selected plans |
| Build/test ownership | Driver macros recompile invariant sources; selected data and normal/LTO engines duplicate work | Give components explicit owners; keep genuinely different machine/PIC/instrumentation contexts |
| Data maintenance | Too many public stages and redundant source identities | Three thin operations: acquire, generate, update pin; domain helpers stay independent |
| Calendar inputs | Ordinary generation parses vendored Rust data | Maintain parsed JSON with provenance/license; parse original sources only when updating |
| Support code | Test-only wait interfaces retain production state; Unicode callback adapter preserves an unused interface | Test production selectors and propagate checked callbacks directly |
| Fuzzing and maintenance | Auxiliary harness status and historical TODO entries are unclear | Retain meaningful input/ownership coverage; remove useless probes and stale instructions |
| Documentation | Current usage, formats, delivery journals and future plans are interleaved | One owner per fact; preserve useful references and approved future backend plans |

Coverage included every major tracked area: engine/compiler/VM/serialization,
host services/public headers, Intl, Temporal/calendar/timezone, cutils/dtoa,
Unicode/RegExp/waits, data/generators, build/cross/release, tests/Test262,
CI/benchmarks, examples/fuzzers, and documentation. This is an architecture and
test-contract review, not a proof of every function or algorithm.

## Constraints

- Keep semantic C module boundaries and compile real translation units. A large
  algorithm or many files alone is not evidence of unnecessary complexity.
- Keep JavaScript coercion, realm identity, references, strict/scope cleanup,
  async state and native ownership correct. Retain independent source oracles.
- During development, binary ABI and old binary-format compatibility are not
  required. Readers/writers/data/tests may change together. Keep current-format
  bounds/invariant checks and source-data versions, provenance and licenses.
  Removing a public source API is a separate decision.
- Keep portable generated C embedding. Do not introduce compression, a runtime
  provider registry, a generic schema/test framework or hot reload for this work.
- Pins belong with their data. An authenticated archive or immutable Git commit
  can replace member-hash inventories only when generation consumes that trusted
  source rather than a mutable extracted cache. Drop redundant Git blob hashes.
- Production CLDR ZIP coverage and explicitly selected full-Git coverage remain
  separate. Do not silently backfill one distribution from another.
- One writer integrates small coherent changes. Common changes belong on main;
  Temporal and Intl changes remain on their respective dependent branches.

## C tests and JavaScript

The Intl snapshot contains 234 tracked C files under tests: one full JavaScript
migration candidate, 40 partial candidates, 189 retained native tests and four
support modules/helpers. Partial migration usually removes no native target.

Prioritize `test_temporal_relative.c`, the stripped-function toString case in
`test_api.c`, and the foreign waitAsync prototype case. Consolidate ordinary
Intl service output tables with existing JavaScript tests only after matching
the exact options, data profile and missing witnesses.

Keep native allocation-failure, raw-context, output-buffer/status, pointer,
fixture/decoder and independent algorithm tests. The digit/number/locale-option
C drivers already run JavaScript and retain useful private-contract checks.
`std.createRealm()` creates a full context, not a raw one; `std.gc()` cannot
replace explicit native release or deterministic allocator failure.

Keep the short Unicode C examples: they run with Intl disabled, whereas a
Segmenter replacement would not. Keep the small Atomics C launcher too: the
existing `-N --module` runner accepts pending modules, so replacing the launcher
would add completion machinery without removing a target. Fixing that runner
requires its own justification and regression coverage.

## Delivery sequence

Each numbered stage contains independent changes, not one large commit.

1. **Correctness and immediate documentation.** Repair JSON/Point initialization
   failures with bounded regressions. Correct capability claims and misplaced
   headings without waiting for larger work.
2. **Proven small reductions.** Remove production collation diagnostics, shrink
   preference scratch, remove List redundancy, and migrate the few useful JS
   groups. Preserve native lifetime/recovery witnesses and default coverage.
3. **Build/test ownership.** Separate driver flags, share selected data and
   algorithm objects, and synchronize the chosen archive/LTO policy with qjsc.
   Only then retire cache machinery and duplicate execution.
4. **Data maintenance.** Add three thin entry points around existing family
   tools, move pins, normalize Calendar JSON, remove redundant identities, and
   make candidate-pin regeneration usable end to end.
5. **Shared admission.** Pass existing QJSIntlData views first; keep module
   capabilities, locale availability and mutable defaults distinct. Publish
   initialization only on success, allowing failure followed by retry.
6. **Support cleanup.** Remove unused wait wrappers/state and Unicode callback
   adapter, then clean obsolete probes and low-value fuzz paths by owner.
7. **Measured composition.** Compare Duration configurations/text transfer,
   Segmenter traversal and parsed-zone reuse against their parents. Refactor
   DateTimeFormat incrementally last; do not retain a more complicated design
   merely because it was attempted.

Independent stages can proceed while another candidate is being investigated.
Keep formatter/source equality stronger than pattern-string equality. Do not
force every family generator into one internal workflow or every module into
one locale inventory.

## Acceptance

- Implementation changes use combined ASan+UBSan repository tests with assertions
  enabled. Keep focused regressions, including C tests where JS cannot express
  the contract. Fault tests must actually exhaust allocator pools when relevant.
- Test build changes in the affected static/shared/both, normal/LTO and cross
  contexts, including installed qjsc output and exact public exports. A shared
  build without private tests needs no extra static engine.
- Keep one complete quick local gate. If CI tooling phases change, update their
  selection invariants together; adding a suite label does not remove duplication.
- Verify data semantics and reproducibility from the selected authenticated
  source. Identify expected schema/fingerprint differences. Preserve malformed
  bytes reaching the C reader and independent oracle coverage.
- For performance candidates, record the affected allocation/state/work before
  changing it, then use paired fresh-process measurements against the parent.
  Use unsanitized GCC/Clang non-LTO, with assertions disabled for both revisions.
  Investigate regressions and retain only justified changes.
- The historical CI campaign was explicitly stopped. This plan does not restart
  it or turn incomplete results into passes. Keep implementation evidence in
  working records rather than adding execution journals here.
