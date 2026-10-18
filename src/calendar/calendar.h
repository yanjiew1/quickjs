/* Shared calendar facade; pure C, no JS values, locales or timezone state.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_CALENDAR_H
#define QUICKJS_CALENDAR_H
#include <stddef.h>
#include "types.h"

/* Core owns ISO; a build-selected backend supplies non-ISO extensions.
   Identifier returns known names even when unavailable. Returned names have
   static lifetime. From-identifier checks availability, accepts ASCII case
   folding and only the specified aliases; its text uses an explicit length.
   Other input strings are borrowed and NUL-terminated. Era names are
   case-sensitive; output month_code and era arrays are NUL-terminated. */
const char *qjs_calendar_identifier(QJSCalendarId calendar);
int qjs_calendar_is_supported(QJSCalendarId calendar);
int qjs_calendar_from_identifier(QJSCalendarId *result, const char *text,
                                 size_t length);
/* Enumerate canonical era codes that this arithmetic can emit, not localized
   labels or every accepted lenient input alias. Zero-based indexes; return
   UNSUPPORTED for unavailable calendars, no eras, or the end. ISO has no eras.
   Borrow immutable data for process lifetime; preserve output on failure. */
int qjs_calendar_era_at(QJSCalendarId calendar, size_t index, const char **out);

/* Arithmetic years include zero and are bounded by QJS_CAL_{MIN,MAX}_YEAR.
   Epoch day zero is 1970-01-01. Temporal/Date limits are consumer policy,
   deliberately absent here so edge-date metadata can bracket those limits.
   Required pointers are non-null; fallible calls preserve all outputs. */
int qjs_calendar_from_epoch_day(QJSCalendarId calendar, int64_t epoch_day,
                                QJSCalendarDate *result);
int qjs_calendar_to_epoch_day(QJSCalendarId calendar, int32_t year,
                              int month, int day, int64_t *result);
int qjs_calendar_month_info(QJSCalendarId calendar, int32_t year, int month,
                            int *months, int *days, char month_code[5]);
int qjs_calendar_year_from_era(QJSCalendarId calendar, const char *era,
                               int32_t era_year, int32_t *result);
/* Constrain a missing Hebrew M05L to M06, and a missing lunisolar MxxL to Mxx.
   Other missing or invalid month codes are rejected. */
int qjs_calendar_month_ordinal(QJSCalendarId calendar, int32_t year,
                               const char *month_code, int constrain, int *result);
int qjs_calendar_add_months(QJSCalendarId calendar, int32_t year, int month,
                            int64_t delta, int32_t *result_year, int *result_month);
#endif /* QUICKJS_CALENDAR_H */
