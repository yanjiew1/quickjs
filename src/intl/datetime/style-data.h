/* Shared date/time source styles; no matching or calendar capability policy.
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
#ifndef QJS_INTL_DATETIME_STYLE_DATA_H
#define QJS_INTL_DATETIME_STYLE_DATA_H
#include "../native/data.h"
#include "style-types.h"

typedef enum QJSIntlDateStyleFamily {
    QJS_INTL_DATE_STYLE_DATE,
    QJS_INTL_DATE_STYLE_TIME,
    QJS_INTL_DATE_STYLE_FAMILY_COUNT
} QJSIntlDateStyleFamily;

typedef enum QJSIntlDateStyleField {
    QJS_INTL_DATE_STYLE_PATTERN,
    QJS_INTL_DATE_STYLE_SKELETON,
    QJS_INTL_DATE_STYLE_FIELD_COUNT
} QJSIntlDateStyleField;

typedef enum QJSIntlDateStyleState {
    QJS_INTL_DATE_STYLE_MISSING,
    QJS_INTL_DATE_STYLE_VALUE,
    QJS_INTL_DATE_STYLE_BLOCKED
} QJSIntlDateStyleState;

typedef struct QJSIntlDateStyleRecord {
    QJSIntlDateStyleState state;
    QJSIntlBytes text, numbers, origin_locale, origin_calendar, origin_alternate;
    QJSIntlDateStyleFamily origin_family;
    QJSIntlDateStyleWidth origin_width;
    QJSIntlDateStyleField origin_field;
} QJSIntlDateStyleRecord;

typedef struct QJSIntlDateStyleData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *calendars, *alternates, *locale_rows, *rows, *records;
    uint32_t calendar_count, alternate_count, row_count, row_width, record_count;
} QJSIntlDateStyleData;

/* Borrow an unchanged validated shared owner, requiring malloc/free only for
   validation scratch, released on every path. Validate sharing, references,
   source identity and states. No formatter capability or calendar arithmetic
   is inferred. Follow pack.h's disjoint-output contract. Pattern/skeleton text
   and numbering overrides remain source facts, not interpreted format options. */
QJSIntlStatus qjs_intl_date_style_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                            QJSIntlDateStyleData *);
/* Inventories and records borrow immutable storage and allocate nothing.
   Calendar IDs are raw CLDR source IDs, including the data-only generic ID.
   Alternate zero is the ordinary empty selector. */
QJSIntlStatus qjs_intl_date_style_calendar_at(const QJSIntlDateStyleData *, uint32_t,
                                              QJSIntlBytes *);
QJSIntlStatus qjs_intl_date_style_alternate_at(const QJSIntlDateStyleData *, uint32_t,
                                               QJSIntlBytes *);
/* Exact indexes only: no fallback between calendars, widths or alternatives.
   MISSING returns OK with a zero record; BLOCKED and empty VALUE are distinct.
   Origin fields are meaningful unless MISSING. Reset non-NULL output on error. */
QJSIntlStatus qjs_intl_date_style_record_at(const QJSIntlDateStyleData *, uint32_t locale,
                                            uint32_t calendar, QJSIntlDateStyleFamily,
                                            QJSIntlDateStyleWidth, QJSIntlDateStyleField,
                                            uint32_t alternate, QJSIntlDateStyleRecord *);
#endif
