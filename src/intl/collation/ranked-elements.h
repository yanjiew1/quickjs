/* Ranked CE emission over shared root/overlay matching.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_RANKED_ELEMENTS_H
#define QJS_INTL_COLLATION_RANKED_ELEMENTS_H
#include "compiled-data.h"
#include "match.h"

typedef struct QJSIntlCollationRankedToken {
    QJSIntlCollationRankedElement element;
    QJSIntlCollationNumber number; /* Nonempty only for a numeric token. */
} QJSIntlCollationRankedToken;

typedef struct QJSIntlCollationRankedElements {
    QJSIntlCompiledCollationProgram program;
    QJSIntlCollationMatch match;
    uint32_t pending_at, pending_count;
    int pending_overlay, numeric;
} QJSIntlCollationRankedElements;

/* Initialize fresh/cleared storage directly at its final address. Program and
   allocator descriptors are copied; archive bytes and allocator opaque state
   outlive the iterator, but input/program descriptors need only survive init.
   The matcher borrows this iterator's trie: never copy/move a live iterator.
   Failure leaves clearable storage; clear before reuse. Arguments are disjoint.
   numeric fixes the mode for the entire scan, independently of later policy. */
QJSIntlStatus qjs_intl_collation_ranked_elements_init(QJSIntlCollationRankedElements *,
                                                    const QJSIntlCompiledCollationProgram *,
                                                    const uint16_t *, size_t, int numeric,
                                                    const QJSIntlAllocator *);
void qjs_intl_collation_ranked_elements_clear(QJSIntlCollationRankedElements *);

/* Emit one CE or one complete adjacent Nd run, without further allocation.
   Numeric digits borrow immutable normalized text until clear; leading zeros
   are removed once, retaining one zero. A number has an empty element payload.
   A completely zero CE is still present. A zero-length CE mapping consumes its
   text and continues scanning. Exhaustion is repeatable success with present=0.
   Reordering, variable/shifted handling and sensitivity belong to comparison.
   After an unexpected data error, clear/abort the scan rather than retrying. */
QJSIntlStatus qjs_intl_collation_ranked_elements_next(QJSIntlCollationRankedElements *,
                                                    QJSIntlCollationRankedToken *, int *present);
#endif
