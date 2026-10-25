/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* Native Temporal calendar arithmetic; no JS types or allocations. */
#ifndef QUICKJS_TEMPORAL_CALENDAR_H
#define QUICKJS_TEMPORAL_CALENDAR_H
#include <stddef.h>
#include "types.h"

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
#define QJS_TEMPORAL_DATE_FIELDS \
    (QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH | \
     QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY)
#define QJS_TEMPORAL_TIME_FIELDS \
    (QJS_TEMPORAL_FIELD_HOUR | QJS_TEMPORAL_FIELD_MINUTE | \
     QJS_TEMPORAL_FIELD_SECOND | QJS_TEMPORAL_FIELD_MILLISECOND | \
     QJS_TEMPORAL_FIELD_MICROSECOND | QJS_TEMPORAL_FIELD_NANOSECOND)
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
/* Inputs are borrowed. Fallible operations leave their result unchanged.
   Only ISO is available in the minimal configuration. */
const char *qjs_temporal_calendar_identifier(QJSCalendarId calendar);
unsigned qjs_temporal_calendar_extra_fields(QJSCalendarId calendar,
                                            unsigned requested);
unsigned qjs_temporal_calendar_fields_to_ignore(QJSCalendarId calendar,
                                                unsigned additional);
int qjs_temporal_calendar_fields(QJSCalendarId calendar,
                                 QJSTemporalISODate date,
                                 QJSTemporalCalendarDate *result);
int qjs_temporal_calendar_date_from_fields(QJSCalendarId calendar,
                                           const QJSTemporalCalendarFields *fields,
                                           QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_year_month_from_fields(QJSCalendarId calendar,
                                                 const QJSTemporalCalendarFields *fields,
                                                 QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_month_day_from_fields(QJSCalendarId calendar,
                                                const QJSTemporalCalendarFields *fields,
                                                QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_date_add(QJSCalendarId calendar,
                                   QJSTemporalISODate date, QJSTemporalDateDuration duration,
                                   QJSTemporalOverflow overflow, QJSTemporalISODate *result);
int qjs_temporal_calendar_date_until(QJSCalendarId calendar,
                                     QJSTemporalISODate start, QJSTemporalISODate end,
                                     QJSTemporalUnit largest, QJSTemporalDateDuration *result);
#endif /* QUICKJS_TEMPORAL_CALENDAR_H */
