/* Group reordering for host-compiled collation policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
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
