/* Comparison of validated rank32 programs, independent of JS option handling.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_RANKED_COMPARE_H
#define QJS_INTL_COLLATION_RANKED_COMPARE_H
#include "compiled-data.h"

/* Use program policy when policy is NULL; otherwise use one effective policy
   computed by the caller before scanning. No input/view ownership transfers.
   Archive bytes are immutable/alive for the call; allocator callbacks cannot
   reenter it. Arguments/output are disjoint; order resets on failure.

   This initial comparison slice supports forward P/S/T, caseLevel/caseFirst,
   numeric tokens and group reordering. Shifted, backwards secondary and
   quaternary/identical policies explicitly return UNSUPPORTED until their
   level consumers are present. It does not advertise public service support. */
QJSIntlStatus qjs_intl_collation_compare_ranked(const QJSIntlCompiledCollationProgram *,
                                                const uint16_t *left, size_t left_length,
                                                const uint16_t *right, size_t right_length,
                                                const QJSIntlAllocator *,
                                                const QJSIntlCollationPolicy *, int *order);
#endif
