/* Borrowed validated scalar dictionary DAGs; no text or segmentation policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENT_DICTIONARY_H
#define QJS_INTL_SEGMENT_DICTIONARY_H
#include "../native/pack.h"

typedef enum QJSIntlSegmentDictionaryFamily {
    QJS_INTL_DICTIONARY_CJK,
    QJS_INTL_DICTIONARY_KHMER,
    QJS_INTL_DICTIONARY_LAO,
    QJS_INTL_DICTIONARY_MYANMAR,
    QJS_INTL_DICTIONARY_THAI,
    QJS_INTL_DICTIONARY_FAMILY_COUNT
} QJSIntlSegmentDictionaryFamily;

typedef struct QJSIntlSegmentDictionary {
    const uint8_t *alphabet, *values, *starts, *terminals, *labels, *targets;
    uint32_t alphabet_count, value_count, state_count, edge_count;
    uint32_t root, words, max_scalars, max_utf16;
    unsigned int valued;
} QJSIntlSegmentDictionary;

typedef struct QJSIntlSegmentDictionaryData {
    QJSIntlSection section;
    QJSIntlSegmentDictionary dictionaries[QJS_INTL_DICTIONARY_FAMILY_COUNT];
    unsigned int present;
} QJSIntlSegmentDictionaryData;

/* Open an optional section from an unchanged pack. Validate every descriptor,
   graph, used pool, accepted-path count and length maximum before publication.
   Temporary bounded scratch uses malloc/free and is released on every path.
   The result owns no allocation and borrows the immutable pack bytes, which
   must outlive it and all returned dictionary pointers. No close is required.
   Source authentication and executable locale coverage are separate contracts.
   Require fresh outputs disjoint from inputs; reset failures. */
QJSIntlStatus qjs_intl_segment_dictionary_data_open(const QJSIntlPack *, const QJSIntlAllocator *,
                                                    QJSIntlSegmentDictionaryData *out);
/* Borrow the selected view until its data owner is changed or released.
   A caller retaining a handle beyond that owner may copy the fixed view by
   value, while keeping the underlying immutable pack bytes alive. */
QJSIntlStatus qjs_intl_segment_dictionary_at(const QJSIntlSegmentDictionaryData *,
                                             QJSIntlSegmentDictionaryFamily,
                                             const QJSIntlSegmentDictionary **out);
/* Query unchanged views from successful open, without allocation or mutation.
   UNSUPPORTED means no edge/terminal, distinct from a terminal with cost zero.
   Outputs are disjoint from views and their immutable bytes. Reset non-NULL
   output on failure. Scalar misses include lone surrogates;
   callers preserve their original UTF-16 input and choose boundary policy.
   Membership dictionaries return value zero, with valued remaining false. */
QJSIntlStatus qjs_intl_segment_dictionary_step(const QJSIntlSegmentDictionary *, uint32_t state,
                                               uint32_t scalar, uint32_t *out);
QJSIntlStatus qjs_intl_segment_dictionary_value(const QJSIntlSegmentDictionary *, uint32_t state,
                                                uint32_t *out);
#endif
