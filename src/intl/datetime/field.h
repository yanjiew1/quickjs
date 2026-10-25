/* ECMA-402 date-field identities, independent of backend data.
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
#ifndef QJS_INTL_DATETIME_FIELD_H
#define QJS_INTL_DATETIME_FIELD_H
#include "../common/types.h"

typedef enum QJSIntlDateField {
    QJS_INTL_DATE_ERA,
    QJS_INTL_DATE_YEAR,
    QJS_INTL_DATE_QUARTER,
    QJS_INTL_DATE_MONTH,
    QJS_INTL_DATE_WEEK_OF_YEAR,
    QJS_INTL_DATE_WEEKDAY,
    QJS_INTL_DATE_DAY,
    QJS_INTL_DATE_DAY_PERIOD,
    QJS_INTL_DATE_HOUR,
    QJS_INTL_DATE_MINUTE,
    QJS_INTL_DATE_SECOND,
    QJS_INTL_DATE_TIME_ZONE_NAME,
    QJS_INTL_DATE_FIELD_COUNT
} QJSIntlDateField;

/* Exact IsValidDateTimeFieldCode spelling, without case folding. Allocate
   nothing; unknown/malformed input returns INVALID_ARGUMENT. Reset output. */
QJSIntlStatus qjs_intl_date_field_from_code(QJSIntlBytes, QJSIntlDateField *);
#endif
