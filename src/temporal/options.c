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
#include "options.h"

uint64_t qjs_temporal_unit_nanoseconds(QJSTemporalUnit unit)
{
    switch (unit) {
    case QJS_TEMPORAL_DAY: return UINT64_C(86400000000000);
    case QJS_TEMPORAL_HOUR: return UINT64_C(3600000000000);
    case QJS_TEMPORAL_MINUTE: return UINT64_C(60000000000);
    case QJS_TEMPORAL_SECOND: return UINT64_C(1000000000);
    case QJS_TEMPORAL_MILLISECOND: return UINT64_C(1000000);
    case QJS_TEMPORAL_MICROSECOND: return UINT64_C(1000);
    case QJS_TEMPORAL_NANOSECOND: return 1;
    default: return 0;
    }
}

QJSTemporalRoundingMode qjs_temporal_negate_rounding_mode(
    QJSTemporalRoundingMode mode)
{
    switch (mode) {
    case QJS_TEMPORAL_ROUND_CEIL: return QJS_TEMPORAL_ROUND_FLOOR;
    case QJS_TEMPORAL_ROUND_FLOOR: return QJS_TEMPORAL_ROUND_CEIL;
    case QJS_TEMPORAL_ROUND_HALF_CEIL: return QJS_TEMPORAL_ROUND_HALF_FLOOR;
    case QJS_TEMPORAL_ROUND_HALF_FLOOR: return QJS_TEMPORAL_ROUND_HALF_CEIL;
    default: return mode;
    }
}

int qjs_temporal_string_precision(QJSTemporalStringPrecision *result,
                                  QJSTemporalUnit unit, int digits)
{
    QJSTemporalStringPrecision precision;

    if (digits < -1 || digits > 9)
        return -1;
    precision.increment = 1;
    switch (unit) {
    case QJS_TEMPORAL_MINUTE:
        precision.precision = -2;
        break;
    case QJS_TEMPORAL_SECOND:
        precision.precision = 0;
        break;
    case QJS_TEMPORAL_MILLISECOND:
        precision.precision = 3;
        break;
    case QJS_TEMPORAL_MICROSECOND:
        precision.precision = 6;
        break;
    case QJS_TEMPORAL_NANOSECOND:
        precision.precision = 9;
        break;
    case QJS_TEMPORAL_UNIT_UNSET:
        precision.precision = digits;
        if (digits == -1)
            unit = QJS_TEMPORAL_NANOSECOND;
        else if (digits == 0)
            unit = QJS_TEMPORAL_SECOND;
        else {
            int remaining = (3 - digits % 3) % 3;
            unit = digits <= 3 ? QJS_TEMPORAL_MILLISECOND :
                   digits <= 6 ? QJS_TEMPORAL_MICROSECOND :
                                 QJS_TEMPORAL_NANOSECOND;
            while (remaining--)
                precision.increment *= 10;
        }
        break;
    default:
        return -1;
    }
    precision.unit = unit;
    *result = precision;
    return 0;
}
