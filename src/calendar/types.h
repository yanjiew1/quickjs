/* Calendar identities shared by independent calendar and Temporal libraries.
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
#ifndef QUICKJS_CALENDAR_TYPES_H
#define QUICKJS_CALENDAR_TYPES_H
#include <stdint.h>

typedef enum QJSCalendarId {
    QJS_CAL_ISO8601,
    QJS_CAL_BUDDHIST,
    QJS_CAL_CHINESE,
    QJS_CAL_COPTIC,
    QJS_CAL_DANGI,
    QJS_CAL_ETHIOAA,
    QJS_CAL_ETHIOPIC,
    QJS_CAL_GREGORY,
    QJS_CAL_HEBREW,
    QJS_CAL_INDIAN,
    QJS_CAL_ISLAMIC_CIVIL,
    QJS_CAL_ISLAMIC_TBLA,
    QJS_CAL_ISLAMIC_UMALQURA,
    QJS_CAL_JAPANESE,
    QJS_CAL_PERSIAN,
    QJS_CAL_ROC,
    QJS_CAL_COUNT,
} QJSCalendarId;

#define QJS_CAL_MIN_YEAR (-1000000)
#define QJS_CAL_MAX_YEAR 1000000

enum {
    QJS_CAL_OK = 0,
    QJS_CAL_RANGE = -1,
    QJS_CAL_UNSUPPORTED = -3,
    QJS_CAL_MEMORY = -4,
    QJS_CAL_BACKEND = -5,
};

typedef struct QJSCalendarDate {
    int32_t year, month, day, era_year;
    char month_code[5], era[32];
    int32_t day_of_year, days_in_month, days_in_year, months_in_year;
    int has_era, in_leap_year;
} QJSCalendarDate;

#endif /* QUICKJS_CALENDAR_TYPES_H */
