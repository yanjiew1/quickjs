/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-native-zone.h"

QJSIntlStatus qjs_intl_native_date_iana_zone(void *opaque, QJSIntlBytes name,
    int64_t seconds, QJSIntlDateZoneInfo *out)
{
    const QJSTimeZone *zone;
    QJSTzInfo info;
    int r;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    out->offset_seconds = 0; out->daylight = -1;
    if (!opaque || !name.data || !name.length) return QJS_INTL_INVALID_ARGUMENT;
    r = qjs_tz_provider_open(opaque, name.data, name.length, &zone);
    if (r == QJS_TZ_MEMORY) return QJS_INTL_NO_MEMORY;
    if (r == QJS_TZ_ABSENT) return QJS_INTL_UNSUPPORTED;
    if (r != QJS_TZ_OK) return QJS_INTL_DATA_ERROR;
    r = qjs_tz_info(zone, seconds, &info);
    if (!r) { out->offset_seconds = info.offset_seconds; out->daylight = info.daylight; }
    return r == QJS_TZ_OK ? QJS_INTL_OK : r == QJS_TZ_RANGE ? QJS_INTL_OVERFLOW : QJS_INTL_DATA_ERROR;
}

QJSIntlStatus qjs_intl_native_date_iana_zone_stable(void *opaque, QJSIntlBytes name,
    int64_t from, int64_t through, int *proven)
{
    const QJSTimeZone *zone;
    int r;
    if (!opaque || !name.data || !name.length || !proven) return QJS_INTL_INVALID_ARGUMENT;
    r = qjs_tz_provider_open(opaque, name.data, name.length, &zone);
    if (!r) r = qjs_tz_name_stable(zone, from, through, proven);
    return r == QJS_TZ_OK ? QJS_INTL_OK : r == QJS_TZ_MEMORY ? QJS_INTL_NO_MEMORY :
        r == QJS_TZ_RANGE ? QJS_INTL_OVERFLOW : QJS_INTL_DATA_ERROR;
}
