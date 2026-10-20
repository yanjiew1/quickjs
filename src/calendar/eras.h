/* Native calendar era identities; no locale or JS state.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_CALENDAR_ERAS_H
#define QUICKJS_CALENDAR_ERAS_H
#include "types.h"
#include <stddef.h>

/* Enumerate only canonical codes that set_era can emit, including data-driven
   modern Japanese codes and their Gregorian historical fallback. */
int qjs_cal_era_at(QJSCalendarId calendar, size_t index, const char **out);

/* date already contains a checked arithmetic year; epoch_day is its date.
   Calendars without eras leave has_era zero and both era outputs empty. */
void qjs_cal_set_era(QJSCalendarId calendar, int64_t epoch_day,
                     QJSCalendarDate *date);
/* Resolve lenient out-of-era years before recomputing the resulting era.
   era is borrowed, case-sensitive and NUL-terminated. Preserve output on error. */
int qjs_cal_year_from_era(QJSCalendarId calendar, const char *era,
                         int32_t era_year, int32_t *result);
#endif /* QUICKJS_CALENDAR_ERAS_H */
