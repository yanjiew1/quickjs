/* Canonical era codes to raw CLDR name keys; no calendar arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_ERA_DATA_H
#define QJS_INTL_DATETIME_ERA_DATA_H
#include "../native/data.h"

typedef struct QJSIntlCalendarEraData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *rows;
} QJSIntlCalendarEraData;

typedef struct QJSIntlCalendarEraEntry {
    QJSIntlBytes calendar, code, key;
} QJSIntlCalendarEraEntry;

/* Borrow a validated shared owner. Validate the optional section's complete
   schema, identities, ordering and both mapping domains. Require malloc/free
   for temporary row-key and string-domain validation, released before return. Follow
   pack.h's immutable/disjoint-output contract. Missing section is UNSUPPORTED.
   No arithmetic, localized-label or inherited-era capability is inferred. */
QJSIntlStatus qjs_intl_calendar_era_data_open(const QJSIntlData *data,
                                            const QJSIntlAllocator *allocator,
                                            QJSIntlCalendarEraData *out);
/* Allocate nothing; exact case-sensitive raw calendar/canonical code lookup.
   Inherited eras, aliases and arithmetic identities are the caller's policy.
   The unchanged pack outlives borrowed text. Reset outputs on failure. */
QJSIntlStatus qjs_intl_calendar_era_at(const QJSIntlCalendarEraData *data, uint32_t index,
                                     QJSIntlCalendarEraEntry *out);
QJSIntlStatus qjs_intl_calendar_era_find(const QJSIntlCalendarEraData *data,
                                       QJSIntlBytes calendar, QJSIntlBytes code,
                                       QJSIntlBytes *key);
#endif
