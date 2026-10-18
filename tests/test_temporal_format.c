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
#include <string.h>
#include "../src/temporal/format.h"
#include "../src/temporal/options.h"

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
    test_iso_formatting();
    return 0;
}
