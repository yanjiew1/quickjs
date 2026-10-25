/* Number/currency layout source metadata, independent of formatter policy.
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
#ifndef QJS_INTL_NUMBER_LAYOUT_DATA_H
#define QJS_INTL_NUMBER_LAYOUT_DATA_H
#include "../locale/numbering-data.h"

typedef enum QJSIntlNumberLayoutField {
    QJS_INTL_LAYOUT_RANGE,
    QJS_INTL_LAYOUT_APPROXIMATELY,
    QJS_INTL_LAYOUT_AT_LEAST,
    QJS_INTL_LAYOUT_AT_MOST,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_EXACT_ZERO,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_EXACT_ONE,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_ZERO,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_ONE,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_TWO,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_FEW,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_MANY,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_OTHER,
    QJS_INTL_LAYOUT_CURRENCY_BEFORE_MATCH,
    QJS_INTL_LAYOUT_CURRENCY_BEFORE_SURROUNDING,
    QJS_INTL_LAYOUT_CURRENCY_BEFORE_INSERT,
    QJS_INTL_LAYOUT_CURRENCY_AFTER_MATCH,
    QJS_INTL_LAYOUT_CURRENCY_AFTER_SURROUNDING,
    QJS_INTL_LAYOUT_CURRENCY_AFTER_INSERT,
    QJS_INTL_LAYOUT_CURRENCY_APPEND_ISO,
    QJS_INTL_LAYOUT_FIELD_COUNT
} QJSIntlNumberLayoutField;

typedef enum QJSIntlCurrencyUnitCount {
    QJS_INTL_CURRENCY_UNIT_EXACT_ZERO,
    QJS_INTL_CURRENCY_UNIT_EXACT_ONE,
    QJS_INTL_CURRENCY_UNIT_ZERO,
    QJS_INTL_CURRENCY_UNIT_ONE,
    QJS_INTL_CURRENCY_UNIT_TWO,
    QJS_INTL_CURRENCY_UNIT_FEW,
    QJS_INTL_CURRENCY_UNIT_MANY,
    QJS_INTL_CURRENCY_UNIT_OTHER,
    QJS_INTL_CURRENCY_UNIT_LAYOUT_COUNT
} QJSIntlCurrencyUnitCount;

enum { QJS_INTL_LAYOUT_VALUE, QJS_INTL_LAYOUT_ABSENT, QJS_INTL_LAYOUT_BLOCKED };

typedef struct QJSIntlNumberLayoutText {
    QJSIntlBytes text;
    unsigned int state;
    unsigned int from_root; /* Currency unit template's terminal source was root. */
} QJSIntlNumberLayoutText;

typedef struct QJSIntlNumberLayouts {
    QJSIntlNumberLayoutText fields[QJS_INTL_LAYOUT_FIELD_COUNT];
} QJSIntlNumberLayouts;

typedef struct QJSIntlCurrencyUnitLayouts {
    /* Exact zero/one, then zero/one/two/few/many/other. */
    QJSIntlNumberLayoutText fields[QJS_INTL_CURRENCY_UNIT_LAYOUT_COUNT];
} QJSIntlCurrencyUnitLayouts;

typedef struct QJSIntlNumberLayoutData {
    QJSIntlNumberingData numbering;
    QJSIntlSection section;
    const uint8_t *systems, *locale_rows, *rows, *vectors;
    const uint8_t *currency_locale_vectors, *currency_vectors;
    uint32_t system_count, row_count, vector_count;
    uint32_t currency_vector_count;
} QJSIntlNumberLayoutData;

/* Require an unchanged validated numbering view. Borrow immutable storage;
   temporary schema validation memory is released before returning. Follow
   pack.h's disjoint output contract; non-NULL outputs are reset on failure.
   No template interpretation, UnicodeSet predicate or formatter fallback is
   implied. Individual consumers validate and implement their own syntax. */
QJSIntlStatus qjs_intl_number_layout_data_open(const QJSIntlNumberingData *,
                                               const QJSIntlAllocator *, QJSIntlNumberLayoutData *);

/* Exact shared locale/numbering indexes, without allocation or fallback.
   Algorithmic numbering systems return UNSUPPORTED. VALUE text can be empty
   for insertions and optional layouts; absent/blocked slices are zeroed.
   Currency unit templates retain from_root even for blocked/empty text, so the
   consumer can exclude root for a selected count but allow its OTHER fallback.
   Returned strings borrow the same unchanged owner. */
QJSIntlStatus qjs_intl_number_layouts_at(const QJSIntlNumberLayoutData *, uint32_t locale,
                                         uint32_t system, QJSIntlNumberLayouts *);

/* Locale-level currency-name placement templates. Each parent's local data
   was selected with its own default numbering system before inheritance;
   requested numeric digits do not select these templates. Aliases retain
   their terminal root provenance. No count fallback or parsing is performed.
   Allocation-free; strings borrow the unchanged owner, errors reset output. */
QJSIntlStatus qjs_intl_currency_unit_layouts_at(const QJSIntlNumberLayoutData *, uint32_t locale,
                                                QJSIntlCurrencyUnitLayouts *);
#endif
