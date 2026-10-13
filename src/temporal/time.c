/*
 * Portable Temporal civil clock arithmetic
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
#include <stdio.h>
#include <string.h>
#include "duration.h"
#include "options.h"
#include "time.h"

int64_t qjs_temporal_time_nanoseconds(QJSTemporalISOTime time)
{
    return (((((int64_t)time.hour * 60 + time.minute) * 60 + time.second) *
               1000 + time.millisecond) * 1000 + time.microsecond) * 1000 +
        time.nanosecond;
}

QJSTemporalISOTime qjs_temporal_time_from_nanoseconds(int64_t nanoseconds)
{
    QJSTemporalISOTime result;

    result.nanosecond = nanoseconds % 1000;
    nanoseconds /= 1000;
    result.microsecond = nanoseconds % 1000;
    nanoseconds /= 1000;
    result.millisecond = nanoseconds % 1000;
    nanoseconds /= 1000;
    result.second = nanoseconds % 60;
    nanoseconds /= 60;
    result.minute = nanoseconds % 60;
    result.hour = nanoseconds / 60;
    return result;
}

int qjs_temporal_time_add(QJSTemporalISOTime *result, int64_t *overflow_days,
                          QJSTemporalISOTime time,
                          QJSTemporalEpochNs duration)
{
    QJSTemporalEpochNs combined, days;
    uint64_t remainder;
    int64_t delta;

    if (!qjs_temporal_iso_time_is_valid(time) ||
        qjs_temporal_epoch_ns_add(&combined, duration,
            qjs_temporal_epoch_ns_from_int64(
                qjs_temporal_time_nanoseconds(time))) ||
        qjs_temporal_epoch_ns_divide(&days, &remainder, combined,
                                      UINT64_C(86400000000000)) ||
        qjs_temporal_epoch_ns_to_int64(&delta, days))
        return -1;
    *result = qjs_temporal_time_from_nanoseconds((int64_t)remainder);
    *overflow_days = delta;
    return 0;
}

int qjs_temporal_time_round(QJSTemporalISOTime *result,
                            QJSTemporalISOTime time, uint32_t increment,
                            QJSTemporalUnit unit,
                            QJSTemporalRoundingMode mode)
{
    QJSTemporalEpochNs rounded;
    uint64_t divisor = qjs_temporal_unit_nanoseconds(unit), remainder;
    QJSTemporalEpochNs quotient;

    if (!qjs_temporal_iso_time_is_valid(time) || !increment ||
        unit < QJS_TEMPORAL_HOUR || unit > QJS_TEMPORAL_NANOSECOND ||
        divisor > UINT64_MAX / increment ||
        qjs_temporal_epoch_ns_round(&rounded,
            qjs_temporal_epoch_ns_from_int64(
                qjs_temporal_time_nanoseconds(time)),
            divisor * increment, mode) ||
        qjs_temporal_epoch_ns_divide(&quotient, &remainder, rounded,
                                      UINT64_C(86400000000000)))
        return -1;
    *result = qjs_temporal_time_from_nanoseconds((int64_t)remainder);
    return 0;
}

int qjs_temporal_format_time(char *buffer, size_t capacity,
                             QJSTemporalISOTime time, int precision)
{
    unsigned fraction;
    char digits[10];
    int p, count, i;

    if (!qjs_temporal_iso_time_is_valid(time) || precision < -2 || precision > 9)
        return -1;
    if (precision == -2)
        p = snprintf(buffer, capacity, "%02d:%02d", time.hour, time.minute);
    else
        p = snprintf(buffer, capacity, "%02d:%02d:%02d", time.hour,
                     time.minute, time.second);
    if (p < 0 || (size_t)p >= capacity)
        return -1;
    if (precision == -2)
        return p;
    fraction = time.millisecond * 1000000 + time.microsecond * 1000 +
        time.nanosecond;
    count = precision;
    if (count == -1) {
        count = fraction ? 9 : 0;
        while (count && fraction % 10 == 0) {
            fraction /= 10;
            count--;
        }
    } else {
        for (i = count; i < 9; i++)
            fraction /= 10;
    }
    if (count) {
        if ((size_t)p + count + 1 >= capacity)
            return -1;
        for (i = count - 1; i >= 0; i--) {
            digits[i] = '0' + fraction % 10;
            fraction /= 10;
        }
        buffer[p++] = '.';
        memcpy(buffer + p, digits, count);
        p += count;
    }
    buffer[p] = '\0';
    return p;
}
