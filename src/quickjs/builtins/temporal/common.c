/*
 * Native Temporal support
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include "../temporal.h"

#ifdef CONFIG_TEMPORAL
#include "temporal-internal.h"
#include "../../internal/atom.h"
#include "../../internal/object.h"

size_t js_temporal_memory_usage(JSValueConst object, JSValue *owned_value)
{
    JSClassID class_id = JS_IsObject(object) ?
        JS_VALUE_GET_OBJ(object)->class_id : JS_INVALID_CLASS_ID;
    void *payload = JS_GetOpaque(object, class_id);

    *owned_value = JS_UNDEFINED;
    if (!payload)
        return 0;
    switch (class_id) {
    case JS_CLASS_TEMPORAL_INSTANT:
        return sizeof(JSTemporalInstantData);
    case JS_CLASS_TEMPORAL_DURATION:
        return sizeof(JSTemporalDurationData);
    case JS_CLASS_TEMPORAL_PLAIN_DATE:
        return sizeof(JSTemporalPlainDateData);
    case JS_CLASS_TEMPORAL_PLAIN_TIME:
        return sizeof(JSTemporalPlainTimeData);
    case JS_CLASS_TEMPORAL_PLAIN_DATE_TIME:
        return sizeof(JSTemporalPlainDateTimeData);
    case JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH:
        return sizeof(JSTemporalPlainYearMonthData);
    case JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY:
        return sizeof(JSTemporalPlainMonthDayData);
    case JS_CLASS_TEMPORAL_ZONED_DATE_TIME:
        *owned_value = ((JSTemporalZonedDateTimeData *)payload)->time_zone.identifier;
        return sizeof(JSTemporalZonedDateTimeData);
    default:
        return 0;
    }
}

static const int js_temporal_classes[] = {
    JS_CLASS_TEMPORAL_INSTANT,
    JS_CLASS_TEMPORAL_DURATION,
    JS_CLASS_TEMPORAL_PLAIN_DATE,
    JS_CLASS_TEMPORAL_PLAIN_TIME,
    JS_CLASS_TEMPORAL_PLAIN_DATE_TIME,
    JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH,
    JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY,
    JS_CLASS_TEMPORAL_ZONED_DATE_TIME,
};

static const JSCFunctionListEntry js_temporal_namespace_properties[] = {
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal", JS_PROP_CONFIGURABLE),
};

static void js_temporal_clear_intrinsics(JSContext *ctx)
{
    int i;

    JS_FreeValue(ctx, ctx->temporal_intrinsics);
    ctx->temporal_intrinsics = JS_UNDEFINED;
    ctx->temporal_published = FALSE;
    for (i = 0; i < countof(js_temporal_classes); i++)
        JS_SetClassProto(ctx, js_temporal_classes[i], JS_NULL);
}

/* Intrinsics belong to a realm independently of its global bindings.
   Keep the complete private namespace rooted even before publication. */
static int js_temporal_ensure_intrinsics(JSContext *ctx)
{
    JSValue namespace_object;

    if (JS_IsObject(ctx->temporal_intrinsics))
        return 0;
    namespace_object = JS_NewObject(ctx);
    if (JS_IsException(namespace_object))
        return -1;
    if (JS_SetPropertyFunctionList(ctx, namespace_object,
                    js_temporal_namespace_properties,
                    countof(js_temporal_namespace_properties)) ||
        js_temporal_init_instant(ctx, namespace_object) ||
        js_temporal_init_duration(ctx, namespace_object) ||
        js_temporal_init_plain_time(ctx, namespace_object) ||
        js_temporal_init_plain_date(ctx, namespace_object) ||
        js_temporal_init_plain_date_time(ctx, namespace_object) ||
        js_temporal_init_plain_year_month(ctx, namespace_object) ||
        js_temporal_init_plain_month_day(ctx, namespace_object) ||
        js_temporal_init_zoned_date_time(ctx, namespace_object) ||
        js_temporal_init_now(ctx, namespace_object)) {
        JS_FreeValue(ctx, namespace_object);
        js_temporal_clear_intrinsics(ctx);
        return -1;
    }
    ctx->temporal_intrinsics = namespace_object;
    return 0;
}

JSValue js_temporal_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                     int class_id)
{
    JSValue proto, object;
    JSContext *realm;

    if (!JS_IsObject(ctx->class_proto[class_id]) &&
        js_temporal_ensure_intrinsics(ctx))
        return JS_EXCEPTION;
    if (JS_IsUndefined(ctor)) {
        proto = JS_DupValue(ctx, ctx->class_proto[class_id]);
    } else {
        proto = JS_GetProperty(ctx, ctor, JS_ATOM_prototype);
        if (JS_IsException(proto))
            return proto;
        if (!JS_IsObject(proto)) {
            JS_FreeValue(ctx, proto);
            realm = JS_GetFunctionRealm(ctx, ctor);
            if (!realm || js_temporal_ensure_intrinsics(realm))
                return JS_EXCEPTION;
            proto = JS_DupValue(ctx, realm->class_proto[class_id]);
        }
    }
    object = JS_NewObjectProtoClass(ctx, proto, class_id);
    JS_FreeValue(ctx, proto);
    return object;
}

int JS_AddIntrinsicTemporal(JSContext *ctx)
{
    BOOL had_intrinsics;

    if (ctx->temporal_published)
        return 0;
    had_intrinsics = JS_IsObject(ctx->temporal_intrinsics);
    if (js_temporal_ensure_intrinsics(ctx))
        return -1;
    if (JS_DefinePropertyValueStr(ctx, ctx->global_obj, "Temporal",
                    JS_DupValue(ctx, ctx->temporal_intrinsics),
                    JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0) {
        if (!had_intrinsics)
            js_temporal_clear_intrinsics(ctx);
        return -1;
    }
    ctx->temporal_published = TRUE;
    return 0;
}

JSValue JS_NewTemporalInstant(JSContext *ctx, uint64_t low, uint64_t high)
{
    QJSTemporalEpochNs epoch = { low, high };

    if (!qjs_temporal_epoch_ns_is_valid(epoch))
        return JS_ThrowRangeError(ctx,
                    "Temporal epoch nanoseconds are outside the valid range");
    return js_temporal_create_instant(ctx, JS_UNDEFINED, epoch);
}

int JS_GetTemporalInstantEpochNanoseconds(JSContext *ctx, JSValueConst value,
                                        uint64_t *plow, uint64_t *phigh)
{
    JSTemporalInstantData *instant;

    if (!plow || !phigh || plow == phigh) {
        JS_ThrowTypeError(ctx, "distinct Temporal epoch outputs are required");
        return -1;
    }
    instant = JS_GetOpaque2(ctx, value, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return -1;
    *plow = instant->low;
    *phigh = instant->high;
    return 0;
}

#else

int JS_AddIntrinsicTemporal(JSContext *ctx)
{
    JS_ThrowTypeError(ctx, "Temporal support is disabled in this build");
    return -1;
}

JSValue JS_NewTemporalInstant(JSContext *ctx, uint64_t low, uint64_t high)
{
    (void)low;
    (void)high;
    return JS_ThrowTypeError(ctx, "Temporal support is disabled in this build");
}

int JS_GetTemporalInstantEpochNanoseconds(JSContext *ctx, JSValueConst value,
                                        uint64_t *plow, uint64_t *phigh)
{
    (void)value;
    (void)plow;
    (void)phigh;
    JS_ThrowTypeError(ctx, "Temporal support is disabled in this build");
    return -1;
}

#endif /* CONFIG_TEMPORAL */
