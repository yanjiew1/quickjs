/* Checked primitives for the native calendar arithmetic families.
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
#ifndef QUICKJS_CALENDAR_ARITHMETIC_H
#define QUICKJS_CALENDAR_ARITHMETIC_H
#include <stdint.h>
#include "types.h"

/* Epoch day zero is 1970-01-01. Arithmetic years are deliberately bounded
   before adding epoch/era adjustments. Year starts also admit the immediately
   adjacent years for bracketing. Fallible calls preserve outputs. */
int qjs_cal_solar_year_start(QJSCalendarId calendar, int32_t year, int64_t *result);
/* Return zero for invalid/unsupported inputs, including out-of-range years. */
int qjs_cal_solar_month_length(QJSCalendarId calendar, int32_t year, int month);

int qjs_cal_hebrew_year_start(int32_t year, int64_t *result);
int qjs_cal_hebrew_month_length(int32_t year, int month);
/* These wide-arithmetic helpers accept the complete int32_t year interval. */
int qjs_cal_hebrew_leap(int32_t year);
int64_t qjs_cal_hebrew_months_before_year(int32_t year);

int qjs_cal_islamic_year_start(QJSCalendarId calendar, int32_t year, int64_t *result);
int qjs_cal_islamic_month_length(QJSCalendarId calendar, int32_t year, int month);

int qjs_cal_persian_year_start(int32_t year, int64_t *result);
int qjs_cal_persian_month_length(int32_t year, int month);
#endif /* QUICKJS_CALENDAR_ARITHMETIC_H */
