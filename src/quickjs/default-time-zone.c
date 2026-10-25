/* Shared runtime time-zone policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "internal/base.h"
#include "internal/runtime.h"
#include "internal/default-time-zone.h"
#include "../timezone/timezone.h"
#include "../timezone/system-zone.h"

#ifdef CONFIG_TEMPORAL
/* Runtime host defaults use primary names. Explicit Temporal time-zone inputs continue
   to preserve their canonical alias identifier in their existing parser. */
static int default_zone_primary(QJSTimeZoneIdentifier *zone)
{
    if (zone->is_offset) {
        if (zone->offset_nanoseconds % INT64_C(60000000000))
            return QJS_TZ_RANGE;
        return 0;
    }
    {
        const char *primary = qjs_timezone_primary_identifier(zone->named);
        if (strlen(primary) >= sizeof(zone->identifier))
            return QJS_TZ_DATA;
        strcpy(zone->identifier, primary);
    }
    return 0;
}

static int default_zone_commit(JSContext *ctx, const QJSTimeZoneIdentifier *zone)
{
    size_t size = strlen(zone->identifier) + 1;
    char *copy = js_malloc_rt(ctx->rt, size), *old;
    if (!copy)
        return QJS_TZ_MEMORY;
    memcpy(copy, zone->identifier, size);
    /* Parsing and allocation are complete. No provider/cache mutation, GC,
       user callback, or fallible operation occurs at or after this commit. */
    old = ctx->rt->default_time_zone;
    ctx->rt->default_time_zone = copy;
    js_free_rt(ctx->rt, old);
    return 0;
}

static int js_default_time_zone_identifier(JSContext *ctx, const char **result)
{
    QJSTimeZoneIdentifier zone;
    int error;
    if (!ctx->rt->default_time_zone) {
        char identifier[256];
        int host_offset_fallback = 0;

#ifdef CONFIG_TIMEZONE_HOST_OFFSET
        host_offset_fallback = 1;
#endif
        error = qjs_timezone_system_default(identifier, sizeof(identifier),
                                            host_offset_fallback);
        if (error)
            return QJS_TZ_DATA;
        error = qjs_timezone_parse_identifier(&zone, identifier, strlen(identifier));
        if (!error)
            error = default_zone_primary(&zone);
        if (!error)
            error = default_zone_commit(ctx, &zone);
        if (error)
            return error;
    }
    *result = ctx->rt->default_time_zone;
    return 0;
}

int js_default_time_zone_get(JSContext *ctx, QJSTimeZoneIdentifier *result)
{
    const char *identifier;
    QJSTimeZoneIdentifier zone;
    int error = js_default_time_zone_identifier(ctx, &identifier);
    if (!error)
        error = qjs_timezone_parse_identifier(&zone, identifier, strlen(identifier));
    if (!error)
        *result = zone;
    return error;
}

static int default_zone_throw(JSContext *ctx, int error)
{
    if (error == QJS_TZ_MEMORY)
        JS_ThrowOutOfMemory(ctx);
    else if (error == QJS_TZ_RANGE)
        JS_ThrowRangeError(ctx, "invalid default time zone");
    else
        JS_ThrowInternalError(ctx, "default time-zone backend failed");
    return -1;
}

int JS_SetDefaultTimeZone(JSContext *ctx, const char *time_zone)
{
    QJSTimeZoneIdentifier zone;
    int error;
    if (!time_zone)
        return default_zone_throw(ctx, QJS_TZ_RANGE);
    error = qjs_timezone_parse_identifier(&zone, time_zone, strlen(time_zone));
    if (!error)
        error = default_zone_primary(&zone);
    if (!error)
        error = default_zone_commit(ctx, &zone);
    return error ? default_zone_throw(ctx, error) : 0;
}

JSValue JS_GetDefaultTimeZone(JSContext *ctx)
{
    const char *identifier;
    int error = js_default_time_zone_identifier(ctx, &identifier);
    if (error) {
        default_zone_throw(ctx, error);
        return JS_EXCEPTION;
    }
    return JS_NewString(ctx, identifier);
}
#else
int JS_SetDefaultTimeZone(JSContext *ctx, const char *time_zone)
{
    (void)time_zone;
    JS_ThrowTypeError(ctx, "default time-zone configuration requires Temporal or Intl");
    return -1;
}
JSValue JS_GetDefaultTimeZone(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "default time-zone configuration requires Temporal or Intl");
}
#endif
