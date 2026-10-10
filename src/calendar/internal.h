/* Internal integer arithmetic; positive divisors only. */
#ifndef QUICKJS_PORTABLE_CALENDAR_INTERNAL_H
#define QUICKJS_PORTABLE_CALENDAR_INTERNAL_H
#include "calendar.h"
static inline int64_t qjs_cal_floor_div(int64_t a, int64_t b)
{
    return a / b - (a % b < 0);
}
static inline int64_t qjs_cal_floor_mod(int64_t a, int64_t b)
{
    int64_t r = a % b;
    return r < 0 ? r + b : r;
}
static inline int qjs_cal_gregorian_leap(int32_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
/* Internal dates can lie just beyond the public domain to bracket edge years.
 * Callers supply validated small years/month/day. */
int64_t qjs_calendar_gregorian_to_epoch_day_unchecked(int32_t year,
                                                    int month, int day);
int32_t qjs_calendar_gregorian_year_from_epoch_day(int64_t epoch_day);
/* Temporal may bracket a valid edge date with its month/year start just
 * beyond the public epoch domain. This validates fields/year but omits the
 * final epoch-domain check; consumers must check their final result. */
int qjs_cal_to_epoch_day_unbounded(QJSCalendarId calendar, int32_t year,
                                  int month, int day, int64_t *result);
int64_t qjs_cal_solar_year_start(QJSCalendarId calendar, int32_t year);
int qjs_cal_solar_month_length(QJSCalendarId calendar, int32_t year, int month);
int64_t qjs_cal_hebrew_year_start(int32_t year);
int qjs_cal_hebrew_month_length(int32_t year, int month);
int qjs_cal_hebrew_leap(int32_t year);
int64_t qjs_cal_hebrew_months_before_year(int32_t year);
int64_t qjs_cal_islamic_year_start(QJSCalendarId calendar, int32_t year);
int qjs_cal_islamic_month_length(QJSCalendarId calendar, int32_t year,
                                int month);
#endif
