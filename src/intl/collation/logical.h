/* Root logical primaries, separate from compact allkeys storage elements.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_LOGICAL_H
#define QJS_INTL_COLLATION_LOGICAL_H
#include "weights.h"

#define QJS_INTL_COLLATION_PRIMARY_ROOT_MAX UINT32_C(7471103)

typedef struct QJSIntlCollationLogical {
    uint32_t primary; /* Virtual root ordinal, not a final tailored weight. */
    uint16_t secondary, tertiary;
    unsigned int variable, implicit;
} QJSIntlCollationLogical;

/* Root primary order keys are ordinary P<<16 or (implicit_head<<16)|tail.
   The virtual domain includes all nonzero ordinary 16-bit primaries and
   tails 8000..FFFF for possible heads FB00..FBE1, without materializing them.
   Extra unused anchors preserve real root order and leave insertion gaps.
   Zero maps to zero. Invalid keys reset *ordinal. Outputs are disjoint. */
QJSIntlStatus qjs_intl_collation_primary_ordinal(uint32_t key, uint32_t *ordinal);

/* Convert ONE logical root CE from one/two raw storage CEs. second may be
   NULL only if no following CE is available. Consume a recognized implicit
   continuation as part of its head, including tails whose values resemble
   heads. Do not independently classify its zero S/T or its primary group.
   Raw root input only: compiled tailored weights use different coordinates.
   No ownership transfer/allocation; outputs are disjoint and reset on failure.
   Malformed root pairs or non-continuation WF1 violations return DATA_ERROR. */
QJSIntlStatus qjs_intl_collation_logical_root(const QJSIntlCollationElement *first,
                                             const QJSIntlCollationElement *second,
                                             QJSIntlCollationLogical *out, unsigned int *consumed);
#endif
