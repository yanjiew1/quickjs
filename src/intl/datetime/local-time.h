/* Exact instants to calendar/clock fields, separate from localized rendering.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_LOCAL_TIME_H
#define QJS_INTL_DATETIME_LOCAL_TIME_H
#include "chronology.h"
#include "../../timezone/timezone.h"

typedef struct QJSIntlDateLocalTime {
    QJSIntlDateCalendarFields fields;
    int64_t epoch_day;
    int32_t offset_seconds;
    uint32_t nanosecond;
    unsigned int subsecond, is_dst;
} QJSIntlDateLocalTime;

/* Supply floor epoch seconds and a nonnegative nanosecond remainder < 1e9.
   E.g. -1 ns is seconds=-1, nanosecond=999999999. Input coercion, Date TimeClip
   and Temporal input-type/range restrictions remain the frontend's policy.
   A named handle borrows immutable timezone data and requires offset_minutes=0;
   NULL selects a fixed offset strictly between -1440 and +1440 minutes.

   Query the selected rule at the instant, apply its exact historical seconds,
   then derive the selected calendar and local clock from the same local day.
   Retain the full subsecond fact independently of displayed milliseconds;
   return the timezone rule's DST designation, not a localized name decision.
   Calendar era/name keys remain chronology's responsibility. Allocate nothing,
   retain nothing; require fresh disjoint output and reset it on every error. */
QJSIntlStatus qjs_intl_date_local_time(QJSCalendarId calendar, int64_t epoch_seconds,
                                      uint32_t nanosecond, const QJSTimeZone *named,
                                      int32_t offset_minutes, QJSIntlDateLocalTime *out);
#endif
