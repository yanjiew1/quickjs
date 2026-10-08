/*
 * Portable Temporal duration word arithmetic tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <math.h>
#include <string.h>
#include "../src/temporal/duration.h"
#include "../src/temporal/time.h"

static void test_exact_conversion(void)
{
    QJSTemporalEpochNs value, saved = { 123, 456 }, denominator;

    value = saved;
    assert(qjs_temporal_integer_from_double(&value, 1.5) == -1);
    assert(value.low == saved.low && value.high == saved.high);
    assert(qjs_temporal_integer_from_double(&value, NAN) == -1);
    assert(value.low == saved.low && value.high == saved.high);
    assert(qjs_temporal_integer_from_double(&value, 0x1p64) == 0);
    assert(value.low == 0 && value.high == 1);
    assert(qjs_temporal_integer_from_double(&value, -0x1p64) == 0);
    assert(value.low == 0 && value.high == UINT64_MAX);
    assert(qjs_temporal_integer_from_double(&value, -0x1p127) == 0);
    assert(value.low == 0 && value.high == (UINT64_C(1) << 63));
    assert(qjs_temporal_ratio_to_double(value,
        qjs_temporal_epoch_ns_from_int64(1)) == -0x1p127);
    denominator = (QJSTemporalEpochNs){ 0, UINT64_C(1) << 62 };
    value = qjs_temporal_epoch_ns_from_int64(1);
    assert(qjs_temporal_ratio_to_double(value, denominator) == 0x1p-126);
    denominator = qjs_temporal_epoch_ns_from_int64(1);
    value = qjs_temporal_epoch_ns_from_int64(INT64_C(9007199254740993));
    assert(qjs_temporal_ratio_to_double(value, denominator) == 0x1p53);
    value = qjs_temporal_epoch_ns_from_int64(INT64_C(9007199254740995));
    assert(qjs_temporal_ratio_to_double(value, denominator) == 9007199254740996.0);
    denominator = qjs_temporal_epoch_ns_from_int64(1000);
    value = qjs_temporal_epoch_ns_from_int64(INT64_C(9007199254740993));
    assert(qjs_temporal_ratio_to_double(value, denominator) == 9007199254740.993);
    value = qjs_temporal_epoch_ns_from_int64(1);
    denominator = qjs_temporal_epoch_ns_from_int64(INT64_C(86400000000000));
    assert(qjs_temporal_ratio_to_double(value, denominator) == 1.0 / 86400000000000.0);
}

static void test_bounds_and_balance(void)
{
    QJSTemporalDuration duration = { { 0 } }, balanced;
    QJSTemporalInternalDuration value;
    QJSTemporalEpochNs limit = { UINT64_C(0x4000000000000000),
                                UINT64_C(0x77359) }, maximum;

    assert(!qjs_temporal_time_duration_is_valid(limit));
    assert(qjs_temporal_epoch_ns_subtract(&maximum, limit,
        qjs_temporal_epoch_ns_from_int64(1)) == 0);
    assert(qjs_temporal_time_duration_is_valid(maximum));
    value.date = (QJSTemporalDateDuration){ 0, 0, 0, 0 };
    value.time = maximum;
    assert(qjs_temporal_duration_from_internal(&balanced, value,
                                               QJS_TEMPORAL_SECOND) == 0);
    assert(balanced.fields[QJS_TEMPORAL_SECOND] == 9007199254740991.0);
    assert(balanced.fields[QJS_TEMPORAL_MILLISECOND] == 999);
    assert(balanced.fields[QJS_TEMPORAL_MICROSECOND] == 999);
    assert(balanced.fields[QJS_TEMPORAL_NANOSECOND] == 999);
    /* A lossy largestUnit conversion can round outside the open bound. */
    assert(qjs_temporal_duration_from_internal(&balanced, value,
                                               QJS_TEMPORAL_NANOSECOND) == -1);
    duration.fields[QJS_TEMPORAL_NANOSECOND] = 0x1p64;
    assert(qjs_temporal_duration_normalize(&value, &duration, 0) == 0);
    assert(value.time.low == 0 && value.time.high == 1);
    duration.fields[QJS_TEMPORAL_HOUR] = 25;
    duration.fields[QJS_TEMPORAL_NANOSECOND] = 0;
    assert(qjs_temporal_duration_normalize(&value, &duration, 1) == 0);
    assert(qjs_temporal_duration_from_internal(&balanced, value,
                                               QJS_TEMPORAL_DAY) == 0);
    assert(balanced.fields[QJS_TEMPORAL_DAY] == 1);
    assert(balanced.fields[QJS_TEMPORAL_HOUR] == 1);
    duration.fields[QJS_TEMPORAL_SECOND] = -1;
    assert(!qjs_temporal_duration_is_valid(&duration));
}

static void test_rounding(void)
{
    QJSTemporalEpochNs value, result;
    QJSTemporalDuration duration = { { 0 } };
    QJSTemporalInternalDuration normalized;
    QJSTemporalRoundingMode mode;
    static const int positive[] = { 3, 2, 3, 2, 3, 2, 3, 2, 2 };
    static const int negative[] = { -2, -3, -3, -2, -2, -3, -3, -2, -2 };

    for (mode = QJS_TEMPORAL_ROUND_CEIL; mode <= QJS_TEMPORAL_ROUND_HALF_EVEN;
         mode++) {
        value = qjs_temporal_epoch_ns_from_int64(INT64_C(2500000000));
        assert(qjs_temporal_time_duration_round(&result, value, 1,
                                                QJS_TEMPORAL_SECOND, mode) == 0);
        assert(qjs_temporal_epoch_ns_compare(result,
            qjs_temporal_epoch_ns_from_int64((int64_t)positive[mode] * 1000000000)) == 0);
        value = qjs_temporal_epoch_ns_from_int64(-INT64_C(2500000000));
        assert(qjs_temporal_time_duration_round(&result, value, 1,
                                                QJS_TEMPORAL_SECOND, mode) == 0);
        assert(qjs_temporal_epoch_ns_compare(result,
            qjs_temporal_epoch_ns_from_int64((int64_t)negative[mode] * 1000000000)) == 0);
    }
    duration.fields[QJS_TEMPORAL_DAY] = 500000000;
    assert(qjs_temporal_duration_normalize(&normalized, &duration, 1) == 0);
    assert(qjs_temporal_time_duration_round(&result, normalized.time, 1000000000,
        QJS_TEMPORAL_DAY, QJS_TEMPORAL_ROUND_HALF_EXPAND) == 0);
    assert(qjs_temporal_time_duration_add_days(&value,
        qjs_temporal_epoch_ns_from_int64(0), 1000000000) == 0);
    assert(qjs_temporal_epoch_ns_compare(value, result) == 0);
}

static void test_parse_format_and_time(void)
{
    QJSTemporalDuration duration;
    QJSTemporalISOTime time = { 0 }, result;
    char text[512];
    int64_t days;
    static const char valid[] = "PT0.999999999H";

    assert(qjs_temporal_parse_duration(&duration, valid, sizeof(valid) - 1) == 0);
    assert(duration.fields[QJS_TEMPORAL_MINUTE] == 59);
    assert(duration.fields[QJS_TEMPORAL_MICROSECOND] == 996);
    assert(duration.fields[QJS_TEMPORAL_NANOSECOND] == 400);
    assert(qjs_temporal_format_duration(text, sizeof(text), &duration, -1) > 0);
    assert(strcmp(text, "PT59M59.9999964S") == 0);
    assert(qjs_temporal_parse_duration(&duration, "PT1.5H1M", 8) == -1);
    assert(qjs_temporal_time_add(&result, &days, time,
        qjs_temporal_epoch_ns_from_int64(-1)) == 0);
    assert(days == -1 && result.hour == 23 && result.nanosecond == 999);
    assert(qjs_temporal_format_time(text, sizeof(text), result, -1) == 18);
    assert(strcmp(text, "23:59:59.999999999") == 0);
    assert(qjs_temporal_time_round(&time, result, 1, QJS_TEMPORAL_SECOND,
                                   QJS_TEMPORAL_ROUND_HALF_EXPAND) == 0);
    assert(qjs_temporal_time_nanoseconds(time) == 0);
}

int main(void)
{
    test_exact_conversion();
    test_bounds_and_balance();
    test_rounding();
    test_parse_format_and_time();
    return 0;
}
