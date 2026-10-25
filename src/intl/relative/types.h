/* Relative-time semantic selectors shared by data and formatting layers.
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
#ifndef QJS_INTL_RELATIVE_TYPES_H
#define QJS_INTL_RELATIVE_TYPES_H
#include "../common/types.h"

typedef enum QJSIntlRelativeUnit {
    QJS_INTL_RELATIVE_SECOND,
    QJS_INTL_RELATIVE_MINUTE,
    QJS_INTL_RELATIVE_HOUR,
    QJS_INTL_RELATIVE_DAY,
    QJS_INTL_RELATIVE_WEEK,
    QJS_INTL_RELATIVE_MONTH,
    QJS_INTL_RELATIVE_QUARTER,
    QJS_INTL_RELATIVE_YEAR,
    QJS_INTL_RELATIVE_UNIT_COUNT
} QJSIntlRelativeUnit;

typedef enum QJSIntlRelativeWidth {
    QJS_INTL_RELATIVE_LONG,
    QJS_INTL_RELATIVE_SHORT,
    QJS_INTL_RELATIVE_NARROW,
    QJS_INTL_RELATIVE_WIDTH_COUNT
} QJSIntlRelativeWidth;

typedef enum QJSIntlRelativeTense {
    QJS_INTL_RELATIVE_PAST,
    QJS_INTL_RELATIVE_FUTURE
} QJSIntlRelativeTense;

#endif
