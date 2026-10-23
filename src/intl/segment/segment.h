/* Segmentation service, independent of JS objects and backend data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENT_H
#define QJS_INTL_SEGMENT_H
#include "../common/format.h"

typedef enum QJSIntlGranularity {
    QJS_INTL_SEGMENT_GRAPHEME,
    QJS_INTL_SEGMENT_WORD,
    QJS_INTL_SEGMENT_SENTENCE
} QJSIntlGranularity;

typedef struct QJSIntlSegmentService QJSIntlSegmentService;
typedef struct QJSIntlSegmenter QJSIntlSegmenter;
typedef struct QJSIntlSegmentInput {
    QJSIntlUTF16 text; /* Borrowed until input_clear, never freed by the service. */
    QJSIntlGranularity granularity;
    void *state; /* Backend-owned immutable preparation; NULL in minimal. */
} QJSIntlSegmentInput;
typedef struct QJSIntlSegment {
    size_t start, end; /* Original UTF-16 code units, end exclusive. */
    int present, is_word_like;
} QJSIntlSegment;

/* Open copies the allocator; its opaque owner outlives every handle. Failures
   reset *out; closing NULL is safe. No JS callbacks or input ownership transfer. */
QJSIntlStatus qjs_intl_segment_open(const QJSIntlAllocator *, QJSIntlSegmentService **out);
void qjs_intl_segment_close(QJSIntlSegmentService *);
/* Borrow a canonical sorted locale inventory until service close. This is
   executable coverage of this service, not the generic locale metadata list. */
QJSIntlStatus qjs_intl_segment_available(const QJSIntlSegmentService *,
                                         const QJSIntlBytes **locales, size_t *count);
/* locale indexes the inventory. A segmenter may outlive its source service.
   Traversal state belongs to each caller, not the shared segmenter handle. */
QJSIntlStatus qjs_intl_segmenter_open(const QJSIntlSegmentService *, size_t locale,
                                      QJSIntlGranularity, QJSIntlSegmenter **out);
void qjs_intl_segmenter_close(QJSIntlSegmenter *);
/* Explicitly prepare one input; a native backend may allocate/refine here.
   Minimal copies only metadata, without allocation. The input may outlive its
   segmenter, but the unchanged borrowed UTF-16 must outlive the input. Never
   shallow-copy ownership-bearing inputs; clear NULL/already-zero safely.
   Fresh output is disjoint from inputs and reset on failure. */
QJSIntlStatus qjs_intl_segment_input_prepare(const QJSIntlSegmenter *, const QJSIntlUTF16 *,
                                             QJSIntlSegmentInput *out);
void qjs_intl_segment_input_clear(QJSIntlSegmentInput *);
/* Query unchanged prepared input, preserving NUL/surrogates. Outputs are
   disjoint and reset on failure. next requires start=0 or a returned boundary;
   start=length is exhaustion. containing accepts any original code-unit index.
   present=0 means exhaustion/out of range. is_word_like is meaningful only
   for word granularity. No allocation, normalization or input mutation; cursor
   state belongs to each caller. Preparation has already finished. */
QJSIntlStatus qjs_intl_segment_next(const QJSIntlSegmentInput *, size_t start, QJSIntlSegment *out);
QJSIntlStatus qjs_intl_segment_containing(const QJSIntlSegmentInput *, size_t index, QJSIntlSegment *out);
#endif
