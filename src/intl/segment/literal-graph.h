/* Shared literal scalar DAG mechanics; containers own routes and payload mode.
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
#ifndef QJS_INTL_SEGMENT_LITERAL_GRAPH_H
#define QJS_INTL_SEGMENT_LITERAL_GRAPH_H

#include "../common/types.h"

typedef struct QJSIntlSegmentLiteralGraph {
    const uint8_t *alphabet, *values, *starts, *terminals, *labels, *targets;
    uint32_t alphabet_count, value_count, state_count, edge_count;
    uint32_t root, words, max_scalars, max_utf16;
} QJSIntlSegmentLiteralGraph;

/* Bind aligned wire arrays without allocation. Only numeric metadata is read
   from metadata; its pointers are ignored. Outputs must be fresh and disjoint
   from inputs and bytes; reset on failure. Success checks extents, dimensions
   and padding, not graph semantics. The container must validate its payload
   mode, then call validate before publishing or querying the view. */
QJSIntlStatus qjs_intl_segment_literal_graph_bind(const uint8_t *bytes, size_t length,
                                                   const QJSIntlSegmentLiteralGraph *metadata,
                                                   QJSIntlSegmentLiteralGraph *out);

/* Require an unchanged successful bind. Validate scalar/value pools, graph
   safety, productivity, reachability and exact language metadata. Temporary
   bounded scratch uses malloc/free and is released on every path. This proves
   postorder, not minimality or unique canonical numbering. */
QJSIntlStatus qjs_intl_segment_literal_graph_validate(const QJSIntlSegmentLiteralGraph *,
                                                       const QJSIntlAllocator *);

/* Require an unchanged successfully validated view. It borrows immutable bytes
   and may be copied while they remain alive. Queries allocate nothing. A miss
   is UNSUPPORTED, distinct from terminal value zero. Outputs are disjoint from
   views/bytes and reset on failure. No close is required. */
QJSIntlStatus qjs_intl_segment_literal_graph_step(const QJSIntlSegmentLiteralGraph *, uint32_t state,
                                                   uint32_t scalar, uint32_t *out);
QJSIntlStatus qjs_intl_segment_literal_graph_value(const QJSIntlSegmentLiteralGraph *, uint32_t state,
                                                    uint32_t *out);

#endif
