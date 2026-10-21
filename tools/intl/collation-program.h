/* Execute checked host rules; runtime publication belongs to the pack writer.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_PROGRAM_H
#define QJS_INTL_COMPILER_PROGRAM_H
#include "collation-builder.h"
#include "collation-reorder.h"
#include "collation-settings.h"

typedef struct QJSIntlCollationProgram {
    QJSIntlCollationBuilder builder;
    QJSIntlCollationPolicy policy;
    QJSIntlCollationReorder reorder;
} QJSIntlCollationProgram;

/* Called once per successfully executed program. All descriptors/state are
   borrowed for this call only. The finalizer may freeze ordering axes and copy
   current mappings/policy into its own staging owner, but must not clear, retain,
   reenter or otherwise mutate program state. Failure stops further programs.
   Earlier callbacks are not rolled back: discard staged output if execute fails. */
typedef QJSIntlStatus (*QJSIntlCollationProgramFinalizer)(void *, const QJSIntlIRProgram *,
                                                         QJSIntlCollationProgram *);
/* Both views were successfully opened and their bytes remain unchanged/alive.
   Check all 32 identity bytes before allocating state. Each program gets fresh
   construction/settings/reordering owners, cleared on every success/error path.
   A NULL finalizer executes/validates without retaining output. Diagnostic/output
   storage is disjoint from the input and live owners. Allocator and finalizer
   opaque bookkeeping may be shared, but callbacks must not reenter execution. */
QJSIntlStatus qjs_intl_collation_program_execute(const QJSIntlRootInput *, const QJSIntlIRView *,
                                                 const QJSIntlAllocator *,
                                                 QJSIntlCollationProgramFinalizer, void *,
                                                 QJSIntlIRDiagnostic *);
#endif
