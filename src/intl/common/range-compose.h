/* Source-preserving assembly of already formatted range fragments.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_RANGE_COMPOSE_H
#define QJS_INTL_COMMON_RANGE_COMPOSE_H
#include "format.h"

typedef struct QJSIntlRangePiece {
    const QJSIntlFormatted *body; /* Non-NULL selects a complete partition. */
    QJSIntlBytes literal; /* Otherwise scalar UTF-8, with the supplied type. */
    QJSIntlPartType type;
    QJSIntlRangeSource source;
} QJSIntlRangePiece;

/* Borrow unchanged fragments, copy text and parts into two final allocations.
   Preserve body parts, including empty parts, and attach each piece's source;
   empty literals emit nothing. Require at least one nonempty output fragment,
   malloc/free, a nonzero UTF-16 bound and fresh disjoint output. Reset errors.
   This performs no endpoint comparison, collapse, ordering or locale policy.
   Clear with formatted_range_clear and the creating allocator. */
QJSIntlStatus qjs_intl_range_compose(const QJSIntlAllocator *allocator,
                                    const QJSIntlRangePiece *pieces, size_t count,
                                    size_t maximum_output_length, QJSIntlFormattedRange *out);
#endif
