/* Checked lunisolar model and month-index interfaces.
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
#ifndef QUICKJS_CALENDAR_LUNISOLAR_H
#define QUICKJS_CALENDAR_LUNISOLAR_H
#include "types.h"

/* Checked model output, NOT a replacement for mandatory PMO/KASI records.
   Used to complete unpublished boundary metadata and for distant years.
   These calls allocate nothing and preserve outputs on failure. */
int qjs_cal_lunisolar_approximate_year(QJSCalendarId calendar, int32_t year,
                                      QJSCalendarYear *result);
/* Conventional astronomical location, separate from IANA civil timezones. */
int qjs_calendar_lunisolar_zone_offset(QJSCalendarId calendar, double utc_ms,
                                      int32_t *result_ms);
/* Search consecutive month indexes against the selected provider's checked
   year records. The mean phase is only a search seed; returned dates and month
   membership must be verified against that provider. This is not a replacement
   of its publication data by the approximation model. */
int qjs_cal_lunisolar_add_months(QJSCalendarId calendar, int32_t year, int month,
                                 int64_t delta, QJSCalendarReadYear *read_year,
                                 int32_t *out_year, int *out_month);
#endif /* QUICKJS_CALENDAR_LUNISOLAR_H */
