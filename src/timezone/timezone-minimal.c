/* Minimum ECMA-262 named timezone capability; fixed offsets are common code.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "timezone.h"

struct QJSTimeZone {
    const char *identifier;
};

static const QJSTimeZone utc = { "UTC" };

int qjs_timezone_lookup(const QJSTimeZone **result,
                        const char *identifier, size_t length)
{
    if (length != 3 || (identifier[0] != 'U' && identifier[0] != 'u') ||
        (identifier[1] != 'T' && identifier[1] != 't') ||
        (identifier[2] != 'C' && identifier[2] != 'c'))
        return QJS_TZ_RANGE;
    *result = &utc;
    return 0;
}

const char *qjs_timezone_identifier(const QJSTimeZone *zone)
{
    return zone->identifier;
}

const char *qjs_timezone_primary_identifier(const QJSTimeZone *zone)
{
    return zone->identifier;
}

const char *qjs_timezone_data_version(void)
{
    return "minimal";
}

size_t qjs_timezone_count(void)
{
    return 1;
}

const QJSTimeZone *qjs_timezone_at(size_t index)
{
    return index == 0 ? &utc : NULL;
}

int qjs_timezone_offset(const QJSTimeZone *zone, int64_t epoch_seconds,
                        int32_t *offset_seconds)
{
    *offset_seconds = 0;
    return 0;
}

int qjs_timezone_candidates(const QJSTimeZone *zone, int64_t local_seconds,
                            QJSTimeZoneVisit *visit, void *opaque,
                            size_t *count, int32_t *gap_before,
                            int32_t *gap_after, int64_t *gap_end)
{
    visit(opaque, local_seconds);
    *count = 1;
    *gap_before = *gap_after = 0;
    return 0;
}

int qjs_timezone_transition(const QJSTimeZone *zone, int64_t epoch_seconds,
                            int next, int inclusive,
                            int64_t *transition_seconds, int *found)
{
    *found = 0;
    return 0;
}
