/* Full literal suppression matching over prepared sentence decisions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
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
