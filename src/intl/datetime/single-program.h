/* Prepare complete single-date hour variants, before ranges and admission.
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
#ifndef QJS_INTL_DATETIME_SINGLE_PROGRAM_H
#define QJS_INTL_DATETIME_SINGLE_PROGRAM_H
#include "format-source.h"

#include "program-adapt.h"

typedef struct QJSIntlDateSingleProgram {
    QJSIntlDatePattern pattern;
    QJSIntlDatePatternInfo source_info; /* Original metadata, before adaptation. */
    size_t source_index;
    unsigned int adaptations;
} QJSIntlDateSingleProgram;

typedef struct QJSIntlDateSinglePrograms {
    QJSIntlAllocator allocator;
    QJSIntlDateTimeComponents components;
    QJSIntlDateSingleProgram normal, twelve;
} QJSIntlDateSinglePrograms;

/* Require the unchanged source pool/grouping pair and a valid group index.
   Choose whole compatible patterns, changing only hour width/family. Both
   variants retain identical year kinds and explicit b/B semantics. Prefer a
   real numeric-year representation over cyclic-only U when available. Minimize
   total family changes, then width changes, then earliest/normal/twelve source
   indexes; within each class prefer direct families and exact widths. A missing
   complete pair is UNSUPPORTED, not an invitation to strip a or revive parents.
   No-hour groups retain source order within that year preference and leave
   twelve empty. Own copied
   programs independently of the input owners; retained source indexes are
   diagnostic unless the caller keeps the pool for later provenance queries.
   Cross-family copies use representative H/h; the eventual renderer must
   apply resolved HourCycle, including h11/h24, rather than raw source letters.
   This does not supply ranges, synthesis, required subsets or renderer/name
   admission. Require malloc/free and fresh disjoint output; reset on failure.
   Empty/NULL/repeated clear is safe with the retained allocator. */
QJSIntlStatus qjs_intl_date_single_programs_open(const QJSIntlDateSourceCandidates *pool,
                                               const QJSIntlDateFormatSources *groups,
                                               size_t group, const QJSIntlAllocator *allocator,
                                               QJSIntlDateSinglePrograms *out);
void qjs_intl_date_single_programs_clear(QJSIntlDateSinglePrograms *programs);
#endif
