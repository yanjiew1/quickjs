/* Unicode 18 default segmentation with an intentionally English-only inventory.
 * This profile does not execute CLDR break rules or dictionary tailoring.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "segment.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include "../../unicode/grapheme.h"
#include "../../unicode/word.h"
#include "../../unicode/sentence.h"
#include "../../unicode/utf16.h"
#include <string.h>

struct QJSIntlSegmentService {
    QJSIntlAllocator allocator;
};
struct QJSIntlSegmenter {
    QJSIntlAllocator allocator;
    QJSIntlGranularity granularity;
};

static const QJSIntlBytes available[] = {{"en", 2}, {"en-US", 5}};

QJSIntlStatus qjs_intl_segment_open(const QJSIntlAllocator *allocator,
                                    QJSIntlSegmentService **out)
{
    QJSIntlSegmentService *service;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    service->allocator = *allocator;
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_segment_close(QJSIntlSegmentService *service)
{
    if (service)
        service->allocator.free(service->allocator.opaque, service);
}

QJSIntlStatus qjs_intl_segment_available(const QJSIntlSegmentService *service,
                                         const QJSIntlBytes **locales, size_t *count)
{
    if (locales)
        *locales = NULL;
    if (count)
        *count = 0;
    if (!service || !locales || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *locales = available;
    *count = countof(available);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segmenter_open(const QJSIntlSegmentService *service, size_t locale,
                                      QJSIntlGranularity granularity, QJSIntlSegmenter **out)
{
    QJSIntlSegmenter *segmenter;
    if (out)
        *out = NULL;
    if (!service || !out || locale >= countof(available) ||
        (unsigned int)granularity > QJS_INTL_SEGMENT_SENTENCE)
        return QJS_INTL_INVALID_ARGUMENT;
    segmenter = service->allocator.malloc(service->allocator.opaque, sizeof(*segmenter));
    if (!segmenter)
        return QJS_INTL_NO_MEMORY;
    *segmenter = (QJSIntlSegmenter){service->allocator, granularity};
    *out = segmenter;
    return QJS_INTL_OK;
}

void qjs_intl_segmenter_close(QJSIntlSegmenter *segmenter)
{
    if (segmenter)
        segmenter->allocator.free(segmenter->allocator.opaque, segmenter);
}

static int valid_input(const QJSIntlSegmenter *segmenter, const QJSIntlUTF16 *input,
                       QJSIntlSegment *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    return segmenter && input && out && (input->data || !input->length) &&
        input->length <= SIZE_MAX / sizeof(*input->data);
}

QJSIntlStatus qjs_intl_segment_next(const QJSIntlSegmenter *segmenter,
                                    const QJSIntlUTF16 *input, size_t start, QJSIntlSegment *out)
{
    size_t end, at;
    if (!valid_input(segmenter, input, out) || start > input->length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (start == input->length)
        return QJS_INTL_OK;
    if (segmenter->granularity == QJS_INTL_SEGMENT_GRAPHEME)
        end = unicode_grapheme_next(input->data, input->length, start);
    else if (segmenter->granularity == QJS_INTL_SEGMENT_WORD)
        end = unicode_word_next(input->data, input->length, start);
    else
        end = unicode_sentence_next(input->data, input->length, start);
    if (end <= start || end > input->length)
        return QJS_INTL_DATA_ERROR;
    *out = (QJSIntlSegment){start, end, 1, 0};
    if (segmenter->granularity == QJS_INTL_SEGMENT_WORD) {
        /* Minimal policy: a word-like segment contains a Letter or Nd. This
           includes Han/Hiragana/complex-context letters without treating a
           punctuation-only underscore or emoji as a word. Native dictionary
           classification can replace this policy behind the same contract. */
        at = start;
        while (at < end) {
            uint32_t c = unicode_utf16_next(input->data, end, &at);
            if (lre_is_letter(c) || lre_is_decimal_digit(c)) {
                out->is_word_like = 1;
                break;
            }
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_containing(const QJSIntlSegmenter *segmenter,
                                          const QJSIntlUTF16 *input, size_t index,
                                          QJSIntlSegment *out)
{
    QJSIntlStatus status;
    size_t start = 0;
    if (!valid_input(segmenter, input, out))
        return QJS_INTL_INVALID_ARGUMENT;
    if (index >= input->length)
        return QJS_INTL_OK;
    /* The default algorithms require known boundaries, so never start them at
       an arbitrary index inside a surrogate pair, cluster or word. */
    do {
        status = qjs_intl_segment_next(segmenter, input, start, out);
        if (status)
            return status;
        start = out->end;
    } while (start <= index);
    return QJS_INTL_OK;
}
