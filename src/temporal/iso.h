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
#ifndef QUICKJS_TEMPORAL_ISO_H
#define QUICKJS_TEMPORAL_ISO_H

#include <stdint.h>
#include "epoch.h"

typedef struct QJSTemporalISODate {
    int32_t year;
    int32_t month;
    int32_t day;
} QJSTemporalISODate;

typedef struct QJSTemporalISOTime {
    int32_t hour;
    int32_t minute;
    int32_t second;
    int32_t millisecond;
    int32_t microsecond;
    int32_t nanosecond;
} QJSTemporalISOTime;

typedef struct QJSTemporalISODateTime {
    QJSTemporalISODate date;
    QJSTemporalISOTime time;
} QJSTemporalISODateTime;

int qjs_temporal_iso_date_is_valid(QJSTemporalISODate date);
int qjs_temporal_iso_time_is_valid(QJSTemporalISOTime time);

/* Gregorian conversion covers the complete int32_t year interval. Days
   are measured from 1970-01-01. Invalid inputs return -1 and preserve the
   required non-null output; no Instant or PlainDate range policy is applied. */
int qjs_temporal_iso_date_to_days(int64_t *result, QJSTemporalISODate date);
int qjs_temporal_iso_date_from_days(QJSTemporalISODate *result, int64_t days);

/* Convert exact civil fields to/from an unzoned UTC nanosecond pair. The
   Gregorian year bounds are checked, but the narrower Instant range is a
   caller policy applied after any numeric UTC-offset adjustment. */
int qjs_temporal_iso_datetime_to_epoch_ns(QJSTemporalEpochNs *result,
                                          QJSTemporalISODateTime datetime);
int qjs_temporal_iso_datetime_from_epoch_ns(QJSTemporalISODateTime *result,
                                            QJSTemporalEpochNs value);

#endif /* QUICKJS_TEMPORAL_ISO_H */
