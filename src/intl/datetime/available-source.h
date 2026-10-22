/* Visible exact source keys, independent of pattern or template admission.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_AVAILABLE_SOURCE_H
#define QJS_INTL_DATETIME_AVAILABLE_SOURCE_H
#include "available-data.h"

typedef struct QJSIntlDateAvailableSource QJSIntlDateAvailableSource;
typedef struct QJSIntlDateAvailableSources {
    QJSIntlDateAvailableData data;
    QJSIntlAllocator allocator;
    QJSIntlDateAvailableSource *records;
    size_t count;
} QJSIntlDateAvailableSources;

/* Resolve one validated same-family source chain. The nearest exact code,
   plural and leaf-alt key wins, including blocked, empty or unusable text.
   Choose no plural/alternate fallback and interpret no pattern/template.
   Output owns one record allocation, borrowing unchanged pack storage, which
   must outlive the inventory. Copy the data view, not its caller's address.
   Require malloc/free. Head zero succeeds without allocations. Output must
   be disjoint from inputs and is cleared on failure. */
QJSIntlStatus qjs_intl_date_available_sources_open(const QJSIntlDateAvailableData *data,
                                                uint32_t head, const QJSIntlAllocator *allocator,
                                                QJSIntlDateAvailableSources *out);
/* Clear NULL or zero inventories safely; allocator is retained by open. */
void qjs_intl_date_available_sources_clear(QJSIntlDateAvailableSources *sources);
/* Enumerate by exact code/plural/alt key order, independent of storage address.
   Borrow all text, allocate nothing and reset non-NULL outputs on failure.
   Origin group/calendar/alternate remain queryable through the copied owner. */
QJSIntlStatus qjs_intl_date_available_source_at(const QJSIntlDateAvailableSources *sources,
                                             size_t index, QJSIntlDateAvailableEntry *entry,
                                             QJSIntlDateAvailableNode *origin);
/* Find an exact visible code/plural/leaf-alt key. Presence includes blocked
   and empty values; callers decide usability only after selecting a key.
   Borrow its unchanged source/origin, allocate nothing and reset failures. */
QJSIntlStatus qjs_intl_date_available_source_find(const QJSIntlDateAvailableSources *,
                                                  QJSIntlBytes code, QJSIntlDateAvailablePlural,
                                                  QJSIntlBytes leaf_alt,
                                                  QJSIntlDateAvailableEntry *, QJSIntlDateAvailableNode *);

#endif
