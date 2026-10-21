/* Mutable host mappings with stable immutable CE sequences.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_MAP_H
#define QJS_INTL_COMPILER_MAP_H
#include "collation-relations.h"
#include "../../src/intl/collation/overlay.h"

struct QJSIntlMapNode;
struct QJSIntlMapEdge;
typedef struct QJSIntlCollationMap {
    QJSIntlAllocator allocator;
    struct QJSIntlMapNode *nodes;
    struct QJSIntlMapEdge *edges;
    QJSIntlRelationMapping *sequences;
    uint32_t node_count, edge_count, sequence_count;
    size_t node_capacity, edge_capacity, sequence_capacity;
} QJSIntlCollationMap;

/* Host-only ownership: initialize fresh/cleared storage, clear before reuse;
   failed initialization is clearable. Never mutate while a matcher borrows the
   query view. Allocator opaque state outlives the map and cannot reenter it. */
QJSIntlStatus qjs_intl_collation_map_init(QJSIntlCollationMap *, const QJSIntlAllocator *);
void qjs_intl_collation_map_clear(QJSIntlCollationMap *);
/* Keys are plain, already NFD Unicode scalar vectors; suffix is nonempty.
   Copy CEs into a new immutable sequence, retaining older sequences even when
   replacing their mapping. Empty CE output is valid and distinct from absence.
   CEs may borrow a prior sequence. Output/owner/key storage are disjoint.
   Failure resets *sequence to NONE and preserves visible mappings; unused trie
   paths/capacity may remain. No ownership transfer. */
QJSIntlStatus qjs_intl_collation_map_put(QJSIntlCollationMap *,
                                        const uint32_t *prefix, uint32_t prefix_count,
                                        const uint32_t *suffix, uint32_t suffix_count,
                                        const QJSIntlRelationElement *, size_t element_count,
                                        uint32_t *sequence);
/* Copy a borrowed sequence descriptor; its CE array remains valid until clear,
   including across subsequent mutations. Do not free or modify that array. */
QJSIntlStatus qjs_intl_collation_map_sequence(const QJSIntlCollationMap *, uint32_t,
                                             QJSIntlRelationMapping *);
/* Remove current contextual entries beginning at this MAPPED scalar and suppress
   inherited root contexts. Retain ordinary singleton mappings. Later put calls
   can reintroduce contexts; copied old sequences remain valid. May allocate a
   first-scalar record; failure preserves suppression and visible mappings. */
QJSIntlStatus qjs_intl_collation_map_suppress(QJSIntlCollationMap *, uint32_t scalar);
QJSIntlStatus qjs_intl_collation_map_overlay(const QJSIntlCollationMap *, QJSIntlCollationOverlay *);
#endif
