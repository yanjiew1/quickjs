/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "native-input.h"
#include <string.h>

struct QJSIntlSegmentNativeInput {
    QJSIntlAllocator allocator;
    size_t *boundaries;
    uint8_t *word_like;
    size_t count, length; /* count includes BOS and EOF (one entry if empty). */
};

QJSIntlStatus qjs_intl_segment_native_input_prepare(const QJSIntlAllocator *allocator,
                                                     const QJSIntlSegmentProgram *selected,
                                                     const QJSIntlSegmentProgram *grapheme,
                                                     const QJSIntlSegmentLiteralGraph *suppressions,
                                                     const QJSIntlUTF16 *source,
                                                     QJSIntlSegmentNativeInput **out)
{
    QJSIntlSegmentText text = {0};
    QJSIntlSegmentScan scan = {0}, clusters = {0};
    QJSIntlSegmentNativeInput *result;
    QJSIntlStatus status;
    size_t i, count = 1, bytes, at = 0;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || !source || !selected ||
        selected->kind > QJS_INTL_SEGMENT_SENTENCE)
        return QJS_INTL_INVALID_ARGUMENT;
    if (selected->kind == QJS_INTL_SEGMENT_WORD)
        return QJS_INTL_UNSUPPORTED;
    if (selected->kind == QJS_INTL_SEGMENT_SENTENCE &&
        (!grapheme || grapheme->kind != QJS_INTL_SEGMENT_GRAPHEME || !suppressions))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_segment_text_prepare(allocator, source, &text);
    if (!status)
        status = qjs_intl_segment_scan_prepare(allocator, selected, &text, &scan);
    if (status)
        goto done;
    if (selected->kind == QJS_INTL_SEGMENT_SENTENCE) {
        status = qjs_intl_segment_suppression_apply(allocator, selected, suppressions, &text, &scan);
        if (!status)
            status = qjs_intl_segment_scan_prepare(allocator, grapheme, &text, &clusters);
        if (status)
            goto done;
        /* Refinement cannot introduce a boundary inside a native cluster. */
        for (i = 1; i < text.count; i++)
            scan.breaks[i] &= clusters.breaks[i];
    }
    for (i = 1; i <= text.count; i++)
        if (scan.breaks[i])
            count++;
    if (count > (SIZE_MAX - sizeof(*result)) / sizeof(*result->boundaries)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    bytes = sizeof(*result) + count * sizeof(*result->boundaries);
    if (count - 1 > SIZE_MAX - bytes) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    bytes += count - 1;
    result = allocator->malloc(allocator->opaque, bytes);
    if (!result) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    result->allocator = *allocator;
    result->count = count;
    result->length = text.length;
    result->boundaries = (size_t *)(result + 1);
    result->word_like = (uint8_t *)(result->boundaries + count);
    result->boundaries[at++] = 0;
    for (i = 1; i <= text.count; i++)
        if (scan.breaks[i])
            result->boundaries[at++] = text.offsets[i];
    if (count > 1)
        memset(result->word_like, 0, count - 1);
    *out = result;
    status = QJS_INTL_OK;
done:
    qjs_intl_segment_scan_clear(&clusters);
    qjs_intl_segment_scan_clear(&scan);
    qjs_intl_segment_text_clear(&text);
    return status;
}

void qjs_intl_segment_native_input_close(QJSIntlSegmentNativeInput *input)
{
    if (input)
        input->allocator.free(input->allocator.opaque, input);
}

static void segment_at(const QJSIntlSegmentNativeInput *input, size_t index, QJSIntlSegment *out)
{
    *out = (QJSIntlSegment){input->boundaries[index], input->boundaries[index + 1],
                           1, input->word_like[index]};
}

QJSIntlStatus qjs_intl_segment_native_input_next(const QJSIntlSegmentNativeInput *input, size_t start,
                                                 QJSIntlSegment *out)
{
    size_t low = 0, high;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!input || !out || start > input->length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (start == input->length)
        return QJS_INTL_OK;
    high = input->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (input->boundaries[middle] < start)
            low = middle + 1;
        else
            high = middle;
    }
    if (input->boundaries[low] != start)
        return QJS_INTL_INVALID_ARGUMENT;
    segment_at(input, low, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_native_input_containing(const QJSIntlSegmentNativeInput *input, size_t index,
                                                       QJSIntlSegment *out)
{
    size_t low = 0, high;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!input || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    if (index >= input->length)
        return QJS_INTL_OK;
    high = input->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (input->boundaries[middle] <= index)
            low = middle + 1;
        else
            high = middle;
    }
    segment_at(input, low - 1, out);
    return QJS_INTL_OK;
}
