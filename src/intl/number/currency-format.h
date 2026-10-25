/* Currency-name placement around an already formatted decimal number.
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
#ifndef QJS_INTL_NUMBER_CURRENCY_FORMAT_H
#define QJS_INTL_NUMBER_CURRENCY_FORMAT_H
#include "../common/format.h"

/* Exact scalar UTF-8 template containing {0} and {1} once each. Apostrophes
   are literal, as in PartitionPattern. {0} inserts the complete number and
   its parts, including numeric sign affixes; {1} emits one currency part.
   Currency-name text may be empty. Other braces and malformed UTF-8 fail.

   Borrow inputs unchanged; fresh disjoint output, reset on errors. Require
   malloc/free, never realloc. Bound complete UTF-16 output; clear with
   qjs_intl_number_body_clear and the creating allocator. */
QJSIntlStatus qjs_intl_number_currency_name_format(const QJSIntlAllocator *, QJSIntlBytes pattern,
                                                    QJSIntlBytes name, const QJSIntlFormatted *,
                                                    size_t maximum_output_length,
                                                    QJSIntlFormatted *);
#endif
