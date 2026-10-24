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
   Borrow component text/parts, preserving NUL, lone surrogates and empty
   elements. Require matching element text and complete part partitions.
   At most UNIT_COUNT components; allocator malloc/free and a nonzero UTF-16
   limit are required. list->text is owned by that allocator (or NULL when
   empty); list->parts remains borrowed. Success transfers text to the fresh,
   disjoint output and clears list->text/length; the caller still releases any
   owned list parts. Failure resets output and leaves list unchanged. Results
   survive the other inputs. No locale or formatting policy here. */
QJSIntlStatus qjs_intl_duration_join_parts(const QJSIntlAllocator *, QJSIntlFormatted *list,
                                         const QJSIntlDurationComponent *, size_t count,
                                         size_t limit, QJSIntlDurationFormatted *);
void qjs_intl_duration_result_clear(const QJSIntlAllocator *, QJSIntlDurationFormatted *);
#endif
