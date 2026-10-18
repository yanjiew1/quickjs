/* Native backend state shared only by its conversion and month-arithmetic TUs.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
