/* NumberFormat scale-before-round policy; PluralRules uses its own policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_NUMBER_ROUND_H
#define QJS_INTL_NUMBER_ROUND_H
#include "number.h"

typedef struct QJSIntlCompactScale {
    uint32_t magnitude, exponent;
} QJSIntlCompactScale;

typedef struct QJSIntlNumberRounding {
    QJSIntlDecimalOptions digits;
    QJSIntlNumberNotation notation;
    unsigned int percent; /* Scale the mathematical input by 100 first. */
    /* COMPACT only: increasing thresholds selected by the native formatter
       from the chosen standard/alpha family before scaling. Exponent zero
       retains an explicit CLDR "0" fallback. Below the first threshold, or
       with no thresholds, use exponent zero. No plural selection happens here. */
    const QJSIntlCompactScale *compact;
    size_t compact_count;
} QJSIntlNumberRounding;

typedef struct QJSIntlRoundedNumber {
    QJSIntlMathematicalKind kind;
    /* Final ComputeExponentForMagnitude input; zero when no lookup occurs. */
    int64_t notation_magnitude;
    int32_t exponent;            /* Notation exponent; does not include percent scaling. */
    QJSIntlDecimalResult number; /* Owned; zeroed for NaN and infinities. */
} QJSIntlRoundedNumber;

/* PartitionNumberPattern and ComputeExponent/ForMagnitude over exact values.
   ComputeExponent probes the positive magnitude, then actual signed rounding
   applies to the original value at the final scale. This distinction matters
   for directed rounding. Preserve negative zero, including rounded zero.

   Borrow inputs unchanged; fresh, disjoint output required. Errors reset it;
   clear using the creating malloc/free allocator. No binary floating point,
   JS coercion, digit-option defaults, localized text or plural selection. */
QJSIntlStatus qjs_intl_number_round(const QJSIntlAllocator *, const QJSIntlNumberRounding *,
                                    const QJSIntlMathematicalValue *, QJSIntlRoundedNumber *);
void qjs_intl_number_round_clear(const QJSIntlAllocator *, QJSIntlRoundedNumber *);
#endif
