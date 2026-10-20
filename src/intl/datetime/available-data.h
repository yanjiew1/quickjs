/* Sparse available date patterns and append templates, without matching policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_AVAILABLE_DATA_H
#define QJS_INTL_DATETIME_AVAILABLE_DATA_H
#include "../native/data.h"

typedef enum QJSIntlDateAvailableFamily {
    QJS_INTL_DATE_AVAILABLE_PATTERN,
    QJS_INTL_DATE_AVAILABLE_APPEND,
    QJS_INTL_DATE_AVAILABLE_FAMILY_COUNT
} QJSIntlDateAvailableFamily;

typedef enum QJSIntlDateAvailablePlural {
    QJS_INTL_DATE_NO_PLURAL,
    QJS_INTL_DATE_PLURAL_ZERO,
    QJS_INTL_DATE_PLURAL_ONE,
    QJS_INTL_DATE_PLURAL_TWO,
    QJS_INTL_DATE_PLURAL_FEW,
    QJS_INTL_DATE_PLURAL_MANY,
    QJS_INTL_DATE_PLURAL_OTHER,
    QJS_INTL_DATE_PLURAL_COUNT
} QJSIntlDateAvailablePlural;

typedef struct QJSIntlDateAvailableGroup {
    uint32_t calendar;
    QJSIntlDateAvailableFamily family;
    QJSIntlBytes alt; /* Container alternate, separate from each leaf alternate. */
} QJSIntlDateAvailableGroup;

typedef struct QJSIntlDateAvailableNode {
    QJSIntlBytes origin_locale;
    uint32_t group, map, parent; /* Zero-based group/map; one-based parent. */
} QJSIntlDateAvailableNode;

typedef struct QJSIntlDateAvailableEntry {
    QJSIntlBytes code, alt, text;
    QJSIntlDateAvailablePlural plural;
    unsigned int blocked;
} QJSIntlDateAvailableEntry;

typedef struct QJSIntlDateAvailableData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *calendars, *groups, *locale_rows, *rows, *maps, *nodes, *entries;
    uint32_t calendar_count, group_count, row_count, map_count, node_count, entry_count;
} QJSIntlDateAvailableData;

/* Borrow an unchanged validated shared owner. Require malloc/free; release all
   scratch before return. Follow pack.h's disjoint-output contract. Non-NULL
   output is reset on failure. This validates source records, not formatter
   capability, skeleton matching or the grammar of stored pattern/template text. */
QJSIntlStatus qjs_intl_date_available_data_open(const QJSIntlData *data,
                                                const QJSIntlAllocator *allocator,
                                                QJSIntlDateAvailableData *out);
/* Queries allocate nothing and borrow pack storage. Calendar/group/map indexes
   are zero-based; source heads/parents are one-based, zero meaning missing. */
QJSIntlStatus qjs_intl_date_available_calendar_at(const QJSIntlDateAvailableData *data,
                                                  uint32_t index, QJSIntlBytes *out);
QJSIntlStatus qjs_intl_date_available_group_at(const QJSIntlDateAvailableData *data,
                                               uint32_t index, QJSIntlDateAvailableGroup *out);
QJSIntlStatus qjs_intl_date_available_find_group(const QJSIntlDateAvailableData *data,
                                                 const QJSIntlDateAvailableGroup *key,
                                                 uint32_t *out);
QJSIntlStatus qjs_intl_date_available_head(const QJSIntlDateAvailableData *data,
                                           uint32_t locale, uint32_t group, uint32_t *out);
QJSIntlStatus qjs_intl_date_available_node(const QJSIntlDateAvailableData *data,
                                           uint32_t index, QJSIntlDateAvailableNode *out);
/* Exact lookup in one layer: do not walk parents or choose plural/alternate
   fallback. NO_PLURAL is distinct from OTHER. Missing returns UNSUPPORTED and
   zero output; blocked and explicit empty text return OK. Aliases may change
   source calendar/container alternate while retaining the semantic family. */
QJSIntlStatus qjs_intl_date_available_lookup(const QJSIntlDateAvailableData *data,
                                             uint32_t index, QJSIntlBytes code,
                                             QJSIntlDateAvailablePlural plural,
                                             QJSIntlBytes alt, QJSIntlDateAvailableEntry *out);
#endif
