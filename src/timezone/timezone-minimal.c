/* Empty named-zone backend; mandatory UTC belongs to timezone.c.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "timezone-backend.h"

int qjs_timezone_backend_lookup(const QJSTimeZone **result,
                                const char *identifier, size_t length)
{
    return QJS_TZ_RANGE;
}

const char *qjs_timezone_backend_data_version(void)
{
    return "minimal";
}

size_t qjs_timezone_backend_count(void)
{
    return 0;
}

const QJSTimeZone *qjs_timezone_backend_at(size_t index)
{
    return NULL;
}

/* No rule handles exist in this configuration. Reaching a rule operation
   indicates an internal contract violation, not an unsupported user zone. */
int qjs_timezone_backend_offset(const QJSTimeZoneRules *rules, int64_t seconds,
                                int32_t *offset)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_info(const QJSTimeZoneRules *rules, int64_t seconds,
                              QJSTimeZoneInfo *result)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_info_is_constant(const QJSTimeZoneRules *rules, int64_t first,
                                          int64_t last, int *constant)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_candidates(const QJSTimeZoneRules *rules, int64_t wall,
                                    QJSTimeZoneVisit *visit, void *opaque,
                                    size_t *count, int32_t *gap_before,
                                    int32_t *gap_after, int64_t *gap_end)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_transition(const QJSTimeZoneRules *rules,
                                    int64_t seconds, int next, int inclusive,
                                    int64_t *result, int *found)
{
    return QJS_TZ_DATA;
}
