/* ECMA-402 PartitionNumberPattern unit-prefix/suffix composition.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
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
#include "format.h"
#include "../common/format-builder.h"
#include <string.h>

static QJSIntlStatus affix(QJSIntlFormatBuilder *builder, const QJSIntlUnitAffix *text, int *has_unit)
{
    size_t i, start = builder->length, first = SIZE_MAX, last = 0;
    QJSIntlStatus status;
    for (i = 0; i < text->count; i++) {
        status = qjs_intl_format_builder_text(builder, text->spans[i], &first, &last);
        if (status)
            return status;
    }
    if (first == SIZE_MAX)
        return qjs_intl_format_builder_part(builder, start, builder->length, QJS_INTL_PART_LITERAL, 0);
    status = qjs_intl_format_builder_part(builder, start, first, QJS_INTL_PART_LITERAL, 0);
    if (!status)
        status = qjs_intl_format_builder_part(builder, first, last, QJS_INTL_PART_UNIT, 0);
    if (!status)
        status = qjs_intl_format_builder_part(builder, last, builder->length, QJS_INTL_PART_LITERAL, 0);
    *has_unit = 1;
    return status;
}

static QJSIntlStatus emit(QJSIntlFormatBuilder *builder, const QJSIntlUnitAffix *prefix,
                          const QJSIntlUnitAffix *suffix,
                          const QJSIntlFormatted *body)
{
    int has_unit = 0;
    QJSIntlStatus status = affix(builder, prefix, &has_unit);
    if (!status)
        status = qjs_intl_format_builder_body(builder, body);
    if (!status)
        status = affix(builder, suffix, &has_unit);
    if (!status && !has_unit)
        status = qjs_intl_format_builder_part(builder, builder->length, builder->length,
                                              QJS_INTL_PART_UNIT, 1);
    return status;
}

void qjs_intl_unit_format_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    qjs_intl_formatted_clear(allocator, out);
}

QJSIntlStatus qjs_intl_unit_affixes_format(const QJSIntlAllocator *allocator,
                                           const QJSIntlUnitAffix *prefix,
                                           const QJSIntlUnitAffix *suffix,
                                           const QJSIntlFormatted *body, size_t limit,
                                           QJSIntlFormatted *out)
{
    QJSIntlFormatBuilder builder = {0};
    QJSIntlStatus status;
    size_t i, at = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !prefix || !suffix ||
        (!prefix->spans && prefix->count) || (!suffix->spans && suffix->count) ||
        prefix->count > SIZE_MAX / sizeof(*prefix->spans) ||
        suffix->count > SIZE_MAX / sizeof(*suffix->spans) || !body ||
        !body->text || !body->length || !body->parts || !body->part_count || !limit ||
        body->part_count > SIZE_MAX / sizeof(QJSIntlPart))
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < body->part_count; i++) {
        const QJSIntlPart *part = &body->parts[i];
        if (part->start != at || part->end < at || part->end > body->length ||
            part->type == QJS_INTL_PART_ELEMENT || (unsigned int)part->type >= QJS_INTL_PART_NUMBER_COUNT)
            return QJS_INTL_INVALID_ARGUMENT;
        at = part->end;
    }
    if (at != body->length)
        return QJS_INTL_INVALID_ARGUMENT;
    builder.limit = limit;
    status = emit(&builder, prefix, suffix, body);
    if (status)
        return status;
    if (builder.length > SIZE_MAX / sizeof(uint16_t))
        return QJS_INTL_OVERFLOW;
    builder.result.length = builder.length;
    builder.result.part_count = builder.count;
    builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
    if (!builder.result.text)
        return QJS_INTL_NO_MEMORY;
    builder.result.parts = allocator->malloc(allocator->opaque, builder.count * sizeof(QJSIntlPart));
    if (!builder.result.parts) {
        qjs_intl_unit_format_clear(allocator, &builder.result);
        return QJS_INTL_NO_MEMORY;
    }
    builder.length = builder.count = 0;
    status = emit(&builder, prefix, suffix, body);
    if (status) {
        qjs_intl_unit_format_clear(allocator, &builder.result);
        return status;
    }
    *out = builder.result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_unit_pattern_format(const QJSIntlAllocator *allocator,
                                           const QJSIntlUnitPattern *pattern,
                                           const QJSIntlFormatted *body, size_t limit,
                                           QJSIntlFormatted *out)
{
    QJSIntlUnitAffix prefix, suffix;
    if (!pattern) {
        if (out)
            memset(out, 0, sizeof(*out));
        return QJS_INTL_INVALID_ARGUMENT;
    }
    prefix = (QJSIntlUnitAffix){&pattern->prefix, 1};
    suffix = (QJSIntlUnitAffix){&pattern->suffix, 1};
    return qjs_intl_unit_affixes_format(allocator, &prefix, &suffix, body, limit, out);
}
