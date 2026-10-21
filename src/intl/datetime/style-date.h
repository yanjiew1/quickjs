/* Native date-style preparation, separate from exact source compilation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
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
void qjs_intl_date_prepared_style_clear(QJSIntlDatePreparedStyle *style);
#endif
