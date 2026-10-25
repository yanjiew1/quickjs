/* Build-time relation semantics over stable symbolic collation weights.
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
#ifndef QJS_INTL_COMPILER_RELATIONS_H
#define QJS_INTL_COMPILER_RELATIONS_H
#include "collation-weights.h"
#include "../../src/intl/collation/coordinates.h"

/* The root catalog owns coordinate allocation, common aliases and protected
   WF2 boundaries. Relation execution must not infer them from one wire format. */
typedef QJSIntlCollationRootLimits QJSIntlRelationRoots;

enum {
    QJS_INTL_RELATION_LOWER = QJS_INTL_COLLATION_LOWER,
    QJS_INTL_RELATION_MIXED = QJS_INTL_COLLATION_MIXED,
    QJS_INTL_RELATION_UPPER = QJS_INTL_COLLATION_UPPER
};

typedef struct QJSIntlRelationCase {
    /* Explicit immutable-root metadata: primary is Boolean, case_value is one
       of the three values above, independent of the ordinary tertiary ID. */
    unsigned int primary, case_value;
} QJSIntlRelationCase;

typedef QJSIntlCollationCoordinate QJSIntlRelationElement;

typedef struct QJSIntlRelationMapping {
    QJSIntlRelationElement *elements;
    size_t count;
} QJSIntlRelationMapping;

typedef struct QJSIntlRelations {
    QJSIntlAllocator allocator;
    QJSIntlRelationRoots roots;
    QJSIntlWeightGraph axes[4];
    QJSIntlRelationMapping cursor;
    int ready;
} QJSIntlRelations;

/* This is a host compiler owner, not an untrusted binary reader. Input CEs must
   be root conversions or results from this same owner, before freezing.
   Initialize fresh/cleared storage; clear before reuse. Do not copy a live owner. Allocator state
   outlives all mappings and cannot reenter these operations. */
QJSIntlStatus qjs_intl_relations_init(QJSIntlRelations *, const QJSIntlAllocator *,
                                      const QJSIntlRelationRoots *);
void qjs_intl_relations_clear(QJSIntlRelations *);
void qjs_intl_relation_mapping_clear(const QJSIntlAllocator *, QJSIntlRelationMapping *);
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
                                       const QJSIntlRelationCase *root_case, size_t root_count,
                                       const QJSIntlRelationElement *extension, size_t extension_count,
                                       QJSIntlRelationMapping *out);
#endif
