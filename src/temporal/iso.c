/*
 * Portable Temporal ISO Gregorian arithmetic
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
#include "iso.h"

int qjs_temporal_iso_time_is_valid(QJSTemporalISOTime time)
{
    return time.hour >= 0 && time.hour <= 23 &&
        time.minute >= 0 && time.minute <= 59 &&
        time.second >= 0 && time.second <= 59 &&
        time.millisecond >= 0 && time.millisecond <= 999 &&
        time.microsecond >= 0 && time.microsecond <= 999 &&
        time.nanosecond >= 0 && time.nanosecond <= 999;
}

int qjs_temporal_iso_datetime_to_epoch_ns(QJSTemporalEpochNs *result,
                                          QJSTemporalISODateTime datetime)
{
    QJSTemporalEpochNs date_ns, time_ns, value;
    QJSTemporalISOTime time = datetime.time;
    int64_t days, nanoseconds;

    if (!qjs_temporal_iso_time_is_valid(time) ||
        qjs_iso_date_to_days(&days, datetime.date))
        return -1;
    date_ns = qjs_temporal_epoch_ns_from_int64(days);
    if (qjs_temporal_epoch_ns_multiply(&date_ns, date_ns, 86400) ||
        qjs_temporal_epoch_ns_multiply(&date_ns, date_ns, 1000000000))
        return -1;
    nanoseconds = (((int64_t)time.hour * 60 + time.minute) * 60 +
                   time.second) * INT64_C(1000000000) +
        (int64_t)time.millisecond * 1000000 +
        time.microsecond * 1000 + time.nanosecond;
    time_ns = qjs_temporal_epoch_ns_from_int64(nanoseconds);
    if (qjs_temporal_epoch_ns_add(&value, date_ns, time_ns))
        return -1;
    *result = value;
    return 0;
}

int qjs_temporal_iso_datetime_from_epoch_ns(QJSTemporalISODateTime *result,
                                            QJSTemporalEpochNs value)
{
    QJSTemporalISODateTime datetime;
    QJSTemporalEpochNs day_count;
    uint64_t within_day;
    int64_t days;

    qjs_temporal_epoch_ns_divide(&day_count, &within_day, value,
                                 UINT64_C(86400000000000));
    if (qjs_temporal_epoch_ns_to_int64(&days, day_count) ||
        qjs_iso_date_from_days(&datetime.date, days))
        return -1;
    datetime.time.hour = within_day / UINT64_C(3600000000000);
    within_day %= UINT64_C(3600000000000);
    datetime.time.minute = within_day / UINT64_C(60000000000);
    within_day %= UINT64_C(60000000000);
    datetime.time.second = within_day / UINT64_C(1000000000);
    within_day %= UINT64_C(1000000000);
    datetime.time.millisecond = within_day / 1000000;
    datetime.time.microsecond = within_day / 1000 % 1000;
    datetime.time.nanosecond = within_day % 1000;
    *result = datetime;
    return 0;
}
