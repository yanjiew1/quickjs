/* Source proposal only. No calendar ID activation or runtime ingestion. */
#ifndef QJS_PUBLISHED_CALENDAR_MONTH_RECORD_H
#define QJS_PUBLISHED_CALENDAR_MONTH_RECORD_H
#include <stdint.h>
#include <stddef.h>

typedef struct QJSPublishedCalendarMonth {
    int32_t epoch_day;       /* Gregorian civil day relative to 1970-01-01. */
    int32_t lunar_year;      /* Arithmetic Gregorian year at lunar M01, for both IDs. */
    uint8_t month;           /* 1..12; MonthCode is Mxx or MxxL. */
    uint8_t is_leap;         /* 0 or 1. */
    uint8_t length;          /* 29 or 30, proven against next start. */
} QJSPublishedCalendarMonth;

typedef struct QJSPublishedCalendarTable {
    const QJSPublishedCalendarMonth *months;
    size_t count;
    int32_t observed_first_day;
    int32_t observed_end_day; /* Exclusive, all daily observations validated. */
    int32_t proven_first_day;
    int32_t proven_end_day;   /* Exclusive, complete months only. */
} QJSPublishedCalendarTable;

/* A complete lunar year requires M01..M12, optional one repeated leap month,
   and a proven next M01 boundary. Partial boundary years are unavailable for
   fields that require complete year metadata. Each record's provenance belongs
   in host audit data, never in runtime PDF/JSON/XML parsing. */
#endif
