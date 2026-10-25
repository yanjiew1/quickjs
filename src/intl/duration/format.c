/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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

void qjs_intl_duration_result_clear(const QJSIntlAllocator *allocator, QJSIntlDurationFormatted *result)
{
    if (!result || !allocator || !allocator->free)
        return;
    if (result->text)
        allocator->free(allocator->opaque, result->text);
    if (result->parts)
        allocator->free(allocator->opaque, result->parts);
    memset(result, 0, sizeof(*result));
}

static QJSIntlStatus component_valid(const QJSIntlDurationComponent *component)
{
    size_t i, end = 0;
    if ((component->text.length && !component->text.data) ||
        (component->part_count && !component->parts))
        return QJS_INTL_INVALID_ARGUMENT;
    if (component->part_count > SIZE_MAX / sizeof(*component->parts))
        return QJS_INTL_OVERFLOW;
    for (i = 0; i < component->part_count; i++) {
        const QJSIntlDurationPart *part = &component->parts[i];
        if (part->start != end || part->end < end || part->end > component->text.length ||
            (unsigned int)part->type >= QJS_INTL_PART_NUMBER_COUNT || part->type == QJS_INTL_PART_ELEMENT ||
            (unsigned int)part->unit > QJS_TEMPORAL_UNIT_COUNT)
            return QJS_INTL_DATA_ERROR;
        end = part->end;
    }
    return end == component->text.length ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

QJSIntlStatus qjs_intl_duration_join_parts(const QJSIntlAllocator *allocator, QJSIntlFormatted *list,
                                         const QJSIntlDurationComponent *components, size_t count,
                                         size_t limit, QJSIntlDurationFormatted *out)
{
    QJSIntlDurationFormatted result = {0};
    QJSIntlStatus status;
    size_t i, j, end = 0, element = 0, parts = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !list || !limit ||
        count > QJS_TEMPORAL_UNIT_COUNT || (count && !components) ||
        (list->length && !list->text) || (list->part_count && !list->parts))
        return QJS_INTL_INVALID_ARGUMENT;
    if (list->length > limit || list->length > SIZE_MAX / sizeof(*result.text) ||
        list->part_count > SIZE_MAX / sizeof(*list->parts))
        return QJS_INTL_OVERFLOW;
    for (i = 0; i < list->part_count; i++) {
        const QJSIntlPart *part = &list->parts[i];
        size_t added = 1;
        if (part->start != end || part->end < end || part->end > list->length)
            return QJS_INTL_DATA_ERROR;
        end = part->end;
        if (part->type == QJS_INTL_PART_ELEMENT) {
            const QJSIntlDurationComponent *component;
            if (element == count)
                return QJS_INTL_DATA_ERROR;
            component = &components[element++];
            status = component_valid(component);
            if (status)
                return status;
            if (component->text.length != part->end - part->start ||
                (component->text.length && memcmp(component->text.data, list->text + part->start,
                                                 component->text.length * sizeof(*list->text))))
                return QJS_INTL_DATA_ERROR;
            added = component->part_count;
        } else if (part->type != QJS_INTL_PART_LITERAL) {
            return QJS_INTL_DATA_ERROR;
        }
        if (added > SIZE_MAX / sizeof(*result.parts) - parts)
            return QJS_INTL_OVERFLOW;
        parts += added;
    }
    if (end != list->length || element != count)
        return QJS_INTL_DATA_ERROR;
    if (parts) {
        result.parts = allocator->malloc(allocator->opaque, parts * sizeof(*result.parts));
        if (!result.parts)
            return QJS_INTL_NO_MEMORY;
    }
    result.length = list->length;
    element = 0;
    for (i = 0; i < list->part_count; i++) {
        const QJSIntlPart *part = &list->parts[i];
        if (part->type == QJS_INTL_PART_ELEMENT) {
            const QJSIntlDurationComponent *component = &components[element++];
            for (j = 0; j < component->part_count; j++) {
                QJSIntlDurationPart value = component->parts[j];
                value.start += part->start;
                value.end += part->start;
                result.parts[result.part_count++] = value;
            }
        } else {
            result.parts[result.part_count++] = (QJSIntlDurationPart){
                part->start, part->end, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_UNIT_COUNT};
        }
    }
    /* The list already owns the final text. Move it only after all fallible
       validation/allocation, keeping the source intact on every error path. */
    result.text = list->text;
    list->text = NULL;
    list->length = 0;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_join_digital(const QJSIntlAllocator *allocator,
                                           const QJSIntlDurationComponent *components, size_t count,
                                           const QJSIntlBytes *separators, size_t limit,
                                           QJSIntlDurationFormatted *out)
{
    QJSIntlDurationFormatted result = {0};
    QJSIntlFormatBuilder builder = {0};
    QJSIntlStatus status;
    size_t i, j, pass, parts;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !limit ||
        !components || !count || count > 3 || (count > 1 && !separators))
        return QJS_INTL_INVALID_ARGUMENT;
    parts = count - 1;
    for (i = 0; i < count; i++) {
        status = component_valid(&components[i]);
        if (status)
            return status;
        if (components[i].part_count > SIZE_MAX / sizeof(*result.parts) - parts)
            return QJS_INTL_OVERFLOW;
        parts += components[i].part_count;
    }

    builder.limit = limit;
    for (pass = 0; pass < 2; pass++) {
        builder.length = 0;
        for (i = 0; i < count; i++) {
            size_t start = builder.length;
            if (i) {
                status = qjs_intl_format_builder_text(&builder, separators[i - 1], NULL, NULL);
                if (status)
                    goto fail;
                if (pass)
                    result.parts[result.part_count++] = (QJSIntlDurationPart){
                        start, builder.length, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_UNIT_COUNT};
            }
            start = builder.length;
            if (components[i].text.length) {
                QJSIntlFormatted body = {(uint16_t *)components[i].text.data,
                                          components[i].text.length, NULL, 0};
                status = qjs_intl_format_builder_body(&builder, &body);
                if (status)
                    goto fail;
            }
            if (pass) {
                for (j = 0; j < components[i].part_count; j++) {
                    QJSIntlDurationPart part = components[i].parts[j];
                    part.start += start;
                    part.end += start;
                    result.parts[result.part_count++] = part;
                }
            }
        }
        if (!pass) {
            if (builder.length > SIZE_MAX / sizeof(*result.text)) {
                status = QJS_INTL_OVERFLOW;
                goto fail;
            }
            result.length = builder.length;
            result.text = allocator->malloc(allocator->opaque,
                (result.length ? result.length : 1) * sizeof(*result.text));
            if (!result.text) {
                status = QJS_INTL_NO_MEMORY;
                goto fail;
            }
            if (parts) {
                result.parts = allocator->malloc(allocator->opaque, parts * sizeof(*result.parts));
                if (!result.parts) {
                    status = QJS_INTL_NO_MEMORY;
                    goto fail;
                }
            }
            builder.result.text = result.text;
        }
    }
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_duration_result_clear(allocator, &result);
    return status;
}
