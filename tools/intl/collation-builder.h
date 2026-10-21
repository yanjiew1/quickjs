/* Host execution of ordered text resets and relations over current mappings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_BUILDER_H
#define QJS_INTL_COMPILER_BUILDER_H
#include "collation-map.h"
#include "collation-root-input.h"

typedef struct QJSIntlCollationBuilder {
    QJSIntlRootInput root;
    QJSIntlAllocator allocator;
    QJSIntlRelations relations;
    QJSIntlCollationMap map;
    unsigned int before;
    int ready, failed;
} QJSIntlCollationBuilder;

/* Borrow validated immutable logical-root bytes, copy their view and
   allocator callbacks. Initialize fresh/cleared storage; clear before reuse.
   A failed command invalidates the builder: abort compilation and clear it,
   rather than observing/retrying a partially completed graph/map transaction.
   No mutation is published to runtime packs by this host owner. */
QJSIntlStatus qjs_intl_collation_builder_init(QJSIntlCollationBuilder *,
                                             const QJSIntlRootInput *,
                                             const QJSIntlAllocator *);
void qjs_intl_collation_builder_clear(QJSIntlCollationBuilder *);
/* Counted original Unicode scalar strings (including NUL); normalized with the
   pinned engine Unicode library. Text resets resolve CURRENT mappings. */
QJSIntlStatus qjs_intl_collation_builder_reset(QJSIntlCollationBuilder *,
                                              const uint32_t *, uint32_t count, unsigned int before);
/* Internal logical-reset bridge: elements already use this owner's stable IDs.
   The instruction owner constructs the requested logical position separately. */
QJSIntlStatus qjs_intl_collation_builder_position(QJSIntlCollationBuilder *,
                                                 const QJSIntlRelationElement *, size_t count,
                                                 unsigned int before);
/* strength 0 equality / 1..4 P/S/T/Q. Recompute case from immutable ROOT text;
   extension lookup uses CURRENT mappings before publishing this relation. */
QJSIntlStatus qjs_intl_collation_builder_relate(QJSIntlCollationBuilder *, unsigned int strength,
                                               const uint32_t *prefix, uint32_t prefix_count,
                                               const uint32_t *text, uint32_t text_count,
                                               const uint32_t *extension, uint32_t extension_count);
/* Read-only CURRENT lookup; output must be fresh/cleared storage and owns its
   resulting CE vector (or is empty), independent of the map. Clear before reuse.
   Scalars/outputs/owner are disjoint. Failure invalidates the owner
   like other commands. Numeric comparison settings do not participate here. */
QJSIntlStatus qjs_intl_collation_builder_lookup(QJSIntlCollationBuilder *,
                                               const uint32_t *, uint32_t count,
                                               QJSIntlRelationMapping *);
#endif
