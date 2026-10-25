/* Shared lunisolar approximation; source provenance is in lunisolar.c.
 * SPDX-License-Identifier: Unicode-3.0
 */
#ifndef QUICKJS_CALENDAR_LUNISOLAR_H
#define QUICKJS_CALENDAR_LUNISOLAR_H
#include "types.h"

/* Checked model output, NOT a replacement for mandatory PMO/KASI records.
   Used to complete unpublished boundary metadata and for distant years.
   These calls allocate nothing and preserve outputs on failure. */
int qjs_cal_lunisolar_approximate_year(QJSCalendarId calendar, int32_t year,
                                      QJSCalendarYear *result);
/* Conventional astronomical location, separate from IANA civil timezones. */
int qjs_calendar_lunisolar_zone_offset(QJSCalendarId calendar, double utc_ms,
                                      int32_t *result_ms);
/* Search consecutive month indexes against the selected provider's checked
   year records. The mean phase is only a search seed; returned dates and month
   membership must be verified against that provider. This is not a replacement
   of its publication data by the approximation model. */
typedef int QJSLunisolarReadYear(QJSCalendarId calendar, int32_t year,
                                 QJSCalendarYearLayout *out);
int qjs_cal_lunisolar_add_months(QJSCalendarId calendar, int32_t year, int month,
                                 int64_t delta, QJSLunisolarReadYear *read_year,
                                 int32_t *out_year, int *out_month);
#endif /* QUICKJS_CALENDAR_LUNISOLAR_H */
