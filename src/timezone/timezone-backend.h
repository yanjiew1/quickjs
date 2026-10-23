/* Private contract for additional named zones selected at build time.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_TIMEZONE_BACKEND_H
#define QUICKJS_TIMEZONE_BACKEND_H

#include "timezone.h"

typedef struct QJSTimeZoneRules QJSTimeZoneRules;

struct QJSTimeZone {
    const char *identifier;
    const char *primary;
    /* NULL means UTC semantics, including aliases whose primary is UTC.
       Political zones retain rules even when their current offset is zero. */
    const QJSTimeZoneRules *rules;
};

/* Backends supply only additional names: canonical UTC belongs to core.
   Enumeration is sorted by identifier in code-unit order, excludes UTC,
   and has no ASCII-case-insensitive duplicates. All handles, strings and
   rules are immutable and borrowed for the process lifetime. */
int qjs_timezone_backend_lookup(const QJSTimeZone **result,
                                const char *identifier, size_t length);
const char *qjs_timezone_backend_data_version(void);
size_t qjs_timezone_backend_count(void);
const QJSTimeZone *qjs_timezone_backend_at(size_t index);
int qjs_timezone_backend_offset(const QJSTimeZoneRules *rules, int64_t seconds,
                                int32_t *offset);
int qjs_timezone_backend_info(const QJSTimeZoneRules *rules, int64_t seconds,
                              QJSTimeZoneInfo *result);
int qjs_timezone_backend_info_is_constant(const QJSTimeZoneRules *rules, int64_t first,
                                          int64_t last, int *constant);
int qjs_timezone_backend_candidates(const QJSTimeZoneRules *rules, int64_t wall,
                                    QJSTimeZoneVisit *visit, void *opaque,
                                    size_t *count, int32_t *gap_before,
                                    int32_t *gap_after, int64_t *gap_end);
int qjs_timezone_backend_transition(const QJSTimeZoneRules *rules,
                                    int64_t seconds, int next, int inclusive,
                                    int64_t *result, int *found);

#endif /* QUICKJS_TIMEZONE_BACKEND_H */
