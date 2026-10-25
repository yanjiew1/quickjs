/* Full literal suppression matching over prepared sentence decisions.
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
#ifndef QJS_INTL_SEGMENT_SUPPRESSION_H
#define QJS_INTL_SEGMENT_SUPPRESSION_H

#include "literal-graph.h"
#include "scan.h"

/* Require a validated sentence program, list from suppression-data, decoded
   text and its matching base scan. Apply only approved soft sentence removals;
   preserve endpoints and every other rule. Explicit empty lists are no-ops.
   Input spelling is literal, including spaces and ignored characters inside
   keys. Original terminator anchors survive only the admitted Close/Sp/ignored
   tail, never paragraph separators. Retain no allocation or borrowed state.
   Temporary scratch uses allocator; failure leaves scan decisions unchanged. */
QJSIntlStatus qjs_intl_segment_suppression_apply(const QJSIntlAllocator *, const QJSIntlSegmentProgram *,
                                                 const QJSIntlSegmentLiteralGraph *,
                                                 const QJSIntlSegmentText *, QJSIntlSegmentScan *);

#endif
