/* Exact formatter input and result slices, independent of engine values.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_FORMAT_H
#define QJS_INTL_COMMON_FORMAT_H
#include "types.h"

/* UTF-16 code units, preserving embedded NUL and unpaired surrogates.
   Input slices are borrowed and remain unchanged during a formatting call. */
typedef struct QJSIntlUTF16 {
    const uint16_t *data;
    size_t length;
} QJSIntlUTF16;

typedef enum QJSIntlPartType {
    QJS_INTL_PART_LITERAL,
    QJS_INTL_PART_ELEMENT
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
#endif
