/* Group reordering for host-compiled collation policy.
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
#ifndef QJS_INTL_COMPILER_REORDER_H
#define QJS_INTL_COMPILER_REORDER_H
#include "collation-ir.h"
#include "collation-root-input.h"

typedef struct QJSIntlCollationReorder {
    QJSIntlRootInput root;
    QJSIntlAllocator allocator;
    uint32_t *ranks; /* Source group ID -> reordered group rank, not P weight. */
    uint32_t count, core[5];
    int ready;
} QJSIntlCollationReorder;

/* Copy a validated root view; unchanged QJCR bytes and allocator state outlive
   the owner. Initialize fresh/cleared storage; clear failed init before reuse.
   Do not copy a live owner or reenter mutation from allocator callbacks. */
QJSIntlStatus qjs_intl_collation_reorder_init(QJSIntlCollationReorder *, const QJSIntlRootInput *,
                                             const QJSIntlAllocator *);
void qjs_intl_collation_reorder_clear(QJSIntlCollationReorder *);
/* A new list replaces the old one atomically. Requested groups are bounded by
   catalog size; failure preserves ranks. No within-group weights, construction
   state, variable classification or fixed/trailing positions are changed. */
QJSIntlStatus qjs_intl_collation_reorder_apply(QJSIntlCollationReorder *, const QJSIntlIRInstruction *);
QJSIntlStatus qjs_intl_collation_reorder_rank(const QJSIntlCollationReorder *, uint32_t group,
                                             uint32_t *rank);
#endif
