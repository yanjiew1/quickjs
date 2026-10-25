/* Unicode 18 default segmentation with an intentionally English-only inventory.
 * This profile does not execute CLDR break rules or dictionary tailoring.
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
#include "segment.h"
#include "minimal-inline.h"
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

static const QJSIntlBytes available[] = {{"en", 2}, {"en-US", 5}};

QJSIntlStatus qjs_intl_segment_open(const QJSIntlAllocator *allocator,
                                    const struct QJSIntlData *checked_data,
                                    QJSIntlSegmentService **out)
{
    QJSIntlSegmentService *service;

    (void)checked_data; /* Unicode-only provider; no pack to admit or borrow. */
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

QJSIntlStatus (qjs_intl_segment_input_prepare)(const QJSIntlSegmenter *segmenter, const QJSIntlUTF16 *text,
                                             QJSIntlSegmentInput *out)
{
    return qjs_intl_segment_input_prepare_inline(segmenter, text, out);
}

void (qjs_intl_segment_input_clear)(QJSIntlSegmentInput *input)
{
    qjs_intl_segment_input_clear_inline(input);
}

static int valid_input(const QJSIntlSegmentInput *input, QJSIntlSegment *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    return input && out && (unsigned int)input->granularity <= QJS_INTL_SEGMENT_SENTENCE &&
        (input->text.data || !input->text.length) &&
        input->text.length <= SIZE_MAX / sizeof(*input->text.data);
}

QJSIntlStatus qjs_intl_segment_next(const QJSIntlSegmentInput *input, size_t start, QJSIntlSegment *out)
{
    size_t end, at;
    if (!valid_input(input, out) || start > input->text.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (start == input->text.length)
        return QJS_INTL_OK;
    if (input->granularity == QJS_INTL_SEGMENT_GRAPHEME)
        end = unicode_grapheme_next(input->text.data, input->text.length, start);
    else if (input->granularity == QJS_INTL_SEGMENT_WORD)
        end = unicode_word_next(input->text.data, input->text.length, start);
    else
        end = unicode_sentence_next(input->text.data, input->text.length, start);
    if (end <= start || end > input->text.length)
        return QJS_INTL_DATA_ERROR;
    *out = (QJSIntlSegment){start, end, 1, 0};
    if (input->granularity == QJS_INTL_SEGMENT_WORD) {
        /* Minimal policy: a word-like segment contains a Letter or Nd. This
           includes Han/Hiragana/complex-context letters without treating a
           punctuation-only underscore or emoji as a word. Native dictionary
           classification can replace this policy behind the same contract. */
        at = start;
        while (at < end) {
            uint32_t c = unicode_utf16_next(input->text.data, end, &at);
            if (lre_is_letter(c) || lre_is_decimal_digit(c)) {
                out->is_word_like = 1;
                break;
            }
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_containing(const QJSIntlSegmentInput *input, size_t index,
                                          QJSIntlSegment *out)
{
    QJSIntlStatus status;
    size_t start = 0;
    if (!valid_input(input, out))
        return QJS_INTL_INVALID_ARGUMENT;
    if (index >= input->text.length)
        return QJS_INTL_OK;
    /* The default algorithms require known boundaries, so never start them at
       an arbitrary index inside a surrogate pair, cluster or word. */
    do {
        status = qjs_intl_segment_next(input, start, out);
        if (status)
            return status;
        start = out->end;
    } while (start <= index);
    return QJS_INTL_OK;
}
