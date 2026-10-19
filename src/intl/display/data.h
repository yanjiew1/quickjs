/* Layered native names; exact data queries without display/fallback policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DISPLAY_DATA_H
#define QJS_INTL_DISPLAY_DATA_H
#include "../native/data.h"

typedef enum QJSIntlNameGroup {
    QJS_INTL_NAME_LANGUAGE,
    QJS_INTL_NAME_SCRIPT,
    QJS_INTL_NAME_REGION,
    QJS_INTL_NAME_VARIANT,
    QJS_INTL_NAME_CALENDAR,
    QJS_INTL_NAME_PATTERN,
    QJS_INTL_NAME_BRACKET,
    QJS_INTL_NAME_GROUP_COUNT
} QJSIntlNameGroup;

typedef struct QJSIntlNameNode {
    QJSIntlBytes origin_locale;
    QJSIntlNameGroup group;
    uint32_t map, parent; /* Zero-based map; one-based parent, zero absent. */
} QJSIntlNameNode;

typedef struct QJSIntlNameEntry {
    QJSIntlBytes normalized, alt, menu, scope, raw_code, text;
    unsigned int blocked; /* Empty text with blocked == 0 is a present value. */
} QJSIntlNameEntry;

typedef struct QJSIntlNameData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *locale_rows, *rows, *maps, *nodes, *entries, *keys;
    uint32_t row_count, map_count, node_count, entry_count, key_count;
} QJSIntlNameData;

/* Borrow an unchanged shared owner. Require malloc/free; all validation scratch
   is released before return. Follow pack.h's disjoint-output contract and reset
   non-NULL output on failure. No locale or code availability is inferred. */
QJSIntlStatus qjs_intl_name_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                      QJSIntlNameData *);
/* All queries borrow storage and allocate nothing. Locale/group heads and node
   parents are one-based; zero denotes no source layer. Map/key indexes are
   zero-based. A node retains its source locale, including external aliases. */
QJSIntlStatus qjs_intl_name_head(const QJSIntlNameData *, uint32_t locale,
                                 QJSIntlNameGroup, uint32_t *head);
QJSIntlStatus qjs_intl_name_node(const QJSIntlNameData *, uint32_t node,
                                 QJSIntlNameNode *);
/* Exact lookup in one layer, without walking parents or normalizing input.
   Pass the normalized code and exact alt/menu/scope selectors. An absent entry
   returns UNSUPPORTED with a zero output; BLOCK and empty return OK. */
QJSIntlStatus qjs_intl_name_lookup(const QJSIntlNameData *, uint32_t node,
                                   QJSIntlBytes code, QJSIntlBytes alt,
                                   QJSIntlBytes menu, QJSIntlBytes scope,
                                   QJSIntlNameEntry *);
/* Sorted global group/code inventory, not service capability or locale presence. */
QJSIntlStatus qjs_intl_name_key_at(const QJSIntlNameData *, uint32_t index,
                                   QJSIntlNameGroup *, QJSIntlBytes *);
#endif
