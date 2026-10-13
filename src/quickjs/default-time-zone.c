/* Shared runtime time-zone policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include "internal/base.h"
#include "internal/runtime.h"
#include "internal/default-time-zone.h"
#include "../timezone/timezone.h"

#ifdef CONFIG_TEMPORAL
static void *default_zone_allocate(void *opaque, size_t size)
{
    return js_malloc_rt(opaque, size);
}
static void default_zone_deallocate(void *opaque, void *pointer)
{
    js_free_rt(opaque, pointer);
}
static int default_zone_native_error(int error)
{
    return error == QJS_TZ_OK ? 0 :
        error == QJS_TZ_MEMORY ? QJS_TEMPORAL_ERROR_MEMORY :
        error == QJS_TZ_RANGE ? QJS_TEMPORAL_ERROR_RANGE :
        QJS_TEMPORAL_ERROR_BACKEND;
}

int js_default_time_zone_bind(JSContext *ctx, QJSTemporalZone *zone)
{
    JSRuntime *rt = ctx->rt;
    const QJSTimeZone *snapshot;
    int error;

    if (zone->is_offset || !strcmp(zone->identifier, "UTC")) {
        zone->provider = NULL;
        return 0;
    }
    if (zone->provider)
        return 0;
    if (!rt->temporal_tz_provider) {
        QJSTzProvider *provider;
        QJSTzAllocator allocator = {
            rt, default_zone_allocate, default_zone_deallocate
        };
        error = qjs_tz_provider_create(&provider, NULL, &allocator);
        if (error)
            return default_zone_native_error(error);
        rt->temporal_tz_provider = provider;
    }
    error = qjs_tz_provider_open(rt->temporal_tz_provider,
                zone->identifier, strlen(zone->identifier), &snapshot);
    if (error)
        return default_zone_native_error(error);
    zone->provider = rt->temporal_tz_provider;
    return 0;
}

/* Runtime host defaults use primary names. Explicit Temporal time-zone inputs continue
   to preserve their canonical alias identifier in their existing parser. */
static int default_zone_primary(QJSTemporalZone *zone)
{
    if (zone->is_offset) {
        if (zone->offset_nanoseconds % INT64_C(60000000000))
            return QJS_TEMPORAL_ERROR_RANGE;
        return 0;
    }
    {
        const char *primary;
        int error = qjs_tz_resolve(zone->identifier, strlen(zone->identifier), NULL, &primary);
        if (error)
            return default_zone_native_error(error);
        if (strlen(primary) >= sizeof(zone->identifier))
            return QJS_TEMPORAL_ERROR_BACKEND;
        strcpy(zone->identifier, primary);
    }
    return 0;
}

static int default_zone_commit(JSContext *ctx, const QJSTemporalZone *zone)
{
    size_t size = strlen(zone->identifier) + 1;
    char *copy = js_malloc_rt(ctx->rt, size), *old;
    if (!copy)
        return QJS_TEMPORAL_ERROR_MEMORY;
    memcpy(copy, zone->identifier, size);
    /* Parsing and allocation are complete. No provider/cache mutation, GC,
       user callback, or fallible operation occurs at or after this commit. */
    old = ctx->rt->default_time_zone;
    ctx->rt->default_time_zone = copy;
    js_free_rt(ctx->rt, old);
    return 0;
}

int js_default_time_zone_identifier(JSContext *ctx, const char **result)
{
    QJSTemporalZone zone;
    int error;
    if (!ctx->rt->default_time_zone) {
        error = qjs_temporal_system_zone(&zone);
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

int js_default_time_zone_get(JSContext *ctx, QJSTemporalZone *result)
{
    const char *identifier;
    QJSTemporalZone zone;
    int error = js_default_time_zone_identifier(ctx, &identifier);
    if (!error)
        error = qjs_temporal_zone_parse(&zone, identifier, strlen(identifier));
    if (!error)
        error = js_default_time_zone_bind(ctx, &zone);
    if (!error)
        *result = zone;
    return error;
}

static int default_zone_throw(JSContext *ctx, int error)
{
    if (error == QJS_TEMPORAL_ERROR_MEMORY)
        JS_ThrowOutOfMemory(ctx);
    else if (error == QJS_TEMPORAL_ERROR_RANGE)
        JS_ThrowRangeError(ctx, "invalid default time zone");
    else
        JS_ThrowInternalError(ctx, "default time-zone backend failed");
    return -1;
}

int JS_SetDefaultTimeZone(JSContext *ctx, const char *time_zone)
{
    QJSTemporalZone zone;
    int error;
    if (!time_zone)
        return default_zone_throw(ctx, QJS_TEMPORAL_ERROR_RANGE);
    error = qjs_temporal_zone_parse(&zone, time_zone, strlen(time_zone));
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
    JS_ThrowTypeError(ctx, "default time-zone configuration requires Temporal");
    return -1;
}
JSValue JS_GetDefaultTimeZone(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "default time-zone configuration requires Temporal");
}
#endif
