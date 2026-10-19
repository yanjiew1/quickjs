/* Currency-symbol adjacency for CLDR currency patterns.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_CURRENCY_BOUNDARY_H
#define QJS_INTL_CURRENCY_BOUNDARY_H
#include "../common/types.h"

typedef struct QJSIntlCurrencyBoundary {
    unsigned int before_number, after_number;
    unsigned int compact_before_number;
    unsigned int spacing_before_number, spacing_after_number;
} QJSIntlCurrencyBoundary;

/* Borrow NUL-free scalar UTF-8 (empty allowed). Standard alphaNextToNumber
   tests the closest scalar's General_Category L. Compact prefix selection
   instead accepts a final letter followed by zero or more marks. A suffix
   uses its first scalar in either case. Do not skip format characters.

   Legacy spacing facts test each closest scalar against the pinned CLDR set
   [[:^S:]&[:^Z:]]. They do not test surrounding numeric text or choose whether
   legacy rules apply. No allocation/retention; disjoint output resets on error. */
QJSIntlStatus qjs_intl_currency_boundary(QJSIntlBytes, QJSIntlCurrencyBoundary *);
#endif
