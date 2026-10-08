/*
 * QuickJS Temporal.Instant native implementation
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
#include "temporal-internal.h"
#include "../temporal.h"
#include "../../../temporal/format.h"

static void js_temporal_instant_finalizer(JSRuntime *rt, JSValue val)
{
    JSTemporalInstantData *s = JS_GetOpaque(val, JS_CLASS_TEMPORAL_INSTANT);
    js_free_rt(rt, s);
}

/* The payload contains only integer words, so it has no GC edges. */
static const JSClassDef js_temporal_instant_class = {
    .class_name = "Temporal.Instant",
    .finalizer = js_temporal_instant_finalizer,
};

JSValue js_temporal_create_instant(JSContext *ctx,
                                   JSValueConst new_target,
                                   QJSTemporalEpochNs epoch_ns)
{
    JSTemporalInstantData *s;
    JSValue obj;

    obj = js_temporal_create_from_ctor(ctx, new_target, JS_CLASS_TEMPORAL_INSTANT);
    if (JS_IsException(obj))
        return obj;
    s = js_malloc(ctx, sizeof(*s));
    if (!s) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    *s = epoch_ns;
    JS_SetOpaque(obj, s);
    return obj;
}

/* ToTemporalInstant produces a fresh intrinsic-realm instance. Native
   branding precedes every observable conversion or property access. */
JSValue js_temporal_to_instant(JSContext *ctx, JSValueConst item)
{
    JSTemporalInstantData *s;
    QJSTemporalEpochNs epoch_ns;
    JSValue primitive;
    const char *text;
    size_t length;
    int ret;

    if (JS_IsObject(item)) {
        s = JS_GetOpaque(item, JS_CLASS_TEMPORAL_INSTANT);
        if (s)
            return js_temporal_create_instant(ctx, JS_UNDEFINED, *s);
        {
            JSTemporalZonedDateTimeData *zoned;
            zoned = JS_GetOpaque(item, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
            if (zoned)
                return js_temporal_create_instant(ctx, JS_UNDEFINED,
                                                  zoned->epoch_nanoseconds);
        }
        primitive = JS_ToPrimitive(ctx, item, HINT_STRING);
        if (JS_IsException(primitive))
            return primitive;
    } else {
        primitive = JS_DupValue(ctx, item);
    }
    if (!JS_IsString(primitive)) {
        JS_FreeValue(ctx, primitive);
        return JS_ThrowTypeError(ctx, "Temporal.Instant input must be a string");
    }
    text = JS_ToCStringLen(ctx, &length, primitive);
    if (!text) {
        JS_FreeValue(ctx, primitive);
        return JS_EXCEPTION;
    }
    ret = qjs_temporal_parse_instant(&epoch_ns, text, length);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, primitive);
    if (ret)
        return JS_ThrowRangeError(ctx, "invalid Temporal.Instant string");
    return js_temporal_create_instant(ctx, JS_UNDEFINED, epoch_ns);
}

static JSValue js_temporal_instant_from(JSContext *ctx,
                                        JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    return js_temporal_to_instant(ctx, argv[0]);
}

static JSValue js_temporal_instant_compare(JSContext *ctx,
                                           JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    JSTemporalInstantData *a, *b;
    JSValue one, two;
    int comparison;

    one = js_temporal_to_instant(ctx, argv[0]);
    if (JS_IsException(one))
        return one;
    two = js_temporal_to_instant(ctx, argv[1]);
    if (JS_IsException(two)) {
        JS_FreeValue(ctx, one);
        return two;
    }
    a = JS_GetOpaque(one, JS_CLASS_TEMPORAL_INSTANT);
    b = JS_GetOpaque(two, JS_CLASS_TEMPORAL_INSTANT);
    comparison = qjs_temporal_epoch_ns_compare(*a, *b);
    JS_FreeValue(ctx, one);
    JS_FreeValue(ctx, two);
    return JS_NewInt32(ctx, comparison);
}

static JSValue js_temporal_instant_equals(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSTemporalInstantData *s;
    QJSTemporalEpochNs epoch_ns;
    JSValue other;
    int equal;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!s)
        return JS_EXCEPTION;
    epoch_ns = *s;
    other = js_temporal_to_instant(ctx, argv[0]);
    if (JS_IsException(other))
        return other;
    s = JS_GetOpaque(other, JS_CLASS_TEMPORAL_INSTANT);
    equal = qjs_temporal_epoch_ns_compare(epoch_ns, *s) == 0;
    JS_FreeValue(ctx, other);
    return JS_NewBool(ctx, equal);
}

static JSValue js_temporal_instant_constructor(JSContext *ctx,
                                               JSValueConst new_target,
                                               int argc, JSValueConst *argv)
{
    QJSTemporalEpochNs epoch_ns;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Temporal.Instant requires new");
    if (js_temporal_to_epoch_ns(ctx, argv[0], &epoch_ns))
        return JS_EXCEPTION;
    return js_temporal_create_instant(ctx, new_target, epoch_ns);
}

static JSValue js_temporal_instant_from_epoch_nanoseconds(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSTemporalEpochNs epoch_ns;

    if (js_temporal_to_epoch_ns(ctx, argv[0], &epoch_ns))
        return JS_EXCEPTION;
    return js_temporal_create_instant(ctx, JS_UNDEFINED, epoch_ns);
}

static JSValue js_temporal_instant_from_epoch_milliseconds(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSTemporalEpochNs epoch_ns;
    double milliseconds;

    if (JS_ToFloat64(ctx, &milliseconds, argv[0]))
        return JS_EXCEPTION;
    if (qjs_temporal_epoch_ns_from_milliseconds(&epoch_ns, milliseconds))
        return JS_ThrowRangeError(ctx, "invalid Temporal epoch milliseconds");
    return js_temporal_create_instant(ctx, JS_UNDEFINED, epoch_ns);
}

static JSValue js_temporal_instant_epoch_nanoseconds(JSContext *ctx,
                                                     JSValueConst this_val)
{
    JSTemporalInstantData *s;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!s)
        return JS_EXCEPTION;
    return JS_NewBigInt128(ctx, s->low, s->high);
}

static JSValue js_temporal_instant_epoch_milliseconds(JSContext *ctx,
                                                      JSValueConst this_val)
{
    JSTemporalInstantData *s;
    int64_t milliseconds;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!s)
        return JS_EXCEPTION;
    if (qjs_temporal_epoch_ns_to_milliseconds(&milliseconds, *s))
        return JS_ThrowRangeError(ctx, "Temporal.Instant is outside the valid range");
    return JS_NewInt64(ctx, milliseconds);
}

static JSValue js_temporal_instant_value_of(JSContext *ctx,
                                            JSValueConst this_val,
                                            int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "Temporal.Instant cannot be converted to a number");
}

static JSValue js_temporal_instant_add(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int subtract)
{
    JSTemporalInstantData *instant;
    JSTemporalDurationData *duration;
    QJSTemporalInternalDuration normalized;
    QJSTemporalEpochNs result;
    JSValue converted;
    int i, ret;

    instant = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return JS_EXCEPTION;
    converted = js_temporal_to_duration(ctx, argv[0]);
    if (JS_IsException(converted))
        return converted;
    duration = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_DURATION);
    for (i = QJS_TEMPORAL_YEAR; i <= QJS_TEMPORAL_DAY; i++) {
        if (duration->fields[i]) {
            JS_FreeValue(ctx, converted);
            return JS_ThrowRangeError(ctx, "Instant arithmetic requires time units");
        }
    }
    ret = qjs_temporal_duration_normalize(&normalized, duration, 1);
    JS_FreeValue(ctx, converted);
    if (ret)
        return JS_ThrowRangeError(ctx, "Temporal duration is outside the valid range");
    ret = subtract ?
        qjs_temporal_epoch_ns_subtract(&result, *instant, normalized.time) :
        qjs_temporal_epoch_ns_add(&result, *instant, normalized.time);
    if (ret || !qjs_temporal_epoch_ns_is_valid(result))
        return JS_ThrowRangeError(ctx, "Temporal.Instant is outside the valid range");
    return js_temporal_create_instant(ctx, JS_UNDEFINED, result);
}

static JSValue js_temporal_instant_difference(JSContext *ctx,
                                             JSValueConst this_val,
                                             int argc, JSValueConst *argv,
                                             int since)
{
    JSTemporalInstantData *instant, *other;
    QJSTemporalDifferenceSettings settings;
    QJSTemporalInternalDuration internal = { 0 };
    QJSTemporalDuration duration;
    QJSTemporalEpochNs difference;
    JSValue converted, options;
    uint64_t increment;
    int ret;

    instant = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return JS_EXCEPTION;
    converted = js_temporal_to_instant(ctx, argv[0]);
    if (JS_IsException(converted))
        return converted;
    other = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_INSTANT);
    ret = qjs_temporal_epoch_ns_subtract(&difference, *other, *instant);
    JS_FreeValue(ctx, converted);
    if (ret)
        return JS_ThrowRangeError(ctx, "Temporal.Instant difference overflow");
    options = js_temporal_get_options(ctx, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (JS_IsException(options))
        return options;
    ret = js_temporal_get_difference_settings(ctx, options, since,
        QJS_TEMPORAL_HOUR, QJS_TEMPORAL_NANOSECOND,
        QJS_TEMPORAL_NANOSECOND, QJS_TEMPORAL_SECOND, &settings);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    increment = qjs_temporal_unit_nanoseconds(settings.smallest_unit) *
                settings.rounding_increment;
    if (qjs_temporal_epoch_ns_round(&internal.time, difference, increment,
                                     settings.rounding_mode))
        return JS_ThrowRangeError(ctx, "Temporal.Instant difference overflow");
    if (since && qjs_temporal_epoch_ns_subtract(&internal.time,
                    qjs_temporal_epoch_ns_from_int64(0), internal.time))
        return JS_ThrowRangeError(ctx, "Temporal.Instant difference overflow");
    if (qjs_temporal_duration_from_internal(&duration, internal,
                                             settings.largest_unit))
        return JS_ThrowRangeError(ctx, "Temporal duration is outside the valid range");
    return js_temporal_create_duration(ctx, JS_UNDEFINED, &duration);
}

static JSValue js_temporal_instant_round(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSTemporalInstantData *instant;
    QJSTemporalEpochNs result;
    QJSTemporalRoundingMode mode;
    QJSTemporalUnit unit;
    JSValue options;
    uint32_t increment;
    uint64_t unit_length;
    int ret;

    instant = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return JS_EXCEPTION;
    if (JS_IsUndefined(argv[0]))
        return JS_ThrowTypeError(ctx, "Temporal round options are required");
    if (JS_IsString(argv[0])) {
        options = JS_NewObjectProto(ctx, JS_NULL);
        if (JS_IsException(options))
            return options;
        if (JS_DefinePropertyValueStr(ctx, options, "smallestUnit",
                                      JS_DupValue(ctx, argv[0]),
                                      JS_PROP_C_W_E) < 0) {
            JS_FreeValue(ctx, options);
            return JS_EXCEPTION;
        }
    } else {
        options = js_temporal_get_options(ctx, argv[0]);
        if (JS_IsException(options))
            return options;
    }
    ret = js_temporal_get_rounding_increment(ctx, options, &increment) ||
          js_temporal_get_rounding_mode(ctx, options,
                                       QJS_TEMPORAL_ROUND_HALF_EXPAND, &mode) ||
          js_temporal_get_unit_option(ctx, options, "smallestUnit", TRUE, &unit);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    if (unit < QJS_TEMPORAL_HOUR || unit > QJS_TEMPORAL_NANOSECOND)
        return JS_ThrowRangeError(ctx, "Temporal.Instant rounding requires time units");
    unit_length = qjs_temporal_unit_nanoseconds(unit);
    if (js_temporal_validate_increment(ctx, increment,
                      UINT64_C(86400000000000) / unit_length, TRUE))
        return JS_EXCEPTION;
    if (qjs_temporal_epoch_ns_round_as_if_positive(&result, *instant,
              increment * unit_length, mode) ||
        !qjs_temporal_epoch_ns_is_valid(result))
        return JS_ThrowRangeError(ctx, "Temporal.Instant is outside the valid range");
    return js_temporal_create_instant(ctx, JS_UNDEFINED, result);
}

static JSValue js_temporal_instant_format(JSContext *ctx,
                                         QJSTemporalEpochNs epoch,
                                         const JSTemporalTimeZone *zone,
                                         int precision)
{
    QJSTemporalISODateTime datetime;
    int64_t offset;
    char buffer[96];
    int length, suffix;

    if (zone) {
        if (js_temporal_time_zone_datetime(ctx, zone, epoch, &datetime) ||
            js_temporal_time_zone_offset(ctx, zone, epoch, &offset))
            return JS_EXCEPTION;
    } else {
        if (qjs_temporal_iso_datetime_from_epoch_ns(&datetime, epoch))
            return JS_ThrowRangeError(ctx, "Temporal ISO date is outside the valid range");
        offset = 0;
    }
    length = qjs_temporal_format_date_time(buffer, sizeof(buffer), datetime,
                                          precision);
    if (length < 0)
        return JS_ThrowRangeError(ctx, "invalid Temporal.Instant date fields");
    if (zone) {
        suffix = qjs_temporal_format_utc_offset(buffer + length,
                    sizeof(buffer) - length, offset, TRUE);
        if (suffix < 0)
            return JS_ThrowRangeError(ctx, "invalid Temporal time zone offset");
        length += suffix;
    } else {
        buffer[length++] = 'Z';
    }
    return JS_NewStringLen(ctx, buffer, length);
}

static JSValue js_temporal_instant_to_string(JSContext *ctx,
                                            JSValueConst this_val,
                                            int argc, JSValueConst *argv)
{
    JSTemporalInstantData *instant;
    QJSTemporalEpochNs rounded;
    QJSTemporalRoundingMode mode;
    QJSTemporalUnit unit;
    QJSTemporalStringPrecision precision;
    JSTemporalTimeZone zone;
    JSValue options, time_zone, result;
    int digits, ret;

    instant = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return JS_EXCEPTION;
    options = js_temporal_get_options(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(options))
        return options;
    ret = js_temporal_get_fractional_digits(ctx, options, &digits) ||
          js_temporal_get_rounding_mode(ctx, options,
                                       QJS_TEMPORAL_ROUND_TRUNC, &mode) ||
          js_temporal_get_unit_option(ctx, options, "smallestUnit", FALSE, &unit);
    if (ret) {
        JS_FreeValue(ctx, options);
        return JS_EXCEPTION;
    }
    time_zone = JS_GetPropertyStr(ctx, options, "timeZone");
    JS_FreeValue(ctx, options);
    if (JS_IsException(time_zone))
        return time_zone;
    if (qjs_temporal_string_precision(&precision, unit, digits)) {
        JS_FreeValue(ctx, time_zone);
        return JS_ThrowRangeError(ctx, "invalid Temporal.Instant string precision");
    }
    zone.identifier = JS_UNDEFINED;
    if (!JS_IsUndefined(time_zone) &&
        js_temporal_to_time_zone(ctx, time_zone, &zone)) {
        JS_FreeValue(ctx, time_zone);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, time_zone);
    if (qjs_temporal_epoch_ns_round_as_if_positive(&rounded, *instant,
                qjs_temporal_unit_nanoseconds(precision.unit) *
                precision.increment, mode) ||
        !qjs_temporal_epoch_ns_is_valid(rounded)) {
        js_temporal_free_time_zone(ctx, &zone);
        return JS_ThrowRangeError(ctx, "Temporal.Instant is outside the valid range");
    }
    result = js_temporal_instant_format(ctx, rounded,
                JS_IsUndefined(zone.identifier) ? NULL : &zone,
                precision.precision);
    js_temporal_free_time_zone(ctx, &zone);
    return result;
}

static JSValue js_temporal_instant_to_json(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSTemporalInstantData *instant;

    instant = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return JS_EXCEPTION;
    return js_temporal_instant_format(ctx, *instant, NULL, -1);
}

static JSValue js_temporal_instant_to_locale_string(JSContext *ctx,
                                                   JSValueConst this_val,
                                                   int argc,
                                                   JSValueConst *argv)
{
    JSTemporalInstantData *instant;

    instant = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return JS_EXCEPTION;
#ifdef CONFIG_INTL
    return js_intl_temporal_to_locale_string(ctx, this_val,
                argc > 0 ? argv[0] : JS_UNDEFINED,
                argc > 1 ? argv[1] : JS_UNDEFINED);
#else
    return js_temporal_instant_format(ctx, *instant, NULL, -1);
#endif
}

static JSValue js_temporal_instant_to_zoned(JSContext *ctx,
                                           JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    JSTemporalInstantData *instant;
    JSTemporalTimeZone zone;
    JSValue result;

    instant = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_INSTANT);
    if (!instant)
        return JS_EXCEPTION;
    if (js_temporal_to_time_zone(ctx, argv[0], &zone))
        return JS_EXCEPTION;
    result = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, *instant,
                                               &zone, QJS_TEMPORAL_CAL_ISO8601);
    js_temporal_free_time_zone(ctx, &zone);
    return result;
}

static const JSCFunctionListEntry js_temporal_instant_funcs[] = {
    JS_CFUNC_DEF("from", 1, js_temporal_instant_from),
    JS_CFUNC_DEF("compare", 2, js_temporal_instant_compare),
    JS_CFUNC_DEF("fromEpochMilliseconds", 1,
                 js_temporal_instant_from_epoch_milliseconds),
    JS_CFUNC_DEF("fromEpochNanoseconds", 1,
                 js_temporal_instant_from_epoch_nanoseconds),
};

static const JSCFunctionListEntry js_temporal_instant_proto_funcs[] = {
    JS_CGETSET_DEF("epochMilliseconds", js_temporal_instant_epoch_milliseconds,
                  NULL),
    JS_CGETSET_DEF("epochNanoseconds", js_temporal_instant_epoch_nanoseconds,
                  NULL),
    JS_CFUNC_DEF("equals", 1, js_temporal_instant_equals),
    JS_CFUNC_MAGIC_DEF("add", 1, js_temporal_instant_add, 0),
    JS_CFUNC_MAGIC_DEF("subtract", 1, js_temporal_instant_add, 1),
    JS_CFUNC_MAGIC_DEF("until", 1, js_temporal_instant_difference, 0),
    JS_CFUNC_MAGIC_DEF("since", 1, js_temporal_instant_difference, 1),
    JS_CFUNC_DEF("round", 1, js_temporal_instant_round),
    JS_CFUNC_DEF("toString", 0, js_temporal_instant_to_string),
    JS_CFUNC_DEF("toJSON", 0, js_temporal_instant_to_json),
    JS_CFUNC_DEF("toLocaleString", 0, js_temporal_instant_to_locale_string),
    JS_CFUNC_DEF("toZonedDateTimeISO", 1, js_temporal_instant_to_zoned),
    JS_CFUNC_DEF("valueOf", 0, js_temporal_instant_value_of),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.Instant",
                       JS_PROP_CONFIGURABLE),
};

int js_temporal_init_instant(JSContext *ctx, JSValueConst namespace_object)
{
    JSValue ctor;

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_TEMPORAL_INSTANT) &&
        JS_NewClass(ctx->rt, JS_CLASS_TEMPORAL_INSTANT,
                    &js_temporal_instant_class) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    ctor = JS_NewCConstructor(ctx, JS_CLASS_TEMPORAL_INSTANT, "Instant",
                              js_temporal_instant_constructor, 1,
                              JS_CFUNC_constructor, 0, JS_UNDEFINED,
                              js_temporal_instant_funcs,
                              countof(js_temporal_instant_funcs),
                              js_temporal_instant_proto_funcs,
                              countof(js_temporal_instant_proto_funcs),
                              JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(ctor))
        return -1;
    if (JS_HasException(ctx)) {
        JS_FreeValue(ctx, ctor);
        return -1;
    }
    return JS_DefinePropertyValueStr(ctx, namespace_object, "Instant", ctor,
                JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0 ? -1 : 0;
}
