/* Execute checked host rules; runtime publication belongs to the pack writer.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
