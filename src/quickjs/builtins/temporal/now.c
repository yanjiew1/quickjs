/* Temporal.Now obtains native time and creates intrinsic-realm values.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include "calendar-fields.h"

static int system_zone(JSContext *ctx, JSTemporalTimeZone *result)
{
    QJSTemporalZone zone;
    result->identifier = JS_UNDEFINED;
    if (js_temporal_calendar_error(ctx, js_temporal_get_system_zone(ctx, &zone))) return -1;
    result->identifier = JS_NewString(ctx, zone.identifier);
    if (JS_IsException(result->identifier)) { result->identifier = JS_UNDEFINED; return -1; }
    result->is_offset = zone.is_offset;
    result->offset_nanoseconds = zone.offset_nanoseconds;
    result->provider = zone.provider;
    return 0;
}
static JSValue js_temporal_now_zone(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSTemporalTimeZone zone;
    if (system_zone(ctx, &zone)) return JS_EXCEPTION;
    return zone.identifier;
}
static JSValue js_temporal_now_instant(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    QJSTemporalEpochNs epoch;
    if (js_temporal_calendar_error(ctx, js_temporal_get_system_epoch(&epoch))) return JS_EXCEPTION;
    return js_temporal_create_instant(ctx, JS_UNDEFINED, epoch);
}
static JSValue js_temporal_now_iso(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    JSValueConst requested = argc ? argv[0] : JS_UNDEFINED;
    JSTemporalTimeZone zone;
    QJSTemporalEpochNs epoch;
    QJSTemporalISODateTime local;
    JSValue result = JS_EXCEPTION;
    if (JS_IsUndefined(requested) ? system_zone(ctx, &zone) :
                                   js_temporal_to_time_zone(ctx, requested, &zone)) return JS_EXCEPTION;
    if (js_temporal_calendar_error(ctx, js_temporal_get_system_epoch(&epoch))) goto done;
    if (!magic) {
        result = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                                   &zone, QJS_TEMPORAL_CAL_ISO8601);
        goto done;
    }
    if (js_temporal_time_zone_datetime(ctx, &zone, epoch, &local)) goto done;
    if (magic == 1) result = js_temporal_create_plain_date_time(ctx, JS_UNDEFINED,
                                                              local, QJS_TEMPORAL_CAL_ISO8601);
    else if (magic == 2) result = js_temporal_create_plain_date(ctx, JS_UNDEFINED,
                                                              local.date, QJS_TEMPORAL_CAL_ISO8601);
    else result = js_temporal_create_plain_time(ctx, JS_UNDEFINED, local.time);
 done:
    js_temporal_free_time_zone(ctx, &zone);
    return result;
}
static const JSCFunctionListEntry js_temporal_now_functions[] = {
    JS_CFUNC_DEF("timeZoneId", 0, js_temporal_now_zone),
    JS_CFUNC_DEF("instant", 0, js_temporal_now_instant),
    JS_CFUNC_MAGIC_DEF("zonedDateTimeISO", 0, js_temporal_now_iso, 0),
    JS_CFUNC_MAGIC_DEF("plainDateTimeISO", 0, js_temporal_now_iso, 1),
    JS_CFUNC_MAGIC_DEF("plainDateISO", 0, js_temporal_now_iso, 2),
    JS_CFUNC_MAGIC_DEF("plainTimeISO", 0, js_temporal_now_iso, 3),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.Now", JS_PROP_CONFIGURABLE),
};
int js_temporal_init_now(JSContext *ctx, JSValueConst namespace_object)
{
    JSValue object = JS_NewObject(ctx);
    if (JS_IsException(object)) return -1;
    if (JS_SetPropertyFunctionList(ctx, object, js_temporal_now_functions,
                                  countof(js_temporal_now_functions)) < 0) {
        JS_FreeValue(ctx, object); return -1;
    }
    return JS_DefinePropertyValueStr(ctx, namespace_object, "Now", object,
                JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0 ? -1 : 0;
}
