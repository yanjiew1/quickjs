/* ECMA-402 MakePartsList, independent of relative pattern/plural selection.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "format.h"
#include "../common/format-builder.h"
#include <string.h>

typedef struct RelativeWriter {
    QJSIntlFormatBuilder text; /* Its ordinary part buffer stays NULL. */
    QJSIntlRelativePart *parts;
} RelativeWriter;

static QJSIntlStatus literal(RelativeWriter *writer, QJSIntlBytes value, int keep_empty)
{
    size_t start = writer->text.length;
    QJSIntlStatus status = qjs_intl_format_builder_text(&writer->text, value, NULL, NULL);
    if (status || (!keep_empty && start == writer->text.length))
        return status;
    if (writer->text.count == SIZE_MAX / sizeof(*writer->parts))
        return QJS_INTL_OVERFLOW;
    if (writer->parts)
        writer->parts[writer->text.count] = (QJSIntlRelativePart){start, writer->text.length,
                                                                 QJS_INTL_PART_LITERAL,
                                                                 QJS_INTL_RELATIVE_UNIT_COUNT};
    writer->text.count++;
    return QJS_INTL_OK;
}

static int body_valid(const QJSIntlFormatted *body)
{
    size_t i, at = 0;
    if (!body || !body->text || !body->length || !body->parts || !body->part_count ||
        body->length > SIZE_MAX / sizeof(*body->text) ||
        body->part_count > SIZE_MAX / sizeof(*body->parts))
        return 0;
    for (i = 0; i < body->part_count; i++) {
        const QJSIntlPart *part = &body->parts[i];
        if (part->start != at || part->end < at || part->end > body->length ||
            part->type == QJS_INTL_PART_ELEMENT || (unsigned int)part->type >= QJS_INTL_PART_COUNT)
            return 0;
        at = part->end;
    }
    return at == body->length;
}

static QJSIntlStatus number(RelativeWriter *writer, const QJSIntlFormatted *body,
                            QJSIntlRelativeUnit unit)
{
    size_t i, start = writer->text.length, count = writer->text.count;
    QJSIntlStatus status;
    if (body->part_count > SIZE_MAX / sizeof(*writer->parts) - count)
        return QJS_INTL_OVERFLOW;
    status = qjs_intl_format_builder_body(&writer->text, body);
    if (!status && writer->parts) {
        for (i = 0; i < body->part_count; i++) {
            const QJSIntlPart *part = &body->parts[i];
            writer->parts[count + i] = (QJSIntlRelativePart){start + part->start, start + part->end,
                                                              part->type, unit};
        }
    }
    return status;
}

static QJSIntlStatus emit(RelativeWriter *writer, QJSIntlBytes prefix, QJSIntlBytes suffix,
                          const QJSIntlFormatted *body, QJSIntlRelativeUnit unit, int auto_literal)
{
    QJSIntlStatus status = literal(writer, prefix, auto_literal);
    if (!status && body)
        status = number(writer, body, unit);
    if (!status)
        status = literal(writer, suffix, 0);
    return status;
}

void qjs_intl_relative_result_clear(const QJSIntlAllocator *allocator, QJSIntlRelativeFormatted *out)
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

static QJSIntlStatus compose(const QJSIntlAllocator *allocator, QJSIntlBytes prefix, QJSIntlBytes suffix,
                             const QJSIntlFormatted *body, QJSIntlRelativeUnit unit, int auto_literal,
                             size_t limit, QJSIntlRelativeFormatted *out)
{
    RelativeWriter writer = {0};
    QJSIntlRelativeFormatted result = {0};
    QJSIntlStatus status;
    writer.text.limit = limit;
    status = emit(&writer, prefix, suffix, body, unit, auto_literal);
    if (status)
        return status;
    if (writer.text.length > SIZE_MAX / sizeof(*result.text))
        return QJS_INTL_OVERFLOW;
    result.length = writer.text.length;
    result.part_count = writer.text.count;
    result.text = allocator->malloc(allocator->opaque, (result.length ? result.length : 1) * sizeof(*result.text));
    if (!result.text)
        return QJS_INTL_NO_MEMORY;
    if (result.part_count) {
        result.parts = allocator->malloc(allocator->opaque, result.part_count * sizeof(*result.parts));
        if (!result.parts) {
            qjs_intl_relative_result_clear(allocator, &result);
            return QJS_INTL_NO_MEMORY;
        }
    }
    writer.text.result.text = result.text;
    writer.parts = result.parts;
    writer.text.length = writer.text.count = 0;
    status = emit(&writer, prefix, suffix, body, unit, auto_literal);
    if (status) {
        qjs_intl_relative_result_clear(allocator, &result);
        return status;
    }
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_relative_pattern_format(const QJSIntlAllocator *allocator, QJSIntlBytes pattern,
                                               const QJSIntlFormatted *body, QJSIntlRelativeUnit unit,
                                               size_t limit, QJSIntlRelativeFormatted *out)
{
    QJSIntlBytes prefix = pattern, suffix = {0};
    size_t i, placeholder = SIZE_MAX;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !limit ||
        (!pattern.data && pattern.length) || pattern.length > PTRDIFF_MAX ||
        (unsigned int)unit >= QJS_INTL_RELATIVE_UNIT_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < pattern.length; i++) {
        if (pattern.data[i] == '{') {
            if (placeholder != SIZE_MAX || pattern.length - i < 3 ||
                pattern.data[i + 1] != '0' || pattern.data[i + 2] != '}')
                return QJS_INTL_DATA_ERROR;
            placeholder = i;
            i += 2;
        } else if (pattern.data[i] == '}') {
            return QJS_INTL_DATA_ERROR;
        }
    }
    if (placeholder != SIZE_MAX) {
        if (!body_valid(body))
            return QJS_INTL_INVALID_ARGUMENT;
        prefix.length = placeholder;
        suffix = (QJSIntlBytes){pattern.data + placeholder + 3, pattern.length - placeholder - 3};
    } else {
        body = NULL;
    }
    return compose(allocator, prefix, suffix, body, unit, 0, limit, out);
}

QJSIntlStatus qjs_intl_relative_literal_format(const QJSIntlAllocator *allocator, QJSIntlBytes text,
                                               size_t limit, QJSIntlRelativeFormatted *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !limit ||
        (!text.data && text.length) || text.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    return compose(allocator, text, (QJSIntlBytes){0}, NULL, QJS_INTL_RELATIVE_UNIT_COUNT, 1, limit, out);
}
