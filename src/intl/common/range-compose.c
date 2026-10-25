/* Range fragment assembly; service owners retain all formatting policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "range-compose.h"
#include "format-builder.h"

typedef struct RangeWriter {
    QJSIntlFormatBuilder text; /* Its ordinary-part output stays NULL. */
    QJSIntlRangePart *parts;
} RangeWriter;

static int valid_body(const QJSIntlFormatted *value)
{
    size_t i, at = 0;
    if (!value->text || !value->length || !value->parts || !value->part_count ||
        value->length > SIZE_MAX / sizeof(*value->text) ||
        value->part_count > SIZE_MAX / sizeof(*value->parts))
        return 0;
    for (i = 0; i < value->part_count; i++) {
        const QJSIntlPart *part = &value->parts[i];
        if (part->start != at || part->end < at || part->end > value->length ||
            (unsigned int)part->type >= QJS_INTL_PART_COUNT)
            return 0;
        at = part->end;
    }
    return at == value->length;
}

static QJSIntlStatus emit(RangeWriter *writer, const QJSIntlRangePiece *pieces, size_t count)
{
    size_t i, j;
    for (i = 0; i < count; i++) {
        const QJSIntlRangePiece *piece = &pieces[i];
        size_t start = writer->text.length, first = writer->text.count;
        QJSIntlStatus status;
        if (piece->body) {
            const QJSIntlFormatted *body = piece->body;
            if (body->part_count > SIZE_MAX / sizeof(*writer->parts) - first)
                return QJS_INTL_OVERFLOW;
            status = qjs_intl_format_builder_body(&writer->text, body);
            if (!status && writer->parts) {
                for (j = 0; j < body->part_count; j++) {
                    const QJSIntlPart *part = &body->parts[j];
                    writer->parts[first + j] = (QJSIntlRangePart){
                        start + part->start, start + part->end, part->type, piece->source,
                    };
                }
            }
        } else {
            status = qjs_intl_format_builder_text(&writer->text, piece->literal, NULL, NULL);
            if (!status && start != writer->text.length) {
                if (first == SIZE_MAX / sizeof(*writer->parts))
                    return QJS_INTL_OVERFLOW;
                if (writer->parts)
                    writer->parts[first] = (QJSIntlRangePart){
                        start, writer->text.length, piece->type, piece->source,
                    };
                writer->text.count++;
            }
        }
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_range_compose(const QJSIntlAllocator *allocator,
                                    const QJSIntlRangePiece *pieces, size_t count,
                                    size_t maximum_output_length, QJSIntlFormattedRange *out)
{
    RangeWriter writer = {0};
    QJSIntlFormattedRange result = {0};
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !pieces ||
        !count || count > PTRDIFF_MAX / sizeof(*pieces) || !maximum_output_length)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < count; i++) {
        const QJSIntlRangePiece *piece = &pieces[i];
        if ((unsigned int)piece->source > QJS_INTL_RANGE_END ||
            (piece->body ? !valid_body(piece->body) :
             (!piece->literal.data && piece->literal.length) ||
             piece->literal.length > PTRDIFF_MAX || (unsigned int)piece->type >= QJS_INTL_PART_COUNT))
            return QJS_INTL_INVALID_ARGUMENT;
    }
    writer.text.limit = maximum_output_length;
    status = emit(&writer, pieces, count);
    if (status)
        return status;
    if (!writer.text.length || !writer.text.count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (writer.text.length > SIZE_MAX / sizeof(*result.text))
        return QJS_INTL_OVERFLOW;
    result.length = writer.text.length;
    result.part_count = writer.text.count;
    result.text = allocator->malloc(allocator->opaque, result.length * sizeof(*result.text));
    if (result.text)
        result.parts = allocator->malloc(allocator->opaque, result.part_count * sizeof(*result.parts));
    if (!result.text || !result.parts) {
        qjs_intl_formatted_range_clear(allocator, &result);
        return QJS_INTL_NO_MEMORY;
    }
    writer.text.result.text = result.text;
    writer.parts = result.parts;
    writer.text.length = writer.text.count = 0;
    status = emit(&writer, pieces, count);
    if (status)
        qjs_intl_formatted_range_clear(allocator, &result);
    else
        *out = result;
    return status;
}
