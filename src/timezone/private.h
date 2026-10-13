/* Internal immutable TZif views. Copyright (c) 2026 Yan-Jie Wang. MIT. */
#ifndef QUICKJS_TIMEZONE_PRIVATE_H
#define QUICKJS_TIMEZONE_PRIVATE_H
#include "timezone.h"

#define QJS_TZ_MAX_FILE_SIZE 65536
#define QJS_TZ_INSTANT_LIMIT INT64_C(8640000000000)
#define QJS_TZ_OFFSET_LIMIT (QJS_TZ_INSTANT_LIMIT + 172800)
#define QJS_TZ_WALL_LIMIT (QJS_TZ_INSTANT_LIMIT + 86400)
typedef struct QJSTzRecord {
    const char *identifier, *primary;
    uint32_t offset, length;
} QJSTzRecord;
extern const unsigned char qjs_tz_embedded_data[];
extern const size_t qjs_tz_embedded_size;
extern const QJSTzRecord qjs_tz_records[];
extern const size_t qjs_tz_record_count;

/* A rule is a zero-based day, a Julian day excluding February 29, or a
   month/week/weekday. seconds may carry a rule into an adjacent day/year. */
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
    int standard_unspecified, daylight_unspecified;
    QJSTzifRule start, end;
} QJSTzifState;

/* Parse borrows bytes without allocating. Successful views remain valid
   for exactly the immutable byte lifetime; output is unchanged on failure. */
int qjs_tzif_parse(QJSTzifState *, const unsigned char *, size_t);
int qjs_tzif_system_complete(const QJSTzifState *, const QJSTzifState *);
int qjs_tzif_is_utc(const QJSTzifState *);
int qjs_tzif_offset(const QJSTzifState *, int64_t, int32_t *);
int qjs_tzif_info(const QJSTzifState *, int64_t, QJSTzInfo *);
int qjs_tzif_name_stable(const QJSTzifState *, int64_t, int64_t, int *);
int qjs_tzif_transition(const QJSTzifState *, int64_t, int, int, int64_t *, int *);
int qjs_tzif_local_offsets(const QJSTzifState *, int64_t, int32_t [2]);
#endif
