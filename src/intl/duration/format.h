/* Duration component parts and bounded text composition, independent of JS.
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
#ifndef QJS_INTL_DURATION_FORMAT_H
#define QJS_INTL_DURATION_FORMAT_H
#include "../common/format.h"
#include "../../temporal/types.h"

typedef struct QJSIntlDurationPart {
    size_t start, end;
    QJSIntlPartType type;
    QJSTemporalUnit unit; /* UNIT_COUNT means no unit field. */
} QJSIntlDurationPart;

typedef struct QJSIntlDurationComponent {
    QJSIntlUTF16 text;
    const QJSIntlDurationPart *parts;
    size_t part_count;
} QJSIntlDurationComponent;

typedef struct QJSIntlDurationFormatted {
    uint16_t *text;
    size_t length;
    QJSIntlDurationPart *parts;
    size_t part_count;
} QJSIntlDurationFormatted;

/* ListFormatParts: replace element parts in order with their typed component
   parts, retaining number-origin units and untyped list/digital separators.
   Borrow component text/parts, preserving NUL, lone surrogates and empty
   elements. Require matching element text and complete part partitions.
   At most UNIT_COUNT components; allocator malloc/free and a nonzero UTF-16
   limit are required. list->text is owned by that allocator (or NULL when
   empty); list->parts remains borrowed. Success transfers text to the fresh,
   disjoint output and clears list->text/length; the caller still releases any
   owned list parts. Failure resets output and leaves list unchanged. Results
   survive the other inputs. No locale or formatting policy here. */
QJSIntlStatus qjs_intl_duration_join_parts(const QJSIntlAllocator *, QJSIntlFormatted *list,
                                         const QJSIntlDurationComponent *, size_t count,
                                         size_t limit, QJSIntlDurationFormatted *);
/* Compose one digital group of 1..3 immutable components and count-1 UTF-8
   separators. Validate component partitions before measuring/allocating. Keep
   even empty separators as unitless literal parts. Output owns exact text/parts
   independently of inputs; require fresh disjoint output, malloc/free and limit>0.
   Failures clear output. */
QJSIntlStatus qjs_intl_duration_join_digital(const QJSIntlAllocator *,
                                           const QJSIntlDurationComponent *, size_t count,
                                           const QJSIntlBytes *separators, size_t limit,
                                           QJSIntlDurationFormatted *);
void qjs_intl_duration_result_clear(const QJSIntlAllocator *, QJSIntlDurationFormatted *);
#endif
