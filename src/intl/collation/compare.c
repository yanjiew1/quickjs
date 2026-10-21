/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "compare.h"
#include "elements.h"
#include <string.h>

typedef struct ElementStream {
    QJSIntlCollationElements iterator;
    QJSIntlCollationElement *elements;
    size_t count, capacity;
    QJSIntlCollationElement small[8];
} ElementStream;

static void stream_clear(ElementStream *stream)
{
    if (stream->elements && stream->elements != stream->small)
        stream->iterator.allocator.free(stream->iterator.allocator.opaque, stream->elements);
    qjs_intl_collation_elements_clear(&stream->iterator);
}

/* Keep lower-level weights only while primaries tie. This avoids building two
   complete sort keys just to decide strings with an early primary difference. */
static QJSIntlStatus stream_primary(ElementStream *stream, unsigned int *weight)
{
    QJSIntlStatus status;
    QJSIntlCollationElement element;
    int present;
    *weight = 0;
    for (;;) {
        status = qjs_intl_collation_elements_next(&stream->iterator, &element, &present);
        if (status || !present)
            return status;
        if (!element.primary && !element.secondary && !element.tertiary)
            continue;
        if (stream->count == stream->capacity) {
            QJSIntlAllocator *a = &stream->iterator.allocator;
            QJSIntlCollationElement *next;
            size_t capacity;
            if (stream->capacity > SIZE_MAX / sizeof(*next) / 2)
                return QJS_INTL_OVERFLOW;
            capacity = stream->capacity * 2;
            if (stream->elements == stream->small) {
                next = a->malloc(a->opaque, capacity * sizeof(*next));
                if (next)
                    memcpy(next, stream->small, stream->count * sizeof(*next));
            } else {
                next = a->realloc(a->opaque, stream->elements, capacity * sizeof(*next));
            }
            if (!next)
                return QJS_INTL_NO_MEMORY;
            stream->elements = next;
            stream->capacity = capacity;
        }
        stream->elements[stream->count++] = element;
        if (element.primary) {
            *weight = element.primary;
            return QJS_INTL_OK;
        }
    }
}

static unsigned int next_level(const ElementStream *stream, size_t *at, int tertiary)
{
    while (*at < stream->count) {
        const QJSIntlCollationElement *element = &stream->elements[(*at)++];
        unsigned int weight = tertiary ? element->tertiary : element->secondary;
        if (weight)
            return weight;
    }
    return 0;
}

QJSIntlStatus qjs_intl_collation_compare_root(const QJSIntlCollationContexts *data,
                                             const uint16_t *left, size_t left_length,
                                             const uint16_t *right, size_t right_length,
                                             const QJSIntlAllocator *allocator, int *order)
{
    ElementStream streams[2] = {0};
    QJSIntlStatus status;
    unsigned int x, y;
    int level;
    if (!order)
        return QJS_INTL_INVALID_ARGUMENT;
    *order = 0;
    if (!data || !allocator || !allocator->malloc || !allocator->realloc ||
        !allocator->free || (!left && left_length) || (!right && right_length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!data->section.bytes)
        return QJS_INTL_UNSUPPORTED;
    if (left_length > SIZE_MAX / sizeof(*left) || right_length > SIZE_MAX / sizeof(*right))
        return QJS_INTL_OVERFLOW;
    /* Identity is safe even across contraction boundaries; a common-prefix
       shortcut would not be, since context can change following elements. */
    if (left_length == right_length &&
        (!left_length || !memcmp(left, right, left_length * sizeof(*left))))
        return QJS_INTL_OK;
    status = qjs_intl_collation_elements_init(&streams[0].iterator, data,
                                              left, left_length, allocator);
    if (status)
        goto done;
    status = qjs_intl_collation_elements_init(&streams[1].iterator, data,
                                              right, right_length, allocator);
    if (status)
        goto done;
    for (level = 0; level < 2; level++) {
        streams[level].elements = streams[level].small;
        streams[level].capacity = sizeof(streams[level].small) / sizeof(*streams[level].small);
    }
    for (;;) {
        status = stream_primary(&streams[0], &x);
        if (status)
            goto done;
        status = stream_primary(&streams[1], &y);
        if (status)
            goto done;
        if (x != y) {
            *order = x < y ? -1 : 1;
            goto done;
        }
        if (!x)
            break;
    }
    /* Each level compares complete nonzero streams. Comparing CEs one by one
       would let an early accent/case difference override a later primary. */
    for (level = 0; level < 2; level++) {
        size_t a = 0, b = 0;
        do {
            x = next_level(&streams[0], &a, level);
            y = next_level(&streams[1], &b, level);
            if (x != y) {
                *order = x < y ? -1 : 1;
                goto done;
            }
        } while (x);
    }
done:
    stream_clear(&streams[0]);
    stream_clear(&streams[1]);
    return status;
}
