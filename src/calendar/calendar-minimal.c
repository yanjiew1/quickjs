/* No non-ISO calendar extensions; mandatory ISO remains in common core.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "calendar-backend.h"

int qjs_calendar_backend_is_supported(QJSCalendarId calendar)
{
    (void)calendar;
    return 0;
}

int qjs_calendar_backend_from_epoch_day(QJSCalendarId calendar, int64_t day,
                                        QJSCalendarDate *result)
{
    (void)calendar;
    (void)day;
    (void)result;
    return QJS_CAL_UNSUPPORTED;
}

int qjs_calendar_backend_to_epoch_day(QJSCalendarId calendar, int32_t year,
                                      int month, int day, int64_t *result)
{
    (void)calendar;
    (void)year;
    (void)month;
    (void)day;
    (void)result;
    return QJS_CAL_UNSUPPORTED;
}

int qjs_calendar_backend_month_info(QJSCalendarId calendar, int32_t year,
                                    int month, int *months, int *days,
                                    char month_code[5])
{
    (void)calendar;
    (void)year;
    (void)month;
    (void)months;
    (void)days;
    (void)month_code;
    return QJS_CAL_UNSUPPORTED;
}

int qjs_calendar_backend_year_from_era(QJSCalendarId calendar, const char *era,
                                       int32_t era_year, int32_t *result)
{
    (void)calendar;
    (void)era;
    (void)era_year;
    (void)result;
    return QJS_CAL_UNSUPPORTED;
}

int qjs_calendar_backend_add_months(QJSCalendarId calendar, int32_t year,
                                    int month, int64_t delta,
                                    int32_t *result_year, int *result_month)
{
    (void)calendar;
    (void)year;
    (void)month;
    (void)delta;
    (void)result_year;
    (void)result_month;
    return QJS_CAL_UNSUPPORTED;
}

int qjs_calendar_backend_month_ordinal(QJSCalendarId calendar, int32_t year,
                                       int nominal, int leap, int constrain,
                                       int *result)
{
    (void)calendar;
    (void)year;
    (void)nominal;
    (void)leap;
    (void)constrain;
    (void)result;
    return QJS_CAL_UNSUPPORTED;
}
