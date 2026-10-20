/* Sparse interval source layers, without matching or range-formatting policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_INTERVAL_DATA_H
#define QJS_INTL_DATETIME_INTERVAL_DATA_H
#include "../native/data.h"

typedef enum QJSIntlDateIntervalFamily {
    QJS_INTL_DATE_INTERVAL_FALLBACK,
    QJS_INTL_DATE_INTERVAL_ITEM,
    QJS_INTL_DATE_INTERVAL_RANGE,
    QJS_INTL_DATE_INTERVAL_FAMILY_COUNT
} QJSIntlDateIntervalFamily;

typedef struct QJSIntlDateIntervalGroup {
    uint32_t calendar;
    QJSIntlBytes alt;
    QJSIntlDateIntervalFamily family;
    QJSIntlBytes skeleton, item_alt;
} QJSIntlDateIntervalGroup;

typedef struct QJSIntlDateIntervalNode {
    QJSIntlBytes origin_locale;
    uint32_t group, map, parent; /* Zero-based group/map; one-based parent. */
} QJSIntlDateIntervalNode;

typedef struct QJSIntlDateIntervalEntry {
    QJSIntlBytes code, alt, text;
    unsigned int blocked;
} QJSIntlDateIntervalEntry;

typedef struct QJSIntlDateIntervalData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *calendars, *suffixes, *locale_rows, *rows, *maps, *nodes, *entries;
    uint32_t calendar_count, suffix_count, group_count, row_count, map_count, node_count, entry_count;
} QJSIntlDateIntervalData;

/* Borrow an unchanged validated shared owner. Require malloc/free; release all
   scratch before return. Follow pack.h's disjoint-output contract. Non-NULL
   output is reset on failure. Validate stored tables, not the implicit
   locale/calendar/suffix cube or the grammar/selection of pattern text. */
QJSIntlStatus qjs_intl_date_interval_data_open(const QJSIntlData *data,
                                               const QJSIntlAllocator *allocator,
                                               QJSIntlDateIntervalData *out);
/* Queries allocate nothing and borrow pack storage. Calendar/group/map indexes
   are zero-based; source heads/parents are one-based, zero meaning missing.
   A global group index is calendar * suffix_count + suffix. */
QJSIntlStatus qjs_intl_date_interval_calendar_at(const QJSIntlDateIntervalData *data,
                                                 uint32_t index, QJSIntlBytes *out);
QJSIntlStatus qjs_intl_date_interval_group_at(const QJSIntlDateIntervalData *data,
                                              uint32_t index, QJSIntlDateIntervalGroup *out);
QJSIntlStatus qjs_intl_date_interval_find_group(const QJSIntlDateIntervalData *data,
                                                const QJSIntlDateIntervalGroup *key, uint32_t *out);
QJSIntlStatus qjs_intl_date_interval_head(const QJSIntlDateIntervalData *data,
                                          uint32_t locale, uint32_t group, uint32_t *out);
QJSIntlStatus qjs_intl_date_interval_node(const QJSIntlDateIntervalData *data,
                                          uint32_t index, QJSIntlDateIntervalNode *out);
/* Exact lookup in one source layer, without parent or alternate fallback.
   Missing returns UNSUPPORTED and zero output; blocked/empty return OK. Source
   aliases may change all group selectors while retaining the semantic family.
   RANGE templates are comparison examples, not production range-format rules. */
QJSIntlStatus qjs_intl_date_interval_lookup(const QJSIntlDateIntervalData *data,
                                            uint32_t index, QJSIntlBytes code, QJSIntlBytes alt,
                                            QJSIntlDateIntervalEntry *out);
#endif
