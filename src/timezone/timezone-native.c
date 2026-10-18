/* Native additional named zones over immutable generated rule descriptors.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "timezone-native.h"

static int folded_compare(const char *canonical, const char *text, size_t length)
{
    size_t i;

    for (i = 0; i < length && canonical[i]; i++) {
        unsigned char a = canonical[i], b = text[i];

        if (a >= 'A' && a <= 'Z')
            a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z')
            b += 'a' - 'A';
        if (a != b)
            return a < b ? -1 : 1;
    }
    if (i < length)
        return -1;
    return canonical[i] ? 1 : 0;
}

int qjs_timezone_backend_lookup(const QJSTimeZone **result,
                               const char *identifier, size_t length)
{
    size_t lo = 0, hi = qjs_native_timezone_zone_count;

    while (lo < hi) {
        size_t middle = lo + (hi - lo) / 2;
        const QJSTimeZone *zone = qjs_native_timezone_zones +
            qjs_native_timezone_lookup_order[middle];
        int order = folded_compare(zone->identifier, identifier, length);

        if (!order) {
            *result = zone;
            return 0;
        }
        if (order < 0)
            lo = middle + 1;
        else
            hi = middle;
    }
    return QJS_TZ_RANGE;
}

const char *qjs_timezone_backend_data_version(void)
{
    return qjs_native_timezone_data_version;
}

size_t qjs_timezone_backend_count(void)
{
    return qjs_native_timezone_zone_count;
}

const QJSTimeZone *qjs_timezone_backend_at(size_t index)
{
    return index < qjs_native_timezone_zone_count ?
        qjs_native_timezone_zones + index : NULL;
}

int qjs_timezone_backend_offset(const QJSTimeZoneRules *rules, int64_t seconds,
                               int32_t *offset)
{
    return qjs_tzif_offset(&rules->tzif, seconds, offset);
}

int qjs_timezone_backend_info(const QJSTimeZoneRules *rules, int64_t seconds,
                              QJSTimeZoneInfo *result)
{
    return qjs_tzif_info(&rules->tzif, seconds, result);
}

int qjs_timezone_backend_candidates(const QJSTimeZoneRules *rules, int64_t wall,
                                   QJSTimeZoneVisit *visit, void *opaque,
                                   size_t *count, int32_t *gap_before,
                                   int32_t *gap_after, int64_t *gap_end)
{
    return qjs_tzif_candidates(&rules->tzif, wall, visit, opaque, count,
                               gap_before, gap_after, gap_end);
}

int qjs_timezone_backend_transition(const QJSTimeZoneRules *rules, int64_t seconds,
                                   int next, int inclusive, int64_t *result,
                                   int *found)
{
    return qjs_tzif_transition(&rules->tzif, seconds, next, inclusive, result, found);
}
