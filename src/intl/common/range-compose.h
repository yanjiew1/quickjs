/* Source-preserving assembly of already formatted range fragments.
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
#ifndef QJS_INTL_COMMON_RANGE_COMPOSE_H
#define QJS_INTL_COMMON_RANGE_COMPOSE_H
#include "format.h"

typedef struct QJSIntlRangePiece {
    const QJSIntlFormatted *body; /* Non-NULL selects a complete partition. */
    QJSIntlBytes literal; /* Otherwise scalar UTF-8, with the supplied type. */
    QJSIntlPartType type;
    QJSIntlRangeSource source;
} QJSIntlRangePiece;

/* Borrow unchanged fragments, copy text and parts into two final allocations.
   Preserve body parts, including empty parts, and attach each piece's source;
   empty literals emit nothing. Require at least one nonempty output fragment,
   malloc/free, a nonzero UTF-16 bound and fresh disjoint output. Reset errors.
   This performs no endpoint comparison, collapse, ordering or locale policy.
   Clear with formatted_range_clear and the creating allocator. */
QJSIntlStatus qjs_intl_range_compose(const QJSIntlAllocator *allocator,
                                    const QJSIntlRangePiece *pieces, size_t count,
                                    size_t maximum_output_length, QJSIntlFormattedRange *out);
#endif
