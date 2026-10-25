/* Shared native date-field labels; no formatting or availability policy.
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
#ifndef QJS_INTL_DATETIME_FIELD_DATA_H
#define QJS_INTL_DATETIME_FIELD_DATA_H
#include "../native/data.h"
#include "field.h"

typedef enum QJSIntlDateFieldWidth {
    QJS_INTL_DATE_FIELD_LONG,
    QJS_INTL_DATE_FIELD_SHORT,
    QJS_INTL_DATE_FIELD_NARROW,
    QJS_INTL_DATE_FIELD_WIDTH_COUNT
} QJSIntlDateFieldWidth;

typedef enum QJSIntlDateFieldState {
    QJS_INTL_DATE_FIELD_MISSING,
    QJS_INTL_DATE_FIELD_VALUE,
    QJS_INTL_DATE_FIELD_BLOCKED
} QJSIntlDateFieldState;

typedef struct QJSIntlDateFieldLabel {
    QJSIntlDateFieldState state;
    QJSIntlBytes text, origin_locale, origin_alternate;
    QJSIntlDateField origin_field; /* Origin fields meaningful unless MISSING. */
    QJSIntlDateFieldWidth origin_width;
} QJSIntlDateFieldLabel;

typedef struct QJSIntlDateFieldData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *alternates, *locale_rows, *rows, *records;
    uint32_t alternate_count, row_count, row_width, record_count;
} QJSIntlDateFieldData;

/* Borrow an unchanged validated shared owner. Require malloc/free and release
   validation scratch on every path. Validate canonical sharing, references,
   states and source identity. Follow pack.h's disjoint-output contract; reset
   non-NULL output on failure. No formatter capability is inferred. */
QJSIntlStatus qjs_intl_date_field_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                            QJSIntlDateFieldData *);
/* Sorted alternate inventory; index zero is the ordinary empty selector.
   Borrow returned slices from the owner. These queries allocate nothing. */
QJSIntlStatus qjs_intl_date_field_alternate_at(const QJSIntlDateFieldData *, uint32_t index,
                                               QJSIntlBytes *);
/* Exact locale/field/width/alternate indexes. No extra width/style fallback.
   MISSING returns OK with a zero label, distinct from BLOCKED and empty VALUE. */
QJSIntlStatus qjs_intl_date_field_label_at(const QJSIntlDateFieldData *, uint32_t locale,
                                           QJSIntlDateField, QJSIntlDateFieldWidth,
                                           uint32_t alternate, QJSIntlDateFieldLabel *);
#endif
