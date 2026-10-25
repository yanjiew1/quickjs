/* Exact instants to calendar/clock fields, separate from localized rendering.
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
#ifndef QJS_INTL_DATETIME_LOCAL_TIME_H
#define QJS_INTL_DATETIME_LOCAL_TIME_H
#include "chronology.h"
#include "../../timezone/timezone.h"

typedef struct QJSIntlDateLocalTime {
    QJSIntlDateCalendarFields fields;
    int64_t epoch_day;
    int32_t offset_seconds;
    uint32_t nanosecond;
    unsigned int subsecond, is_dst;
} QJSIntlDateLocalTime;

/* Supply floor epoch seconds and a nonnegative nanosecond remainder < 1e9.
   E.g. -1 ns is seconds=-1, nanosecond=999999999. Input coercion, Date TimeClip
   and Temporal input-type/range restrictions remain the frontend's policy.
   A named handle borrows immutable timezone data and requires offset_minutes=0;
   NULL selects a fixed offset strictly between -1440 and +1440 minutes.

   Query the selected rule at the instant, apply its exact historical seconds,
   then derive the selected calendar and local clock from the same local day.
   Retain the full subsecond fact independently of displayed milliseconds;
   return the timezone rule's DST designation, not a localized name decision.
   Calendar era/name keys remain chronology's responsibility. Allocate nothing,
   retain nothing; require fresh disjoint output and reset it on every error. */
QJSIntlStatus qjs_intl_date_local_time(QJSCalendarId calendar, int64_t epoch_seconds,
                                      uint32_t nanosecond, const QJSTimeZone *named,
                                      int32_t offset_minutes, QJSIntlDateLocalTime *out);
#endif
