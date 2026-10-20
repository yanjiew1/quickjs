/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "format.h"
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
