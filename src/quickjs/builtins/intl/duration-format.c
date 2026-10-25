/* DurationFormat observable options, duration conversion and native ownership.
 * ECMA-402 PR1044 d026068ed653130a37d79a95c0122ecf5319f1a4, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../intl.h"
#include "../duration-value.h"
#include "../../internal/runtime.h"
#include <stdio.h>

static const char *const styles[] = {"long", "short", "narrow", "digital"};
static const char *const unit_styles[] = {"long", "short", "narrow", "numeric", "2-digit"};
static const char *const displays[] = {"auto", "always"};
static const char *const fields[] = {"years", "months", "weeks", "days", "hours", "minutes", "seconds",
                                    "milliseconds", "microseconds", "nanoseconds"};
static const char *const units[] = {"year", "month", "week", "day", "hour", "minute", "second",
                                   "millisecond", "microsecond", "nanosecond"};
static const char *const part_names[QJS_INTL_PART_NUMBER_COUNT] = {
    "literal", "element", "integer", "group", "decimal", "fraction", "nan", "infinity",
    "minusSign", "plusSign", "percentSign", "currency", "exponentSeparator", "exponentMinusSign",
    "exponentInteger", "compact", "unit", "approximatelySign"};

typedef struct JSIntlDurationFormat {
    QJSIntlDurationFormatter *formatter;
    QJSIntlDurationOptions options;
    char *locale, *numbering_system;
} JSIntlDurationFormat;

typedef struct DurationCapabilities {
    const QJSIntlDurationService *service;
    size_t locale;
} DurationCapabilities;

static QJSIntlStatus system_supported(void *opaque, QJSIntlBytes name, int *out)
{
    DurationCapabilities *capabilities = opaque;
    return qjs_intl_duration_system_supported(capabilities->service, capabilities->locale, name, out);
}

static QJSIntlDurationService *duration_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_duration) {
        const QJSIntlData *data = js_intl_data(ctx);
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlDurationService *service;

        if (!data)
            return NULL;
        if (js_intl_error(ctx, qjs_intl_duration_open(&allocator, data, &service), "duration initialization"))
            return NULL;
        rt->intl_duration = service;
    }
    return rt->intl_duration;
}

static void duration_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlDurationFormat *state = JS_GetOpaque(value, JS_CLASS_INTL_DURATION_FORMAT);
    if (state) {
        qjs_intl_duration_formatter_close(state->formatter);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state->numbering_system);
        js_free_rt(rt, state);
    }
}

static int unit_options(JSContext *ctx, JSValueConst object, int two_digit_hours,
                        QJSIntlDurationOptions *out)
{
    QJSIntlDurationUnitStyle previous = QJS_INTL_DURATION_UNIT_UNSET;
    unsigned int unit;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        char display_name[32];
        int count = unit < QJS_TEMPORAL_HOUR ? 3 : unit <= QJS_TEMPORAL_SECOND ? 5 : 4;
        int style, display;
        style = js_intl_string_option(ctx, object, fields[unit], unit_styles, count, count);
        if (style < 0)
            return -1;
        snprintf(display_name, sizeof(display_name), "%sDisplay", fields[unit]);
        display = js_intl_string_option(ctx, object, display_name, displays, countof(displays), countof(displays));
        if (display < 0)
            return -1;
        if (qjs_intl_duration_unit_options(unit, out->style,
                style == count ? QJS_INTL_DURATION_UNIT_UNSET : (QJSIntlDurationUnitStyle)style,
                display == countof(displays) ? QJS_INTL_DURATION_DISPLAY_UNSET : (QJSIntlDurationDisplay)display,
                previous, two_digit_hours, &out->units[unit])) {
            JS_ThrowRangeError(ctx, "invalid duration %s style/display transition", fields[unit]);
            return -1;
        }
        if (unit >= QJS_TEMPORAL_HOUR && unit < QJS_TEMPORAL_NANOSECOND)
            previous = out->units[unit].style;
    }
    return js_intl_get_number_option(ctx, object, "fractionalDigits", 0, 9, -1, &out->fractional_digits);
}

static JSValue duration_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                                    JSValueConst *argv)
{
    JSIntlLocaleList requested = {0};
    JSIntlResolvedKeyword resolved = {0};
    QJSIntlLocaleMatch match;
    QJSIntlDurationService *service;
    QJSIntlDurationOptions native = {.maximum_output_length = JS_STRING_LEN_MAX};
    const QJSIntlBytes *available;
    QJSIntlBytes option, default_system;
    const char *owned_option = NULL;
    DurationCapabilities capabilities;
    JSIntlDurationFormat *state;
    size_t count;
    int style, two_digit_hours;
    JSValue object, options = JS_UNDEFINED, result = JS_EXCEPTION;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.DurationFormat requires new");
    object = js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_DURATION_FORMAT);
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto done;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, FALSE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0 ||
        js_intl_unicode_type_option(ctx, options, "numberingSystem", &option, &owned_option))
        goto done;
    service = duration_service(ctx);
    if (!service || js_intl_error(ctx, qjs_intl_duration_available(service, &available, &count), "duration locales") ||
        js_intl_resolve_locale_match(ctx, available, count, &requested, &match) ||
        js_intl_error(ctx, qjs_intl_duration_default_system(service, match.available_index, &default_system), "numbering system") ||
        js_intl_error(ctx, qjs_intl_duration_two_digit_hours(service, match.available_index, &two_digit_hours), "duration hour policy"))
        goto done;
    capabilities = (DurationCapabilities){service, match.available_index};
    if (js_intl_resolve_locale_key(ctx, available[match.available_index], match.extension,
                                     "nu", default_system, option, system_supported, &capabilities, &resolved))
        goto done;
    style = js_intl_string_option(ctx, options, "style", styles, countof(styles), QJS_INTL_DURATION_SHORT);
    if (style < 0)
        goto done;
    native.style = style;
    if (unit_options(ctx, options, two_digit_hours, &native))
        goto done;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto done;
    state->options = native;
    state->locale = resolved.locale;
    state->numbering_system = resolved.value;
    resolved = (JSIntlResolvedKeyword){0};
    JS_SetOpaque(object, state);
    if (js_intl_error(ctx, qjs_intl_duration_formatter_open(service, match.available_index,
                                (QJSIntlBytes){state->numbering_system, strlen(state->numbering_system)},
                                &native, &state->formatter), "DurationFormat"))
        goto done;
    result = object;
    object = JS_UNDEFINED;
done:
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, options);
    JS_FreeCString(ctx, owned_option);
    js_free(ctx, resolved.locale);
    js_free(ctx, resolved.value);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return result;
}

static JSValue duration_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlDurationService *service = duration_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service || js_intl_error(ctx, qjs_intl_duration_available(service, &available, &count), "duration locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                  argc > 1 ? argv[1] : JS_UNDEFINED);
}

static int property(JSContext *ctx, JSValueConst object, const char *name, JSValue value)
{
    if (JS_IsException(value))
        return -1;
    return JS_DefinePropertyValueStr(ctx, object, name, value, JS_PROP_C_W_E | JS_PROP_THROW);
}

static JSValue duration_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlDurationFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DURATION_FORMAT);
    JSValue result;
    unsigned int unit;
    if (!state)
        return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    if (property(ctx, result, "locale", JS_NewString(ctx, state->locale)) < 0 ||
        property(ctx, result, "numberingSystem", JS_NewString(ctx, state->numbering_system)) < 0 ||
        property(ctx, result, "style", JS_NewString(ctx, styles[state->options.style])) < 0)
        goto fail;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        QJSIntlDurationUnitOptions options = state->options.units[unit];
        char display_name[32];
        int style = options.style == QJS_INTL_DURATION_UNIT_FRACTIONAL ? QJS_INTL_DURATION_UNIT_NUMERIC : options.style;
        snprintf(display_name, sizeof(display_name), "%sDisplay", fields[unit]);
        if (property(ctx, result, fields[unit], JS_NewString(ctx, unit_styles[style])) < 0 ||
            property(ctx, result, display_name, JS_NewString(ctx, displays[options.display])) < 0)
            goto fail;
    }
    if (state->options.fractional_digits >= 0 &&
        property(ctx, result, "fractionalDigits", JS_NewInt32(ctx, state->options.fractional_digits)) < 0)
        goto fail;
    return result;
fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static JSValue duration_parts(JSContext *ctx, const QJSIntlDurationFormatted *formatted)
{
    JSValue array;
    size_t i, end = 0;
    if (formatted->part_count > UINT32_MAX)
        return JS_ThrowRangeError(ctx, "too many duration parts");
    array = JS_NewArray(ctx);
    if (JS_IsException(array))
        return array;
    for (i = 0; i < formatted->part_count; i++) {
        const QJSIntlDurationPart *span = &formatted->parts[i];
        JSValue part;
        if (span->start != end || span->end < end || span->end > formatted->length ||
            (unsigned int)span->type >= QJS_INTL_PART_NUMBER_COUNT || span->type == QJS_INTL_PART_ELEMENT ||
            (unsigned int)span->unit > QJS_TEMPORAL_UNIT_COUNT)
            goto invalid;
        end = span->end;
        part = JS_NewObject(ctx);
        if (JS_IsException(part))
            goto fail;
        if (property(ctx, part, "type", JS_NewString(ctx, part_names[span->type])) < 0 ||
            property(ctx, part, "value", JS_NewStringUTF16(ctx, formatted->text + span->start, span->end - span->start)) < 0 ||
            (span->unit != QJS_TEMPORAL_UNIT_COUNT && property(ctx, part, "unit", JS_NewString(ctx, units[span->unit])) < 0)) {
            JS_FreeValue(ctx, part);
            goto fail;
        }
        if (JS_DefinePropertyValueUint32(ctx, array, (uint32_t)i, part, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
    }
    if (end != formatted->length)
        goto invalid;
    return array;
invalid:
    JS_ThrowInternalError(ctx, "invalid duration parts");
fail:
    JS_FreeValue(ctx, array);
    return JS_EXCEPTION;
}

static JSValue duration_format(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv, int parts)
{
    JSIntlDurationFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DURATION_FORMAT);
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlDurationFormatted formatted = {0};
    QJSTemporalDuration duration;
    JSValue result = JS_EXCEPTION;
    if (!state || js_duration_to_record(ctx, argc ? argv[0] : JS_UNDEFINED, &duration))
        return result;
    if (!js_intl_error(ctx, qjs_intl_duration_format(state->formatter, &duration, &formatted), "duration formatting"))
        result = parts ? duration_parts(ctx, &formatted) : JS_NewStringUTF16(ctx, formatted.text, formatted.length);
    qjs_intl_duration_result_clear(&allocator, &formatted);
    return result;
}

JSValue js_intl_format_duration(JSContext *ctx, JSValueConst value, int argc, JSValueConst *argv)
{
    JSValue formatter, result;
    if (js_intl_ensure_intrinsics(ctx))
        return JS_EXCEPTION;
    formatter = duration_constructor(ctx, ctx->intl_duration_ctor, argc, argv);
    if (JS_IsException(formatter))
        return formatter;
    result = duration_format(ctx, formatter, 1, &value, 0);
    JS_FreeValue(ctx, formatter);
    return result;
}

static const JSCFunctionListEntry duration_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, duration_supported),
};
static const JSCFunctionListEntry duration_properties[] = {
    JS_CFUNC_MAGIC_DEF("format", 1, duration_format, 0),
    JS_CFUNC_MAGIC_DEF("formatToParts", 1, duration_format, 1),
    JS_CFUNC_DEF("resolvedOptions", 0, duration_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.DurationFormat", JS_PROP_CONFIGURABLE),
};

int js_intl_init_duration_format(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.DurationFormat",
        .finalizer = duration_finalizer,
    };
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_DURATION_FORMAT) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_DURATION_FORMAT, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_DURATION_FORMAT, "DurationFormat",
                                     duration_constructor, 0, JS_CFUNC_constructor, 0, JS_UNDEFINED,
                                     duration_statics, countof(duration_statics), duration_properties,
                                     countof(duration_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    ctx->intl_duration_ctor = JS_DupValue(ctx, constructor);
    return JS_DefinePropertyValueStr(ctx, namespace_object, "DurationFormat", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
}
