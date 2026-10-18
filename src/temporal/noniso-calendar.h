/* Temporal's consumer policy for non-ISO calendar arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_TEMPORAL_NONISO_CALENDAR_H
#define QUICKJS_TEMPORAL_NONISO_CALENDAR_H
#include "calendar.h"

int qjs_temporal_noniso_has_era(QJSCalendarId calendar);
/* type is 0 for a date or 1 for a year-month. Inputs are already coerced;
   preserve the output on error. Final Temporal limits belong to the caller. */
int qjs_temporal_noniso_resolve(QJSCalendarId calendar,
                                const QJSTemporalCalendarFields *input, int type,
                                QJSTemporalOverflow overflow,
                                QJSTemporalISODate *result);
int qjs_temporal_noniso_fields(QJSCalendarId calendar, QJSTemporalISODate date,
                               QJSTemporalCalendarDate *result);
#endif /* QUICKJS_TEMPORAL_NONISO_CALENDAR_H */
