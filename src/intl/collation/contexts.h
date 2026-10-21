/* CLDR root prefix metadata over the existing immutable UCA pools.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_CONTEXTS_H
#define QJS_INTL_COLLATION_CONTEXTS_H
#include "root-data.h"

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
#endif
