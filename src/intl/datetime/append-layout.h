/* Interpret visible CLDR append items independently of program construction.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
#ifndef QJS_INTL_DATETIME_APPEND_LAYOUT_H
#define QJS_INTL_DATETIME_APPEND_LAYOUT_H
#include "available-source.h"
#include "../common/pair-pattern.h"

typedef struct QJSIntlDateAppendLayout {
    QJSIntlPairPattern pattern;
    QJSIntlDateAvailableEntry entry;
    QJSIntlDateAvailableNode origin;
} QJSIntlDateAppendLayout;

/* Select the exact uncounted, unalternated key from an already resolved APPEND
   inventory and parse its two-slot layout. NULL inventory, absent/blocked/empty
   text return UNSUPPORTED without ancestor revival. Borrow unchanged pack text;
   allocate nothing, reset non-NULL output on failure, require a nonempty key
   and disjoint output. This lookup does not choose a field, width or ordering
   policy beyond the source template itself. */
QJSIntlStatus qjs_intl_date_append_layout(const QJSIntlDateAvailableSources *sources,
                                         QJSIntlBytes key, QJSIntlDateAppendLayout *out);
#endif
