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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/temporal/epoch.h"
#include "../src/temporal/iso.h"
#include "../src/temporal/parse.h"
#include "../src/temporal/format.h"
#include "../src/temporal/types.h"
#include "../src/temporal/options.h"

static void assert_ns(QJSTemporalEpochNs actual, QJSTemporalEpochNs expected)
{
    assert(qjs_temporal_epoch_ns_compare(actual, expected) == 0);
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

static void assert_date(QJSTemporalISODate actual, QJSTemporalISODate expected)
{
    assert(actual.year == expected.year && actual.month == expected.month &&
           actual.day == expected.day);
}

static void test_gregorian_dates(void)
{
    static const struct {
        QJSTemporalISODate date;
        int64_t days;
    } cases[] = {
        { { 1970, 1, 1 }, 0 },
        { { 1969, 12, 31 }, -1 },
        { { 2000, 2, 29 }, 11016 },
        { { 1900, 3, 1 }, -25508 },
        { { 0, 1, 1 }, -719528 },
        { { -1, 12, 31 }, -719529 },
        { { -400, 1, 1 }, -865625 },
        { { -271821, 4, 20 }, -100000000 },
        { { 275760, 9, 13 }, 100000000 },
    };
    QJSTemporalISODate date, expected;
    int64_t days;
    unsigned i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(qjs_temporal_iso_date_to_days(&days, cases[i].date) == 0);
        assert(days == cases[i].days);
        assert(qjs_temporal_iso_date_from_days(&date, days) == 0);
        assert_date(date, cases[i].date);
    }
    expected.year = INT32_MIN;
    expected.month = 1;
    expected.day = 1;
    assert(qjs_temporal_iso_date_to_days(&days, expected) == 0);
    assert(qjs_temporal_iso_date_from_days(&date, days) == 0);
    assert_date(date, expected);
    assert(qjs_temporal_iso_date_from_days(&date, days - 1) == -1);
    assert_date(date, expected);
    expected.year = INT32_MAX;
    expected.month = 12;
    expected.day = 31;
    assert(qjs_temporal_iso_date_to_days(&days, expected) == 0);
    assert(qjs_temporal_iso_date_from_days(&date, days) == 0);
    assert_date(date, expected);
    assert(qjs_temporal_iso_date_from_days(&date, days + 1) == -1);
    assert_date(date, expected);
    expected.year = 1900;
    expected.month = 2;
    expected.day = 29;
    days = 123;
    assert(qjs_temporal_iso_date_to_days(&days, expected) == -1);
    assert(days == 123);
    expected.year = 2000;
    assert(qjs_temporal_iso_date_is_valid(expected));
    expected.year = -100;
    assert(!qjs_temporal_iso_date_is_valid(expected));
    expected.year = -400;
    assert(qjs_temporal_iso_date_is_valid(expected));
}

static void test_civil_epoch_conversion(void)
{
    QJSTemporalISODateTime datetime = {
        { 1969, 12, 31 }, { 23, 59, 59, 999, 999, 999 },
    };
    QJSTemporalISODateTime converted;
    QJSTemporalEpochNs value;
    int year, month, day;

    assert(qjs_temporal_iso_datetime_to_epoch_ns(&value, datetime) == 0);
    assert_ns(value, qjs_temporal_epoch_ns_from_int64(-1));
    assert(qjs_temporal_iso_datetime_from_epoch_ns(&converted, value) == 0);
    assert_date(converted.date, datetime.date);
    assert(converted.time.hour == 23 && converted.time.minute == 59 &&
           converted.time.second == 59 && converted.time.millisecond == 999 &&
           converted.time.microsecond == 999 && converted.time.nanosecond == 999);
    /* Check every date in one complete Gregorian cycle, including year
       zero and negative centuries, independently of the estimate formula. */
    for (year = -400; year < 0; year++) {
        for (month = 1; month <= 12; month++) {
            for (day = 1; day <= 31; day++) {
                datetime.date.year = year;
                datetime.date.month = month;
                datetime.date.day = day;
                if (!qjs_temporal_iso_date_is_valid(datetime.date))
                    continue;
                assert(qjs_temporal_iso_datetime_to_epoch_ns(&value, datetime) == 0);
                assert(qjs_temporal_iso_datetime_from_epoch_ns(&converted, value) == 0);
                assert_date(converted.date, datetime.date);
                assert(converted.time.nanosecond == 999);
            }
        }
    }
    datetime.date.year = 1970;
    datetime.date.month = 1;
    datetime.date.day = 1;
    datetime.time.hour = 24;
    value.low = 123;
    value.high = 456;
    assert(qjs_temporal_iso_datetime_to_epoch_ns(&value, datetime) == -1);
    assert(value.low == 123 && value.high == 456);
}

static void test_numeric_iso_parsing(void)
{
    static const char * const invalid[] = {
        "1970-01-01", "1970-01-01T00:00", "1970-01-01T24Z",
        "1970-01-01T12:3456Z", "1970-01-01T1234:56Z",
        "1970-01-01T12:34.5Z", "1970-01-01T12:34:56.1234567890Z",
        "1970-01-01T12:34:61Z", "1970-01-01T12:34:56+24:00",
        "1970-01-01T12:34:56+00:00:60", "1900-02-29T00Z",
        "-000000-01-01T00Z", "1970-0101T00Z", "197001-01T00Z",
        "1970-01-01T00Z ", "1970-01-01T00Zgarbage",
        ("\xe2\x88\x92" "000001-01-01T00Z"),
    };
    static const char bounded[] = {
        '1', '9', '7', '0', '0', '1', '0', '1', 'T', '0', '0', 'Z',
    };
    const char *text;
    QJSTemporalInstantPrefix prefix;
    QJSTemporalEpochNs utc, offset, adjusted;
    QJSTemporalUTCOffset zone;
    size_t consumed;
    unsigned i;

    /* The parser does not need a terminating NUL and must not read past
       the supplied bound at any truncated grammar position. */
    for (i = 0; i < sizeof(bounded); i++) {
        prefix.datetime.date.year = 123;
        consumed = 456;
        assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                                bounded, i) == -1);
        assert(prefix.datetime.date.year == 123 && consumed == 456);
    }
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                            bounded, sizeof(bounded)) == 0);
    assert(consumed == sizeof(bounded));
    assert(prefix.datetime.date.year == 1970 && prefix.offset.is_z);
    text = "19691231t235959,999999999z";
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                            text, strlen(text)) == 0);
    assert(consumed == strlen(text));
    assert(prefix.datetime.time.nanosecond == 999 && prefix.offset.is_z);
    assert(qjs_temporal_iso_datetime_to_epoch_ns(&utc, prefix.datetime) == 0);
    assert_ns(utc, qjs_temporal_epoch_ns_from_int64(-1));
    text = "2000-02-29T23:59:60.123456789+01:02:03.987654321";
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                            text, strlen(text)) == 0);
    assert(consumed == strlen(text));
    assert(prefix.datetime.time.second == 59);
    assert(prefix.datetime.time.millisecond == 123);
    assert(prefix.datetime.time.microsecond == 456);
    assert(prefix.datetime.time.nanosecond == 789);
    assert(prefix.offset.nanoseconds == INT64_C(3723987654321));
    assert(prefix.offset.has_seconds);
    text = "+275760-09-13T00:00Z";
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                            text, strlen(text)) == 0);
    assert(qjs_temporal_iso_datetime_to_epoch_ns(&utc, prefix.datetime) == 0);
    assert(qjs_temporal_epoch_ns_is_valid(utc));
    /* Raw local fields can be outside Instant range before an offset
       makes the resulting exact UTC epoch valid. */
    text = "+275760-09-13T01+01";
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                            text, strlen(text)) == 0);
    assert(qjs_temporal_iso_datetime_to_epoch_ns(&utc, prefix.datetime) == 0);
    assert(!qjs_temporal_epoch_ns_is_valid(utc));
    offset = qjs_temporal_epoch_ns_from_int64(prefix.offset.nanoseconds);
    assert(qjs_temporal_epoch_ns_subtract(&adjusted, utc, offset) == 0);
    assert(qjs_temporal_epoch_ns_is_valid(adjusted));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        prefix.datetime.date.year = 123;
        consumed = 456;
        assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                                invalid[i], strlen(invalid[i])) == -1);
        assert(prefix.datetime.date.year == 123 && consumed == 456);
    }
    text = "1970-01-01T00Z[!unknown=value]";
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                            text, strlen(text)) == 0);
    assert(text[consumed] == '[');
    /* This explicit success is only the prefix contract: the caller must
       reject the unknown critical annotation in its pending continuation. */
    text = "+23:59:59.999999999";
    assert(qjs_temporal_parse_utc_offset_prefix(&zone, &consumed,
                                               text, strlen(text),
                                               QJS_TEMPORAL_OFFSET_ALLOW_SECONDS) == 0);
    assert(zone.nanoseconds == INT64_C(86399999999999));
    zone.nanoseconds = 123;
    consumed = 456;
    assert(qjs_temporal_parse_utc_offset_prefix(&zone, &consumed,
                                               text, strlen(text), 0) == -1);
    assert(zone.nanoseconds == 123 && consumed == 456);
    assert(qjs_temporal_parse_utc_offset_prefix(&zone, &consumed, "Z", 1, 0) == -1);
    assert(zone.nanoseconds == 123 && consumed == 456);
    assert(qjs_temporal_parse_utc_offset_prefix(&zone, &consumed,
                                               "-00:00", 6, 0) == 0);
    assert(zone.nanoseconds == 0 && zone.is_z == 0 && !zone.has_seconds);
}

/* Compare concrete representations directly so the comparison helper cannot
   hide an arithmetic defect in these independent fixed-value expectations. */
static void expect_words(QJSTemporalEpochNs actual, uint64_t low, uint64_t high)
{
    assert(actual.low == low && actual.high == high);
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

static void test_lower_limit_after_offset(void)
{
    const char text[] = "-271821-04-19T23-01";
    QJSTemporalInstantPrefix prefix;
    QJSTemporalEpochNs local, offset, utc, minimum;
    size_t consumed;

    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                             text, sizeof(text) - 1) == 0);
    assert(consumed == sizeof(text) - 1);
    assert(prefix.offset.nanoseconds == -INT64_C(3600000000000));
    assert(qjs_temporal_iso_datetime_to_epoch_ns(&local, prefix.datetime) == 0);
    assert(!qjs_temporal_epoch_ns_is_valid(local));
    offset = qjs_temporal_epoch_ns_from_int64(prefix.offset.nanoseconds);
    assert(qjs_temporal_epoch_ns_subtract(&utc, local, offset) == 0);
    assert(qjs_temporal_epoch_ns_is_valid(utc));
    assert(qjs_temporal_epoch_ns_from_milliseconds(
        &minimum, -8640000000000000.0) == 0);
    expect_words(utc, minimum.low, minimum.high);
}

static void test_embedded_nul_and_unvalidated_annotation(void)
{
    const char with_nul[] = "1970-01-01T00Z\0[unknown=value]";
    const char annotation[] = "1970-01-01T00Z[";
    QJSTemporalInstantPrefix prefix;
    size_t consumed = 456;

    prefix.datetime.date.year = 123;
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                             with_nul,
                                             sizeof(with_nul) - 1) == -1);
    assert(prefix.datetime.date.year == 123 && consumed == 456);
    /* Even an unfinished '[' is an allowed boundary for this prefix API.
       A future complete string adapter must reject this entire input. */
    assert(qjs_temporal_parse_instant_prefix(&prefix, &consumed,
                                             annotation,
                                             sizeof(annotation) - 1) == 0);
    assert(consumed == sizeof("1970-01-01T00Z") - 1);
    assert(annotation[consumed] == '[');
}

typedef enum ParserBufferKind {
    PARSER_BUFFER_DATE,
    PARSER_BUFFER_TIME,
    PARSER_BUFFER_OFFSET,
    PARSER_BUFFER_INSTANT,
} ParserBufferKind;

static void check_exact_sized_parser_buffer(ParserBufferKind kind,
                                             const char *sample, size_t length,
                                             int success, unsigned flags)
{
    union {
        QJSTemporalISODate date;
        QJSTemporalISOTime time;
        QJSTemporalUTCOffset offset;
        QJSTemporalInstantPrefix instant;
    } result;
    unsigned char unchanged[sizeof(result)];
    size_t consumed = 12345;
    char *buffer = NULL;
    int status = -999;

    /* No NUL and no suffix bytes exist in this allocation. With ASan,
       the first byte beyond length is an actual heap red zone. */
    if (length) {
        buffer = malloc(length);
        assert(buffer != NULL);
        memcpy(buffer, sample, length);
    }
    memset(&result, 0xa5, sizeof(result));
    memcpy(unchanged, &result, sizeof(result));
    switch (kind) {
    case PARSER_BUFFER_DATE:
        status = qjs_temporal_parse_iso_date_prefix(&result.date, &consumed,
                                                     buffer, length);
        break;
    case PARSER_BUFFER_TIME:
        status = qjs_temporal_parse_iso_time_prefix(&result.time, &consumed,
                                                     buffer, length);
        break;
    case PARSER_BUFFER_OFFSET:
        status = qjs_temporal_parse_utc_offset_prefix(&result.offset, &consumed,
                                                       buffer, length, flags);
        break;
    case PARSER_BUFFER_INSTANT:
        status = qjs_temporal_parse_instant_prefix(&result.instant, &consumed,
                                                    buffer, length);
        break;
    }
    free(buffer);
    assert(status == (success ? 0 : -1));
    if (success) {
        assert(consumed == length);
    } else {
        assert(consumed == 12345);
        assert(memcmp(&result, unchanged, sizeof(result)) == 0);
    }
}

static void test_exact_sized_iso_parser_buffers(void)
{
    static const char * const dates[] = {
        "2000-02-29", "20000229", "+275760-09-13", "-0000010101",
    };
    static const char * const instants[] = {
        "1970-01-01T00Z", "+275760-09-13T01+01",
        "2000-02-29T23:59:60.123456789Z",
    };
    const char *sample;
    size_t index, length, full;

    /* Complete date/Instant samples have no valid earlier prefix. */
    for (index = 0; index < sizeof(dates) / sizeof(dates[0]); index++) {
        full = strlen(dates[index]);
        for (length = 0; length <= full; length++)
            check_exact_sized_parser_buffer(PARSER_BUFFER_DATE, dates[index],
                                             length, length == full, 0);
    }
    for (index = 0; index < sizeof(instants) / sizeof(instants[0]); index++) {
        full = strlen(instants[index]);
        for (length = 0; length <= full; length++)
            check_exact_sized_parser_buffer(PARSER_BUFFER_INSTANT,
                                             instants[index], length,
                                             length == full, 0);
    }

    /* Fragment APIs accept shorter complete forms: HH, HH:mm, HH:mm:ss,
       and any one-through-nine-digit fractional seconds. */
    sample = "23:59:60.123456789";
    full = strlen(sample);
    for (length = 0; length <= full; length++) {
        int success = length == 2 || length == 5 || length == 8 || length >= 10;
        check_exact_sized_parser_buffer(PARSER_BUFFER_TIME, sample, length,
                                         success, 0);
    }
    sample = "-01:02:03.123456789";
    full = strlen(sample);
    for (length = 0; length <= full; length++) {
        int success = length == 3 || length == 6 || length == 9 || length >= 11;
        check_exact_sized_parser_buffer(PARSER_BUFFER_OFFSET, sample, length,
                                         success, QJS_TEMPORAL_OFFSET_ALLOW_SECONDS);
        check_exact_sized_parser_buffer(PARSER_BUFFER_OFFSET, sample, length,
                                         length == 3 || length == 6, 0);
    }
    check_exact_sized_parser_buffer(PARSER_BUFFER_OFFSET, "z", 1, 1,
                                     QJS_TEMPORAL_OFFSET_ALLOW_Z);
    check_exact_sized_parser_buffer(PARSER_BUFFER_OFFSET, "z", 1, 0, 0);

    /* Malformed complete buffers also end immediately before a red zone. */
    sample = "1970-01-01T00:00:00.1234567890Z";
    check_exact_sized_parser_buffer(PARSER_BUFFER_INSTANT, sample,
                                     strlen(sample), 0, 0);
    sample = "+01:02:03.";
    check_exact_sized_parser_buffer(PARSER_BUFFER_OFFSET, sample,
                                     strlen(sample), 0,
                                     QJS_TEMPORAL_OFFSET_ALLOW_SECONDS);
}

static void test_complete_instant_parsing(void)
{
    static const char * const valid[] = {
        "1970-01-01T00Z", "19700101t00z[Unknown/Zone]",
        "1970-01-01T00Z[!Unknown/Zone]", "1970-01-01T00Z[!-01:30]",
        "1970-01-01T00Z[+00][u-ca=iso8601][foo=bar-baz]",
        "1970-01-01T00Z[foo=bar][u-ca=made-up-calendar]",
        "1970-01-01T00Z[!u-ca=ISO8601]",
        "1970-01-01T00Z[u-ca=iso8601][u-ca=other]",
        "1970-01-01T00Z[_key=ABC-123][x=Y][x=Z]",
        "1970-01-01T00Z[.]", "1970-01-01T00Z[Etc/GMT+1]",
    };
    static const char * const invalid[] = {
        "1970-01-01T00Z[", "1970-01-01T00Z[]", "1970-01-01T00Z[!]",
        "1970-01-01T00Z[UTC]garbage", "1970-01-01T00Z[UTC][Europe/London]",
        "1970-01-01T00Z[x=y][UTC]", "1970-01-01T00Z[!unknown=value]",
        "1970-01-01T00Z[u-ca=iso8601][!u-ca=iso8601]",
        "1970-01-01T00Z[!u-ca=iso8601][u-ca=iso8601]",
        "1970-01-01T00Z[U-ca=iso8601]", "1970-01-01T00Z[1key=value]",
        "1970-01-01T00Z[x=]", "1970-01-01T00Z[x=-a]",
        "1970-01-01T00Z[x=a-]", "1970-01-01T00Z[x=a--b]",
        "1970-01-01T00Z[x=a_b]", "1970-01-01T00Z[x=a=b]",
        "1970-01-01T00Z[/UTC]", "1970-01-01T00Z[UTC/]",
        "1970-01-01T00Z[Etc//UTC]", "1970-01-01T00Z[1UTC]",
        "1970-01-01T00Z[+24:00]", "1970-01-01T00Z[+01:00:00]",
        "1970-01-01T00Z[UTC][x=y]]", "1970-01-01T00Z[[UTC]]",
        "+275760-09-13T00:00:00.000000001Z",
        "-271821-04-19T23:59:59.999999999Z",
    };
    static const char embedded_nul[] = "1970-01-01T00Z\0[UTC]";
    QJSTemporalEpochNs value, unchanged = { 123, 456 };
    unsigned i;

    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        assert(qjs_temporal_parse_instant(&value, valid[i], strlen(valid[i])) == 0);
        assert_ns(value, qjs_temporal_epoch_ns_from_int64(0));
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        value = unchanged;
        assert(qjs_temporal_parse_instant(&value,
                                          invalid[i], strlen(invalid[i])) == -1);
        assert_ns(value, unchanged);
    }
    value = unchanged;
    assert(qjs_temporal_parse_instant(&value, embedded_nul,
                                      sizeof(embedded_nul) - 1) == -1);
    assert_ns(value, unchanged);
    assert(qjs_temporal_parse_instant(&value, "1969-12-31T23:59:59.999999999Z",
                                      sizeof("1969-12-31T23:59:59.999999999Z") - 1) == 0);
    assert_ns(value, qjs_temporal_epoch_ns_from_int64(-1));
    assert(qjs_temporal_parse_instant(&value, "1970-01-01T01+01[!UTC]",
                                      sizeof("1970-01-01T01+01[!UTC]") - 1) == 0);
    assert_ns(value, qjs_temporal_epoch_ns_from_int64(0));
    assert(qjs_temporal_parse_instant(&value, "-271821-04-19T23-01",
                                      sizeof("-271821-04-19T23-01") - 1) == 0);
    assert(qjs_temporal_epoch_ns_is_valid(value));
    assert(qjs_temporal_parse_instant(&value, "+275760-09-13T01+01",
                                      sizeof("+275760-09-13T01+01") - 1) == 0);
    assert(qjs_temporal_epoch_ns_is_valid(value));
}

static void test_shared_iso_grammar(void)
{
    static const struct {
        const char *text;
        unsigned grammar;
        int has_time, year_absent, short_year_month;
    } valid[] = {
        { "2000-02-29", QJS_TEMPORAL_PARSE_DATE_TIME, 0, 0, 0 },
        { "2000-02-29T12:34:60.123456789+01:02:03.4",
          QJS_TEMPORAL_PARSE_DATE_TIME, 1, 0, 0 },
        { "2000-02-29[!UTC][u-ca=iso8601]", QJS_TEMPORAL_PARSE_ZONED, 0, 0, 0 },
        { "2000-02-29T12Z[UTC]", QJS_TEMPORAL_PARSE_ZONED, 1, 0, 0 },
        { "2000-02-29T12Z", QJS_TEMPORAL_PARSE_INSTANT, 1, 0, 0 },
        { "T0701", QJS_TEMPORAL_PARSE_TIME, 1, 0, 0 },
        { "07:01", QJS_TEMPORAL_PARSE_TIME, 1, 0, 0 },
        { "202113", QJS_TEMPORAL_PARSE_TIME, 1, 0, 0 },
        { "13-12", QJS_TEMPORAL_PARSE_TIME, 1, 0, 0 },
        { "2000-02-29T12:30[Unknown/Zone][!u-ca=imaginary]",
          QJS_TEMPORAL_PARSE_TIME, 1, 0, 0 },
        { "2020-02[u-ca=iso8601]", QJS_TEMPORAL_PARSE_YEAR_MONTH, 0, 0, 1 },
        { "2020-02[u-ca=ISO8601]", QJS_TEMPORAL_PARSE_YEAR_MONTH, 0, 0, 1 },
        { "202002", QJS_TEMPORAL_PARSE_YEAR_MONTH, 0, 0, 1 },
        { "+001970-01", QJS_TEMPORAL_PARSE_YEAR_MONTH, 0, 0, 1 },
        { "2020-02-29T12", QJS_TEMPORAL_PARSE_YEAR_MONTH, 1, 0, 0 },
        { "--02-29", QJS_TEMPORAL_PARSE_MONTH_DAY, 0, 1, 0 },
        { "0229", QJS_TEMPORAL_PARSE_MONTH_DAY, 0, 1, 0 },
        { "2000-02-29T12", QJS_TEMPORAL_PARSE_MONTH_DAY, 1, 0, 0 },
    };
    static const struct { const char *text; unsigned grammar; } invalid[] = {
        { "2001-02-29", QJS_TEMPORAL_PARSE_DATE_TIME },
        { "2000-02-29Z", QJS_TEMPORAL_PARSE_DATE_TIME },
        { "2000-02-29T12Z", QJS_TEMPORAL_PARSE_DATE_TIME },
        { "2000-02-29T12", QJS_TEMPORAL_PARSE_INSTANT },
        { "2000-02-29Z[UTC]", QJS_TEMPORAL_PARSE_ZONED },
        { "2000-02-29T12Z", QJS_TEMPORAL_PARSE_ZONED },
        { "2000-02-29", QJS_TEMPORAL_PARSE_TIME },
        { "0701", QJS_TEMPORAL_PARSE_TIME },
        { "07-12", QJS_TEMPORAL_PARSE_TIME },
        { "202112", QJS_TEMPORAL_PARSE_TIME },
        { "12Z", QJS_TEMPORAL_PARSE_TIME },
        { "2020-02Z", QJS_TEMPORAL_PARSE_YEAR_MONTH },
        { "2020-02[u-ca=gregory]", QJS_TEMPORAL_PARSE_YEAR_MONTH },
        { "--02-29[u-ca=gregory]", QJS_TEMPORAL_PARSE_MONTH_DAY },
        { "--02-30", QJS_TEMPORAL_PARSE_MONTH_DAY },
        { "-000000-01", QJS_TEMPORAL_PARSE_YEAR_MONTH },
        { "12:30[!unknown=value]", QJS_TEMPORAL_PARSE_TIME },
        { "12:30[u-ca=iso8601][!u-ca=iso8601]", QJS_TEMPORAL_PARSE_TIME },
        { "12:30[x=value][UTC]", QJS_TEMPORAL_PARSE_TIME },
        { "12:30[UTC][UTC]", QJS_TEMPORAL_PARSE_TIME },
    };
    QJSTemporalParsedISO parsed, unchanged;
    char *exact;
    size_t length;
    unsigned i;

    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        length = strlen(valid[i].text);
        exact = malloc(length);
        assert(exact);
        memcpy(exact, valid[i].text, length);
        assert(qjs_temporal_parse_iso_datetime(&parsed, exact, length,
                                               valid[i].grammar) == 0);
        assert(parsed.has_time == valid[i].has_time);
        assert(parsed.year_absent == valid[i].year_absent);
        assert(parsed.short_year_month == valid[i].short_year_month);
        free(exact);
    }
    memset(&unchanged, 0x5a, sizeof(unchanged));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        length = strlen(invalid[i].text);
        exact = malloc(length);
        assert(exact);
        memcpy(exact, invalid[i].text, length);
        memcpy(&parsed, &unchanged, sizeof(parsed));
        assert(qjs_temporal_parse_iso_datetime(&parsed, exact, length,
                                               invalid[i].grammar) == -1);
        assert(!memcmp(&parsed, &unchanged, sizeof(parsed)));
        free(exact);
    }
    memcpy(&parsed, &unchanged, sizeof(parsed));
    assert(qjs_temporal_parse_iso_datetime(&parsed, NULL, 0,
                                           QJS_TEMPORAL_PARSE_TIME) == -1);
    assert(!memcmp(&parsed, &unchanged, sizeof(parsed)));
    {
        const char text[] = "2000-01-01T01:02:03.4+05:30[Asia/Kolkata]"
                            "[u-ca=iso8601][u-ca=gregory]";
        assert(qjs_temporal_parse_iso_datetime(&parsed, text, sizeof(text) - 1,
                                               QJS_TEMPORAL_PARSE_ZONED) == 0);
        assert(parsed.offset.nanoseconds == INT64_C(19800000000000));
        assert(parsed.datetime.time.millisecond == 400);
        assert(parsed.time_zone_length == 12);
        assert(!memcmp(parsed.time_zone, "Asia/Kolkata", 12));
        assert(parsed.calendar_length == 7);
        assert(!memcmp(parsed.calendar, "iso8601", 7));
    }
}

static void test_iso_formatting(void)
{
    QJSTemporalISODateTime datetime = { { 1970, 1, 1 }, { 0, 0, 0, 123, 456, 789 } };
    QJSTemporalStringPrecision precision;
    char buffer[80];
    int i;

    assert(qjs_temporal_format_date_time(buffer, sizeof(buffer), datetime, -1) == 29);
    assert(!strcmp(buffer, "1970-01-01T00:00:00.123456789"));
    assert(qjs_temporal_format_date_time(buffer, sizeof(buffer), datetime, 2) == 22);
    assert(!strcmp(buffer, "1970-01-01T00:00:00.12"));
    assert(qjs_temporal_format_date_time(buffer, sizeof(buffer), datetime, -2) == 16);
    assert(!strcmp(buffer, "1970-01-01T00:00"));
    datetime.date.year = -1;
    datetime.time = (QJSTemporalISOTime){ 0 };
    assert(qjs_temporal_format_date_time(buffer, sizeof(buffer), datetime, -1) == 22);
    assert(!strcmp(buffer, "-000001-01-01T00:00:00"));
    strcpy(buffer, "sentinel");
    assert(qjs_temporal_format_date_time(buffer, 4, datetime, -1) == -1);
    assert(!strcmp(buffer, "sentinel"));
    datetime.time.second = 61;
    assert(qjs_temporal_format_date_time(buffer, sizeof(buffer), datetime, -1) == -1);
    assert(!strcmp(buffer, "sentinel"));
    assert(qjs_temporal_format_utc_offset(buffer, sizeof(buffer), 0, 0) == 6);
    assert(!strcmp(buffer, "+00:00"));
    assert(qjs_temporal_format_utc_offset(buffer, sizeof(buffer), -1, 1) == 6);
    assert(!strcmp(buffer, "+00:00"));
    assert(qjs_temporal_format_utc_offset(buffer, sizeof(buffer),
                                         -INT64_C(30000000000), 1) == 6);
    assert(!strcmp(buffer, "-00:01"));
    assert(qjs_temporal_format_utc_offset(buffer, sizeof(buffer),
                                         INT64_C(19803123456789), 0) == 19);
    assert(!strcmp(buffer, "+05:30:03.123456789"));
    assert(qjs_temporal_format_utc_offset(buffer, sizeof(buffer),
                                         INT64_C(86399999999999), 1) == 6);
    assert(!strcmp(buffer, "+24:00"));
    strcpy(buffer, "sentinel");
    assert(qjs_temporal_format_utc_offset(buffer, sizeof(buffer), INT64_MIN, 1) == -1);
    assert(!strcmp(buffer, "sentinel"));
    for (i = 0; i <= 9; i++) {
        assert(qjs_temporal_string_precision(&precision, QJS_TEMPORAL_UNIT_UNSET, i) == 0);
        assert(precision.precision == i);
        assert(qjs_temporal_unit_nanoseconds(precision.unit) * precision.increment ==
               ((uint64_t[]){ 1000000000, 100000000, 10000000, 1000000, 100000,
                             10000, 1000, 100, 10, 1 })[i]);
    }
    assert(qjs_temporal_string_precision(&precision, QJS_TEMPORAL_HOUR, -1) == -1);
    assert(qjs_temporal_negate_rounding_mode(QJS_TEMPORAL_ROUND_HALF_CEIL) ==
           QJS_TEMPORAL_ROUND_HALF_FLOOR);
}

int main(void)
{
    test_checked_arithmetic();
    test_floor_division();
    test_rounding();
    test_gregorian_dates();
    test_civil_epoch_conversion();
    test_numeric_iso_parsing();
    test_complete_instant_parsing();
    test_full_width_increment();
    test_rounding_below_signed_minimum();
    test_lower_limit_after_offset();
    test_embedded_nul_and_unvalidated_annotation();
    test_exact_sized_iso_parser_buffers();
    test_shared_iso_grammar();
    test_iso_formatting();
    return 0;
}
