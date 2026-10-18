/* Common timezone operations over core UTC and a selected named-zone backend.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_TIMEZONE_H
#define QUICKJS_TIMEZONE_H

#include <stddef.h>
#include <stdint.h>

typedef struct QJSTimeZone QJSTimeZone;

typedef enum QJSTimeZoneError {
    QJS_TZ_OK = 0,
    QJS_TZ_RANGE = -1,
    QJS_TZ_UNSUPPORTED = -2,
    QJS_TZ_MEMORY = -3,
    QJS_TZ_DATA = -4,
} QJSTimeZoneError;

/* Handles and identifier strings borrow immutable core or backend data for the
   process lifetime. No caller frees them. Fallible calls preserve outputs.
   Seconds use the ECMAScript epoch, without leap-second corrections.
   UTC offsets are integral seconds strictly between -86400 and +86400. */
int qjs_timezone_lookup(const QJSTimeZone **result,
                        const char *identifier, size_t length);
const char *qjs_timezone_identifier(const QJSTimeZone *zone);
const char *qjs_timezone_primary_identifier(const QJSTimeZone *zone);
const char *qjs_timezone_data_version(void);
size_t qjs_timezone_count(void);
const QJSTimeZone *qjs_timezone_at(size_t index);
int qjs_timezone_offset(const QJSTimeZone *zone, int64_t epoch_seconds,
                        int32_t *offset_seconds);

/* Visit every candidate in ascending epoch order, without assuming at most
   two. Local seconds retain the caller's fractional nanoseconds. For a gap,
   return zero candidates, offsets immediately before and after the gap,
   and the epoch second at which the gap ends.
   A backend validates its data before invoking callbacks. */
typedef void QJSTimeZoneVisit(void *opaque, int64_t epoch_seconds);
int qjs_timezone_candidates(const QJSTimeZone *zone, int64_t local_seconds,
                            QJSTimeZoneVisit *visit, void *opaque,
                            size_t *count, int32_t *gap_before,
                            int32_t *gap_after, int64_t *gap_end);
/* Only UTC-offset changes count. Inclusivity applies to the given second;
   the common layer handles fractional instants. A missing transition leaves
   transition_seconds unchanged and sets found to zero. */
int qjs_timezone_transition(const QJSTimeZone *zone, int64_t epoch_seconds,
                            int next, int inclusive,
                            int64_t *transition_seconds, int *found);

#endif /* QUICKJS_TIMEZONE_H */
