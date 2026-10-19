/* Duration component parts and ListFormat composition, independent of JS.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DURATION_FORMAT_H
#define QJS_INTL_DURATION_FORMAT_H
#include "../common/format.h"
#include "../../temporal/types.h"

typedef struct QJSIntlDurationPart {
    size_t start, end;
    QJSIntlPartType type;
    QJSTemporalUnit unit; /* UNIT_COUNT means no unit field. */
} QJSIntlDurationPart;

typedef struct QJSIntlDurationComponent {
    QJSIntlUTF16 text;
    const QJSIntlDurationPart *parts;
    size_t part_count;
} QJSIntlDurationComponent;

typedef struct QJSIntlDurationFormatted {
    uint16_t *text;
    size_t length;
    QJSIntlDurationPart *parts;
    size_t part_count;
} QJSIntlDurationFormatted;

/* ListFormatParts: replace element parts in order with their typed component
   parts, retaining number-origin units and untyped list/digital separators.
   Borrow unchanged UTF-16 inputs, preserving NUL, lone surrogates and empty
   elements. Require matching element text and complete part partitions.
   At most UNIT_COUNT components; allocator malloc/free and a nonzero UTF-16
   limit are required. Fresh disjoint output owns two buffers at most, survives
   all inputs, and resets on failure. No locale or formatting policy here. */
QJSIntlStatus qjs_intl_duration_join_parts(const QJSIntlAllocator *, const QJSIntlFormatted *list,
                                         const QJSIntlDurationComponent *, size_t count,
                                         size_t limit, QJSIntlDurationFormatted *);
void qjs_intl_duration_result_clear(const QJSIntlAllocator *, QJSIntlDurationFormatted *);
#endif
