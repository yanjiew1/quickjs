/*
 * Portable Temporal epoch arithmetic
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
#ifndef QUICKJS_TEMPORAL_EPOCH_H
#define QUICKJS_TEMPORAL_EPOCH_H

#include <stdint.h>

/* Signed two's-complement nanoseconds; the word order is independent of
   machine byte order. No signed 128-bit type is required. */
typedef struct QJSTemporalEpochNs {
    uint64_t low;
    uint64_t high;
} QJSTemporalEpochNs;

/* Compare any two signed 128-bit word pairs, returning -1, 0, or 1. */
int qjs_temporal_epoch_ns_compare(QJSTemporalEpochNs a, QJSTemporalEpochNs b);

/* Test the inclusive Temporal epoch interval of +/- 100,000,000 days. */
int qjs_temporal_epoch_ns_is_valid(QJSTemporalEpochNs value);

/* Convert a finite integral native double in the valid millisecond range.
   Return 0 on success, -1 on invalid input; failure leaves *result intact.
   The permitted range is below 2^53, so its integers are exactly representable
   on the IEEE binary64 platforms used by QuickJS. */
int qjs_temporal_epoch_ns_from_milliseconds(QJSTemporalEpochNs *result,
                                          double milliseconds);

/* Floor a valid epoch to integer milliseconds, including negative fractions.
   Return 0 on success, -1 outside the epoch range, leaving *result intact.
   Both conversion functions require a non-null result pointer. */
int qjs_temporal_epoch_ns_to_milliseconds(int64_t *result,
                                        QJSTemporalEpochNs value);

typedef enum QJSTemporalRoundingMode {
    QJS_TEMPORAL_ROUND_CEIL,
    QJS_TEMPORAL_ROUND_FLOOR,
    QJS_TEMPORAL_ROUND_EXPAND,
    QJS_TEMPORAL_ROUND_TRUNC,
    QJS_TEMPORAL_ROUND_HALF_CEIL,
    QJS_TEMPORAL_ROUND_HALF_FLOOR,
    QJS_TEMPORAL_ROUND_HALF_EXPAND,
    QJS_TEMPORAL_ROUND_HALF_TRUNC,
    QJS_TEMPORAL_ROUND_HALF_EVEN,
} QJSTemporalRoundingMode;

QJSTemporalEpochNs qjs_temporal_epoch_ns_from_int64(int64_t value);
int qjs_temporal_epoch_ns_to_int64(int64_t *result, QJSTemporalEpochNs value);

/* Checked arithmetic over the whole signed 128-bit word interval. These
   operations do not apply the narrower Instant range policy. Return -1
   on overflow, leaving *result unchanged; all result pointers are required. */
int qjs_temporal_epoch_ns_add(QJSTemporalEpochNs *result,
                              QJSTemporalEpochNs a, QJSTemporalEpochNs b);
int qjs_temporal_epoch_ns_subtract(QJSTemporalEpochNs *result,
                                   QJSTemporalEpochNs a, QJSTemporalEpochNs b);
int qjs_temporal_epoch_ns_multiply(QJSTemporalEpochNs *result,
                                   QJSTemporalEpochNs value, int32_t factor);

/* Divide by a positive native integer: value = quotient * divisor +
   remainder, with 0 <= remainder < divisor. Return -1 for divisor zero,
   leaving both outputs unchanged. The quotient is mathematical floor.
   Both required output pointers must refer to nonoverlapping storage. */
int qjs_temporal_epoch_ns_divide(QJSTemporalEpochNs *quotient,
                                 uint64_t *remainder,
                                 QJSTemporalEpochNs value, uint64_t divisor);

/* Round to a positive integer multiple. Invalid mode, zero increment or
   signed 128-bit overflow returns -1 without changing *result. */
int qjs_temporal_epoch_ns_round(QJSTemporalEpochNs *result,
                                QJSTemporalEpochNs value, uint64_t increment,
                                QJSTemporalRoundingMode mode);

/* Exact-time rounding uses the positive rounding direction even for epochs
   before 1970. Instant's RoundTemporalInstant requires this variant. */
int qjs_temporal_epoch_ns_round_as_if_positive(
    QJSTemporalEpochNs *result, QJSTemporalEpochNs value, uint64_t increment,
    QJSTemporalRoundingMode mode);

#endif /* QUICKJS_TEMPORAL_EPOCH_H */
