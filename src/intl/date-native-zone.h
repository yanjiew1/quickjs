/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_NATIVE_ZONE_H
#define QJS_INTL_DATE_NATIVE_ZONE_H
#include "date-native.h"
#include "timezone/timezone.h"
/* environment.opaque is the borrowed runtime QJSTzProvider. Its owner must
 * outlive all formatters and serialize access. No ICU or process localtime.
 * The current provider exposes total offset only: daylight is unknown, and
 * specific timezone labels take the localized offset fallback. A richer
 * explicit callback may supply the real daylight flag without changing core.
 */
QJSIntlStatus qjs_intl_native_date_iana_zone(void *, QJSIntlBytes, int64_t,
                                           QJSIntlDateZoneInfo *);
#endif
