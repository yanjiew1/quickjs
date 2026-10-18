/* Native rule descriptors over one generated immutable binary owner.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_TIMEZONE_NATIVE_H
#define QUICKJS_TIMEZONE_NATIVE_H

#include "timezone-backend.h"
#include "tzif.h"

struct QJSTimeZoneRules {
    QJSTzifState tzif;
};

extern const unsigned char qjs_native_timezone_data[];
extern const size_t qjs_native_timezone_data_size;
/* Canonical UTC is excluded; core supplies it. UTC aliases have NULL rules. */
extern const QJSTimeZone qjs_native_timezone_zones[];
extern const size_t qjs_native_timezone_zone_count;
extern const uint16_t qjs_native_timezone_lookup_order[];
extern const char *const qjs_native_timezone_data_version;

#endif /* QUICKJS_TIMEZONE_NATIVE_H */
