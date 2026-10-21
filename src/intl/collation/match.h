/* Immutable NFD input and context/contraction matching, independent of CE policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_MATCH_H
#define QJS_INTL_COLLATION_MATCH_H
#include "contexts.h"
#include "overlay.h"

typedef struct QJSIntlCollationMatch {
    QJSIntlCollationContexts data;
    QJSIntlAllocator allocator;
    QJSIntlCollationOverlay overlay;
    uint32_t *text, *indexes, *groups;
    uint32_t length;
    int ready, has_overlay;
} QJSIntlCollationMatch;

enum {
    QJS_INTL_COLLATION_MATCH_NONE,
    QJS_INTL_COLLATION_MATCH_ROOT,
    QJS_INTL_COLLATION_MATCH_OVERLAY,
    QJS_INTL_COLLATION_MATCH_SCALAR,
    QJS_INTL_COLLATION_MATCH_NUMBER
};

typedef struct QJSIntlCollationMatchResult {
    QJSIntlCollationEntry entry;
    uint32_t scalar, sequence;
    unsigned int kind;
} QJSIntlCollationMatchResult;

typedef struct QJSIntlCollationNumber {
    const uint32_t *digits;
    uint32_t length;
} QJSIntlCollationNumber;

/* Copy an unchanged validated root/context view and allocator callbacks.
   Pack bytes and allocator opaque state outlive the matcher; input is borrowed
   only during init. Initialize fresh/cleared storage, clear before reuse, and
   do not copy live matchers. Inputs/outputs/storage are disjoint. Failure is
   clearable. Prefix lookback always sees the original normalized text. */
QJSIntlStatus qjs_intl_collation_match_init(QJSIntlCollationMatch *,
                                           const QJSIntlCollationContexts *,
                                           const uint16_t *, size_t, const QJSIntlAllocator *);
/* As above, with an optional copied query descriptor. Its opaque owner and pack
   bytes must remain alive and unchanged until clear; mutation during/between
   next calls of this scan is forbidden. Root-only init needs no descriptor. */
QJSIntlStatus qjs_intl_collation_match_init_overlay(QJSIntlCollationMatch *,
                                                   const QJSIntlCollationContexts *,
                                                   const uint16_t *, size_t,
                                                   const QJSIntlCollationOverlay *,
                                                   const QJSIntlAllocator *);
void qjs_intl_collation_match_clear(QJSIntlCollationMatch *);
/* Return one mapping, unmatched scalar, or (when number != NULL) adjacent Nd
   run. Matching consumes suffix text only; callers decode/emit its CEs. Only
   entry for ROOT, scalar for SCALAR, or sequence for OVERLAY is meaningful.
   Root entries borrow pack bytes; overlay sequence IDs belong to its owner.
   Number digits borrow immutable normalized text
   until clear and have leading zeros removed, retaining one zero. Do not mix
   numeric/non-numeric calls. Exhaustion is success with present=0, repeatable. */
QJSIntlStatus qjs_intl_collation_match_next(QJSIntlCollationMatch *,
                                           QJSIntlCollationMatchResult *,
                                           QJSIntlCollationNumber *, int *present);
#endif
