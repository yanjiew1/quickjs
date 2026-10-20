/* ECMA-402 PartitionNumberRangePattern / FormatApproximately; no collapse.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "range.h"
#include "number.h"
#include "../common/format-builder.h"
#include <string.h>

QJSIntlStatus qjs_intl_number_range_separator(QJSIntlBytes pattern, QJSIntlBytes *out)
{
    QJSIntlFormatBuilder validator = {0};
    QJSIntlBytes separator;
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!pattern.data && pattern.length) || pattern.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!pattern.length)
        return QJS_INTL_UNSUPPORTED;
    if (pattern.length < 6 || memcmp(pattern.data, "{0}", 3) ||
        memcmp(pattern.data + pattern.length - 3, "{1}", 3))
        return QJS_INTL_DATA_ERROR;
    separator = (QJSIntlBytes){pattern.data + 3, pattern.length - 6};
    if (!separator.length)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < separator.length; i++) {
        if (separator.data[i] == '{' || separator.data[i] == '}')
            return QJS_INTL_DATA_ERROR;
    }
    validator.limit = SIZE_MAX;
    status = qjs_intl_format_builder_text(&validator, separator, NULL, NULL);
    if (!status)
        *out = separator;
    return status;
}

typedef struct RangeWriter {
    QJSIntlFormatBuilder text; /* Its ordinary-part output stays NULL. */
    QJSIntlRangePart *parts;
} RangeWriter;

static QJSIntlStatus literal(RangeWriter *writer, QJSIntlBytes value, QJSIntlPartType type)
{
    size_t start = writer->text.length;
    QJSIntlStatus status = qjs_intl_format_builder_text(&writer->text, value, NULL, NULL);
    if (status || start == writer->text.length)
        return status;
    if (writer->text.count == SIZE_MAX / sizeof(*writer->parts))
        return QJS_INTL_OVERFLOW;
    if (writer->parts)
        writer->parts[writer->text.count] = (QJSIntlRangePart){start, writer->text.length,
                                                              type, QJS_INTL_RANGE_SHARED};
    writer->text.count++;
    return QJS_INTL_OK;
}

static QJSIntlStatus body(RangeWriter *writer, const QJSIntlFormatted *value, QJSIntlRangeSource source)
{
    size_t i, start = writer->text.length, count = writer->text.count;
    QJSIntlStatus status;
    if (value->part_count > SIZE_MAX / sizeof(*writer->parts) - count)
        return QJS_INTL_OVERFLOW;
    status = qjs_intl_format_builder_body(&writer->text, value);
    if (!status && writer->parts) {
        for (i = 0; i < value->part_count; i++) {
            const QJSIntlPart *part = &value->parts[i];
            writer->parts[count + i] = (QJSIntlRangePart){part->start + start, part->end + start,
                                                          part->type, source};
        }
    }
    return status;
}

static QJSIntlStatus emit(RangeWriter *writer, QJSIntlBytes separator, QJSIntlBytes approximately,
                          const QJSIntlFormatted *start, const QJSIntlFormatted *end, int equal)
{
    QJSIntlStatus status;
    if (equal) {
        status = literal(writer, approximately, QJS_INTL_PART_APPROXIMATELY_SIGN);
        if (!status)
            status = body(writer, start, QJS_INTL_RANGE_SHARED);
    } else {
        status = body(writer, start, QJS_INTL_RANGE_START);
        if (!status)
            status = literal(writer, separator, QJS_INTL_PART_LITERAL);
        if (!status)
            status = body(writer, end, QJS_INTL_RANGE_END);
    }
    return status;
}

static int body_valid(const QJSIntlFormatted *value)
{
    size_t i, at = 0;
    if (!value || !value->text || !value->length || !value->parts || !value->part_count ||
        value->length > SIZE_MAX / sizeof(*value->text) ||
        value->part_count > SIZE_MAX / sizeof(*value->parts))
        return 0;
    for (i = 0; i < value->part_count; i++) {
        const QJSIntlPart *part = &value->parts[i];
        if (part->start != at || part->end < at || part->end > value->length ||
            part->type == QJS_INTL_PART_ELEMENT || (unsigned int)part->type >= QJS_INTL_PART_NUMBER_COUNT)
            return 0;
        at = part->end;
    }
    return at == value->length;
}

void qjs_intl_number_range_result_clear(const QJSIntlAllocator *allocator, QJSIntlFormattedRange *out)
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

QJSIntlStatus qjs_intl_number_range_compose(const QJSIntlAllocator *allocator, QJSIntlBytes separator,
                                          QJSIntlBytes approximately,
                                          const QJSIntlFormatted *start, const QJSIntlFormatted *end,
                                          size_t limit, QJSIntlFormattedRange *out)
{
    RangeWriter writer = {0};
    QJSIntlFormattedRange result = {0};
    QJSIntlStatus status;
    int equal;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free ||
        !separator.data || !separator.length || separator.length > PTRDIFF_MAX ||
        (!approximately.data && approximately.length) || approximately.length > PTRDIFF_MAX ||
        !limit || !body_valid(start) || !body_valid(end))
        return QJS_INTL_INVALID_ARGUMENT;
    if (start->length > limit || end->length > limit)
        return QJS_INTL_OVERFLOW;
    equal = start->length == end->length &&
            !memcmp(start->text, end->text, start->length * sizeof(*start->text));
    writer.text.limit = limit;
    status = emit(&writer, separator, approximately, start, end, equal);
    if (status)
        return status;
    if (writer.text.length > SIZE_MAX / sizeof(*result.text))
        return QJS_INTL_OVERFLOW;
    result.length = writer.text.length;
    result.part_count = writer.text.count;
    result.text = allocator->malloc(allocator->opaque, result.length * sizeof(*result.text));
    if (!result.text)
        return QJS_INTL_NO_MEMORY;
    result.parts = allocator->malloc(allocator->opaque, result.part_count * sizeof(*result.parts));
    if (!result.parts) {
        qjs_intl_number_range_result_clear(allocator, &result);
        return QJS_INTL_NO_MEMORY;
    }
    writer.text.result.text = result.text;
    writer.parts = result.parts;
    writer.text.length = writer.text.count = 0;
    status = emit(&writer, separator, approximately, start, end, equal);
    if (status) {
        qjs_intl_number_range_result_clear(allocator, &result);
        return status;
    }
    *out = result;
    return QJS_INTL_OK;
}
