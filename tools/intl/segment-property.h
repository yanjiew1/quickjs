/* Host-side Unicode range queries for segmentation data preparation.
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
#ifndef QJS_INTL_SEGMENT_PROPERTY_H
#define QJS_INTL_SEGMENT_PROPERTY_H
#include "../../src/intl/common/types.h"

typedef struct QJSIntlSegmentRanges {
    uint32_t *points;
    uint32_t count; /* Even: sorted disjoint [start, end) code-point pairs. */
} QJSIntlSegmentRanges;

/* Query the project's pinned C Unicode tables, never the host Python database.
   Names use exact case-sensitive C-table spellings (including their value
   aliases); gc/sc/scx/GCB/WB/SB/InCB field aliases are also accepted. This is
   not a Unicode loose-name resolver or a UnicodeSet expression parser.
   An empty property tries category, script, then binary.
   Binary values accept True/Yes or False/No. Unknown names return UNSUPPORTED;
   counted names containing NUL or non-ASCII are invalid arguments.

   The allocator supplies all three callbacks and out is fresh/disjoint.
   It owns the returned points until clear, including surrogate code points
   where selected by the source property. No value exceeds U+10FFFF; endpoints
   may be U+110000. Errors reset out. No public RegExp property surface changes. */
QJSIntlStatus qjs_intl_segment_property(const QJSIntlAllocator *, QJSIntlBytes property,
                                       QJSIntlBytes value, QJSIntlSegmentRanges *out);
void qjs_intl_segment_ranges_clear(const QJSIntlAllocator *, QJSIntlSegmentRanges *);
#endif
