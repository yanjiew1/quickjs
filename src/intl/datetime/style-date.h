/* Native date-style preparation, separate from exact source compilation.
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
#ifndef QJS_INTL_DATETIME_STYLE_DATE_H
#define QJS_INTL_DATETIME_STYLE_DATE_H
#include "style-program.h"

enum { QJS_INTL_DATE_STYLE_REBUILT = 1 };

typedef struct QJSIntlDatePreparedStyle {
    QJSIntlDateStyleSource style; /* Effective program; source/skeleton stay original. */
    QJSIntlDateAvailableEntry replacement;
    QJSIntlDateAvailableNode origin;
    size_t source_index; /* SIZE_MAX for an exact style, otherwise pool index. */
    unsigned int adaptations;
} QJSIntlDatePreparedStyle;

/* First prepare the selected DATE style, including the shared month/no-week
   CLDR data workaround. Remaining width-one Y sources with otherwise supported
   date fields may request native reconstruction from
   the supplied locale/calendar's ordinary visible available pool. Require a
   real y-only whole source with the same fields/contexts; prefer exact widths,
   then SHORT-to-LONG month/weekday names whose source skeleton permits expansion.
   Preserve candidate punctuation and field order, not the unsupported style's.
   Missing/BLOCK/empty and all other unsupported sources stay unsupported.
   Keep original style/skeleton provenance plus actual replacement provenance and
   REBUILT flag. Raw pack/source strings stay unchanged. Generic source provenance
   never selects arithmetic; the caller retains its requested calendar identity.
   Use the style owner's own indexes; the pool has its independent owner indexes.
   Own the effective program, borrowing provenance from unchanged packs. Pool
   storage may be cleared after success. Require malloc/free, unchanged inputs
   and fresh disjoint output. Reset/unwind errors; clear with the same allocator. */
QJSIntlStatus qjs_intl_date_style_date_open(const QJSIntlDateStyleData *styles,
                                          uint32_t locale, uint32_t calendar,
                                          QJSIntlDateStyleWidth width, uint32_t alternate,
                                          const QJSIntlDateSourceCandidates *pool,
                                          const QJSIntlAllocator *allocator,
                                          QJSIntlDatePreparedStyle *out);
/* Preferred/fallback selection also applies during style recovery. If the
   preferred source cannot be prepared, try the separately resolved fallback
   source. Operational errors propagate. Alternate indexes belong to styles. */
QJSIntlStatus qjs_intl_date_style_date_preferred(const QJSIntlDateStyleData *, uint32_t locale,
                                                 uint32_t calendar, QJSIntlDateStyleWidth,
                                                 uint32_t preferred, uint32_t fallback,
                                                 const QJSIntlDateSourceCandidates *,
                                                 const QJSIntlAllocator *, QJSIntlDatePreparedStyle *);
void qjs_intl_date_prepared_style_clear(QJSIntlDatePreparedStyle *style);
#endif
