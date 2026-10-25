/* One-operand literal templates, independent of service and LDML quoting.
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
#ifndef QJS_INTL_COMMON_UNARY_PATTERN_H
#define QJS_INTL_COMMON_UNARY_PATTERN_H
#include "types.h"

typedef struct QJSIntlUnaryPattern {
    QJSIntlBytes prefix, suffix;
} QJSIntlUnaryPattern;

/* Borrow validated scalar UTF-8 containing exactly one {0}; apostrophes are
   literal. Empty affixes are allowed. Literal-only text is UNSUPPORTED; repeated
   or unsupported placeholders/braces are DATA_ERROR. No allocation or retained
   state. Reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_unary_pattern_parse(QJSIntlBytes, QJSIntlUnaryPattern *);
#endif
