/* Compact family/divisor selection before numeric rounding.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_COMPACT_FAMILY_H
#define QJS_INTL_NUMBER_COMPACT_FAMILY_H
#include "compact-data.h"
#include "../currency/boundary.h"

typedef struct QJSIntlCompactFamily {
    QJSIntlCompactPattern baseline;
    QJSIntlCompactVariant requested;
    unsigned int alpha_available; /* Any usable alpha leaf, not only OTHER. */
} QJSIntlCompactFamily;

/* Require unchanged open data/map and an existing standard OTHER threshold.
   NULL boundary selects the ordinary decimal family. Otherwise currency alpha
   requires direct NUMBER/currency adjacency and the compact L M* / initial L
   facts. Raw-zero fallback is returned before considering any alpha variant.
   A usable requested alpha OTHER fixes its divisor before rounding; sparse
   alpha count leaves can still be requested without an alpha OTHER.
   Any usable alpha leaf marks the family as alpha-capable for spacing, even
   when adjacency selects the standard form. Missing/blocked/empty leaves and
   raw-zero fallbacks do not establish that capability.
   No allocation. Views borrow the data; disjoint output resets on errors. */
QJSIntlStatus qjs_intl_compact_family_at(const QJSIntlCompactData *, const QJSIntlCompactMap *,
                                        uint32_t magnitude, const QJSIntlCurrencyBoundary *,
                                        QJSIntlCompactFamily *);

/* Try a count in the requested family, retaining the baseline divisor. If
   alpha OTHER was unavailable, a compatible standard count can be used.
   Missing, blocked, empty, raw-zero or differently scaled candidates return
   UNSUPPORTED; the caller then tries its next count or the family baseline.
   Require a family from family_at on the same unchanged data/map. */
QJSIntlStatus qjs_intl_compact_family_pattern(const QJSIntlCompactData *, const QJSIntlCompactMap *,
                                             const QJSIntlCompactFamily *, QJSIntlCompactCount,
                                             QJSIntlCompactPattern *);
#endif
