/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "decimal.h"
#include "../../temporal/duration.h"
#include <string.h>

QJSIntlStatus qjs_intl_duration_component_decimal(const QJSTemporalDuration *duration, QJSTemporalUnit unit,
                                                 int fractional, char *buffer, size_t capacity,
                                                 QJSIntlMathematicalValue *out)
{
    QJSTemporalEpochNs value = {0}, field, quotient;
    QJSTemporalUnit last;
    char reversed[40];
    size_t count = 0, scale, length, at = 0, i;
    uint64_t remainder;
    int negative, current;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!duration || !buffer || !capacity || !out ||
        unit < QJS_TEMPORAL_YEAR || unit >= QJS_TEMPORAL_UNIT_COUNT ||
        (fractional && (unit < QJS_TEMPORAL_SECOND || unit >= QJS_TEMPORAL_NANOSECOND)))
        return QJS_INTL_INVALID_ARGUMENT;
    last = fractional ? QJS_TEMPORAL_NANOSECOND : unit;
    scale = (size_t)(last - unit) * 3;
    for (current = unit; current <= (int)last; current++) {
        if (qjs_temporal_integer_from_double(&field, duration->fields[current]))
            return QJS_INTL_INVALID_ARGUMENT;
        /* Durations can contain exact binary64 integers larger than int64_t.
           Combine them as mathematical integers before applying decimal scale. */
        if (qjs_temporal_epoch_ns_multiply(&value, value, 1000) ||
            qjs_temporal_epoch_ns_add(&value, value, field))
            return QJS_INTL_OVERFLOW;
    }
    negative = value.high >> 63;
    if (negative && qjs_temporal_epoch_ns_subtract(&value, (QJSTemporalEpochNs){0}, value))
        return QJS_INTL_OVERFLOW;
    do {
        if (qjs_temporal_epoch_ns_divide(&quotient, &remainder, value, 10))
            return QJS_INTL_OVERFLOW;
        reversed[count++] = (char)('0' + remainder);
        value = quotient;
    } while (value.low || value.high);
    length = (size_t)negative + (count > scale ? count : scale + 1) + (scale != 0);
    if (length >= capacity)
        return QJS_INTL_OVERFLOW;
    if (negative)
        buffer[at++] = '-';
    if (count <= scale) {
        buffer[at++] = '0';
        buffer[at++] = '.';
        for (i = scale; i > count; i--)
            buffer[at++] = '0';
    }
    for (i = count; i; i--) {
        buffer[at++] = reversed[i - 1];
        if (i - 1 == scale && scale)
            buffer[at++] = '.';
    }
    buffer[at] = 0;
    out->decimal = (QJSIntlBytes){buffer, at};
    return QJS_INTL_OK;
}
