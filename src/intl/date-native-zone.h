/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_NATIVE_ZONE_H
#define QJS_INTL_DATE_NATIVE_ZONE_H
#include "date-native.h"
#include "timezone/timezone.h"
/* environment.opaque is the borrowed runtime QJSTzProvider. Its owner must
 * outlive all formatters and serialize access. No ICU or process localtime.
 * The provider supplies raw TZif daylight classification. Core applies any
 * explicit CLDR period stdOffset/dstOffset for name classification. The lazy
 * stability callback proves signed adjustment constancy without inferring
 * historical SAVE from either DST flag or a neighboring type.
 */
QJSIntlStatus qjs_intl_native_date_iana_zone(void *, QJSIntlBytes, int64_t,
                                           QJSIntlDateZoneInfo *);
QJSIntlStatus qjs_intl_native_date_iana_zone_stable(void *, QJSIntlBytes,
    int64_t from_seconds, int64_t through_seconds, int *proven);
#endif
