/* Immutable validated TZif views, private to the native backend and tools.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_TIMEZONE_TZIF_H
#define QUICKJS_TIMEZONE_TZIF_H

#include "timezone.h"

#define QJS_TZ_MAX_FILE_SIZE 65536
#define QJS_TZ_INSTANT_LIMIT INT64_C(8640000000000)
#define QJS_TZ_OFFSET_LIMIT (QJS_TZ_INSTANT_LIMIT + 172800)
#define QJS_TZ_WALL_LIMIT (QJS_TZ_INSTANT_LIMIT + 86400)

enum { QJS_TZ_RULE_DAY, QJS_TZ_RULE_JULIAN, QJS_TZ_RULE_MONTH };
typedef struct QJSTzifRule {
    int kind, day, month, week;
    int32_t seconds;
} QJSTzifRule;

typedef struct QJSTzifState {
    const unsigned char *times, *indices, *types, *characters;
    uint32_t time_count, type_count, character_count;
    /* 0: no footer, 1: fixed, 2: periodic, 3: perpetual daylight time. */
    int future;
    int32_t standard_offset, daylight_offset;
    QJSTzifRule start, end;
} QJSTzifState;

/* Parse without allocating. Views borrow immutable input bytes; the output
   is unchanged on failure. Queries require a successfully validated view. */
int qjs_tzif_parse(QJSTzifState *result, const unsigned char *bytes,
                   size_t length);
int qjs_tzif_offset(const QJSTzifState *state, int64_t seconds,
                    int32_t *offset);
int qjs_tzif_info(const QJSTzifState *state, int64_t seconds,
                  QJSTimeZoneInfo *result);
/* Raw rule lookup includes transitions just outside the Instant interval.
   Common Temporal code applies public Instant bounds after disambiguation. */
int qjs_tzif_transition(const QJSTzifState *state, int64_t seconds, int next,
                        int inclusive, int64_t *result, int *found);
int qjs_tzif_candidates(const QJSTzifState *state, int64_t wall,
                        QJSTimeZoneVisit *visit, void *opaque, size_t *count,
                        int32_t *gap_before, int32_t *gap_after,
                        int64_t *gap_end);

#endif /* QUICKJS_TIMEZONE_TZIF_H */
