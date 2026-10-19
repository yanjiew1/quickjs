/* Sparse compiled compact patterns, independent of JS and formatter policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_COMPACT_DATA_H
#define QJS_INTL_NUMBER_COMPACT_DATA_H
#include "pattern-data.h"

typedef enum QJSIntlCompactKind {
    QJS_INTL_COMPACT_DECIMAL_SHORT,
    QJS_INTL_COMPACT_DECIMAL_LONG,
    QJS_INTL_COMPACT_CURRENCY_SHORT,
    QJS_INTL_COMPACT_KIND_COUNT
} QJSIntlCompactKind;

typedef enum QJSIntlCompactCount {
    QJS_INTL_COMPACT_EXACT_ZERO,
    QJS_INTL_COMPACT_EXACT_ONE,
    QJS_INTL_COMPACT_ZERO,
    QJS_INTL_COMPACT_ONE,
    QJS_INTL_COMPACT_TWO,
    QJS_INTL_COMPACT_FEW,
    QJS_INTL_COMPACT_MANY,
    QJS_INTL_COMPACT_OTHER,
    QJS_INTL_COMPACT_COUNT_COUNT
} QJSIntlCompactCount;

typedef enum QJSIntlCompactVariant {
    QJS_INTL_COMPACT_STANDARD,
    QJS_INTL_COMPACT_ALPHA,
    QJS_INTL_COMPACT_NO_CURRENCY,
    QJS_INTL_COMPACT_VARIANT_COUNT
} QJSIntlCompactVariant;

enum { QJS_INTL_COMPACT_SCALED, QJS_INTL_COMPACT_LITERAL, QJS_INTL_COMPACT_FALLBACK };

typedef struct QJSIntlCompactMap {
    const uint8_t *entries;
    uint32_t count;
} QJSIntlCompactMap;

typedef struct QJSIntlCompactPattern {
    unsigned int state, mode; /* State follows QJS_INTL_NUMBER_PATTERN_*. */
    uint32_t magnitude, exponent, zeros;
    QJSIntlCompactCount count;
    QJSIntlCompactVariant variant;
    QJSIntlNumberAffix forms[QJS_INTL_NUMBER_PATTERN_FORM_COUNT];
} QJSIntlCompactPattern;

typedef struct QJSIntlCompactData {
    QJSIntlNumberingData numbering;
    QJSIntlSection section;
    const uint8_t *systems, *locale_rows, *rows, *maps, *entries;
    const uint8_t *patterns, *affixes, *tokens;
    uint32_t system_count, row_count, map_count, entry_count;
    uint32_t pattern_count, affix_count, token_count;
} QJSIntlCompactData;

/* Require an unchanged validated numbering view. Borrow immutable storage;
   release all temporary validation memory before returning. Follow pack.h's
   disjoint output contract; non-NULL outputs are reset on failure. No formatter
   fallback, plural selection or service capability is implied by opening. */
QJSIntlStatus qjs_intl_compact_data_open(const QJSIntlNumberingData *, const QJSIntlAllocator *,
                                         QJSIntlCompactData *);

/* Exact shared locale and numbering indexes. Algorithmic systems return
   UNSUPPORTED. Currency maps may be empty; decimal maps cannot be empty. */
QJSIntlStatus qjs_intl_compact_map_at(const QJSIntlCompactData *, uint32_t locale, uint32_t system,
                                      QJSIntlCompactKind, QJSIntlCompactMap *);

/* Require a map returned by this same unchanged owner. Indexed access follows
   magnitude/count/variant order. Exact lookup returns UNSUPPORTED if absent;
   blocked and explicitly empty entries return OK with their distinct states.
   No fallback between count selectors or currency families occurs here. */
QJSIntlStatus qjs_intl_compact_pattern_at(const QJSIntlCompactData *, const QJSIntlCompactMap *,
                                          uint32_t index, QJSIntlCompactPattern *);
QJSIntlStatus qjs_intl_compact_pattern_find(const QJSIntlCompactData *, const QJSIntlCompactMap *,
                                            uint32_t magnitude, QJSIntlCompactCount,
                                            QJSIntlCompactVariant, QJSIntlCompactPattern *);
QJSIntlStatus qjs_intl_compact_token_at(const QJSIntlCompactData *, const QJSIntlNumberAffix *,
                                        uint32_t index, QJSIntlNumberPatternToken *);
#endif
