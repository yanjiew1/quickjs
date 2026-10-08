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
#include "epoch.h"

/* 100,000,000 days. This translation unit owns the epoch bounds. */
static const int64_t qjs_temporal_epoch_max_ms = INT64_C(8640000000000000);
static const QJSTemporalEpochNs qjs_temporal_epoch_max_ns = {
    UINT64_C(6923773503929843712), UINT64_C(468)
};

static QJSTemporalEpochNs qjs_temporal_epoch_ns_negate(QJSTemporalEpochNs value)
{
    value.low = ~value.low + 1;
    value.high = ~value.high + (value.low == 0);
    return value;
}

int qjs_temporal_epoch_ns_compare(QJSTemporalEpochNs a, QJSTemporalEpochNs b)
{
    int a_negative = a.high >> 63;
    int b_negative = b.high >> 63;

    if (a_negative != b_negative)
        return a_negative ? -1 : 1;
    if (a.high != b.high)
        return a.high < b.high ? -1 : 1;
    if (a.low != b.low)
        return a.low < b.low ? -1 : 1;
    return 0;
}

int qjs_temporal_epoch_ns_is_valid(QJSTemporalEpochNs value)
{
    QJSTemporalEpochNs minimum;

    minimum = qjs_temporal_epoch_ns_negate(qjs_temporal_epoch_max_ns);
    return qjs_temporal_epoch_ns_compare(value, minimum) >= 0 &&
        qjs_temporal_epoch_ns_compare(value, qjs_temporal_epoch_max_ns) <= 0;
}

int qjs_temporal_epoch_ns_from_milliseconds(QJSTemporalEpochNs *result,
                                          double milliseconds)
{
    QJSTemporalEpochNs value;
    uint64_t magnitude, product_low, product_high;
    int64_t integer_ms;

    /* Test the double before conversion: NaN and infinities fail these
       ordered comparisons, and every accepted cast is within int64_t. */
    if (!(milliseconds >= -qjs_temporal_epoch_max_ms &&
          milliseconds <= qjs_temporal_epoch_max_ms))
        return -1;
    integer_ms = (int64_t)milliseconds;
    if ((double)integer_ms != milliseconds)
        return -1;
    magnitude = integer_ms < 0 ? -integer_ms : integer_ms;
    product_low = (magnitude & UINT32_MAX) * UINT64_C(1000000);
    product_high = (magnitude >> 32) * UINT64_C(1000000) +
        (product_low >> 32);
    value.low = (product_high << 32) | (uint32_t)product_low;
    value.high = product_high >> 32;
    if (integer_ms < 0)
        value = qjs_temporal_epoch_ns_negate(value);
    *result = value;
    return 0;
}

int qjs_temporal_epoch_ns_to_milliseconds(int64_t *result,
                                        QJSTemporalEpochNs value)
{
    uint64_t quotient = 0, remainder = 0, dividend;
    uint32_t words[4];
    int64_t milliseconds;
    int i, is_negative;

    if (!qjs_temporal_epoch_ns_is_valid(value))
        return -1;
    is_negative = value.high >> 63;
    if (is_negative)
        value = qjs_temporal_epoch_ns_negate(value);
    words[0] = value.high >> 32;
    words[1] = value.high;
    words[2] = value.low >> 32;
    words[3] = value.low;
    for (i = 0; i < 4; i++) {
        dividend = (remainder << 32) | words[i];
        quotient = (quotient << 32) | (dividend / 1000000);
        remainder = dividend % 1000000;
    }
    /* The valid interval bounds the quotient by 8.64e15; every partial
       quotient fits in uint64_t, and remainder << 32 also fits. */
    milliseconds = quotient;
    if (is_negative)
        milliseconds = -milliseconds - (remainder != 0);
    *result = milliseconds;
    return 0;
}

QJSTemporalEpochNs qjs_temporal_epoch_ns_from_int64(int64_t value)
{
    QJSTemporalEpochNs result;

    result.low = (uint64_t)value;
    result.high = value < 0 ? UINT64_MAX : 0;
    return result;
}

int qjs_temporal_epoch_ns_to_int64(int64_t *result, QJSTemporalEpochNs value)
{
    uint64_t magnitude;
    int64_t integer;

    if (value.high == 0 && value.low <= INT64_MAX) {
        integer = (int64_t)value.low;
    } else if (value.high == UINT64_MAX && value.low >> 63) {
        magnitude = ~value.low + 1;
        integer = magnitude == (UINT64_C(1) << 63) ?
            INT64_MIN : -(int64_t)magnitude;
    } else {
        return -1;
    }
    *result = integer;
    return 0;
}

int qjs_temporal_epoch_ns_add(QJSTemporalEpochNs *result,
                              QJSTemporalEpochNs a, QJSTemporalEpochNs b)
{
    QJSTemporalEpochNs sum;
    int a_negative = a.high >> 63;
    int b_negative = b.high >> 63;

    sum.low = a.low + b.low;
    sum.high = a.high + b.high + (sum.low < a.low);
    if (a_negative == b_negative && (sum.high >> 63) != a_negative)
        return -1;
    *result = sum;
    return 0;
}

int qjs_temporal_epoch_ns_subtract(QJSTemporalEpochNs *result,
                                   QJSTemporalEpochNs a, QJSTemporalEpochNs b)
{
    QJSTemporalEpochNs difference;
    int a_negative = a.high >> 63;
    int b_negative = b.high >> 63;

    difference.low = a.low - b.low;
    difference.high = a.high - b.high - (a.low < b.low);
    if (a_negative != b_negative && (difference.high >> 63) != a_negative)
        return -1;
    *result = difference;
    return 0;
}

int qjs_temporal_epoch_ns_multiply(QJSTemporalEpochNs *result,
                                   QJSTemporalEpochNs value, int32_t factor)
{
    uint32_t words[4], multiplier;
    uint64_t carry = 0, product;
    int negative, i;

    negative = (value.high >> 63) != (factor < 0);
    if (value.high >> 63)
        value = qjs_temporal_epoch_ns_negate(value);
    multiplier = factor < 0 ? -(int64_t)factor : factor;
    words[0] = value.low;
    words[1] = value.low >> 32;
    words[2] = value.high;
    words[3] = value.high >> 32;
    for (i = 0; i < 4; i++) {
        product = (uint64_t)words[i] * multiplier + carry;
        words[i] = product;
        carry = product >> 32;
    }
    if (carry)
        return -1;
    value.low = words[0] | ((uint64_t)words[1] << 32);
    value.high = words[2] | ((uint64_t)words[3] << 32);
    if (negative) {
        if (value.high > (UINT64_C(1) << 63) ||
            (value.high == (UINT64_C(1) << 63) && value.low != 0))
            return -1;
        value = qjs_temporal_epoch_ns_negate(value);
    } else if (value.high >> 63) {
        return -1;
    }
    *result = value;
    return 0;
}

/* Binary long division avoids a 128-bit native type and does not require
   doubling the remainder to fit in 64 bits. Carry represents bit 64. */
static void qjs_temporal_epoch_ns_unsigned_divide(
    QJSTemporalEpochNs *quotient, uint64_t *remainder,
    QJSTemporalEpochNs value, uint64_t divisor)
{
    QJSTemporalEpochNs q = { 0, 0 };
    uint64_t r = 0, bit, carry, subtract;
    int i;

    if (divisor == 1) {
        *quotient = value;
        *remainder = 0;
        return;
    }
    if (value.high == 0) {
        q.low = value.low / divisor;
        *quotient = q;
        *remainder = value.low % divisor;
        return;
    }
    for (i = 127; i >= 0; i--) {
        bit = i >= 64 ? (value.high >> (i - 64)) & 1 :
            (value.low >> i) & 1;
        carry = r >> 63;
        r = (r << 1) | bit;
        subtract = carry || r >= divisor;
        if (subtract)
            r -= divisor;
        q.high = (q.high << 1) | (q.low >> 63);
        q.low = (q.low << 1) | subtract;
    }
    *quotient = q;
    *remainder = r;
}

int qjs_temporal_epoch_ns_divide(QJSTemporalEpochNs *quotient,
                                 uint64_t *remainder,
                                 QJSTemporalEpochNs value, uint64_t divisor)
{
    QJSTemporalEpochNs q;
    uint64_t r;
    int negative = value.high >> 63;

    if (divisor == 0)
        return -1;
    if (negative)
        value = qjs_temporal_epoch_ns_negate(value);
    qjs_temporal_epoch_ns_unsigned_divide(&q, &r, value, divisor);
    if (negative) {
        if (r != 0) {
            q.low++;
            q.high += q.low == 0;
            r = divisor - r;
        }
        q = qjs_temporal_epoch_ns_negate(q);
    }
    *quotient = q;
    *remainder = r;
    return 0;
}

static int qjs_temporal_epoch_ns_round_internal(
    QJSTemporalEpochNs *result, QJSTemporalEpochNs value, uint64_t increment,
    QJSTemporalRoundingMode mode, int as_if_positive)
{
    QJSTemporalEpochNs quotient, adjustment;
    uint64_t remainder;
    int negative, round_up = 0;

    if (increment == 0 || (unsigned)mode > QJS_TEMPORAL_ROUND_HALF_EVEN)
        return -1;
    qjs_temporal_epoch_ns_divide(&quotient, &remainder, value, increment);
    if (remainder == 0) {
        *result = value;
        return 0;
    }
    negative = !as_if_positive && (value.high >> 63);
    switch (mode) {
    case QJS_TEMPORAL_ROUND_CEIL:
        round_up = 1;
        break;
    case QJS_TEMPORAL_ROUND_FLOOR:
        break;
    case QJS_TEMPORAL_ROUND_EXPAND:
        round_up = !negative;
        break;
    case QJS_TEMPORAL_ROUND_TRUNC:
        round_up = negative;
        break;
    default:
        if (remainder > increment - remainder) {
            round_up = 1;
        } else if (remainder == increment - remainder) {
            switch (mode) {
            case QJS_TEMPORAL_ROUND_HALF_CEIL:
                round_up = 1;
                break;
            case QJS_TEMPORAL_ROUND_HALF_FLOOR:
                break;
            case QJS_TEMPORAL_ROUND_HALF_EXPAND:
                round_up = !negative;
                break;
            case QJS_TEMPORAL_ROUND_HALF_TRUNC:
                round_up = negative;
                break;
            case QJS_TEMPORAL_ROUND_HALF_EVEN:
                round_up = quotient.low & 1;
                break;
            default:
                break;
            }
        }
        break;
    }
    adjustment.high = 0;
    if (round_up) {
        adjustment.low = increment - remainder;
        return qjs_temporal_epoch_ns_add(result, value, adjustment);
    }
    adjustment.low = remainder;
    return qjs_temporal_epoch_ns_subtract(result, value, adjustment);
}

int qjs_temporal_epoch_ns_round(QJSTemporalEpochNs *result,
                                QJSTemporalEpochNs value, uint64_t increment,
                                QJSTemporalRoundingMode mode)
{
    return qjs_temporal_epoch_ns_round_internal(result, value, increment,
                                               mode, 0);
}

int qjs_temporal_epoch_ns_round_as_if_positive(
    QJSTemporalEpochNs *result, QJSTemporalEpochNs value, uint64_t increment,
    QJSTemporalRoundingMode mode)
{
    return qjs_temporal_epoch_ns_round_internal(result, value, increment,
                                               mode, 1);
}
