/* Build-time relation semantics over stable symbolic collation weights.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_RELATIONS_H
#define QJS_INTL_COMPILER_RELATIONS_H
#include "collation-weights.h"
#include "../../src/intl/collation/logical.h"

/* Separate ordinary weak weights from weights of ignorable CEs (UCA WF2).
   The boundary itself is an insertion anchor, never an emitted CE weight. */
#define QJS_INTL_RELATION_WEAK_BOUNDARY UINT32_C(0x10000)
#define QJS_INTL_RELATION_WEAK_MAX UINT32_C(0x1ffff)

enum {
    QJS_INTL_RELATION_LOWER,
    QJS_INTL_RELATION_MIXED,
    QJS_INTL_RELATION_UPPER
};

typedef struct QJSIntlRelationElement {
    uint32_t weights[4]; /* Stable P/S/T/Q IDs until graph freeze/resolution. */
    uint32_t root_primary; /* Original root anchor for grouping/variable policy. */
    unsigned char case_value, variable, implicit;
} QJSIntlRelationElement;

typedef struct QJSIntlRelationMapping {
    QJSIntlRelationElement *elements;
    size_t count;
} QJSIntlRelationMapping;

typedef struct QJSIntlRelations {
    QJSIntlAllocator allocator;
    QJSIntlWeightGraph axes[4];
    QJSIntlRelationMapping cursor;
    int ready;
} QJSIntlRelations;

/* This is a host compiler owner, not an untrusted binary reader. Input CEs must
   be root conversions or results from this same owner, before freezing.
   Initialize fresh/cleared storage; clear before reuse. Do not copy a live owner. Allocator state
   outlives all mappings and cannot reenter these operations. */
QJSIntlStatus qjs_intl_relations_init(QJSIntlRelations *, const QJSIntlAllocator *);
void qjs_intl_relations_clear(QJSIntlRelations *);
void qjs_intl_relation_mapping_clear(const QJSIntlAllocator *, QJSIntlRelationMapping *);
/* logical must be a valid logical_root result, not a raw implicit continuation.
   Root conversion does not allocate. No pointers are retained. */
QJSIntlStatus qjs_intl_relation_root(const QJSIntlCollationLogical *, QJSIntlRelationElement *);
/* Copy a CURRENT lookup result as a reset. Empty lookup means one zero CE.
   Allocation failure leaves the cursor unchanged. */
QJSIntlStatus qjs_intl_relations_reset(QJSIntlRelations *, const QJSIntlRelationElement *, size_t);
/* strength: 0 equality, 1..4 P/S/T/Q. before: first relation of a [before 1..3]
   reset, with the matching strength already checked by the instruction owner.
   root_case is the immutable ROOT lookup of the relation text only, excluding
   prefix/extension. Extension CEs are a CURRENT lookup, copied verbatim and
   excluded from the next relation's cursor. Inputs may borrow the old cursor;
   out must be empty and disjoint. Success creates an independently owned
   mapping; failure leaves its output empty and the cursor/order unchanged. */
QJSIntlStatus qjs_intl_relations_apply(QJSIntlRelations *, unsigned int strength, int before,
                                      const QJSIntlCollationLogical *root_case, size_t root_count,
                                      const QJSIntlRelationElement *extension, size_t extension_count,
                                      QJSIntlRelationMapping *out);
#endif
