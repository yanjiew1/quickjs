/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_COLLATION_COMPARE_H
#define QJS_INTL_COLLATION_COMPARE_H
#include "contexts.h"
#include "policy.h"

typedef struct QJSIntlRootCollationOptions {
    int numeric; /* Nonzero: adjacent Nd runs compare by unbounded integer value. */
    QJSIntlCollationSensitivity sensitivity;
    QJSIntlCollationCaseFirst case_first;
    int ignore_punctuation; /* CLDR root shifted, default maxVariable=punct. */
} QJSIntlRootCollationOptions;

/* Default root comparison: forward primary, secondary and tertiary levels,
   non-ignorable variable weights, no identical tie-breaker. This is a private
   foundation, not a locale-tailored Collator. NULL options selects defaults.
   data is an unchanged validated contexts view; inputs and output are disjoint.
   order is reset on failure and otherwise receives -1, 0 or 1. No input/view
   ownership is transferred; allocator callbacks may not reenter this operation. */
QJSIntlStatus qjs_intl_collation_compare_root(const QJSIntlCollationContexts *data,
                                             const uint16_t *left, size_t left_length,
                                             const uint16_t *right, size_t right_length,
                                             const QJSIntlAllocator *allocator,
                                             const QJSIntlRootCollationOptions *options, int *order);
#endif
