/* Native dictionary path selection over protected base/grapheme decisions.
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
