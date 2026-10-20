/* Native whole-source time-style pairs and explicit zone-append synthesis.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_STYLE_HOUR_H
#define QJS_INTL_DATETIME_STYLE_HOUR_H
#include "style-program.h"

/* Require an unchanged selected TIME style and its locale/calendar's ordinary
   source pool and resolved APPEND inventory (independent owner indexes). NULL
   pools/inventories mean absent inputs, not another locale fallback. Prefer the
   style's own complete family, then whole available sources, then zone-free
   clocks with the exact visible Timezone template and style zone token. Each
   source tier tries exact m/s widths before explicit numeric-width adaptation;
   prefer direct hour family, fewer width changes, then original pool order.
   Preserve b/B class and all target components, never strip a or equate v/z.
   Own both programs, retaining allocator; original source_info is unchanged,
   pool indexes identify available sources and SIZE_MAX denotes the own style.
   ZONE_APPEND records synthesis from a bare clock, not an authored whole source.
   Return UNSUPPORTED when a complete counterpart cannot be proven by these
   tiers; AM/PM-layout synthesis and final range/rendering admission are separate.
   Require malloc/free and fresh disjoint output; reset and unwind failures.
   Clear with single_programs_clear; no source/pool/inventory storage is retained. */
QJSIntlStatus qjs_intl_date_style_time_programs_open(const QJSIntlDateStyleSource *time,
                                                   const QJSIntlDateSourceCandidates *pool,
                                                   const QJSIntlDateAvailableSources *append,
                                                   const QJSIntlAllocator *allocator,
                                                   QJSIntlDateSinglePrograms *out);
#endif
