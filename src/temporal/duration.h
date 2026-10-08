/*
 * Portable Temporal duration arithmetic interfaces
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
#ifndef QUICKJS_TEMPORAL_DURATION_H
#define QUICKJS_TEMPORAL_DURATION_H

#include <stddef.h>
#include "types.h"

int qjs_temporal_integer_from_double(QJSTemporalEpochNs *result, double value);
/* One correctly rounded binary64 conversion of the exact signed ratio.
   denominator must be a strictly positive signed 128-bit integer. */
double qjs_temporal_ratio_to_double(QJSTemporalEpochNs numerator,
                                    QJSTemporalEpochNs denominator);
int qjs_temporal_duration_sign(const QJSTemporalDuration *duration);
QJSTemporalUnit qjs_temporal_duration_largest_unit(
                                                   const QJSTemporalDuration *duration);
int qjs_temporal_duration_is_valid(const QJSTemporalDuration *duration);
int qjs_temporal_duration_normalize(QJSTemporalInternalDuration *result,
                                    const QJSTemporalDuration *duration,
                                    int with_24_hour_days);
int qjs_temporal_duration_from_internal(QJSTemporalDuration *result,
                                        QJSTemporalInternalDuration value,
                                        QJSTemporalUnit largest_unit);
/* Duration's open bound is 10^9 * 2^53 nanoseconds. */
int qjs_temporal_time_duration_is_valid(QJSTemporalEpochNs value);
/* The day increment may exceed uint64_t nanoseconds. */
int qjs_temporal_time_duration_round(QJSTemporalEpochNs *result,
                                     QJSTemporalEpochNs value,
                                     uint32_t increment, QJSTemporalUnit unit,
                                     QJSTemporalRoundingMode mode);
int qjs_temporal_time_duration_add_days(QJSTemporalEpochNs *result,
                                        QJSTemporalEpochNs value, int64_t days);
int qjs_temporal_parse_duration(QJSTemporalDuration *result,
                                const char *text, size_t length);
/* precision -1 is auto; otherwise 0 through 9. Required buffer capacity
   512 covers every permitted field without exponential notation. */
int qjs_temporal_format_duration(char *buffer, size_t capacity,
                                 const QJSTemporalDuration *duration,
                                 int precision);

#endif /* QUICKJS_TEMPORAL_DURATION_H */
