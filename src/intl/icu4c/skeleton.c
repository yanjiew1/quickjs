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
#include "skeleton.h"
#include <stdio.h>
#include <string.h>

void qjs_icu4c_skeleton_append(QJSICU4CSkeleton *s, QJSIntlBytes ascii)
{
    size_t i;

    if (s->error)
        return;
    if ((ascii.length && !ascii.data) ||
        ascii.length > sizeof(s->text) / sizeof(*s->text) - (size_t)s->length) {
        s->error = QJS_INTL_OVERFLOW;
        return;
    }
    for (i = 0; i < ascii.length; i++) {
        if ((unsigned char)ascii.data[i] > 127 || !ascii.data[i]) {
            s->error = QJS_INTL_INVALID_ARGUMENT;
            return;
        }
        s->text[s->length++] = (unsigned char)ascii.data[i];
    }
}

void qjs_icu4c_skeleton_token(QJSICU4CSkeleton *s, const char *ascii)
{
    qjs_icu4c_skeleton_append(s, (QJSIntlBytes){ascii, strlen(ascii)});
}

static void repeat(QJSICU4CSkeleton *s, char code, unsigned int count)
{
    while (count-- && !s->error)
        qjs_icu4c_skeleton_append(s, (QJSIntlBytes){&code, 1});
}

QJSIntlStatus qjs_icu4c_digit_skeleton(QJSICU4CSkeleton *s, const QJSIntlDecimalOptions *digits)
{
    static const char *const modes[] = {
        "ceiling",    "floor",   "up",        "down",      "half-ceiling",
        "half-floor", "half-up", "half-down", "half-even",
    };
    QJSIntlStatus error = qjs_intl_decimal_options_validate(digits);

    if (error)
        return error;
    if (digits->rounding_increment != 1) {
        char increment[8];
        int length =
            snprintf(increment, sizeof(increment), "%u", (unsigned int)digits->rounding_increment);
        int point = length - digits->maximum_fraction_digits;

        qjs_icu4c_skeleton_token(s, "precision-increment/");
        if (point > 0) {
            qjs_icu4c_skeleton_append(s, (QJSIntlBytes){increment, (size_t)point});
            if (point < length) {
                qjs_icu4c_skeleton_token(s, ".");
                qjs_icu4c_skeleton_append(
                    s, (QJSIntlBytes){increment + point, (size_t)(length - point)});
            }
        } else {
            qjs_icu4c_skeleton_token(s, "0.");
            repeat(s, '0', (unsigned int)-point);
            qjs_icu4c_skeleton_append(s, (QJSIntlBytes){increment, (size_t)length});
        }
    } else if (digits->rounding_type == QJS_INTL_ROUND_SIGNIFICANT) {
        repeat(s, '@', digits->minimum_significant_digits);
        repeat(s, '#', digits->maximum_significant_digits - digits->minimum_significant_digits);
    } else {
        qjs_icu4c_skeleton_token(s, ".");
        repeat(s, '0', digits->minimum_fraction_digits);
        repeat(s, '#', digits->maximum_fraction_digits - digits->minimum_fraction_digits);
        if (digits->rounding_type == QJS_INTL_ROUND_MORE_PRECISION ||
            digits->rounding_type == QJS_INTL_ROUND_LESS_PRECISION) {
            qjs_icu4c_skeleton_token(s, "/");
            repeat(s, '@', digits->minimum_significant_digits);
            repeat(s, '#', digits->maximum_significant_digits - digits->minimum_significant_digits);
            qjs_icu4c_skeleton_token(
                s, digits->rounding_type == QJS_INTL_ROUND_MORE_PRECISION ? "r" : "s");
        }
    }
    if (digits->trailing_zero_display == QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER)
        qjs_icu4c_skeleton_token(s, "/w");
    qjs_icu4c_skeleton_token(s, " rounding-mode-");
    qjs_icu4c_skeleton_token(s, modes[digits->rounding_mode]);
    qjs_icu4c_skeleton_token(s, " integer-width/+");
    repeat(s, '0', digits->minimum_integer_digits);
    qjs_icu4c_skeleton_token(s, " ");
    return s->error;
}
