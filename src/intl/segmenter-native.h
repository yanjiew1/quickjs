/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Private provider building block, not an embedding ABI. */
#ifndef QJS_INTL_SEGMENTER_NATIVE_H
#define QJS_INTL_SEGMENTER_NATIVE_H
#include "provider.h"

typedef enum QJSIntlSegmentGranularity {
    QJS_INTL_SEGMENT_GRAPHEME = 0,
    QJS_INTL_SEGMENT_WORD = 1,
    QJS_INTL_SEGMENT_SENTENCE = 2
} QJSIntlSegmentGranularity;
typedef enum QJSIntlGraphemeProperty {
    QJS_GCB_OTHER = 0, QJS_GCB_CR, QJS_GCB_LF, QJS_GCB_CONTROL,
    QJS_GCB_EXTEND, QJS_GCB_ZWJ, QJS_GCB_RI, QJS_GCB_PREPEND,
    QJS_GCB_SPACING_MARK, QJS_GCB_L, QJS_GCB_V, QJS_GCB_T,
    QJS_GCB_LV, QJS_GCB_LVT, QJS_GCB_COUNT
} QJSIntlGraphemeProperty;
typedef enum QJSIntlWordProperty {
    QJS_WB_OTHER = 0, QJS_WB_CR, QJS_WB_LF, QJS_WB_NEWLINE,
    QJS_WB_EXTEND, QJS_WB_FORMAT, QJS_WB_ZWJ, QJS_WB_WSEGSPACE,
    QJS_WB_ALETTER, QJS_WB_HEBREW_LETTER, QJS_WB_NUMERIC,
    QJS_WB_KATAKANA, QJS_WB_EXTENDNUMLET, QJS_WB_MIDLETTER,
    QJS_WB_MIDNUM, QJS_WB_MIDNUMLET, QJS_WB_SINGLE_QUOTE,
    QJS_WB_DOUBLE_QUOTE, QJS_WB_RI, QJS_WB_COUNT
} QJSIntlWordProperty;
typedef enum QJSIntlSentenceProperty {
    QJS_SB_OTHER = 0, QJS_SB_CR, QJS_SB_LF, QJS_SB_SEP,
    QJS_SB_EXTEND, QJS_SB_FORMAT, QJS_SB_SP, QJS_SB_LOWER,
    QJS_SB_UPPER, QJS_SB_OLETTER, QJS_SB_NUMERIC, QJS_SB_ATERM,
    QJS_SB_STERM, QJS_SB_CLOSE, QJS_SB_SCONTINUE, QJS_SB_COUNT
} QJSIntlSentenceProperty;
typedef enum QJSIntlIndicConjunctProperty {
    QJS_INCB_NONE = 0, QJS_INCB_EXTEND, QJS_INCB_CONSONANT,
    QJS_INCB_LINKER, QJS_INCB_COUNT
} QJSIntlIndicConjunctProperty;
typedef struct QJSIntlSegmentProperties {
    uint8_t grapheme, word, sentence, indic_conjunct, extended_pictographic;
} QJSIntlSegmentProperties;
typedef struct QJSIntlSegmentPropertySource {
    void *opaque;
    QJSIntlStatus (*lookup)(void *, uint32_t scalar,
                            QJSIntlSegmentProperties *);
    uint32_t unicode_version; /* exact18<<16; mismatch returns UNSUPPORTED */
} QJSIntlSegmentPropertySource;
/* lookup is called for Unicode scalar values only; the engine assigns Other/
 * None/false to unpaired surrogates and preserves their original code units.
 * The source is borrowed until segmenter close, and must use Unicode18.0.0.
 * It may read packed binary tables or existing libunicode property tables. */
typedef struct QJSIntlNativeSegmenter QJSIntlNativeSegmenter;
typedef struct QJSIntlNativeSegments QJSIntlNativeSegments;
typedef struct QJSIntlSegmentResult {
    QJSIntlUTF16 segment;     /* borrowed from immutable snapshot */
    size_t start, end;        /* original UTF16 offsets, [start,end) */
    uint8_t has_word_like;    /* true only for word granularity */
    uint8_t is_word_like;     /* default policy: AHLetter/Numeric/Katakana */
} QJSIntlSegmentResult;
/* Explicit default UAX29 API. No locale-sensitive service coverage is claimed.
 * The frontend performs coercion, locale resolution, ToIntegerOrInfinity,
 * iterator object creation, and retention of the original JS input string. */
QJSIntlStatus qjs_intl_native_segmenter_open_default(
    const QJSIntlAllocator *, const QJSIntlSegmentPropertySource *,
    QJSIntlSegmentGranularity, QJSIntlNativeSegmenter **out);
void qjs_intl_native_segmenter_close(QJSIntlNativeSegmenter *);
QJSIntlStatus qjs_intl_native_segments_new(QJSIntlNativeSegmenter *,
    QJSIntlUTF16, QJSIntlNativeSegments **out);
void qjs_intl_native_segments_free(QJSIntlNativeSegments *);
QJSIntlUTF16 qjs_intl_native_segments_input(const QJSIntlNativeSegments *);
/* Snapshot owns copied input and boundaries and survives segmenter close.
 * cursor initially0 is a segment ordinal, not a code-unit index. It is
 * independent for every iterator. Success with done sets a zero result.
 * containing returns found=false for index>=length, including SIZE_MAX.
 * An index inside either code unit of a pair returns its complete segment.
 * By algorithm structure, next is O(1), containing O(log segment_count);
 * construction is O(n log R) with packed properties, O(n) rule evaluation,
 * and O(n) memory. These are complexity bounds, not measured performance. */
QJSIntlStatus qjs_intl_native_segments_next(const QJSIntlNativeSegments *,
    size_t *cursor, QJSIntlSegmentResult *out, int *done);
QJSIntlStatus qjs_intl_native_segments_containing(const QJSIntlNativeSegments *,
    size_t index, QJSIntlSegmentResult *out, int *found);
#endif
