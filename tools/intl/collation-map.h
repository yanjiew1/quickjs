/* Mutable host mappings with stable immutable CE sequences.
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
    uint32_t live_first, live_last, live_count;
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
/* Visit CURRENT terminals, never abandoned paths or historical sequences.
   The descriptor is borrowed only during its callback; later mutation may move
   descriptor storage. Copy its fields to retain a CE view: its immutable element
   array remains valid until map clear. Do not free that borrowed array.
   Opaque terminal IDs
   identify a key: replacement preserves them, and suppression/reintroduction can
   make the same ID live again. They are not generation-checked handles.
   No mutation/reentrant mutation during the visit; callback errors propagate.
   Traversal allocates nothing and costs O(current terminal count). */
typedef QJSIntlStatus (*QJSIntlCollationMapVisitor)(void *opaque, uint32_t terminal_id,
                                                  uint32_t sequence_id,
                                                  const QJSIntlRelationMapping *);
QJSIntlStatus qjs_intl_collation_map_visit(const QJSIntlCollationMap *,
                                          QJSIntlCollationMapVisitor, void *opaque);
/* Enumerate root-context suppression independently of live terminals. Each
   scalar is emitted once in unspecified order, including groups with no mapping.
   No allocation/mutation/reentry; callback errors propagate immediately. */
typedef QJSIntlStatus (*QJSIntlCollationSuppressedVisitor)(void *, uint32_t scalar);
QJSIntlStatus qjs_intl_collation_map_visit_suppressed(const QJSIntlCollationMap *,
                                                     QJSIntlCollationSuppressedVisitor, void *);
/* Original-order NFD keys of a CURRENT terminal, without allocation/recursion.
   Count outputs reset on failure. Copy preflights both capacities and complete
   parent paths before modifying either buffer. Insufficient capacity returns
   OVERFLOW without writes. A zero-length prefix may use NULL. Buffers/counts
   are disjoint from the map and each other; the map is unchanged between size
   and copy. Work is proportional to the combined key length. */
QJSIntlStatus qjs_intl_collation_map_key_size(const QJSIntlCollationMap *, uint32_t terminal_id,
                                             uint32_t *prefix_count, uint32_t *suffix_count);
QJSIntlStatus qjs_intl_collation_map_key_copy(const QJSIntlCollationMap *, uint32_t terminal_id,
                                             uint32_t *prefix, size_t prefix_capacity,
                                             uint32_t *suffix, size_t suffix_capacity);
#endif
