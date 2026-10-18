/* Portable Temporal civil arithmetic tests.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include <string.h>
#include "../src/temporal/civil.h"

static void test_limits(void)
{
    QJSTemporalISODateTime value = {
        { -271821, 4, 19 }, { 0, 0, 0, 0, 0, 0 }
    };

    assert(qjs_temporal_iso_date_within_limits(value.date));
    assert(!qjs_temporal_iso_date_within_limits(
        (QJSTemporalISODate){ -271821, 4, 18 }));
    assert(!qjs_temporal_iso_datetime_within_limits(value));
    value.time.nanosecond = 1;
    assert(qjs_temporal_iso_datetime_within_limits(value));
    assert(qjs_temporal_iso_year_month_within_limits(
        (QJSTemporalISODate){ -271821, 4, 1 }));
    assert(!qjs_temporal_iso_year_month_within_limits(
        (QJSTemporalISODate){ -271821, 3, 31 }));
    value = (QJSTemporalISODateTime){
        { 275760, 9, 13 }, { 23, 59, 59, 999, 999, 999 }
    };
    assert(qjs_temporal_iso_datetime_within_limits(value));
    value.date.day++;
    assert(!qjs_temporal_iso_datetime_within_limits(value));
}

static void test_date_arithmetic(void)
{
    QJSTemporalISODate date, unchanged;
    QJSTemporalDateDuration duration;
    int32_t week_year;

    assert(qjs_iso_days_in_month(2000, 2) == 29);
    assert(qjs_iso_days_in_month(1900, 2) == 28);
    assert(qjs_iso_days_in_month(2000, 0) == 0);
    assert(qjs_iso_days_in_month(2000, 13) == 0);
    assert(qjs_temporal_iso_date_add(&date,
        (QJSTemporalISODate){ 2020, 2, 29 },
        (QJSTemporalDateDuration){ 1, 0, 0, 0 },
        QJS_TEMPORAL_OVERFLOW_CONSTRAIN) == 0);
    assert(date.year == 2021 && date.month == 2 && date.day == 28);
    unchanged = date;
    assert(qjs_temporal_iso_date_add(&date,
        (QJSTemporalISODate){ 2020, 2, 29 },
        (QJSTemporalDateDuration){ 1, 0, 0, 0 },
        QJS_TEMPORAL_OVERFLOW_REJECT) == -1);
    assert(memcmp(&date, &unchanged, sizeof(date)) == 0);
    assert(qjs_temporal_iso_add_days(&date, unchanged, INT64_MAX) == -1);
    assert(memcmp(&date, &unchanged, sizeof(date)) == 0);
    assert(qjs_temporal_iso_add_days(&date, unchanged, INT64_MIN) == -1);
    assert(memcmp(&date, &unchanged, sizeof(date)) == 0);

    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){ 2020, 1, 31 },
        (QJSTemporalISODate){ 2020, 2, 29 }, QJS_TEMPORAL_MONTH) == 0);
    assert(duration.months == 0 && duration.days == 29);
    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){ 2021, 1, 31 },
        (QJSTemporalISODate){ 2021, 2, 28 }, QJS_TEMPORAL_MONTH) == 0);
    assert(duration.months == 0 && duration.days == 28);
    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){ 2020, 2, 29 },
        (QJSTemporalISODate){ 2021, 2, 28 }, QJS_TEMPORAL_YEAR) == 0);
    assert(duration.years == 0 && duration.months == 11 && duration.days == 30);
    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){ 2020, 2, 29 },
        (QJSTemporalISODate){ 2020, 1, 31 }, QJS_TEMPORAL_MONTH) == 0);
    assert(duration.months == 0 && duration.days == -29);
    assert(qjs_temporal_iso_week_of_year(
        (QJSTemporalISODate){ 2021, 1, 1 }, &week_year) == 53);
    assert(week_year == 2020);
}

static void test_datetime_rounding(void)
{
    QJSTemporalISODateTime value = {
        { 2024, 1, 1 }, { 12, 0, 0, 0, 0, 0 }
    };
    QJSTemporalISODateTime rounded;

    assert(qjs_temporal_iso_datetime_round(&rounded, value,
        UINT64_C(86400000000000), QJS_TEMPORAL_ROUND_HALF_EVEN) == 0);
    assert(rounded.date.day == 1 && rounded.time.hour == 0);
    value.time.nanosecond = 1;
    assert(qjs_temporal_iso_datetime_round(&rounded, value,
        UINT64_C(86400000000000), QJS_TEMPORAL_ROUND_HALF_EVEN) == 0);
    assert(rounded.date.day == 2 && rounded.time.hour == 0);
}

int main(void)
{
    test_limits();
    test_date_arithmetic();
    test_datetime_rounding();
    return 0;
}
