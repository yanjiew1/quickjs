/* Currency-symbol adjacency for CLDR currency patterns.
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
#ifndef QJS_INTL_CURRENCY_BOUNDARY_H
#define QJS_INTL_CURRENCY_BOUNDARY_H
#include "../common/types.h"

typedef struct QJSIntlCurrencyBoundary {
    unsigned int before_number, after_number;
    unsigned int compact_before_number;
    unsigned int spacing_before_number, spacing_after_number;
} QJSIntlCurrencyBoundary;

/* Borrow NUL-free scalar UTF-8 (empty allowed). Standard alphaNextToNumber
   tests the closest scalar's General_Category L. Compact prefix selection
   instead accepts a final letter followed by zero or more marks. A suffix
   uses its first scalar in either case. Do not skip format characters.

   Legacy spacing facts test each closest scalar against the pinned CLDR set
   [[:^S:]&[:^Z:]]. They do not test surrounding numeric text or choose whether
   legacy rules apply. No allocation/retention; disjoint output resets on error. */
QJSIntlStatus qjs_intl_currency_boundary(QJSIntlBytes, QJSIntlCurrencyBoundary *);
#endif
