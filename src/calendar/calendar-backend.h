/* Direct build-selected non-ISO calendar extension contract.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_CALENDAR_BACKEND_H
#define QUICKJS_CALENDAR_BACKEND_H
#include "calendar.h"

/* ISO never reaches these operations. Backends follow the facade's checked
   arithmetic-year, borrowing and failure-output contracts. No runtime vtable,
   backend loading or replacement is required; one backend may span many TUs. */
int qjs_calendar_backend_is_supported(QJSCalendarId calendar);
int qjs_calendar_backend_from_epoch_day(QJSCalendarId calendar, int64_t day,
                                        QJSCalendarDate *result);
int qjs_calendar_backend_to_epoch_day(QJSCalendarId calendar, int32_t year,
                                      int month, int day, int64_t *result);
int qjs_calendar_backend_month_info(QJSCalendarId calendar, int32_t year,
                                    int month, int *months, int *days,
                                    char month_code[5]);
int qjs_calendar_backend_year_from_era(QJSCalendarId calendar, const char *era,
                                       int32_t era_year, int32_t *result);
/* Spelling is validated: nominal is 1..13 (1..12 for leap), leap/constrain
   are Boolean. Reject unsupported combinations for this calendar/year before
   indexing metadata. Resolve with at most one year construction. */
int qjs_calendar_backend_month_ordinal(QJSCalendarId calendar, int32_t year,
                                       int nominal, int leap, int constrain,
                                       int *result);
int qjs_calendar_backend_add_months(QJSCalendarId calendar, int32_t year,
                                    int month, int64_t delta,
                                    int32_t *result_year, int *result_month);
#endif /* QUICKJS_CALENDAR_BACKEND_H */
