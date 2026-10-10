/* Portable proleptic calendar arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 * No JS values, locale data, timezone state, allocation, or ICU dependency.
 */
#ifndef QUICKJS_PORTABLE_CALENDAR_H
#define QUICKJS_PORTABLE_CALENDAR_H
#include <stddef.h>
#include <stdint.h>

/* Values deliberately match QJSTemporalCalendar. */
typedef enum QJSCalendarId {
    QJS_CAL_ISO8601, QJS_CAL_BUDDHIST, QJS_CAL_CHINESE, QJS_CAL_COPTIC,
    QJS_CAL_DANGI, QJS_CAL_ETHIOAA, QJS_CAL_ETHIOPIC, QJS_CAL_GREGORY,
    QJS_CAL_HEBREW, QJS_CAL_INDIAN, QJS_CAL_ISLAMIC_CIVIL,
    QJS_CAL_ISLAMIC_TBLA, QJS_CAL_ISLAMIC_UMALQURA, QJS_CAL_JAPANESE,
    QJS_CAL_PERSIAN, QJS_CAL_ROC, QJS_CAL_COUNT
} QJSCalendarId;
enum {
    QJS_CAL_OK = 0, QJS_CAL_RANGE = -1, QJS_CAL_UNSUPPORTED = -3,
    QJS_CAL_BACKEND = -5
};
#define QJS_CAL_MIN_EPOCH_DAY (-INT64_C(100000001))
#define QJS_CAL_MAX_EPOCH_DAY INT64_C(100000000)
/* Intl civil/reference values and their local timezone equality views.
 * Checked rendering only; Temporal construction/arithmetic limits stay above. */
#define QJS_CAL_INTL_MIN_EPOCH_DAY (-INT64_C(100000033))
#define QJS_CAL_INTL_MAX_EPOCH_DAY INT64_C(100000033)
#define QJS_CAL_MIN_YEAR (-1000000)
#define QJS_CAL_MAX_YEAR 1000000

typedef struct QJSCalendarDate {
    int32_t year, month, day, era_year;
    char month_code[5], era[32];
    int32_t day_of_year, days_in_month, days_in_year, months_in_year;
    int has_era, in_leap_year;
} QJSCalendarDate;

/* All failures leave caller outputs unchanged. Epoch day 0 is 1970-01-01.
 * year is the proposal's arithmetic year and month is a 1-based ordinal.
 * These primitives reject invalid dates: overflow regulation belongs to the
 * Temporal/Intl consumer. Month-info accepts a year even if that whole year
 * lies outside the epoch-day domain, so edge-date metadata stays available.
 */
const char *qjs_calendar_identifier(QJSCalendarId calendar);
int qjs_calendar_from_identifier(QJSCalendarId *result,
                                const char *text, size_t length);
/* Arithmetic availability is distinct from an Intl supportedValuesOf list.
 * A frontend must gate advertisement on its complete, validated service. */
int qjs_calendar_is_supported(QJSCalendarId calendar);
int qjs_calendar_from_epoch_day(QJSCalendarId calendar, int64_t epoch_day,
                                QJSCalendarDate *result);
/* The same checked conversion/arithmetic, with a bounded Intl rendering
 * domain covering complete PlainYearMonth reference months and <24h zone
 * adjustment. No calendar advertisement or Temporal range-policy change.
 */
int qjs_calendar_from_epoch_day_for_intl(QJSCalendarId calendar,
                                         int64_t epoch_day,
                                         QJSCalendarDate *result);
int qjs_calendar_to_epoch_day(QJSCalendarId calendar, int32_t year,
                             int month, int day, int64_t *result);
int qjs_calendar_month_info(QJSCalendarId calendar, int32_t year, int month,
                           int *months, int *days, char month_code[5]);
/* constrain is 0 for reject, 1 to map a missing leap month to its proposal
 * fallback: Hebrew M05L -> M06; Chinese/Dangi MxxL -> Mxx. */
int qjs_calendar_month_ordinal(QJSCalendarId calendar, int32_t year,
                              const char *month_code, int constrain,
                              int *result);
int qjs_calendar_add_months(QJSCalendarId calendar, int32_t year, int month,
                           int64_t delta, int32_t *result_year,
                           int *result_month);
#endif
