/* Private ICU4X calendar bridge.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#ifndef QJS_ICU4X_CALENDAR_H
#define QJS_ICU4X_CALENDAR_H
#include "../calendar/types.h"
#include "bridge.h"
#include <stddef.h>

typedef struct QJSICU4XCalendarDate {
    int32_t year, month, day;
    int32_t day_of_year, days_in_month, days_in_year, months_in_year, in_leap_year;
    char month_code[5];
} QJSICU4XCalendarDate;

/* IDs follow QJSCalendarId; ISO stays in the C core. Results have no foreign
   layout or handle ownership. Calls preserve outputs on error, including panic.
   Required outputs must be aligned writable objects, disjoint from inputs.
   Metadata years are limited to +/-900000, safely bracketing all Temporal
   calendar years without bypassing ICU4X's fundamental date bounds. */
int qjs_icu4x_calendar_year(uint32_t id, int32_t year, QJSCalendarYearLayout *out);
int qjs_icu4x_calendar_date(uint32_t id, int64_t day, QJSICU4XCalendarDate *out);
typedef struct QJSICU4XCalendarYearMonth {
    int32_t year, month;
} QJSICU4XCalendarYearMonth;
int qjs_icu4x_calendar_add_months(uint32_t id, int32_t year, int32_t month,
                                  int64_t delta, QJSICU4XCalendarYearMonth *out);
/* Borrow immutable era names until process exit. ICU4X owns its compiled era
   data; the bridge retains copied NUL-terminated names after first use. */
int qjs_icu4x_japanese_era_count(size_t *out);
int qjs_icu4x_japanese_era_at(size_t index, QJSCalendarEra *out);
#endif
