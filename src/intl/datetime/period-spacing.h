/* Source-proved native AM/PM spacing policy, before range rendering.
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
#ifndef QJS_INTL_DATETIME_PERIOD_SPACING_H
#define QJS_INTL_DATETIME_PERIOD_SPACING_H
#include "pattern.h"

/* Return h/m/s adjacency bits only for an actually selected ascii alternate
   containing one automatic a with an isolated unquoted ASCII space before it.
   Unknown/quoted/mixed layouts grant no proof. Inputs are validated raw LDML. */
unsigned int qjs_intl_date_period_ascii_source(QJSIntlBytes source, QJSIntlBytes alternate);
/* Intersect raw source evidence with an unchanged actual ordinary program.
   Constructor adaptation/composition may have changed its adjacent layout. */
unsigned int qjs_intl_date_period_ascii_mask(const QJSIntlDatePattern *, unsigned int proof);
/* Apply proved suffix spacing to an owned interval_compile result from this
   same raw source, after field-width adaptation. Only isolated unquoted
   U+202F/U+2009 gaps before a after the matching clock field are changed.
   Preserve token count, field metadata and all other literals. Allocate nothing;
   the unchanged raw pack remains separate from the mutable owned token storage. */
void qjs_intl_date_period_ascii_interval(QJSIntlDatePattern *owned,
                                         QJSIntlBytes source, unsigned int proof);
#endif
