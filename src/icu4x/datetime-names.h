/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_ICU4X_DATETIME_NAMES_H
#define QJS_ICU4X_DATETIME_NAMES_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"

typedef struct QJSICU4XDateNames QJSICU4XDateNames;
typedef struct QJSICU4XDateText {
    const uint8_t *data;
    size_t length;
} QJSICU4XDateText;
typedef enum QJSICU4XDateNamesKind {
    QJS_ICU4X_DATE_ERAS,
    QJS_ICU4X_DATE_CYCLIC,
    QJS_ICU4X_DATE_MONTH_LINEAR,
    QJS_ICU4X_DATE_MONTH_LEAP,
    QJS_ICU4X_DATE_MONTH_NUMERIC_LEAP,
    QJS_ICU4X_DATE_MONTH_NUMERIC_BASE,
    QJS_ICU4X_DATE_MONTH_NUMERIC,
    QJS_ICU4X_DATE_WEEKDAYS,
    QJS_ICU4X_DATE_PERIODS
} QJSICU4XDateNamesKind;
typedef struct QJSICU4XDateNamesMeta {
    uint32_t kind;
    size_t count;
} QJSICU4XDateNamesMeta;
typedef struct QJSICU4XDateMonthPattern {
    struct {
        QJSICU4XDateText prefix, suffix;
        uint32_t has_value;
    } pattern;
    int32_t offset;
} QJSICU4XDateMonthPattern;

/* Source identity is selected by C overlays; root maps to und, no inherited
   public payload substitutes another source. Calendar mirrors QJSCalendarId.
   Families: 0 year/era, 1 month, 2 weekday, 3 fixed day periods. Width is LDML,
   not the different ICU marker-length encoding; standalone is a Boolean.
   Failure preserves outputs. All returned text borrows the retained handle. */
int qjs_icu4x_datetime_names_open(const uint8_t *, size_t, uint32_t calendar, uint32_t family,
                                  uint32_t width, uint32_t standalone, QJSICU4XDateNames **);
void qjs_icu4x_datetime_names_close(QJSICU4XDateNames *);
int qjs_icu4x_datetime_names_meta(const QJSICU4XDateNames *, QJSICU4XDateNamesMeta *);
/* Era indexes are ICU era indexes, cyclic indexes are year-1; month indexes
   are nominal code-1, weekdays start Sunday. Fixed periods: AM/PM/noon/midnight;
   empty text remains observable. Encoded patterns/rule headers are not labels. */
int qjs_icu4x_datetime_names_text(const QJSICU4XDateNames *, size_t index, QJSICU4XDateText *);
/* Phase: 0 ordinary, 1 leap, 2 base paired with leap. Decoded affixes retain
   literal braces. Numeric offsets apply to nominal months, never blindly to
   C ordinal months (notably Hebrew 6a/6b). A literal-only pattern has no value. */
int qjs_icu4x_datetime_month_pattern(const QJSICU4XDateNames *, uint32_t phase,
                                     QJSICU4XDateMonthPattern *);
int qjs_icu4x_datetime_check_version(const uint8_t *, size_t);
#endif
