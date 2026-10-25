/* Native backend state shared only by its conversion and month-arithmetic TUs.
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
#ifndef QUICKJS_CALENDAR_NATIVE_INTERNAL_H
#define QUICKJS_CALENDAR_NATIVE_INTERNAL_H
#include "calendar-backend.h"

typedef struct QJSNativeCalendarYear {
    int64_t first_day, end_day;
    unsigned char lengths[13];
    int months, leap_month;
} QJSNativeCalendarYear;

int qjs_cal_native_year(QJSCalendarId calendar, int32_t year,
                        QJSNativeCalendarYear *result);
int qjs_cal_native_lunar(QJSCalendarId calendar);
int64_t qjs_cal_native_month_start(const QJSNativeCalendarYear *year, int month);
#endif /* QUICKJS_CALENDAR_NATIVE_INTERNAL_H */
