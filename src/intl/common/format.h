/* Exact formatter input and result slices, independent of engine values.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_FORMAT_H
#define QJS_INTL_COMMON_FORMAT_H
#include "types.h"
#include <string.h>

/* UTF-16 code units, preserving embedded NUL and unpaired surrogates.
   Input slices are borrowed and remain unchanged during a formatting call. */
typedef struct QJSIntlUTF16 {
    const uint16_t *data;
    size_t length;
} QJSIntlUTF16;

typedef enum QJSIntlPartType {
    QJS_INTL_PART_LITERAL,
    QJS_INTL_PART_ELEMENT,
    QJS_INTL_PART_INTEGER,
    QJS_INTL_PART_GROUP,
    QJS_INTL_PART_DECIMAL,
    QJS_INTL_PART_FRACTION,
    QJS_INTL_PART_NAN,
    QJS_INTL_PART_INFINITY,
    QJS_INTL_PART_MINUS_SIGN,
    QJS_INTL_PART_PLUS_SIGN,
    QJS_INTL_PART_PERCENT_SIGN,
    QJS_INTL_PART_CURRENCY,
    QJS_INTL_PART_EXPONENT_SEPARATOR,
    QJS_INTL_PART_EXPONENT_MINUS_SIGN,
    QJS_INTL_PART_EXPONENT_INTEGER,
    QJS_INTL_PART_COMPACT,
    QJS_INTL_PART_UNIT,
    QJS_INTL_PART_APPROXIMATELY_SIGN,
    /* Number-bearing services validate this prefix, not later date fields. */
    QJS_INTL_PART_NUMBER_COUNT,
    QJS_INTL_PART_YEAR = QJS_INTL_PART_NUMBER_COUNT,
    QJS_INTL_PART_MONTH,
    QJS_INTL_PART_DAY,
    QJS_INTL_PART_HOUR,
    QJS_INTL_PART_MINUTE,
    QJS_INTL_PART_SECOND,
    QJS_INTL_PART_FRACTIONAL_SECOND,
    QJS_INTL_PART_RELATED_YEAR,
    QJS_INTL_PART_ERA,
    QJS_INTL_PART_WEEKDAY,
    QJS_INTL_PART_YEAR_NAME,
    QJS_INTL_PART_DAY_PERIOD,
    QJS_INTL_PART_TIME_ZONE_NAME,
    QJS_INTL_PART_COUNT
} QJSIntlPartType;

typedef struct QJSIntlPart {
    size_t start, end; /* UTF-16 offsets, inclusive start/exclusive end. */
    QJSIntlPartType type;
} QJSIntlPart;

typedef struct QJSIntlFormatted {
    uint16_t *text; /* Owned, explicit length; no trailing NUL required. */
    size_t length;
    QJSIntlPart *parts; /* Owned spans partition text, including empty elements. */
    size_t part_count;
} QJSIntlFormatted;

typedef enum QJSIntlRangeSource {
    QJS_INTL_RANGE_SHARED,
    QJS_INTL_RANGE_START,
    QJS_INTL_RANGE_END
} QJSIntlRangeSource;

/* Range formatting has its own part record; ordinary results need no source
   field. Number/DateTime range policies decide which spans are shared. */
typedef struct QJSIntlRangePart {
    size_t start, end;
    QJSIntlPartType type;
    QJSIntlRangeSource source;
} QJSIntlRangePart;

typedef struct QJSIntlFormattedRange {
    uint16_t *text;
    size_t length;
    QJSIntlRangePart *parts;
    size_t part_count;
} QJSIntlFormattedRange;

/* Clear owned result buffers with the creating allocator. A live result
   requires its original free callback and opaque; never clear borrowed input
   slices. NULL and already-zero results are harmless. Shared by independent
   service clear operations, with no backend or formatting policy. */
static inline void qjs_intl_formatted_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    if (!out)
        return;
    if (allocator && allocator->free) {
        if (out->text)
            allocator->free(allocator->opaque, out->text);
        if (out->parts)
            allocator->free(allocator->opaque, out->parts);
    }
    memset(out, 0, sizeof(*out));
}

static inline void qjs_intl_formatted_range_clear(const QJSIntlAllocator *allocator,
                                                 QJSIntlFormattedRange *out)
{
    if (!out)
        return;
    if (allocator && allocator->free) {
        if (out->text)
            allocator->free(allocator->opaque, out->text);
        if (out->parts)
            allocator->free(allocator->opaque, out->parts);
    }
    memset(out, 0, sizeof(*out));
}
#endif
