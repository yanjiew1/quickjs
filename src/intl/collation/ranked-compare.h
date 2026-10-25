/* Comparison of validated rank32 programs, independent of JS option handling.
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
#ifndef QJS_INTL_COLLATION_RANKED_COMPARE_H
#define QJS_INTL_COLLATION_RANKED_COMPARE_H
#include "compiled-data.h"

/* Use program policy when policy is NULL; otherwise use one effective policy
   computed by the caller before scanning. No input/view ownership transfers.
   Archive bytes are immutable/alive for the call; allocator callbacks cannot
   reenter it. Arguments/output are disjoint; order resets on failure.

   Compare P/S/case/T/Q and the NFD identical tie-breaker according to strength,
   with backwards secondary segments, shifted variables, numeric tokens and
   group reordering. This engine interface does not advertise public service
   support or perform JS coercion/locale option resolution. */
QJSIntlStatus qjs_intl_collation_compare_ranked(const QJSIntlCompiledCollationProgram *,
                                                const uint16_t *left, size_t left_length,
                                                const uint16_t *right, size_t right_length,
                                                const QJSIntlAllocator *,
                                                const QJSIntlCollationPolicy *, int *order);
#endif
