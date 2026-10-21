/* Immutable host output model, before runtime wire encoding.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_SNAPSHOT_H
#define QJS_INTL_COMPILER_SNAPSHOT_H
#include "collation-program.h"
#include "../../src/intl/collation/ranked.h"

typedef struct QJSIntlCollationRootOffset {
    uint32_t root_gap, added;
} QJSIntlCollationRootOffset;

typedef struct QJSIntlCollationSnapshot {
    QJSIntlAllocator allocator;
    QJSIntlRootInput root; /* Only the unchanged validated root bytes are borrowed. */
    QJSIntlIRProgram identity; /* Labels borrow this owner's labels allocation. */
    QJSIntlCollationPolicy policy;
    QJSIntlCollationKeyIndex keys;
    uint8_t *rows, *key_bytes, *contexts, *labels;
    QJSIntlCollationRankedElement *elements;
    uint32_t *suppressed, *group_ranks;
    QJSIntlCollationRootOffset *offsets[4];
    uint32_t element_count, suppressed_count, group_count;
    uint32_t offset_counts[4], root_maxima[4];
    int ready;
} QJSIntlCollationSnapshot;

/* Create during the program's successful finalizer. Freeze axes, then copy only
   current keys/CEs plus persistent suppression, sparse root shifts and policy.
   Output is fresh/cleared, disjoint storage, empty on failure. Partial freezing
   on failure is allowed: abort execution; do not resume rules. Snapshot arrays
   and labels survive program clear. Root bytes/allocator state must outlive it.
   Do not copy a live owner; after moving it, zero the old descriptor without
   freeing its transferred arrays. */
QJSIntlStatus qjs_intl_collation_snapshot_create(QJSIntlCollationProgram *, const QJSIntlIRProgram *,
                                                 QJSIntlCollationSnapshot *);
void qjs_intl_collation_snapshot_clear(QJSIntlCollationSnapshot *);
/* Resolve an unchanged root ordinal in this program's final rank domain.
   Snapshot arrays stay immutable. Zero/own-gap ordering is preserved. */
QJSIntlStatus qjs_intl_collation_snapshot_root_rank(const QJSIntlCollationSnapshot *, unsigned int axis,
                                                    uint32_t root, uint32_t *rank);
#endif
