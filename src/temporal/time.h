/*
 * Portable Temporal civil clock interfaces
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
#ifndef QUICKJS_TEMPORAL_TIME_H
#define QUICKJS_TEMPORAL_TIME_H

#include <stddef.h>
#include "types.h"

/* Requires a valid ISO time. The inverse requires 0 <= ns < nsPerDay. */
int64_t qjs_temporal_time_nanoseconds(QJSTemporalISOTime time);
QJSTemporalISOTime qjs_temporal_time_from_nanoseconds(int64_t nanoseconds);
int qjs_temporal_time_add(QJSTemporalISOTime *result, int64_t *overflow_days,
                          QJSTemporalISOTime time,
                          QJSTemporalEpochNs duration);
int qjs_temporal_time_round(QJSTemporalISOTime *result,
                            QJSTemporalISOTime time, uint32_t increment,
                            QJSTemporalUnit unit,
                            QJSTemporalRoundingMode mode);
/* precision -2 omits seconds, -1 is auto, otherwise 0 through 9. */
int qjs_temporal_format_time(char *buffer, size_t capacity,
                             QJSTemporalISOTime time, int precision);

#endif /* QUICKJS_TEMPORAL_TIME_H */
