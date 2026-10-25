/* NumberFormat handles shared by native date and zone text.
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
#ifndef QJS_INTL_DATETIME_NUMBER_FORMATS_H
#define QJS_INTL_DATETIME_NUMBER_FORMATS_H
#include "../number/number.h"

typedef struct QJSIntlDateNumberFormats {
    const QJSIntlNumberFormatter *numeric, *two_digit;
    const QJSIntlNumberFormatter *fraction[3];
} QJSIntlDateNumberFormats;

/* Selected-locale decimal NumberFormat handles: no grouping or fraction
   digits, minimum integer widths 1/2 and 1/2/3 for fractional milliseconds.
   Only handles used by the program are required. They remain immutable and
   outlive rendering, using the same allocator as the date text result. */
#endif
