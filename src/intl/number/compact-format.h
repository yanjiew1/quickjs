/* Native compact-notation bodies from validated compiled patterns.
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
#ifndef QJS_INTL_NUMBER_COMPACT_FORMAT_H
#define QJS_INTL_NUMBER_COMPACT_FORMAT_H
#include "compact-data.h"
#include "affix.h"

/* Require a VALUE SCALED/LITERAL pattern from this unchanged data owner.
   Format its positive decimal form; the outer style owns the number sign.
   SCALED inserts the complete nonempty, partitioned numeric body once.
   LITERAL omits it and permits NULL body. Nonspace affix text becomes compact
   parts; leading/trailing Unicode White_Space becomes literal parts. Inner
   spaces in a compact name remain in its compact part. Unquoted MINUS/PLUS
   tokens use supplied scalar UTF-8 symbols without splitting the compact part.
   Currency tokens emit the resolved currency text, including empty parts.
   Optional before_currency/after_currency gaps are literal parts at direct
   NUMBER/currency token boundaries, never for empty currency. The caller
   already checked currency/number scalar predicates and spacing policy.
   The outer style owns the value's sign and must not emit currency a second time.

   Borrow inputs unchanged; fresh, disjoint output required. malloc/free are
   required, realloc unused. Bound the full UTF-16 result; errors reset it.
   Clear with qjs_intl_number_body_clear and the creating allocator. */
QJSIntlStatus qjs_intl_number_compact_format(const QJSIntlAllocator *, const QJSIntlCompactData *,
                                             const QJSIntlCompactPattern *,
                                             const QJSIntlNumberAffixSymbols *,
                                             const QJSIntlFormatted *body,
                                             size_t maximum_output_length, QJSIntlFormatted *);
#endif
