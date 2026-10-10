/* Chinese/Dangi approximation candidate; see lunisolar.c and the handoff.
 * SPDX-License-Identifier: Unicode-3.0
 */
#ifndef QUICKJS_CALENDAR_LUNISOLAR_H
#define QUICKJS_CALENDAR_LUNISOLAR_H
#include "calendar.h"

/* These direct functions are review candidates. They do not authorize
 * advertising Chinese or Dangi as supported calendars. All failures leave
 * outputs unchanged; no fallback calendar or runtime ICU call is used. */
int qjs_calendar_lunisolar_from_epoch_day(QJSCalendarId calendar,
                                        int64_t epoch_day,
                                        QJSCalendarDate *result);
int qjs_calendar_lunisolar_from_epoch_day_for_intl(QJSCalendarId calendar,
                                                   int64_t epoch_day,
                                                   QJSCalendarDate *result);
int qjs_calendar_lunisolar_from_epoch_day_unbounded(QJSCalendarId calendar,
                                                   int64_t epoch_day,
                                                   QJSCalendarDate *result);
int qjs_calendar_lunisolar_to_epoch_day(QJSCalendarId calendar, int32_t year,
                                      int month, int day, int64_t *result);
/* Internal metadata/Temporal bracketing primitive. Validated calendar fields
 * and internal year bounds remain required; public epoch bounds are omitted. */
int qjs_calendar_lunisolar_to_epoch_day_unbounded(QJSCalendarId calendar,
                                      int32_t year, int month, int day,
                                      int64_t *result);
int qjs_calendar_lunisolar_month_info(QJSCalendarId calendar, int32_t year,
                                    int month, int *months, int *days,
                                    char month_code[5]);
int qjs_calendar_lunisolar_add_months(QJSCalendarId calendar, int32_t year,
                                    int month, int64_t delta,
                                    int32_t *result_year, int *result_month);

/* Astronomy-zone diagnostic, also used to review the exact ICU historical
 * transition semantics. This is not an IANA civil timezone provider. */
int qjs_calendar_lunisolar_zone_offset(QJSCalendarId calendar, double utc_ms,
                                     int32_t *result_ms);
#endif
