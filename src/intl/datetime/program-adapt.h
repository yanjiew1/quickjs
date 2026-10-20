/* Explicit numeric adaptations shared by native single-date constructors.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_PROGRAM_ADAPT_H
#define QJS_INTL_DATETIME_PROGRAM_ADAPT_H
#include "pattern-info.h"

enum {
    QJS_INTL_DATE_ADAPT_HOUR_WIDTH = 1,
    QJS_INTL_DATE_ADAPT_HOUR_FAMILY = 2,
    QJS_INTL_DATE_ADAPT_MINUTE_WIDTH = 4,
    QJS_INTL_DATE_ADAPT_SECOND_WIDTH = 8,
    QJS_INTL_DATE_ADAPT_ZONE_APPEND = 16,
    QJS_INTL_DATE_ADAPT_ZONE_EXTRACT = 32,
    QJS_INTL_DATE_ADAPT_PERIOD_LAYOUT = 64
};

/* Copy an admitted ordinary program to an exact component vector. Permit only
   explicitly allowed HOUR/MINUTE/SECOND numeric-width changes, plus the requested
   hour family (twelve is 0/1). Other component differences are UNSUPPORTED.
   Preserve year kinds, period, nonnumeric fields and literals. Reproject before
   returning; h11/h24 presentation remains with the renderer. Own copied storage,
   clear via pattern_clear with the same allocator. Require unchanged inputs and
   fresh disjoint outputs; reset both on failure, release partial copies. */
QJSIntlStatus qjs_intl_date_program_adapt(const QJSIntlAllocator *allocator,
                                        const QJSIntlDatePattern *source,
                                        const QJSIntlDateTimeComponents *components,
                                        unsigned int allowed_widths, unsigned int twelve,
                                        QJSIntlDatePattern *out, unsigned int *adaptations);
#endif
