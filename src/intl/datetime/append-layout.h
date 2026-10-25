/* Interpret visible CLDR append items independently of program construction.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
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
#ifndef QJS_INTL_DATETIME_APPEND_LAYOUT_H
#define QJS_INTL_DATETIME_APPEND_LAYOUT_H
#include "available-source.h"
#include "../common/pair-pattern.h"

typedef struct QJSIntlDateAppendLayout {
    QJSIntlPairPattern pattern;
    QJSIntlDateAvailableEntry entry;
    QJSIntlDateAvailableNode origin;
} QJSIntlDateAppendLayout;

/* Select the exact uncounted, unalternated key from an already resolved APPEND
   inventory and parse its two-slot layout. NULL inventory, absent/blocked/empty
   text return UNSUPPORTED without ancestor revival. Borrow unchanged pack text;
   allocate nothing, reset non-NULL output on failure, require a nonempty key
   and disjoint output. This lookup does not choose a field, width or ordering
   policy beyond the source template itself. */
QJSIntlStatus qjs_intl_date_append_layout(const QJSIntlDateAvailableSources *sources,
                                         QJSIntlBytes key, QJSIntlDateAppendLayout *out);
#endif
