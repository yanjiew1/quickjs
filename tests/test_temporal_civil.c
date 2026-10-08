/* Civil arithmetic units; runtime gate is separate from source review. */
#include "../src/temporal/civil.h"
#include "../src/temporal/relative.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    QJSTemporalISODate date, end;
    QJSTemporalDateDuration duration;
    QJSTemporalISODateTime value, rounded;
    QJSTemporalInternalDuration difference;
    QJSTemporalDifferenceSettings settings = {
        QJS_TEMPORAL_YEAR, QJS_TEMPORAL_MONTH,
        QJS_TEMPORAL_ROUND_HALF_EXPAND, 1
    };
    int32_t week_year;
    double total;
    char text[32];
    assert(qjs_temporal_iso_date_within_limits(
                (QJSTemporalISODate){ -271821, 4, 19 }));
    assert(!qjs_temporal_iso_date_within_limits(
                (QJSTemporalISODate){ -271821, 4, 18 }));
    assert(qjs_temporal_iso_year_month_within_limits(
                (QJSTemporalISODate){ -271821, 4, 1 }));
    value = (QJSTemporalISODateTime){ { -271821, 4, 19 }, {0} };
    assert(!qjs_temporal_iso_datetime_within_limits(value));
    value.time.nanosecond = 1;
    assert(qjs_temporal_iso_datetime_within_limits(value));
    assert(qjs_temporal_iso_date_add(&date,
        (QJSTemporalISODate){ 2020, 2, 29 },
        (QJSTemporalDateDuration){1,0,0,0},
        QJS_TEMPORAL_OVERFLOW_CONSTRAIN) == 0);
    assert(date.year == 2021 && date.month == 2 && date.day == 28);
    end = date;
    assert(qjs_temporal_iso_date_add(&date,
        (QJSTemporalISODate){ 2020, 2, 29 },
        (QJSTemporalDateDuration){1,0,0,0},
        QJS_TEMPORAL_OVERFLOW_REJECT) == -1);
    assert(memcmp(&date, &end, sizeof(date)) == 0);
    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){2020,1,31},
        (QJSTemporalISODate){2020,2,29}, QJS_TEMPORAL_MONTH) == 0);
    assert(duration.months == 0 && duration.days == 29);
    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){2021,1,31},
        (QJSTemporalISODate){2021,2,28}, QJS_TEMPORAL_MONTH) == 0);
    assert(duration.months == 0 && duration.days == 28);
    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){2020,2,29},
        (QJSTemporalISODate){2021,2,28}, QJS_TEMPORAL_YEAR) == 0);
    assert(duration.years == 0 && duration.months == 11 && duration.days == 30);
    assert(qjs_temporal_iso_date_until(&duration,
        (QJSTemporalISODate){2020,2,29},
        (QJSTemporalISODate){2020,1,31}, QJS_TEMPORAL_MONTH) == 0);
    assert(duration.months == 0 && duration.days == -29);
    assert(qjs_temporal_iso_week_of_year(
        (QJSTemporalISODate){2021,1,1}, &week_year) == 53);
    assert(week_year == 2020);
    value = (QJSTemporalISODateTime){ {2024,1,1}, {12,0,0,0,0,0} };
    assert(qjs_temporal_iso_datetime_round(&rounded, value,
        UINT64_C(86400000000000), QJS_TEMPORAL_ROUND_HALF_EVEN) == 0);
    assert(rounded.date.day == 1 && rounded.time.hour == 0);
    /* Preserve a retained year when rounding a zero-month remainder. */
    value = (QJSTemporalISODateTime){ {2020,1,1}, {0} };
    rounded = (QJSTemporalISODateTime){ {2021,1,20}, {0} };
    assert(qjs_temporal_plain_datetime_difference_round(value, rounded,
        QJS_TEMPORAL_CAL_ISO8601, &settings, &difference) == 0);
    assert(difference.date.years == 1 && difference.date.months == 1);
    assert(qjs_temporal_plain_datetime_difference_total(value,
        (QJSTemporalISODateTime){ {2020,1,16}, {12,0,0,0,0,0} },
        QJS_TEMPORAL_CAL_ISO8601, QJS_TEMPORAL_MONTH, &total) == 0);
    assert(total == 0.5);
    assert(qjs_temporal_iso_date_format(text, sizeof(text),
        (QJSTemporalISODate){ -1,1,1 }) == 13);
    assert(strcmp(text, "-000001-01-01") == 0);
    return 0;
}
