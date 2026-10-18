/* Checked primitives for the native calendar arithmetic families.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_CALENDAR_ARITHMETIC_H
#define QUICKJS_CALENDAR_ARITHMETIC_H
#include <stdint.h>
#include "types.h"

#define QJS_CAL_MIN_YEAR (-1000000)
#define QJS_CAL_MAX_YEAR 1000000

enum {
    QJS_CAL_OK = 0,
    QJS_CAL_RANGE = -1,
    QJS_CAL_UNSUPPORTED = -3,
    QJS_CAL_BACKEND = -5,
};

/* Epoch day zero is 1970-01-01. Arithmetic years are deliberately bounded
   before adding epoch/era adjustments. Year starts also admit the immediately
   adjacent years for bracketing. Fallible calls preserve outputs. */
int qjs_cal_solar_year_start(QJSCalendarId calendar, int32_t year, int64_t *result);
/* Return zero for invalid/unsupported inputs, including out-of-range years. */
int qjs_cal_solar_month_length(QJSCalendarId calendar, int32_t year, int month);
#endif /* QUICKJS_CALENDAR_ARITHMETIC_H */
