/* Number/currency layout source metadata, independent of formatter policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_LAYOUT_DATA_H
#define QJS_INTL_NUMBER_LAYOUT_DATA_H
#include "../locale/numbering-data.h"

typedef enum QJSIntlNumberLayoutField {
    QJS_INTL_LAYOUT_RANGE,
    QJS_INTL_LAYOUT_APPROXIMATELY,
    QJS_INTL_LAYOUT_AT_LEAST,
    QJS_INTL_LAYOUT_AT_MOST,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_EXACT_ZERO,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_EXACT_ONE,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_ZERO,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_ONE,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_TWO,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_FEW,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_MANY,
    QJS_INTL_LAYOUT_CURRENCY_UNIT_OTHER,
    QJS_INTL_LAYOUT_CURRENCY_BEFORE_MATCH,
    QJS_INTL_LAYOUT_CURRENCY_BEFORE_SURROUNDING,
    QJS_INTL_LAYOUT_CURRENCY_BEFORE_INSERT,
    QJS_INTL_LAYOUT_CURRENCY_AFTER_MATCH,
    QJS_INTL_LAYOUT_CURRENCY_AFTER_SURROUNDING,
    QJS_INTL_LAYOUT_CURRENCY_AFTER_INSERT,
    QJS_INTL_LAYOUT_CURRENCY_APPEND_ISO,
    QJS_INTL_LAYOUT_FIELD_COUNT
} QJSIntlNumberLayoutField;

enum { QJS_INTL_LAYOUT_VALUE, QJS_INTL_LAYOUT_ABSENT, QJS_INTL_LAYOUT_BLOCKED };

typedef struct QJSIntlNumberLayoutText {
    QJSIntlBytes text;
    unsigned int state;
    unsigned int from_root; /* Currency unit template's terminal source was root. */
} QJSIntlNumberLayoutText;

typedef struct QJSIntlNumberLayouts {
    QJSIntlNumberLayoutText fields[QJS_INTL_LAYOUT_FIELD_COUNT];
} QJSIntlNumberLayouts;

typedef struct QJSIntlNumberLayoutData {
    QJSIntlNumberingData numbering;
    QJSIntlSection section;
    const uint8_t *systems, *locale_rows, *rows, *vectors;
    uint32_t system_count, row_count, vector_count;
} QJSIntlNumberLayoutData;

/* Require an unchanged validated numbering view. Borrow immutable storage;
   temporary schema validation memory is released before returning. Follow
   pack.h's disjoint output contract; non-NULL outputs are reset on failure.
   No template interpretation, UnicodeSet predicate or formatter fallback is
   implied. Individual consumers validate and implement their own syntax. */
QJSIntlStatus qjs_intl_number_layout_data_open(const QJSIntlNumberingData *,
                                               const QJSIntlAllocator *, QJSIntlNumberLayoutData *);

/* Exact shared locale/numbering indexes, without allocation or fallback.
   Algorithmic numbering systems return UNSUPPORTED. VALUE text can be empty
   for insertions and optional layouts; absent/blocked slices are zeroed.
   Currency unit templates retain from_root even for blocked/empty text, so the
   consumer can exclude root for a selected count but allow its OTHER fallback.
   Returned strings borrow the same unchanged owner. */
QJSIntlStatus qjs_intl_number_layouts_at(const QJSIntlNumberLayoutData *, uint32_t locale,
                                         uint32_t system, QJSIntlNumberLayouts *);
#endif
