/* Raw digital-duration source facts; no display or locale admission policy.
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
#ifndef QJS_INTL_DURATION_DATA_H
#define QJS_INTL_DURATION_DATA_H
#include "../native/data.h"

typedef enum QJSIntlDurationSkeleton {
    QJS_INTL_DURATION_HM,
    QJS_INTL_DURATION_HMS,
    QJS_INTL_DURATION_MS,
    QJS_INTL_DURATION_SKELETON_COUNT
} QJSIntlDurationSkeleton;

typedef enum QJSIntlDurationPatternState {
    QJS_INTL_DURATION_PATTERN_MISSING,
    QJS_INTL_DURATION_PATTERN_VALUE,
    QJS_INTL_DURATION_PATTERN_BLOCKED
} QJSIntlDurationPatternState;

typedef struct QJSIntlDurationPattern {
    QJSIntlDurationPatternState state;
    QJSIntlBytes text, origin_locale;
    QJSIntlDurationSkeleton origin_skeleton; /* Meaningful unless MISSING. */
} QJSIntlDurationPattern;

typedef struct QJSIntlDurationData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *locale_rows, *rows, *records;
    uint32_t row_count, record_count;
} QJSIntlDurationData;

/* Borrow an unchanged validated shared owner. Validate schema, canonical
   sharing, states and terminal source identities; no runtime capability is
   inferred. Require malloc/free and release all validation scratch before
   return. Follow pack.h's disjoint-output contract; outputs reset on error. */
QJSIntlStatus qjs_intl_duration_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                        QJSIntlDurationData *);
/* Exact shared-locale index and skeleton. Allocate nothing and perform no
   locale/style fallback or raw-pattern projection. MISSING returns OK with
   a zero record, distinct from BLOCKED and a present empty VALUE. */
QJSIntlStatus qjs_intl_duration_pattern_at(const QJSIntlDurationData *, uint32_t locale,
                                         QJSIntlDurationSkeleton, QJSIntlDurationPattern *);
#endif
