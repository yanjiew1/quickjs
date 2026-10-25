/* CLDR root prefix metadata over the existing immutable UCA pools.
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
#ifndef QJS_INTL_COLLATION_CONTEXTS_H
#define QJS_INTL_COLLATION_CONTEXTS_H
#include "root-data.h"
#include "keys.h"

typedef struct QJSIntlCollationContexts {
    QJSIntlCollationRoot root;
    QJSIntlSection section;
} QJSIntlCollationContexts;

typedef struct QJSIntlCollationContext {
    uint32_t source_index;
    QJSIntlCollationEntry before, mapping;
} QJSIntlCollationContext;

/* Storage metadata, not a context matcher. The source_index identifies the
   allkeys pseudo-contraction which an element iterator must replace with this prefix
   rule. 'before' describes already-consumed text and must not be emitted again;
   'mapping' holds only the suffix key/CEs. Both borrow unchanged pack bytes.
   Follow pack.h's disjoint-output contract. Missing section is UNSUPPORTED. */
QJSIntlStatus qjs_intl_collation_contexts_open(const QJSIntlPack *, QJSIntlCollationContexts *);
QJSIntlStatus qjs_intl_collation_context_at(const QJSIntlCollationContexts *, uint32_t,
                                           QJSIntlCollationContext *);
/* Copy a CE-free borrowed key view from validated raw storage. */
QJSIntlStatus qjs_intl_collation_contexts_keys(const QJSIntlCollationContexts *,
                                               QJSIntlCollationKeyIndex *);
/* Decode one selected source row, applying its raw prefix CE skip if present.
   The matcher already chose its prefix; it must not be emitted a second time. */
QJSIntlStatus qjs_intl_collation_contexts_mapping(const QJSIntlCollationContexts *, uint32_t,
                                                  QJSIntlCollationEntry *);
#endif
