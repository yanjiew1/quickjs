/* Literal two-operand templates; no service or backend state.
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
#ifndef QJS_INTL_COMMON_PAIR_PATTERN_H
#define QJS_INTL_COMMON_PAIR_PATTERN_H
#include "types.h"

typedef struct QJSIntlPairPattern {
    QJSIntlBytes prefix, separator, suffix;
    unsigned int first; /* Index of the first operand (0 or 1). */
} QJSIntlPairPattern;

/* Borrow literal fragments around exactly one {0} and one {1}, in either
   order. Missing operands return UNSUPPORTED; invalid/repeated placeholders
   return DATA_ERROR. Apostrophes are literal. Input comes from scalar UTF-8
   data; this parser only checks template syntax. Allocate/retain nothing,
   require disjoint output and reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_pair_pattern_parse(QJSIntlBytes, QJSIntlPairPattern *);

#endif
