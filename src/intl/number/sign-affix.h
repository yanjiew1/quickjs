/* Sign-only projection for complete compact currency bodies.
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
#ifndef QJS_INTL_NUMBER_SIGN_AFFIX_H
#define QJS_INTL_NUMBER_SIGN_AFFIX_H
#include "pattern-data.h"

/* Require decoded, scalar-text pattern forms with exactly one NUMBER each.
   Subtract common positive prefix/suffix context on each side of NUMBER,
   comparing literal scalars and typed tokens. Preserve only the signed form's
   remaining context. This is a native composition policy, not an LDML rewrite.
   Reject residual currency or percent tokens, which cannot be a sign envelope.

   Rewrite selected in place, using at most its original count. Positive and
   selected arrays must be disjoint; literal slices borrow their original
   unchanged storage. No allocation. On failure *count is zero; array contents
   are unspecified. Output count must not alias either token array. */
QJSIntlStatus qjs_intl_number_sign_affix(const QJSIntlNumberPatternToken *positive,
                                        size_t positive_count,
                                        QJSIntlNumberPatternToken *selected, size_t selected_count,
                                        size_t *count);
#endif
