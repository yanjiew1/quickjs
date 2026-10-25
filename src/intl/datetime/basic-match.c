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
#include "basic-match.h"

QJSIntlStatus qjs_intl_datetime_basic_score(const QJSIntlDateTimeComponents *requested,
                                            const QJSIntlDateTimeComponents *candidate, int *out)
{
    /* Legal widths in DateTimeFormat Components, not DisplayNames fields or
       CLDR pattern symbols. Fractional precision and timezone styles have
       separate value domains despite sharing this compact record. */
    enum {
        NUMERIC_WIDTHS = (1u << QJS_INTL_DATETIME_TWO_DIGIT) | (1u << QJS_INTL_DATETIME_NUMERIC),
        TEXT_WIDTHS = (1u << QJS_INTL_DATETIME_NARROW) | (1u << QJS_INTL_DATETIME_SHORT) |
                      (1u << QJS_INTL_DATETIME_LONG)
    };
    static const unsigned int masks[QJS_INTL_DATETIME_COMPONENT_COUNT] = {
        [QJS_INTL_DATETIME_WEEKDAY] = TEXT_WIDTHS,
        [QJS_INTL_DATETIME_ERA] = TEXT_WIDTHS,
        [QJS_INTL_DATETIME_YEAR] = NUMERIC_WIDTHS,
        [QJS_INTL_DATETIME_MONTH] = NUMERIC_WIDTHS | TEXT_WIDTHS,
        [QJS_INTL_DATETIME_DAY] = NUMERIC_WIDTHS,
        [QJS_INTL_DATETIME_DAY_PERIOD] = TEXT_WIDTHS,
        [QJS_INTL_DATETIME_HOUR] = NUMERIC_WIDTHS,
        [QJS_INTL_DATETIME_MINUTE] = NUMERIC_WIDTHS,
        [QJS_INTL_DATETIME_SECOND] = NUMERIC_WIDTHS,
        [QJS_INTL_DATETIME_FRACTION] = (1u << 4) - 2, /* 1..3 digits. */
        [QJS_INTL_DATETIME_ZONE] = (1u << (QJS_INTL_DATETIME_ZONE_LONG_GENERIC + 1)) - 2
    };
    int i, score = 0;
    if (out)
        *out = 0;
    if (!out || !requested || !candidate)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        int a = requested->values[i], b = candidate->values[i], delta;
        if (a < 0 || a > 6 || b < 0 || b > 6 ||
            (a && !(masks[i] & (1u << a))) || (b && !(masks[i] & (1u << b))))
            return QJS_INTL_INVALID_ARGUMENT;
        if (!a && b)
            score -= 20;
        else if (a && !b)
            score -= 120;
        else if (a == b)
            continue;
        else if (i == QJS_INTL_DATETIME_ZONE) {
            if (a == QJS_INTL_DATETIME_ZONE_SHORT || a == QJS_INTL_DATETIME_ZONE_SHORT_GENERIC) {
                if (b == QJS_INTL_DATETIME_ZONE_SHORT_OFFSET)
                    score -= 1;
                else if (b == QJS_INTL_DATETIME_ZONE_LONG_OFFSET)
                    score -= 4;
                else if (b == a + 1)
                    score -= 3;
                else
                    score -= 120;
            } else if (a == QJS_INTL_DATETIME_ZONE_LONG || a == QJS_INTL_DATETIME_ZONE_LONG_GENERIC) {
                if (b == QJS_INTL_DATETIME_ZONE_LONG_OFFSET)
                    score -= 1;
                else if (b == QJS_INTL_DATETIME_ZONE_SHORT_OFFSET)
                    score -= 9;
                else if (b == a - 1)
                    score -= 8;
                else
                    score -= 120;
            } else if (a == QJS_INTL_DATETIME_ZONE_SHORT_OFFSET && b == QJS_INTL_DATETIME_ZONE_LONG_OFFSET)
                score -= 3;
            else if (a == QJS_INTL_DATETIME_ZONE_LONG_OFFSET && b == QJS_INTL_DATETIME_ZONE_SHORT_OFFSET)
                score -= 8;
            else
                score -= 120;
        } else {
            delta = b - a;
            if (delta >= 2)
                score -= 6;
            else if (delta == 1)
                score -= 3;
            else if (delta == -1)
                score -= 6;
            else if (delta <= -2)
                score -= 8;
        }
    }
    *out = score;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_basic_match(const QJSIntlDateTimeComponents *requested,
                                            const QJSIntlDateTimeComponents *candidates, size_t count, size_t *out)
{
    size_t i, best = 0;
    int score, best_score = 0;
    if (out)
        *out = 0;
    if (!out || !requested || (!candidates && count) || count > PTRDIFF_MAX / sizeof(*candidates))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!count)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < count; i++) {
        QJSIntlStatus status = qjs_intl_datetime_basic_score(requested, &candidates[i], &score);
        if (status)
            return status;
        if (!i || score > best_score) {
            best = i;
            best_score = score;
        }
    }
    *out = best;
    return QJS_INTL_OK;
}
