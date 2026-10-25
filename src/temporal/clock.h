/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* Host UTC clock, shared by Date and Temporal without timezone lookup. */
#ifndef QUICKJS_TEMPORAL_CLOCK_H
#define QUICKJS_TEMPORAL_CLOCK_H

#include "types.h"

/* Normalize floor seconds plus 0..999999999 nanoseconds and clamp to
   Temporal's Instant range. Errors preserve the output. */
int qjs_temporal_epoch_from_clock(QJSTemporalEpochNs *result,
                                  int64_t seconds, int32_t nanoseconds);
/* Equivalent to flooring the clamped exact epoch, without constructing or
   dividing a two-word value. Errors preserve the output. */
int qjs_temporal_milliseconds_from_clock(int64_t *result,
                                         int64_t seconds, int32_t nanoseconds);
int qjs_temporal_system_epoch(QJSTemporalEpochNs *result);
int qjs_temporal_system_milliseconds(int64_t *result);

#endif /* QUICKJS_TEMPORAL_CLOCK_H */
