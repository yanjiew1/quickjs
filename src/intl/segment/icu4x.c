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
#include "prepared.h"
#include "../icu4x/common.h"
#include "../icu4x/locale-data.h"
#include "../../icu4x/segment.h"
#include <limits.h>
#include <string.h>

struct QJSIntlSegmentService {
    QJSIntlAllocator allocator;
    QJSICU4XInventory locales;
};
struct QJSIntlSegmenter {
    QJSIntlAllocator allocator;
    QJSIntlGranularity granularity;
    QJSICU4XSegment *handle;
};

static int enumerate_locales(void *opaque, QJSICU4XEmit *emit)
{
    const char *version = qjs_icu4x_locale_strings + qjs_icu4x_locale_aliases.version;
    size_t i;
    int status;

    for (i = 0; i < qjs_icu4x_locale_registry_count; i++) {
        const char *tag = qjs_icu4x_locale_strings + qjs_icu4x_locale_registry[i];
        size_t length = strlen(tag);
        status = qjs_icu4x_segment_check((const uint8_t *)tag, length, (const uint8_t *)version,
                                         strlen(version));
        if (status)
            return status;
        status = emit(opaque, (const uint8_t *)tag, length);
        if (status)
            return status;
    }
    return QJS_ICU4X_OK;
}

QJSIntlStatus qjs_intl_segment_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                    QJSIntlSegmentService **out)
{
    QJSIntlSegmentService *service;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    // Global break rules and sparse overrides do not enumerate locale support.
    // Use the matching CLDR registry and verify all three real constructors.
    error = qjs_icu4x_inventory_open(allocator, enumerate_locales, &service->locales);
    if (error) {
        qjs_intl_segment_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_segment_close(QJSIntlSegmentService *service)
{
    if (service) {
        qjs_icu4x_inventory_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_segment_available(const QJSIntlSegmentService *service,
                                         const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!out || !count || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.items;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segmenter_open(const QJSIntlSegmentService *service, size_t locale,
                                      QJSIntlGranularity granularity, QJSIntlSegmenter **out)
{
    QJSIntlSegmenter *segmenter;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->locales.count ||
        (unsigned)granularity > QJS_INTL_SEGMENT_SENTENCE)
        return QJS_INTL_INVALID_ARGUMENT;
    segmenter = service->allocator.malloc(service->allocator.opaque, sizeof(*segmenter));
    if (!segmenter)
        return QJS_INTL_NO_MEMORY;
    segmenter->allocator = service->allocator;
    segmenter->granularity = granularity;
    segmenter->handle = NULL;
    error = qjs_icu4x_status(qjs_icu4x_segment_open(
        (const uint8_t *)service->locales.items[locale].data, service->locales.items[locale].length,
        granularity, &segmenter->handle));
    if (error) {
        qjs_intl_segmenter_close(segmenter);
        return error;
    }
    *out = segmenter;
    return QJS_INTL_OK;
}

void qjs_intl_segmenter_close(QJSIntlSegmenter *segmenter)
{
    if (segmenter) {
        qjs_icu4x_segment_close(segmenter->handle);
        segmenter->allocator.free(segmenter->allocator.opaque, segmenter);
    }
}

typedef struct BoundaryOutput {
    QJSIntlPreparedSegments *input;
    size_t count, previous, length;
} BoundaryOutput;

static int emit_boundary(void *opaque, size_t end, int32_t word_like)
{
    BoundaryOutput *output = opaque;

    if (end <= output->previous || end > output->length || (unsigned)word_like > 1)
        return QJS_ICU4X_BACKEND;
    if (output->input) {
        if (output->count >= output->input->count)
            return QJS_ICU4X_BACKEND;
        output->input->boundaries[output->count] =
            (QJSIntlSegmentBoundary){(int32_t)end, word_like};
    }
    output->previous = end;
    output->count++;
    return QJS_ICU4X_OK;
}

QJSIntlStatus qjs_intl_segment_input_prepare(const QJSIntlSegmenter *segmenter,
                                             const QJSIntlUTF16 *text, QJSIntlSegmentInput *out)
{
    QJSIntlPreparedSegments *input;
    BoundaryOutput output = {0};
    QJSICU4XBoundarySink sink = {&output, emit_boundary};
    QJSIntlStatus error;
    size_t count;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !segmenter || !text || (text->length && !text->data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (text->length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    if (!text->length) {
        *out = (QJSIntlSegmentInput){*text, segmenter->granularity, NULL};
        return QJS_INTL_OK;
    }
    output.length = text->length;
    error = qjs_icu4x_status(
        qjs_icu4x_segment_boundaries(segmenter->handle, text->data, text->length, sink));
    if (error)
        return error;
    if (!output.count || output.previous != text->length)
        return QJS_INTL_DATA_ERROR;
    count = output.count;
    if (count > (SIZE_MAX - sizeof(*input)) / sizeof(*input->boundaries))
        return QJS_INTL_OVERFLOW;
    input = segmenter->allocator.malloc(segmenter->allocator.opaque,
                                        sizeof(*input) + count * sizeof(*input->boundaries));
    if (!input)
        return QJS_INTL_NO_MEMORY;
    input->allocator = segmenter->allocator;
    input->count = count;
    output = (BoundaryOutput){input, 0, 0, text->length};
    error = qjs_icu4x_status(
        qjs_icu4x_segment_boundaries(segmenter->handle, text->data, text->length, sink));
    if (!error && (output.count != count || output.previous != text->length))
        error = QJS_INTL_DATA_ERROR;
    if (error) {
        segmenter->allocator.free(segmenter->allocator.opaque, input);
        return error;
    }
    *out = (QJSIntlSegmentInput){*text, segmenter->granularity, input};
    return QJS_INTL_OK;
}
