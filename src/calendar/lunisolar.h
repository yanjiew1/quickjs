/* Native lunisolar approximation; source provenance is in lunisolar.c.
 * SPDX-License-Identifier: Unicode-3.0
 */
#ifndef QUICKJS_CALENDAR_LUNISOLAR_H
#define QUICKJS_CALENDAR_LUNISOLAR_H
#include "arithmetic.h"
#include "data.h"

/* Checked model output, NOT a replacement for mandatory PMO/KASI records.
   Used to complete unpublished boundary metadata and for distant years.
   These calls allocate nothing and preserve outputs on failure. */
int qjs_cal_lunisolar_approximate_year(QJSCalendarId calendar, int32_t year,
                                      QJSCalendarYear *result);
/* Conventional astronomical location, separate from IANA civil timezones. */
int qjs_calendar_lunisolar_zone_offset(QJSCalendarId calendar, double utc_ms,
                                      int32_t *result_ms);
#endif /* QUICKJS_CALENDAR_LUNISOLAR_H */
