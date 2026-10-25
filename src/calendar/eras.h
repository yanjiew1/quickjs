/* Native calendar era identities; no locale or JS state.
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
#ifndef QUICKJS_CALENDAR_ERAS_H
#define QUICKJS_CALENDAR_ERAS_H
#include "types.h"
#include <stddef.h>

/* Enumerate only canonical codes that set_era can emit, including data-driven
   modern Japanese codes and their Gregorian historical fallback. */
int qjs_cal_era_at(QJSCalendarId calendar, size_t index, const char **out);

/* date already contains a checked arithmetic year; epoch_day is its date.
   Calendars without eras leave has_era zero and both era outputs empty. */
void qjs_cal_set_era(QJSCalendarId calendar, int64_t epoch_day,
                     QJSCalendarDate *date);
/* Resolve lenient out-of-era years before recomputing the resulting era.
   era is borrowed, case-sensitive and NUL-terminated. Preserve output on error. */
int qjs_cal_year_from_era(QJSCalendarId calendar, const char *era,
                         int32_t era_year, int32_t *result);
#endif /* QUICKJS_CALENDAR_ERAS_H */
