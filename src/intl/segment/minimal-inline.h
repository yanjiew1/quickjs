/* Minimal-only input metadata fast path, shared with the JS binding.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENT_MINIMAL_INLINE_H
#define QJS_INTL_SEGMENT_MINIMAL_INLINE_H

#include "segment.h"
#include <string.h>

/* This handle layout belongs to the minimal provider. Native and future
   providers retain their own opaque implementations behind segment.h. */
struct QJSIntlSegmenter {
    QJSIntlAllocator allocator;
    QJSIntlGranularity granularity;
};

static inline QJSIntlStatus qjs_intl_segment_input_prepare_inline(const QJSIntlSegmenter *segmenter,
                                                               const QJSIntlUTF16 *text,
                                                               QJSIntlSegmentInput *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!segmenter || !text || !out || (!text->data && text->length) ||
        text->length > SIZE_MAX / sizeof(*text->data))
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlSegmentInput){*text, segmenter->granularity, NULL};
    return QJS_INTL_OK;
}

static inline void qjs_intl_segment_input_clear_inline(QJSIntlSegmentInput *input)
{
    if (input)
        memset(input, 0, sizeof(*input));
}

/* Retain the callable private symbol for other clients/function addresses;
   only actual minimal-provider consumers include this scoped call alias. */
#define qjs_intl_segment_input_prepare(...) qjs_intl_segment_input_prepare_inline(__VA_ARGS__)
#define qjs_intl_segment_input_clear(...) qjs_intl_segment_input_clear_inline(__VA_ARGS__)

#endif
