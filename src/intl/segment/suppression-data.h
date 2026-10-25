/* Borrowed complete literal sentence suppression lists and locale routes.
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
#ifndef QJS_INTL_SEGMENT_SUPPRESSION_DATA_H
#define QJS_INTL_SEGMENT_SUPPRESSION_DATA_H

#include "../native/data.h"
#include "literal-graph.h"

typedef struct QJSIntlSegmentSuppressionData {
    QJSIntlSection section;
    const uint8_t *descriptors, *bindings, *fingerprint;
    uint32_t list_count, binding_count, locale_count;
} QJSIntlSegmentSuppressionData;

/* Require an unchanged successful shared-data view. Validate the complete
   optional section before publication, preflighting all descriptors and routes
   before temporary graph scratch allocation. Missing section/route and unknown
   version/Unicode/profile are UNSUPPORTED; malformed known data is DATA_ERROR.
   Retain no allocation; copied views borrow immutable pack bytes, which must
   outlive all views and queries. Source identity is not authentication or
   executable Segmenter coverage. Outputs must be fresh and disjoint from
   inputs and bytes; reset on failure. No close is required. */
QJSIntlStatus qjs_intl_segment_suppression_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                                     QJSIntlSegmentSuppressionData *out);

/* Require unchanged views from successful open. Lookup allocates nothing and
   returns a copied graph view. An explicit empty policy succeeds with a zero
   graph (words == 0); it differs from a missing route. Do not traverse an empty
   graph. Nonempty views support the common literal graph step/value queries. */
QJSIntlStatus qjs_intl_segment_suppression_at(const QJSIntlSegmentSuppressionData *, uint32_t index,
                                              QJSIntlSegmentLiteralGraph *out);
QJSIntlStatus qjs_intl_segment_suppression_find(const QJSIntlSegmentSuppressionData *, uint32_t locale,
                                                QJSIntlSegmentLiteralGraph *out);

#endif
