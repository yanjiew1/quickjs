/* Native dictionary path selection over protected base/grapheme decisions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENT_DICTIONARY_REFINE_H
#define QJS_INTL_SEGMENT_DICTIONARY_REFINE_H

#include "routing.h"
#include "scan.h"

/* Require validated word programs/dictionaries/routing and matching successful
   decoded/base/grapheme scans. Refine raw script-family runs without crossing
   protected breaks or creating cuts at protected no-breaks. CJK uses additive
   uint32 costs with checked uint64 DP; other families use latest legal complete
   terminals. Unknown steps preserve base decisions. Run endpoints are virtual
   path vertices, not permission to change surrounding cuts. All scratch is
   temporary; failure leaves word decisions unchanged. */
QJSIntlStatus qjs_intl_segment_dictionary_refine(const QJSIntlAllocator *,
                                                 const QJSIntlSegmentProgram *word_program,
                                                 const QJSIntlSegmentText *,
                                                 const QJSIntlSegmentScan *graphemes,
                                                 const QJSIntlSegmentDictionaryData *,
                                                 const QJSIntlSegmentRouting *,
                                                 QJSIntlSegmentScan *words);

#endif
