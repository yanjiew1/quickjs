/*
 * Native Temporal support
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
#ifndef QUICKJS_TEMPORAL_FORMAT_H
#define QUICKJS_TEMPORAL_FORMAT_H

#include <stddef.h>
#include "types.h"

/* Return the byte count excluding the terminator, or -1 for invalid
   native fields / insufficient capacity. Failure leaves output intact.
   Precision is -1 (auto), -2 (minute), or a digit count from zero to nine. */
int qjs_temporal_format_date_time(char *output, size_t capacity,
                                 QJSTemporalISODateTime datetime,
                                 int precision);
int qjs_temporal_format_utc_offset(char *output, size_t capacity,
                                  int64_t nanoseconds, int round_to_minutes);

#endif /* QUICKJS_TEMPORAL_FORMAT_H */
