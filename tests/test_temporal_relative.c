/* Exact ISO calendar-relative duration arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include "../src/temporal/relative.h"

int main(void)
{
    QJSTemporalISODateTime start = { { 2024, 1, 1 }, { 0 } };
    QJSTemporalISODateTime end = { { 2024, 2, 1 }, { 0 } };
    QJSTemporalDifferenceSettings settings = {
        QJS_TEMPORAL_MONTH, QJS_TEMPORAL_DAY, QJS_TEMPORAL_ROUND_TRUNC, 1
    };
    QJSTemporalInternalDuration difference;
    double total;

    assert(!qjs_temporal_plain_datetime_difference_round(start, end,
        QJS_CAL_ISO8601, &settings, &difference));
    assert(difference.date.months == 1 && !difference.date.days);
    assert(!difference.time.low && !difference.time.high);
    assert(!qjs_temporal_plain_datetime_difference_total(start, end,
        QJS_CAL_ISO8601, QJS_TEMPORAL_MONTH, &total));
    assert(total == 1);
    assert(!qjs_temporal_plain_datetime_difference_total(start, end,
        QJS_CAL_ISO8601, QJS_TEMPORAL_DAY, &total));
    assert(total == 31);
    assert(!qjs_temporal_plain_datetime_difference_total(end, start,
        QJS_CAL_ISO8601, QJS_TEMPORAL_DAY, &total));
    assert(total == -31);
    end = start;
    end.time.hour = 12;
    settings.largest_unit = settings.smallest_unit = QJS_TEMPORAL_DAY;
    settings.rounding_mode = QJS_TEMPORAL_ROUND_HALF_EXPAND;
    assert(!qjs_temporal_plain_datetime_difference_round(start, end,
        QJS_CAL_ISO8601, &settings, &difference));
    assert(difference.date.days == 1 && !difference.time.low && !difference.time.high);
    assert(!qjs_temporal_plain_datetime_difference_round(end, start,
        QJS_CAL_ISO8601, &settings, &difference));
    assert(difference.date.days == -1 && !difference.time.low && !difference.time.high);
    /* Month rounding must retain already-extracted years. */
    start = (QJSTemporalISODateTime){ { 2020, 1, 1 }, { 0 } };
    end = (QJSTemporalISODateTime){ { 2021, 1, 20 }, { 0 } };
    settings.largest_unit = QJS_TEMPORAL_YEAR;
    settings.smallest_unit = QJS_TEMPORAL_MONTH;
    assert(!qjs_temporal_plain_datetime_difference_round(start, end,
        QJS_CAL_ISO8601, &settings, &difference));
    assert(difference.date.years == 1 && difference.date.months == 1);
    end = (QJSTemporalISODateTime){ { 2020, 1, 16 }, { 12, 0, 0, 0, 0, 0 } };
    assert(!qjs_temporal_plain_datetime_difference_total(start, end,
        QJS_CAL_ISO8601, QJS_TEMPORAL_MONTH, &total));
    assert(total == 0.5);
    return 0;
}
