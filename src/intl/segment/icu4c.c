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
#include "../icu4c/common.h"
#include <limits.h>
#include <string.h>
#include <unicode/ubrk.h>

struct QJSIntlSegmentService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
};

struct QJSIntlSegmenter {
    QJSIntlAllocator allocator;
    QJSIntlGranularity granularity;
    char *locale;
};

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
    error = qjs_icu4c_locales_open(allocator, ubrk_countAvailable(), ubrk_getAvailable,
                                   &service->locales);
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
        qjs_icu4c_locales_clear(&service->locales);
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
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.tags;
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
        (unsigned int)granularity > QJS_INTL_SEGMENT_SENTENCE)
        return QJS_INTL_INVALID_ARGUMENT;
    segmenter = service->allocator.malloc(service->allocator.opaque, sizeof(*segmenter));
    if (!segmenter)
        return QJS_INTL_NO_MEMORY;
    segmenter->allocator = service->allocator;
    segmenter->granularity = granularity;
    error =
        qjs_icu4c_locale_id(&service->allocator, service->locales.tags[locale], &segmenter->locale);
    if (error) {
        service->allocator.free(service->allocator.opaque, segmenter);
        return error;
    }
    *out = segmenter;
    return QJS_INTL_OK;
}

void qjs_intl_segmenter_close(QJSIntlSegmenter *segmenter)
{
    if (segmenter) {
        segmenter->allocator.free(segmenter->allocator.opaque, segmenter->locale);
        segmenter->allocator.free(segmenter->allocator.opaque, segmenter);
    }
}

QJSIntlStatus qjs_intl_segment_input_prepare(const QJSIntlSegmenter *segmenter,
                                             const QJSIntlUTF16 *text, QJSIntlSegmentInput *out)
{
    static const UBreakIteratorType types[] = {UBRK_CHARACTER, UBRK_WORD, UBRK_SENTENCE};
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *iterator;
    QJSIntlPreparedSegments *input = NULL;
    QJSIntlStatus error = QJS_INTL_OK;
    size_t count = 0, index = 0;
    int32_t end, previous = 0;

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
    iterator = ubrk_open(types[segmenter->granularity], segmenter->locale, text->data,
                         (int32_t)text->length, &status);
    if (U_FAILURE(status)) {
        ubrk_close(iterator);
        return qjs_icu4c_status(status);
    }
    /* Materialize all boundaries and word statuses while ICU owns a private
       cursor. Later next/containing calls allocate nothing and share no mutable
       BreakIterator, including queries interleaved by distinct JS iterators. */
    ubrk_first(iterator);
    while ((end = ubrk_next(iterator)) != UBRK_DONE) {
        if (end <= previous || (size_t)end > text->length) {
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
        previous = end;
        count++;
    }
    if ((size_t)previous != text->length || !count) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    if (count > (SIZE_MAX - sizeof(*input)) / sizeof(*input->boundaries)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    input = segmenter->allocator.malloc(segmenter->allocator.opaque,
                                        sizeof(*input) + count * sizeof(*input->boundaries));
    if (!input) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    input->allocator = segmenter->allocator;
    input->count = count;
    ubrk_first(iterator);
    previous = 0;
    while ((end = ubrk_next(iterator)) != UBRK_DONE) {
        if (index >= count || end <= previous || (size_t)end > text->length) {
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
        input->boundaries[index++] =
            (QJSIntlSegmentBoundary){end, segmenter->granularity == QJS_INTL_SEGMENT_WORD &&
                                              ubrk_getRuleStatus(iterator) >= UBRK_WORD_NUMBER};
        previous = end;
    }
    if (index != count || (size_t)previous != text->length) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    *out = (QJSIntlSegmentInput){*text, segmenter->granularity, input};
    input = NULL;
done:
    if (input)
        segmenter->allocator.free(segmenter->allocator.opaque, input);
    ubrk_close(iterator);
    return error;
}
