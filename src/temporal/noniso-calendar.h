/* Temporal's consumer policy for non-ISO calendar arithmetic.
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
#ifndef QUICKJS_TEMPORAL_NONISO_CALENDAR_H
#define QUICKJS_TEMPORAL_NONISO_CALENDAR_H
#include "calendar.h"

int qjs_temporal_noniso_has_era(QJSCalendarId calendar);
/* type is 0 for a date or 1 for a year-month. Inputs are already coerced;
   preserve the output on error. Final Temporal limits belong to the caller. */
int qjs_temporal_noniso_resolve(QJSCalendarId calendar,
                                const QJSTemporalCalendarFields *input, int type,
                                QJSTemporalOverflow overflow,
                                QJSTemporalISODate *result);
int qjs_temporal_noniso_fields(QJSCalendarId calendar, QJSTemporalISODate date,
                               QJSTemporalCalendarDate *result);
int qjs_temporal_noniso_month_day(QJSCalendarId calendar,
                                  const QJSTemporalCalendarFields *input,
                                  QJSTemporalOverflow overflow,
                                  QJSTemporalISODate *result);
int qjs_temporal_noniso_date_add(QJSCalendarId calendar, QJSTemporalISODate date,
                                 QJSTemporalDateDuration duration,
                                 QJSTemporalOverflow overflow,
                                 QJSTemporalISODate *result);
int qjs_temporal_noniso_date_until(QJSCalendarId calendar,
                                   QJSTemporalISODate start,
                                   QJSTemporalISODate end,
                                   QJSTemporalUnit largest,
                                   QJSTemporalDateDuration *result);
#endif /* QUICKJS_TEMPORAL_NONISO_CALENDAR_H */
