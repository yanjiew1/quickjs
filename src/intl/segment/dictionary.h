/* Borrowed validated scalar dictionary DAGs; no text or segmentation policy.
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
#ifndef QJS_INTL_SEGMENT_DICTIONARY_H
#define QJS_INTL_SEGMENT_DICTIONARY_H
#include "../native/pack.h"
#include "literal-graph.h"

typedef enum QJSIntlSegmentDictionaryFamily {
    QJS_INTL_DICTIONARY_CJK,
    QJS_INTL_DICTIONARY_KHMER,
    QJS_INTL_DICTIONARY_LAO,
    QJS_INTL_DICTIONARY_MYANMAR,
    QJS_INTL_DICTIONARY_THAI,
    QJS_INTL_DICTIONARY_FAMILY_COUNT
} QJSIntlSegmentDictionaryFamily;

typedef struct QJSIntlSegmentDictionary {
    QJSIntlSegmentLiteralGraph graph;
    unsigned int valued;
} QJSIntlSegmentDictionary;

typedef struct QJSIntlSegmentDictionaryData {
    QJSIntlSection section;
    QJSIntlSegmentDictionary dictionaries[QJS_INTL_DICTIONARY_FAMILY_COUNT];
    unsigned int present;
} QJSIntlSegmentDictionaryData;

/* Open an optional section from an unchanged pack. Validate every descriptor,
   graph, used pool, accepted-path count and length maximum before publication.
   Temporary bounded scratch uses malloc/free and is released on every path.
   The result owns no allocation and borrows the immutable pack bytes, which
   must outlive it and all returned dictionary pointers. No close is required.
   Source authentication and executable locale coverage are separate contracts.
   Require fresh outputs disjoint from inputs; reset failures. */
QJSIntlStatus qjs_intl_segment_dictionary_data_open(const QJSIntlPack *, const QJSIntlAllocator *,
                                                    QJSIntlSegmentDictionaryData *out);
/* Borrow the selected view until its data owner is changed or released.
   A caller retaining a handle beyond that owner may copy the fixed view by
   value, while keeping the underlying immutable pack bytes alive. */
QJSIntlStatus qjs_intl_segment_dictionary_at(const QJSIntlSegmentDictionaryData *,
                                             QJSIntlSegmentDictionaryFamily,
                                             const QJSIntlSegmentDictionary **out);
/* Query unchanged views from successful open, without allocation or mutation.
   UNSUPPORTED means no edge/terminal, distinct from a terminal with cost zero.
   Outputs are disjoint from views and their immutable bytes. Reset non-NULL
   output on failure. Scalar misses include lone surrogates;
   callers preserve their original UTF-16 input and choose boundary policy.
   Membership dictionaries return value zero, with valued remaining false. */
QJSIntlStatus qjs_intl_segment_dictionary_step(const QJSIntlSegmentDictionary *, uint32_t state,
                                               uint32_t scalar, uint32_t *out);
QJSIntlStatus qjs_intl_segment_dictionary_value(const QJSIntlSegmentDictionary *, uint32_t state,
                                                uint32_t *out);
#endif
