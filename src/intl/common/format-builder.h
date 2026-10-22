/* Shared bounded UTF-16 emission for typed numeric/unit affixes.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_FORMAT_BUILDER_H
#define QJS_INTL_COMMON_FORMAT_BUILDER_H
#include "format.h"
#include <string.h>

typedef struct QJSIntlFormatBuilder {
    QJSIntlFormatted result;
    size_t length, count, limit;
} QJSIntlFormatBuilder;

/* Internal two-pass writer: zero-init result, choose a nonzero limit, measure,
   allocate exact text/part capacities, reset length/count, then replay the same
   immutable inputs. Consumers own allocation and cleanup. Body parts must
   form a valid partition, or be empty when the caller assigns one enclosing
   part to the copied text. Optional first/last track nonspace UTF-16
   bounds across multiple text appends; supply both, initially SIZE_MAX/0.
   Scalar text excludes NUL and malformed UTF-8. Leading/trailing Unicode
   White_Space is available for callers to classify separately as literal. */
QJSIntlStatus qjs_intl_format_builder_text(QJSIntlFormatBuilder *, QJSIntlBytes,
                                           size_t *first, size_t *last);

static inline QJSIntlStatus qjs_intl_format_builder_part(QJSIntlFormatBuilder *builder,
                                                         size_t start, size_t end,
                                                         QJSIntlPartType type, int keep_empty)
{
    if (start == end && !keep_empty)
        return QJS_INTL_OK;
    if (builder->count == SIZE_MAX / sizeof(QJSIntlPart))
        return QJS_INTL_OVERFLOW;
    if (builder->result.parts)
        builder->result.parts[builder->count] = (QJSIntlPart){start, end, type};
    builder->count++;
    return QJS_INTL_OK;
}

static inline QJSIntlStatus qjs_intl_format_builder_body(QJSIntlFormatBuilder *builder,
                                                         const QJSIntlFormatted *body)
{
    size_t i;
    if (body->length > builder->limit - builder->length ||
        body->part_count > SIZE_MAX / sizeof(QJSIntlPart) - builder->count)
        return QJS_INTL_OVERFLOW;
    if (builder->result.text)
        memcpy(builder->result.text + builder->length, body->text, body->length * sizeof(uint16_t));
    if (builder->result.parts) {
        for (i = 0; i < body->part_count; i++) {
            QJSIntlPart *part = &builder->result.parts[builder->count + i];
            *part = body->parts[i];
            part->start += builder->length;
            part->end += builder->length;
        }
    }
    builder->length += body->length;
    builder->count += body->part_count;
    return QJS_INTL_OK;
}

#endif
