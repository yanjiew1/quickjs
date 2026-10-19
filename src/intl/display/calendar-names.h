/* Native calendar display identities, independent of calendar arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DISPLAY_CALENDAR_NAMES_H
#define QJS_INTL_DISPLAY_CALENDAR_NAMES_H
#include "data.h"
#include "../locale/native-data.h"

typedef struct QJSIntlCalendarNameRecord QJSIntlCalendarNameRecord;
typedef struct QJSIntlCalendarNames {
    QJSIntlAllocator allocator;
    QJSIntlCalendarNameRecord *records;
    size_t count;
} QJSIntlCalendarNames;

/* Build once from unchanged validated owners. Own one allocation for sorted
   API keys and records; source lookup keys borrow the names blob, which must
   outlive the projection. Metadata is borrowed only during open. Require
   malloc/free; no realloc or JS callbacks.
   Keep core calendar canonicalization separate: this projects source fields,
   never rewrites API identities or advertises arithmetic capabilities.
   Follow pack.h's disjoint-output contract; reset output on failure. */
QJSIntlStatus qjs_intl_calendar_names_open(const QJSIntlLocaleMetadata *, const QJSIntlNameData *,
                                         const QJSIntlAllocator *, QJSIntlCalendarNames *);
/* NULL and already-zero projections are safe. */
void qjs_intl_calendar_names_clear(QJSIntlCalendarNames *);
/* Exact lowercase Unicode-type API key. Allocate nothing; return a normalized
   source lookup key, not raw provenance (which remains with name entries).
   Absent keys return UNSUPPORTED. Non-NULL output resets on failure. */
QJSIntlStatus qjs_intl_calendar_names_find(const QJSIntlCalendarNames *, QJSIntlBytes code,
                                         QJSIntlBytes *source_key);
#endif
