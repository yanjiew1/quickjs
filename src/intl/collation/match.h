/* Immutable NFD input and context/contraction matching, independent of CE policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_MATCH_H
#define QJS_INTL_COLLATION_MATCH_H
#include "contexts.h"

typedef struct QJSIntlCollationMatch {
    QJSIntlCollationContexts data;
    QJSIntlAllocator allocator;
    uint32_t *text, *indexes, *groups;
    uint32_t length;
    int ready;
} QJSIntlCollationMatch;

enum {
    QJS_INTL_COLLATION_MATCH_NONE,
    QJS_INTL_COLLATION_MATCH_ROOT,
    QJS_INTL_COLLATION_MATCH_SCALAR,
    QJS_INTL_COLLATION_MATCH_NUMBER
};

typedef struct QJSIntlCollationMatchResult {
    QJSIntlCollationEntry entry;
    uint32_t scalar;
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
void qjs_intl_collation_match_clear(QJSIntlCollationMatch *);
/* Return one mapping, unmatched scalar, or (when number != NULL) adjacent Nd
   run. Matching consumes suffix text only; callers decode/emit its CEs. Root
   entries borrow pack bytes. Number digits borrow immutable normalized text
   until clear and have leading zeros removed, retaining one zero. Do not mix
   numeric/non-numeric calls. Exhaustion is success with present=0, repeatable. */
QJSIntlStatus qjs_intl_collation_match_next(QJSIntlCollationMatch *,
                                           QJSIntlCollationMatchResult *,
                                           QJSIntlCollationNumber *, int *present);
#endif
