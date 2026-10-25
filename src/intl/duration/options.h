/* Resolved DurationFormat options, independent of JS property access.
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
#ifndef QJS_INTL_DURATION_OPTIONS_H
#define QJS_INTL_DURATION_OPTIONS_H
#include "../common/types.h"
#include "../../temporal/types.h"

typedef enum QJSIntlDurationStyle {
    QJS_INTL_DURATION_LONG,
    QJS_INTL_DURATION_SHORT,
    QJS_INTL_DURATION_NARROW,
    QJS_INTL_DURATION_DIGITAL
} QJSIntlDurationStyle;

typedef enum QJSIntlDurationUnitStyle {
    QJS_INTL_DURATION_UNIT_UNSET = -1,
    QJS_INTL_DURATION_UNIT_LONG,
    QJS_INTL_DURATION_UNIT_SHORT,
    QJS_INTL_DURATION_UNIT_NARROW,
    QJS_INTL_DURATION_UNIT_NUMERIC,
    QJS_INTL_DURATION_UNIT_2DIGIT,
    QJS_INTL_DURATION_UNIT_FRACTIONAL
} QJSIntlDurationUnitStyle;

typedef enum QJSIntlDurationDisplay {
    QJS_INTL_DURATION_DISPLAY_UNSET = -1,
    QJS_INTL_DURATION_AUTO,
    QJS_INTL_DURATION_ALWAYS
} QJSIntlDurationDisplay;

typedef struct QJSIntlDurationUnitOptions {
    QJSIntlDurationUnitStyle style;
    QJSIntlDurationDisplay display;
} QJSIntlDurationUnitOptions;

typedef struct QJSIntlDurationOptions {
    QJSIntlDurationStyle style;
    QJSIntlDurationUnitOptions units[QJS_TEMPORAL_UNIT_COUNT];
    int fractional_digits; /* -1 is undefined; otherwise 0..9. */
    size_t maximum_output_length;
} QJSIntlDurationOptions;

/* GetDurationUnitOptions after the binding's ordered GetOption calls. UNSET
   represents an absent request; FRACTIONAL is internal and cannot be requested.
   previous stays UNSET through hours, then tracks the resolved preceding
   time unit, as in the constructor's prevStyle record.
   Apply the normative TwoDigitHours override when its effective policy flag
   is true; the backend chooses that flag separately from raw CLDR hour width.
   Failure leaves *out unchanged. No allocation, coercion or locale access. */
QJSIntlStatus qjs_intl_duration_unit_options(QJSTemporalUnit, QJSIntlDurationStyle,
                                            QJSIntlDurationUnitStyle requested,
                                            QJSIntlDurationDisplay display,
                                            QJSIntlDurationUnitStyle previous,
                                            int two_digit_hours, QJSIntlDurationUnitOptions *out);
/* Validate a fully resolved record before opening formatter handles. */
QJSIntlStatus qjs_intl_duration_options_validate(const QJSIntlDurationOptions *);
#endif
