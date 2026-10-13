/* Native Temporal calendar arithmetic; no JS types or allocations. */
#ifndef QUICKJS_TEMPORAL_CALENDAR_H
#define QUICKJS_TEMPORAL_CALENDAR_H
#include <stddef.h>
#include "types.h"
typedef enum QJSTemporalError {
    QJS_TEMPORAL_OK = 0,
    QJS_TEMPORAL_ERROR_RANGE = -1,
    QJS_TEMPORAL_ERROR_MISSING = -2,
    QJS_TEMPORAL_ERROR_UNSUPPORTED = -3,
    QJS_TEMPORAL_ERROR_MEMORY = -4,
    QJS_TEMPORAL_ERROR_BACKEND = -5,
} QJSTemporalError;
#define QJS_TEMPORAL_FIELD_DAY (1U << 0)
#define QJS_TEMPORAL_FIELD_ERA (1U << 1)
#define QJS_TEMPORAL_FIELD_ERA_YEAR (1U << 2)
#define QJS_TEMPORAL_FIELD_HOUR (1U << 3)
#define QJS_TEMPORAL_FIELD_MICROSECOND (1U << 4)
#define QJS_TEMPORAL_FIELD_MILLISECOND (1U << 5)
#define QJS_TEMPORAL_FIELD_MINUTE (1U << 6)
#define QJS_TEMPORAL_FIELD_MONTH (1U << 7)
#define QJS_TEMPORAL_FIELD_MONTH_CODE (1U << 8)
#define QJS_TEMPORAL_FIELD_NANOSECOND (1U << 9)
#define QJS_TEMPORAL_FIELD_OFFSET (1U << 10)
#define QJS_TEMPORAL_FIELD_SECOND (1U << 11)
#define QJS_TEMPORAL_FIELD_TIME_ZONE (1U << 12)
#define QJS_TEMPORAL_FIELD_YEAR (1U << 13)
#define QJS_TEMPORAL_DATE_FIELDS (QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH | QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY)
#define QJS_TEMPORAL_TIME_FIELDS (QJS_TEMPORAL_FIELD_HOUR | QJS_TEMPORAL_FIELD_MINUTE | QJS_TEMPORAL_FIELD_SECOND | QJS_TEMPORAL_FIELD_MILLISECOND | QJS_TEMPORAL_FIELD_MICROSECOND | QJS_TEMPORAL_FIELD_NANOSECOND)
/* Raw integral Numbers retain their entire finite magnitude until resolution.
   Narrowing during Get would throw before later observable field accesses. */
typedef struct QJSTemporalCalendarFields {
    double year, month, day, era_year;
    char month_code[5];
    char era[32];
    unsigned present;
} QJSTemporalCalendarFields;
typedef struct QJSTemporalCalendarDate {
    int32_t year, month, day, era_year;
    char month_code[5];
    char era[32];
    int32_t day_of_week, day_of_year, week_of_year, year_of_week;
    int32_t days_in_week, days_in_month, days_in_year, months_in_year;
    int has_era, has_week, in_leap_year;
} QJSTemporalCalendarDate;
const char *qjs_temporal_calendar_identifier(QJSTemporalCalendar calendar);
int qjs_temporal_calendar_from_identifier(QJSTemporalCalendar *result,
                                        const char *text, size_t length);
unsigned qjs_temporal_calendar_extra_fields(QJSTemporalCalendar calendar,
                                            unsigned requested);
unsigned qjs_temporal_calendar_fields_to_ignore(QJSTemporalCalendar calendar,
                                                unsigned additional);
int qjs_temporal_calendar_fields(QJSTemporalCalendar calendar,
                                QJSTemporalISODate date,
                                QJSTemporalCalendarDate *result);
int qjs_temporal_calendar_date_from_fields(QJSTemporalCalendar calendar,
                    const QJSTemporalCalendarFields *fields,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_year_month_from_fields(QJSTemporalCalendar calendar,
                    const QJSTemporalCalendarFields *fields,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_month_day_from_fields(QJSTemporalCalendar calendar,
                    const QJSTemporalCalendarFields *fields,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_date_add(QJSTemporalCalendar calendar,
                    QJSTemporalISODate date, QJSTemporalDateDuration duration,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_date_until(QJSTemporalCalendar calendar,
                    QJSTemporalISODate start, QJSTemporalISODate end,
                    QJSTemporalUnit largest, QJSTemporalDateDuration *result);
#endif
