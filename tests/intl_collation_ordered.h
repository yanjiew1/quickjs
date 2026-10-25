/* Shared source-file ordering audit for raw and ranked collation readers.
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
#ifndef QJS_TEST_INTL_COLLATION_ORDERED_H
#define QJS_TEST_INTL_COLLATION_ORDERED_H
#include "../src/intl/common/types.h"

typedef QJSIntlStatus (*QJSIntlOrderedCompare)(void *, const uint16_t *, size_t,
                                              const uint16_t *, size_t, int *);

/* Optional complete CLDR source-file audit, separate from bounded repo tests.
   Compare neighboring counted UTF-16 strings; comments/sort keys are diagnostics
   only. Reject future input that exceeds explicit parser bounds. */
void qjs_test_intl_collation_ordered(const char *, QJSIntlOrderedCompare, void *);
#endif
