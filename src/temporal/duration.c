/*
 * Portable Temporal duration arithmetic
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
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "duration.h"
#include "options.h"

static QJSTemporalEpochNs qjs_temporal_unsigned_negate(QJSTemporalEpochNs a)
{
    QJSTemporalEpochNs result;

    result.low = ~a.low + 1;
    result.high = ~a.high + (result.low == 0);
    return result;
}

static int qjs_temporal_unsigned_compare(QJSTemporalEpochNs a,
                                         QJSTemporalEpochNs b)
{
    if (a.high != b.high)
        return a.high > b.high ? 1 : -1;
    return (a.low > b.low) - (a.low < b.low);
}

static QJSTemporalEpochNs qjs_temporal_unsigned_subtract(QJSTemporalEpochNs a,
                                                         QJSTemporalEpochNs b)
{
    QJSTemporalEpochNs result;

    result.low = a.low - b.low;
    result.high = a.high - b.high - (a.low < b.low);
    return result;
}

static int qjs_temporal_unsigned_shift(QJSTemporalEpochNs *result,
                                       QJSTemporalEpochNs value, int count)
{
    QJSTemporalEpochNs output;

    if (count < 0 || count >= 128)
        return -1;
    if (count == 0) {
        output = value;
    } else if (count < 64) {
        if (value.high >> (64 - count))
            return -1;
        output.high = (value.high << count) | (value.low >> (64 - count));
        output.low = value.low << count;
    } else {
        if (value.high || (count > 64 && value.low >> (128 - count)))
            return -1;
        output.high = value.low << (count - 64);
        output.low = 0;
    }
    *result = output;
    return 0;
}

/* Unsigned long division also covers divisors spanning both words.
   The explicit carry permits a remainder shift past bit 127. */
static void qjs_temporal_unsigned_divide(QJSTemporalEpochNs *quotient,
                                         QJSTemporalEpochNs *remainder,
                                         QJSTemporalEpochNs numerator,
                                         QJSTemporalEpochNs denominator)
{
    QJSTemporalEpochNs q = { 0, 0 }, r = { 0, 0 };
    uint64_t bit;
    int i, carry;

    for (i = 127; i >= 0; i--) {
        bit = i >= 64 ? (numerator.high >> (i - 64)) & 1 :
            (numerator.low >> i) & 1;
        carry = (r.high >> 63) != 0;
        r.high = (r.high << 1) | (r.low >> 63);
        r.low = (r.low << 1) | bit;
        if (carry || qjs_temporal_unsigned_compare(r, denominator) >= 0) {
            r = qjs_temporal_unsigned_subtract(r, denominator);
            if (i >= 64)
                q.high |= UINT64_C(1) << (i - 64);
            else
                q.low |= UINT64_C(1) << i;
        }
    }
    *quotient = q;
    *remainder = r;
}

static int qjs_temporal_unsigned_bit_length(QJSTemporalEpochNs value)
{
    uint64_t word = value.high ? value.high : value.low;
    int result = value.high ? 64 : 0;

    while (word) {
        result++;
        word >>= 1;
    }
    return result;
}

int qjs_temporal_integer_from_double(QJSTemporalEpochNs *result, double value)
{
    QJSTemporalEpochNs output = { 0, 0 };
    uint64_t mantissa;
    int exponent, shift, negative = value < 0;
    double fraction;

    if (!isfinite(value) || trunc(value) != value)
        return -1;
    if (value == 0) {
        *result = output;
        return 0;
    }
    fraction = frexp(fabs(value), &exponent);
    if (exponent == 128 && negative && fraction == 0.5) {
        output.high = UINT64_C(1) << 63;
        *result = output;
        return 0;
    }
    if (exponent > 127)
        return -1;
    mantissa = (uint64_t)ldexp(fraction, 53);
    shift = exponent - 53;
    output.low = mantissa;
    if (shift < 0)
        output.low >>= -shift;
    else if (qjs_temporal_unsigned_shift(&output, output, shift))
        return -1;
    if (negative)
        output = qjs_temporal_unsigned_negate(output);
    *result = output;
    return 0;
}

/* Form the 53 significant bits and a discarded exact rational remainder,
   then round once to nearest, ties to even. Converting numerator first
   would introduce a second rounding in Duration.total(). */
double qjs_temporal_ratio_to_double(QJSTemporalEpochNs numerator,
                                    QJSTemporalEpochNs denominator)
{
    QJSTemporalEpochNs scaled_n, scaled_d, remainder, twice;
    int exponent, comparison, negative, i, carry;
    uint64_t mantissa;
    double result;

    if ((!denominator.low && !denominator.high) || denominator.high >> 63)
        return NAN;
    if (!numerator.low && !numerator.high)
        return 0;
    negative = (numerator.high >> 63) != 0;
    if (negative)
        numerator = qjs_temporal_unsigned_negate(numerator);
    exponent = qjs_temporal_unsigned_bit_length(numerator) -
        qjs_temporal_unsigned_bit_length(denominator);
    if (exponent >= 0) {
        if (qjs_temporal_unsigned_shift(&scaled_d, denominator, exponent))
            return NAN;
        if (qjs_temporal_unsigned_compare(numerator, scaled_d) < 0)
            exponent--;
    } else {
        if (qjs_temporal_unsigned_shift(&scaled_n, numerator, -exponent))
            return NAN;
        if (qjs_temporal_unsigned_compare(scaled_n, denominator) < 0)
            exponent--;
    }
    scaled_n = numerator;
    scaled_d = denominator;
    if (exponent < 0) {
        if (qjs_temporal_unsigned_shift(&scaled_n, numerator, -exponent))
            return NAN;
    } else if (qjs_temporal_unsigned_shift(&scaled_d, denominator, exponent)) {
        return NAN;
    }
    remainder = qjs_temporal_unsigned_subtract(scaled_n, scaled_d);
    mantissa = UINT64_C(1) << 52;
    for (i = 51; i >= 0; i--) {
        carry = (remainder.high >> 63) != 0;
        remainder.high = (remainder.high << 1) | (remainder.low >> 63);
        remainder.low <<= 1;
        if (carry || qjs_temporal_unsigned_compare(remainder, scaled_d) >= 0) {
            remainder = qjs_temporal_unsigned_subtract(remainder, scaled_d);
            mantissa |= UINT64_C(1) << i;
        }
    }
    if (qjs_temporal_unsigned_shift(&twice, remainder, 1))
        comparison = 1;
    else
        comparison = qjs_temporal_unsigned_compare(twice, scaled_d);
    if (comparison > 0 || (comparison == 0 && (mantissa & 1)))
        mantissa++;
    result = ldexp((double)mantissa, exponent - 52);
    return negative ? -result : result;
}

int qjs_temporal_duration_sign(const QJSTemporalDuration *duration)
{
    int i;

    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        if (duration->fields[i])
            return duration->fields[i] < 0 ? -1 : 1;
    }
    return 0;
}

QJSTemporalUnit qjs_temporal_duration_largest_unit(
                                                   const QJSTemporalDuration *duration)
{
    int i;

    for (i = QJS_TEMPORAL_YEAR; i < QJS_TEMPORAL_NANOSECOND; i++) {
        if (duration->fields[i])
            return i;
    }
    return QJS_TEMPORAL_NANOSECOND;
}

int qjs_temporal_time_duration_is_valid(QJSTemporalEpochNs value)
{
    QJSTemporalEpochNs limit;

    if (value.high >> 63)
        value = qjs_temporal_unsigned_negate(value);
    /* 2^53 * 10^9 = 0x77359 4000000000000000. */
    limit.low = UINT64_C(0x4000000000000000);
    limit.high = UINT64_C(0x77359);
    return qjs_temporal_unsigned_compare(value, limit) < 0;
}

int qjs_temporal_time_duration_round(QJSTemporalEpochNs *result,
                                     QJSTemporalEpochNs value,
                                     uint32_t increment, QJSTemporalUnit unit,
                                     QJSTemporalRoundingMode mode)
{
    QJSTemporalEpochNs divisor, magnitude, quotient, remainder, twice, rounded;
    uint64_t length = qjs_temporal_unit_nanoseconds(unit);
    int negative = (value.high >> 63) != 0, expand = 0, comparison;

    if (!length || !increment || increment > 1000000000 ||
        (unsigned)mode > QJS_TEMPORAL_ROUND_HALF_EVEN ||
        !qjs_temporal_time_duration_is_valid(value) ||
        qjs_temporal_epoch_ns_multiply(&divisor,
            qjs_temporal_epoch_ns_from_int64((int64_t)length),
            (int32_t)increment))
        return -1;
    magnitude = negative ? qjs_temporal_unsigned_negate(value) : value;
    qjs_temporal_unsigned_divide(&quotient, &remainder, magnitude, divisor);
    if (remainder.low || remainder.high) {
        if (mode == QJS_TEMPORAL_ROUND_CEIL)
            expand = !negative;
        else if (mode == QJS_TEMPORAL_ROUND_FLOOR)
            expand = negative;
        else if (mode == QJS_TEMPORAL_ROUND_EXPAND)
            expand = 1;
        else if (mode != QJS_TEMPORAL_ROUND_TRUNC) {
            if (qjs_temporal_unsigned_shift(&twice, remainder, 1))
                comparison = 1;
            else
                comparison = qjs_temporal_unsigned_compare(twice, divisor);
            if (comparison > 0)
                expand = 1;
            else if (comparison == 0) {
                if (mode == QJS_TEMPORAL_ROUND_HALF_CEIL)
                    expand = !negative;
                else if (mode == QJS_TEMPORAL_ROUND_HALF_FLOOR)
                    expand = negative;
                else if (mode == QJS_TEMPORAL_ROUND_HALF_EXPAND)
                    expand = 1;
                else if (mode == QJS_TEMPORAL_ROUND_HALF_EVEN)
                    expand = quotient.low & 1;
            }
        }
    }
    rounded = qjs_temporal_unsigned_subtract(magnitude, remainder);
    if (expand && qjs_temporal_epoch_ns_add(&rounded, rounded, divisor))
        return -1;
    if (negative)
        rounded = qjs_temporal_unsigned_negate(rounded);
    if (!qjs_temporal_time_duration_is_valid(rounded))
        return -1;
    *result = rounded;
    return 0;
}

int qjs_temporal_time_duration_add_days(QJSTemporalEpochNs *result,
                                        QJSTemporalEpochNs value, int64_t days)
{
    QJSTemporalEpochNs adjustment = qjs_temporal_epoch_ns_from_int64(days);
    QJSTemporalEpochNs output;

    if (qjs_temporal_epoch_ns_multiply(&adjustment, adjustment, 86400) ||
        qjs_temporal_epoch_ns_multiply(&adjustment, adjustment, 1000000000) ||
        qjs_temporal_epoch_ns_add(&output, value, adjustment) ||
        !qjs_temporal_time_duration_is_valid(output))
        return -1;
    *result = output;
    return 0;
}

static int qjs_temporal_duration_scaled_component(QJSTemporalEpochNs *result,
                                                  double component,
                                                  QJSTemporalUnit unit)
{
    QJSTemporalEpochNs value;
    uint64_t length = qjs_temporal_unit_nanoseconds(unit);

    if (qjs_temporal_integer_from_double(&value, component))
        return -1;
    if (length >= 1000000000) {
        if (qjs_temporal_epoch_ns_multiply(&value, value,
                                           (int32_t)(length / 1000000000)) ||
            qjs_temporal_epoch_ns_multiply(&value, value, 1000000000))
            return -1;
    } else if (qjs_temporal_epoch_ns_multiply(&value, value, (int32_t)length)) {
        return -1;
    }
    *result = value;
    return 0;
}

int qjs_temporal_duration_is_valid(const QJSTemporalDuration *duration)
{
    QJSTemporalEpochNs total = { 0, 0 }, value;
    int i, sign = 0, current;

    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        double field = duration->fields[i];

        if (!isfinite(field) || trunc(field) != field)
            return 0;
        current = (field > 0) - (field < 0);
        if (current) {
            if (sign && current != sign)
                return 0;
            sign = current;
        }
        if (i < QJS_TEMPORAL_DAY) {
            if (fabs(field) >= 4294967296.0)
                return 0;
        } else {
            if (qjs_temporal_duration_scaled_component(&value, field, i) ||
                qjs_temporal_epoch_ns_add(&total, total, value))
                return 0;
        }
    }
    return qjs_temporal_time_duration_is_valid(total);
}

int qjs_temporal_duration_normalize(QJSTemporalInternalDuration *result,
                                    const QJSTemporalDuration *duration,
                                    int with_24_hour_days)
{
    QJSTemporalInternalDuration output;
    QJSTemporalEpochNs value;
    int i;

    if (!qjs_temporal_duration_is_valid(duration))
        return -1;
    output.date.years = (int64_t)duration->fields[QJS_TEMPORAL_YEAR];
    output.date.months = (int64_t)duration->fields[QJS_TEMPORAL_MONTH];
    output.date.weeks = (int64_t)duration->fields[QJS_TEMPORAL_WEEK];
    output.date.days = with_24_hour_days ? 0 :
        (int64_t)duration->fields[QJS_TEMPORAL_DAY];
    output.time.low = output.time.high = 0;
    for (i = with_24_hour_days ? QJS_TEMPORAL_DAY : QJS_TEMPORAL_HOUR;
         i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        if (qjs_temporal_duration_scaled_component(&value, duration->fields[i],
                                                   i) ||
            qjs_temporal_epoch_ns_add(&output.time, output.time, value))
            return -1;
    }
    *result = output;
    return 0;
}

int qjs_temporal_duration_from_internal(QJSTemporalDuration *result,
                                        QJSTemporalInternalDuration value,
                                        QJSTemporalUnit largest_unit)
{
    static const uint64_t divisors[] = { 24, 60, 60, 1000, 1000, 1000 };
    QJSTemporalDuration output = { { 0 } };
    QJSTemporalEpochNs remainder, quotient;
    uint64_t r;
    int i, sign = value.time.high >> 63 ? -1 : 1;
    QJSTemporalUnit last = largest_unit < QJS_TEMPORAL_DAY ?
        QJS_TEMPORAL_DAY : largest_unit;

    if ((unsigned)largest_unit > QJS_TEMPORAL_NANOSECOND ||
        !qjs_temporal_time_duration_is_valid(value.time))
        return -1;
    remainder = sign < 0 ? qjs_temporal_unsigned_negate(value.time) : value.time;
    for (i = QJS_TEMPORAL_NANOSECOND; i > (int)last; i--) {
        if (qjs_temporal_epoch_ns_divide(&quotient, &r, remainder,
                                         divisors[i - QJS_TEMPORAL_HOUR]))
            return -1;
        output.fields[i] = r ? (double)r * sign : 0;
        remainder = quotient;
    }
    output.fields[last] = qjs_temporal_ratio_to_double(
        remainder, qjs_temporal_epoch_ns_from_int64(1)) * sign;
    output.fields[QJS_TEMPORAL_YEAR] = (double)value.date.years;
    output.fields[QJS_TEMPORAL_MONTH] = (double)value.date.months;
    output.fields[QJS_TEMPORAL_WEEK] = (double)value.date.weeks;
    output.fields[QJS_TEMPORAL_DAY] += (double)value.date.days;
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        if (!output.fields[i])
            output.fields[i] = 0;
    }
    if (!qjs_temporal_duration_is_valid(&output))
        return -1;
    *result = output;
    return 0;
}

static int qjs_temporal_duration_ascii_digit(char c)
{
    return c >= '0' && c <= '9';
}

int qjs_temporal_parse_duration(QJSTemporalDuration *result,
                                const char *text, size_t length)
{
    QJSTemporalDuration output = { { 0 } };
    QJSTemporalEpochNs integer, digit;
    uint64_t fraction, fraction_ns;
    size_t p = 0, start;
    int negative = 0, in_time = 0, any = 0, time_any = 0, last = -1;
    int fraction_digits, field, designator;

    if (p < length && (text[p] == '+' || text[p] == '-'))
        negative = text[p++] == '-';
    if (p == length || (text[p] != 'P' && text[p] != 'p'))
        return -1;
    p++;
    while (p < length) {
        if (text[p] == 'T' || text[p] == 't') {
            if (in_time)
                return -1;
            in_time = 1;
            p++;
            continue;
        }
        start = p;
        integer.low = integer.high = 0;
        while (p < length && qjs_temporal_duration_ascii_digit(text[p])) {
            digit = qjs_temporal_epoch_ns_from_int64(text[p++] - '0');
            if (qjs_temporal_epoch_ns_multiply(&integer, integer, 10) ||
                qjs_temporal_epoch_ns_add(&integer, integer, digit))
                return -1;
        }
        if (p == start)
            return -1;
        fraction = 0;
        fraction_digits = 0;
        if (p < length && (text[p] == '.' || text[p] == ',')) {
            p++;
            while (p < length && qjs_temporal_duration_ascii_digit(text[p])) {
                if (fraction_digits == 9)
                    return -1;
                fraction = fraction * 10 + (text[p++] - '0');
                fraction_digits++;
            }
            if (!fraction_digits)
                return -1;
        }
        if (p == length)
            return -1;
        designator = text[p++];
        if (designator >= 'a' && designator <= 'z')
            designator -= 'a' - 'A';
        if (!in_time) {
            field = designator == 'Y' ? QJS_TEMPORAL_YEAR :
                designator == 'M' ? QJS_TEMPORAL_MONTH :
                designator == 'W' ? QJS_TEMPORAL_WEEK :
                designator == 'D' ? QJS_TEMPORAL_DAY : -1;
            if (fraction_digits)
                return -1;
        } else {
            field = designator == 'H' ? QJS_TEMPORAL_HOUR :
                designator == 'M' ? QJS_TEMPORAL_MINUTE :
                designator == 'S' ? QJS_TEMPORAL_SECOND : -1;
            time_any = 1;
        }
        if (field < 0 || field <= last)
            return -1;
        output.fields[field] = qjs_temporal_ratio_to_double(
            integer, qjs_temporal_epoch_ns_from_int64(1));
        last = field;
        any = 1;
        if (fraction_digits) {
            if (p != length)
                return -1;
            while (fraction_digits++ < 9)
                fraction *= 10;
            fraction_ns = fraction *
                (qjs_temporal_unit_nanoseconds(field) / 1000000000);
            for (field++; field <= QJS_TEMPORAL_NANOSECOND; field++) {
                uint64_t divisor = qjs_temporal_unit_nanoseconds(field);

                output.fields[field] = (double)(fraction_ns / divisor);
                fraction_ns %= divisor;
            }
        }
    }
    if (!any || (in_time && !time_any))
        return -1;
    if (negative) {
        for (field = 0; field < QJS_TEMPORAL_UNIT_COUNT; field++) {
            if (output.fields[field])
                output.fields[field] = -output.fields[field];
        }
    }
    if (!qjs_temporal_duration_is_valid(&output))
        return -1;
    *result = output;
    return 0;
}

static int qjs_temporal_duration_put_integer(char *buffer, size_t capacity,
                                             size_t *length,
                                             QJSTemporalEpochNs value)
{
    char digits[40];
    QJSTemporalEpochNs quotient;
    uint64_t remainder;
    size_t n = 0;

    do {
        if (qjs_temporal_epoch_ns_divide(&quotient, &remainder, value, 10))
            return -1;
        digits[n++] = '0' + remainder;
        value = quotient;
    } while (value.low || value.high);
    if (n >= capacity - *length)
        return -1;
    while (n)
        buffer[(*length)++] = digits[--n];
    return 0;
}

int qjs_temporal_format_duration(char *buffer, size_t capacity,
                                 const QJSTemporalDuration *duration,
                                 int precision)
{
    static const char designators[] = { 'Y', 'M', 'W', 'D', 'H', 'M' };
    QJSTemporalEpochNs value, seconds, time = { 0, 0 };
    uint64_t remainder;
    char fraction[10];
    size_t p = 0;
    int i, digits, have_time = 0;

    if (capacity < 4 || precision < -1 || precision > 9 ||
        !qjs_temporal_duration_is_valid(duration))
        return -1;
    if (qjs_temporal_duration_sign(duration) < 0)
        buffer[p++] = '-';
    buffer[p++] = 'P';
    for (i = QJS_TEMPORAL_YEAR; i <= QJS_TEMPORAL_MINUTE; i++) {
        if (!duration->fields[i])
            continue;
        if (i >= QJS_TEMPORAL_HOUR && !have_time) {
            if (p + 1 >= capacity)
                return -1;
            buffer[p++] = 'T';
            have_time = 1;
        }
        if (qjs_temporal_integer_from_double(&value, fabs(duration->fields[i])) ||
            qjs_temporal_duration_put_integer(buffer, capacity, &p, value) ||
            p + 1 >= capacity)
            return -1;
        buffer[p++] = designators[i];
    }
    for (i = QJS_TEMPORAL_SECOND; i <= QJS_TEMPORAL_NANOSECOND; i++) {
        if (qjs_temporal_duration_scaled_component(&value,
                                                   fabs(duration->fields[i]),
                                                   i) ||
            qjs_temporal_epoch_ns_add(&time, time, value))
            return -1;
    }
    if (time.low || time.high || precision != -1 ||
        qjs_temporal_duration_largest_unit(duration) >= QJS_TEMPORAL_SECOND) {
        if (!have_time) {
            if (p + 1 >= capacity)
                return -1;
            buffer[p++] = 'T';
        }
        if (qjs_temporal_epoch_ns_divide(&seconds, &remainder, time, 1000000000) ||
            qjs_temporal_duration_put_integer(buffer, capacity, &p, seconds))
            return -1;
        digits = precision;
        if (digits == -1) {
            digits = remainder ? 9 : 0;
            while (digits && remainder % 10 == 0) {
                remainder /= 10;
                digits--;
            }
        } else {
            for (i = digits; i < 9; i++)
                remainder /= 10;
        }
        if (digits) {
            if (p + 1 + (size_t)digits >= capacity)
                return -1;
            for (i = digits - 1; i >= 0; i--) {
                fraction[i] = '0' + remainder % 10;
                remainder /= 10;
            }
            buffer[p++] = '.';
            memcpy(buffer + p, fraction, digits);
            p += digits;
        }
        if (p + 1 >= capacity)
            return -1;
        buffer[p++] = 'S';
    }
    buffer[p] = '\0';
    return (int)p;
}
