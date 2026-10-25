/* Relative-time parts distinguish inserted number spans from template text.
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
#ifndef QJS_INTL_RELATIVE_FORMAT_H
#define QJS_INTL_RELATIVE_FORMAT_H
#include "types.h"
#include "../common/format.h"

typedef struct QJSIntlRelativePart {
    size_t start, end;
    QJSIntlPartType type;
    QJSIntlRelativeUnit unit; /* UNIT_COUNT means no unit field. */
} QJSIntlRelativePart;

typedef struct QJSIntlRelativeFormatted {
    uint16_t *text;
    size_t length;
    QJSIntlRelativePart *parts;
    size_t part_count;
} QJSIntlRelativeFormatted;

/* Scalar UTF-8 template, containing zero or one {0}. Substitute an unchanged
   borrowed numeric result, preserving all part boundaries and attaching unit
   even to number-origin literals. Template literals have no unit. A body is
   required/validated only when the template contains {0}. Empty numeric
   templates produce no parts. Non-token braces are DATA_ERROR.
   A nonzero UTF-16 limit and malloc/free are required. Fresh disjoint output;
   errors reset it. The result owns at most two buffers and survives inputs. */
QJSIntlStatus qjs_intl_relative_pattern_format(const QJSIntlAllocator *, QJSIntlBytes pattern,
                                               const QJSIntlFormatted *body, QJSIntlRelativeUnit,
                                               size_t limit, QJSIntlRelativeFormatted *);
/* Auto text is a literal, never a template: preserve braces and always return
   exactly one literal part, including for an empty string. Same ownership,
   scalar text, bounds and output contracts as pattern_format. */
QJSIntlStatus qjs_intl_relative_literal_format(const QJSIntlAllocator *, QJSIntlBytes literal,
                                               size_t limit, QJSIntlRelativeFormatted *);
void qjs_intl_relative_result_clear(const QJSIntlAllocator *, QJSIntlRelativeFormatted *);
#endif
