/* Scientific/engineering exponent parts over a localized numeric body.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
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
#ifndef QJS_INTL_NUMBER_EXPONENT_H
#define QJS_INTL_NUMBER_EXPONENT_H
#include "../common/format.h"

/* Append ECMA scientific/engineering exponent parts to a complete numeric
   body. Digits and separators are already validated selected-backend data;
   separator is nonempty, as is minus for a negative exponent;
   localized exponent digits follow this project's documented policy.
   The integer exponent has no optional plus or leading-zero padding.
   Borrow inputs unchanged; output is fresh/disjoint, bounded in UTF-16 units
   and owns both arrays with the supplied malloc/free allocator. Failures
   clear output. The body must contain a nonempty complete part partition. */
QJSIntlStatus qjs_intl_number_exponent_format(const QJSIntlAllocator *, const QJSIntlFormatted *,
                                              const uint32_t digits[10], QJSIntlUTF16 separator,
                                              QJSIntlUTF16 minus, int32_t exponent, size_t limit,
                                              QJSIntlFormatted *);
#endif
