/* Number range rendering over complete endpoint results, without JS values.
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
#ifndef QJS_INTL_NUMBER_RANGE_H
#define QJS_INTL_NUMBER_RANGE_H
#include "../common/format.h"

/* Parse the native range layout {0}<separator>{1}. The separator must be
   nonempty scalar UTF-8 without braces; endpoint reordering or added affixes
   are unsupported by this adapter. Empty input/separator returns UNSUPPORTED,
   other invalid syntax DATA_ERROR. Borrow input; allocate/retain nothing.
   Require disjoint output and reset non-NULL output on errors. */
QJSIntlStatus qjs_intl_number_range_separator(QJSIntlBytes, QJSIntlBytes *);

/* Both endpoint results must be complete nonempty numeric part partitions.
   Compare their UTF-16 text, not values or part lists. Equal strings use the
   first endpoint's parts, prefixed by the complete approximately sign, with
   all sources SHARED. An empty sign emits no additional part. Prefix placement
   is this native adapter's ECMA-402 ILND policy. Unequal strings keep both
   endpoints and insert the shared separator; no range collapse is performed.

   Borrow immutable inputs, preserve every endpoint part (including empties),
   and use only two final allocations. Require scalar UTF-8 affixes, nonempty
   separator, malloc/free, and a nonzero complete UTF-16 output bound.
   Only the affix emitted by the selected equality branch is decoded; the
   unused affix's scalar contents are not inspected. Disjoint output resets
   on errors and owns both arrays independently of the inputs.
   Clear with qjs_intl_number_range_result_clear and the creating allocator.
   Coercion, NaN rejection and endpoint formatting belong to the caller. */
QJSIntlStatus qjs_intl_number_range_compose(const QJSIntlAllocator *, QJSIntlBytes separator,
                                          QJSIntlBytes approximately,
                                          const QJSIntlFormatted *start, const QJSIntlFormatted *end,
                                          size_t maximum_output_length, QJSIntlFormattedRange *);
#endif
