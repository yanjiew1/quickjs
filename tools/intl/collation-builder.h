/* Host execution of ordered text resets and relations over current mappings.
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
/* Internal cursor bridge: resolved elements already use this owner's stable
   IDs. The logical-position owner constructs a single-CE result separately. */
QJSIntlStatus qjs_intl_collation_builder_position(QJSIntlCollationBuilder *,
                                                 const QJSIntlRelationElement *, size_t count,
                                                 unsigned int before);
/* Execute one of the six logical reset forms needed by the pinned corpus:
   first/last PI, first/last SI, last TI and last regular. Other named forms return
   UNSUPPORTED. Source baselines and established rule positions persist even when
   their mapping is replaced/suppressed; text resets still use current lookup. */
QJSIntlStatus qjs_intl_collation_builder_logical(QJSIntlCollationBuilder *,
                                                QJSIntlRootReset, unsigned int before);
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
