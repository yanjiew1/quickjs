/* Sparse build-time weight ordering; no JS objects or runtime pack ownership.
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
#ifndef QJS_INTL_COMPILER_WEIGHTS_H
#define QJS_INTL_COMPILER_WEIGHTS_H
#include "../../src/intl/common/types.h"

#define QJS_INTL_WEIGHT_NONE UINT32_MAX

typedef struct QJSIntlWeightParents {
    uint32_t primary, secondary;
} QJSIntlWeightParents;

struct QJSIntlWeightNode;
struct QJSIntlWeightGap;
struct QJSIntlWeightOffset;
typedef struct QJSIntlWeightGraph {
    QJSIntlAllocator allocator;
    struct QJSIntlWeightNode *nodes;
    struct QJSIntlWeightGap *gaps;
    struct QJSIntlWeightOffset *offsets;
    uint32_t root_max, node_count, gap_count;
    size_t node_capacity, gap_capacity;
    int frozen;
} QJSIntlWeightGraph;

/* Root IDs are virtual ordinal weights 0..root_max (< 2^31). Zero denotes the
   shared ignorable root. Tailored IDs have bit31 set and remain stable across
   growth and freezing. The semantic owner supplies legal weak-axis band
   sentinels, common root aliases, and implicit-primary order-key conversion.
   This graph alone does not implement UCA relation semantics or WF2. Initialize
   fresh/cleared storage; clear before reuse. Graph, input and output storage
   are disjoint. Allocator state outlives the graph and cannot reenter mutation. */
QJSIntlStatus qjs_intl_weight_graph_init(QJSIntlWeightGraph *, const QJSIntlAllocator *,
                                        uint32_t root_max);
void qjs_intl_weight_graph_clear(QJSIntlWeightGraph *);
/* Insert immediately beside reference, within its virtual root gap. Parents
   are stable IDs in the stronger axes, copied as provenance (NULL means zero).
   No existing ID/order changes. Failure leaves the ordering unchanged and
   resets *id to NONE; allocation capacity may have grown. No input ownership. */
QJSIntlStatus qjs_intl_weight_insert(QJSIntlWeightGraph *, uint32_t reference, int before,
                                    const QJSIntlWeightParents *, uint32_t *id);
QJSIntlStatus qjs_intl_weight_parents(const QJSIntlWeightGraph *, uint32_t id,
                                     QJSIntlWeightParents *out);
/* Compare stable IDs without freezing or allocating. Root/gap comparisons
   are constant-time; two tailored IDs in one unfrozen gap require a bounded
   linked walk. Frozen graphs use resolved ranks. This query does not prevent
   later insertion. Output is -1/0/+1, reset on failure; storage is disjoint. */
QJSIntlStatus qjs_intl_weight_compare(const QJSIntlWeightGraph *, uint32_t left,
                                      uint32_t right, int *order);
/* Freeze once all relations have executed. Allocate a sorted sparse root
   offset map, assign final uint32 ranks, then prohibit insertion. Failure is
   retryable. Repeated successful freeze is harmless. Resolution requires a
   frozen graph; outputs are reset on failure. Root zero always remains zero. */
QJSIntlStatus qjs_intl_weight_freeze(QJSIntlWeightGraph *);
QJSIntlStatus qjs_intl_weight_resolve(const QJSIntlWeightGraph *, uint32_t id, uint32_t *rank);
/* Copy one sorted sparse offset from a frozen graph (index < gap_count).
   Root r includes cumulative additions only from gaps STRICTLY below r.
   Scalars reset on failure; no internal allocation/pointer escapes. */
QJSIntlStatus qjs_intl_weight_offset_at(const QJSIntlWeightGraph *, uint32_t index,
                                       uint32_t *root_gap, uint32_t *cumulative_added);
#endif
