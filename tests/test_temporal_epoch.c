/*
 * Native Temporal support tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include "../src/temporal/epoch.h"

static void assert_ns(QJSTemporalEpochNs actual, QJSTemporalEpochNs expected)
{
    assert(qjs_temporal_epoch_ns_compare(actual, expected) == 0);
}

static void expect_words(QJSTemporalEpochNs actual, uint64_t low, uint64_t high)
{
    assert(actual.low == low && actual.high == high);
}

static void test_epoch_limits(void)
{
    const double limit_ms = 8640000000000000.0;
    const double invalid[] = {
        NAN, INFINITY, -INFINITY, 0.5, -0.5,
        8640000000000001.0, -8640000000000001.0,
    };
    const QJSTemporalEpochNs unchanged = { 123, 456 };
    QJSTemporalEpochNs value, outside, one;
    int64_t milliseconds;
    unsigned i;

    one = qjs_temporal_epoch_ns_from_int64(1);
    assert(qjs_temporal_epoch_ns_from_milliseconds(&value, limit_ms) == 0);
    assert(qjs_temporal_epoch_ns_is_valid(value));
    assert(qjs_temporal_epoch_ns_to_milliseconds(&milliseconds, value) == 0);
    assert(milliseconds == INT64_C(8640000000000000));
    assert(qjs_temporal_epoch_ns_add(&outside, value, one) == 0);
    assert(!qjs_temporal_epoch_ns_is_valid(outside));
    milliseconds = 77;
    assert(qjs_temporal_epoch_ns_to_milliseconds(&milliseconds, outside) == -1);
    assert(milliseconds == 77);

    assert(qjs_temporal_epoch_ns_from_milliseconds(&value, -limit_ms) == 0);
    assert(qjs_temporal_epoch_ns_is_valid(value));
    assert(qjs_temporal_epoch_ns_to_milliseconds(&milliseconds, value) == 0);
    assert(milliseconds == -INT64_C(8640000000000000));
    assert(qjs_temporal_epoch_ns_subtract(&outside, value, one) == 0);
    assert(!qjs_temporal_epoch_ns_is_valid(outside));

    value = qjs_temporal_epoch_ns_from_int64(-1);
    assert(qjs_temporal_epoch_ns_to_milliseconds(&milliseconds, value) == 0);
    assert(milliseconds == -1);
    value = qjs_temporal_epoch_ns_from_int64(1);
    assert(qjs_temporal_epoch_ns_to_milliseconds(&milliseconds, value) == 0);
    assert(milliseconds == 0);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        value = unchanged;
        assert(qjs_temporal_epoch_ns_from_milliseconds(&value, invalid[i]) == -1);
        assert_ns(value, unchanged);
    }
}

static void test_checked_arithmetic(void)
{
    const QJSTemporalEpochNs maximum = { UINT64_MAX, UINT64_MAX >> 1 };
    const QJSTemporalEpochNs minimum = { 0, UINT64_C(1) << 63 };
    const QJSTemporalEpochNs unchanged = { 123, 456 };
    const QJSTemporalEpochNs one = { 1, 0 };
    QJSTemporalEpochNs result, a, b;
    int64_t integer;

    a = qjs_temporal_epoch_ns_from_int64(INT64_MIN);
    assert(qjs_temporal_epoch_ns_to_int64(&integer, a) == 0);
    assert(integer == INT64_MIN);
    a = qjs_temporal_epoch_ns_from_int64(INT64_MAX);
    assert(qjs_temporal_epoch_ns_to_int64(&integer, a) == 0);
    assert(integer == INT64_MAX);
    integer = 77;
    assert(qjs_temporal_epoch_ns_to_int64(&integer, maximum) == -1);
    assert(integer == 77);

    a.low = UINT64_MAX;
    a.high = 0;
    assert(qjs_temporal_epoch_ns_add(&result, a, one) == 0);
    b.low = 0;
    b.high = 1;
    assert_ns(result, b);
    assert(qjs_temporal_epoch_ns_subtract(&result, b, one) == 0);
    assert_ns(result, a);
    result = unchanged;
    assert(qjs_temporal_epoch_ns_add(&result, maximum, one) == -1);
    assert_ns(result, unchanged);
    assert(qjs_temporal_epoch_ns_subtract(&result, minimum, one) == -1);
    assert_ns(result, unchanged);
    assert(qjs_temporal_epoch_ns_subtract(&result, maximum, minimum) == -1);
    assert_ns(result, unchanged);
    assert(qjs_temporal_epoch_ns_add(&result, minimum, maximum) == 0);
    assert_ns(result, qjs_temporal_epoch_ns_from_int64(-1));

    a = qjs_temporal_epoch_ns_from_int64(INT32_MIN);
    assert(qjs_temporal_epoch_ns_multiply(&result, a, INT32_MIN) == 0);
    assert_ns(result, qjs_temporal_epoch_ns_from_int64(INT64_C(1) << 62));
    assert(qjs_temporal_epoch_ns_multiply(&result, minimum, 1) == 0);
    assert_ns(result, minimum);
    assert(qjs_temporal_epoch_ns_multiply(&result, minimum, 0) == 0);
    assert_ns(result, qjs_temporal_epoch_ns_from_int64(0));
    result = unchanged;
    assert(qjs_temporal_epoch_ns_multiply(&result, minimum, -1) == -1);
    assert_ns(result, unchanged);
    assert(qjs_temporal_epoch_ns_multiply(&result, maximum, 2) == -1);
    assert_ns(result, unchanged);
}

static void test_floor_division(void)
{
    const QJSTemporalEpochNs minimum = { 0, UINT64_C(1) << 63 };
    QJSTemporalEpochNs quotient, value = { UINT64_MAX, UINT64_MAX };
    QJSTemporalEpochNs two_words = { 0, 1 };
    uint64_t remainder;

    assert(qjs_temporal_epoch_ns_divide(&quotient, &remainder, value, 10) == 0);
    assert_ns(quotient, qjs_temporal_epoch_ns_from_int64(-1));
    assert(remainder == 9);
    assert(qjs_temporal_epoch_ns_divide(&quotient, &remainder,
                                       two_words, UINT64_MAX) == 0);
    assert_ns(quotient, qjs_temporal_epoch_ns_from_int64(1));
    assert(remainder == 1);
    two_words.low = 0;
    two_words.high = UINT64_MAX;
    assert(qjs_temporal_epoch_ns_divide(&quotient, &remainder,
                                       two_words, UINT64_MAX) == 0);
    assert_ns(quotient, qjs_temporal_epoch_ns_from_int64(-2));
    assert(remainder == UINT64_MAX - 1);
    assert(qjs_temporal_epoch_ns_divide(&quotient, &remainder, minimum, 1) == 0);
    assert_ns(quotient, minimum);
    assert(remainder == 0);
    quotient.low = 123;
    quotient.high = 456;
    remainder = 789;
    assert(qjs_temporal_epoch_ns_divide(&quotient, &remainder, value, 0) == -1);
    assert(quotient.low == 123 && quotient.high == 456 && remainder == 789);
}

static void test_rounding(void)
{
    static const int expected_positive[] = {
        20, 10, 20, 10, 20, 10, 20, 10, 20,
    };
    static const int expected_negative[] = {
        -10, -20, -20, -10, -10, -20, -20, -10, -20,
    };
    static const int expected_exact_time_negative[] = {
        -10, -20, -10, -20, -10, -20, -10, -20, -20,
    };
    QJSTemporalEpochNs result, value;
    unsigned mode;

    for (mode = 0; mode <= QJS_TEMPORAL_ROUND_HALF_EVEN; mode++) {
        value = qjs_temporal_epoch_ns_from_int64(15);
        assert(qjs_temporal_epoch_ns_round(&result, value, 10, mode) == 0);
        assert_ns(result, qjs_temporal_epoch_ns_from_int64(expected_positive[mode]));
        value = qjs_temporal_epoch_ns_from_int64(-15);
        assert(qjs_temporal_epoch_ns_round(&result, value, 10, mode) == 0);
        assert_ns(result, qjs_temporal_epoch_ns_from_int64(expected_negative[mode]));
        assert(qjs_temporal_epoch_ns_round_as_if_positive(&result,
                                                         value, 10, mode) == 0);
        assert_ns(result, qjs_temporal_epoch_ns_from_int64(
            expected_exact_time_negative[mode]));
    }
    value = qjs_temporal_epoch_ns_from_int64(-25);
    assert(qjs_temporal_epoch_ns_round(&result, value, 10,
                                      QJS_TEMPORAL_ROUND_HALF_EVEN) == 0);
    assert_ns(result, qjs_temporal_epoch_ns_from_int64(-20));
    value.low = UINT64_MAX;
    value.high = UINT64_MAX >> 1;
    result.low = 123;
    result.high = 456;
    assert(qjs_temporal_epoch_ns_round(&result, value, 2,
                                      QJS_TEMPORAL_ROUND_CEIL) == -1);
    assert(result.low == 123 && result.high == 456);
    assert(qjs_temporal_epoch_ns_round(&result, value, 0,
                                      QJS_TEMPORAL_ROUND_FLOOR) == -1);
    assert(result.low == 123 && result.high == 456);
    assert(qjs_temporal_epoch_ns_round(&result, value, 1,
                                      (QJSTemporalRoundingMode)-1) == -1);
    assert(result.low == 123 && result.high == 456);
}

static void test_full_width_increment(void)
{
    QJSTemporalEpochNs value = { 0, 1 }, result;

    /* 2^64 / (2^64 - 1) has floor quotient 1 and remainder 1. */
    assert(qjs_temporal_epoch_ns_round(&result, value, UINT64_MAX,
                                      QJS_TEMPORAL_ROUND_FLOOR) == 0);
    expect_words(result, UINT64_MAX, 0);
    assert(qjs_temporal_epoch_ns_round(&result, value, UINT64_MAX,
                                      QJS_TEMPORAL_ROUND_CEIL) == 0);
    expect_words(result, UINT64_MAX - 1, 1);
    assert(qjs_temporal_epoch_ns_round(&result, value, UINT64_MAX,
                                      QJS_TEMPORAL_ROUND_HALF_EVEN) == 0);
    expect_words(result, UINT64_MAX, 0);
    value.low = UINT64_MAX;
    value.high = UINT64_MAX;
    assert(qjs_temporal_epoch_ns_round(&result, value, UINT64_MAX,
                                      QJS_TEMPORAL_ROUND_TRUNC) == 0);
    expect_words(result, 0, 0);
    assert(qjs_temporal_epoch_ns_round_as_if_positive(
        &result, value, UINT64_MAX, QJS_TEMPORAL_ROUND_TRUNC) == 0);
    expect_words(result, 1, UINT64_MAX);
}

static void test_rounding_below_signed_minimum(void)
{
    QJSTemporalEpochNs value = { 0, UINT64_C(1) << 63 };
    QJSTemporalEpochNs result = { 123, 456 };

    /* -2^127 = 3 * floor(-2^127 / 3) + 1. */
    assert(qjs_temporal_epoch_ns_round(&result, value, 3,
                                      QJS_TEMPORAL_ROUND_FLOOR) == -1);
    expect_words(result, 123, 456);
    assert(qjs_temporal_epoch_ns_round(&result, value, 3,
                                      QJS_TEMPORAL_ROUND_CEIL) == 0);
    expect_words(result, 2, UINT64_C(1) << 63);
    result.low = 123;
    result.high = 456;
    assert(qjs_temporal_epoch_ns_round_as_if_positive(
        &result, value, 3, QJS_TEMPORAL_ROUND_TRUNC) == -1);
    expect_words(result, 123, 456);
}

int main(void)
{
    test_epoch_limits();
    test_checked_arithmetic();
    test_floor_division();
    test_rounding();
    test_full_width_increment();
    test_rounding_below_signed_minimum();
    return 0;
}
