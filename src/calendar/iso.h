/*
 * Portable ISO Gregorian date arithmetic
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#ifndef QUICKJS_CALENDAR_ISO_H
#define QUICKJS_CALENDAR_ISO_H

#include <stdint.h>

typedef struct QJSISODate {
    int32_t year;
    int32_t month;
    int32_t day;
} QJSISODate;

int qjs_iso_date_is_valid(QJSISODate date);
int qjs_iso_leap_year(int32_t year);
/* Return zero for a month outside 1 through 12. */
int qjs_iso_days_in_month(int32_t year, int32_t month);

/* Gregorian conversion covers the complete int32_t year interval. Days are
   measured from 1970-01-01. Invalid inputs return -1 and preserve the required
   non-null output. Temporal's public date limits are a caller policy. */
int qjs_iso_date_to_days(int64_t *result, QJSISODate date);
int qjs_iso_date_from_days(QJSISODate *result, int64_t days);

#endif /* QUICKJS_CALENDAR_ISO_H */
