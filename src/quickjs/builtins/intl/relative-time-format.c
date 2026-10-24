/* RelativeTimeFormat observable coercion, locale options and native ownership.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../../internal/runtime.h"
#include <math.h>

static const char *const widths[] = {"long", "short", "narrow"};
static const char *const numerics[] = {"always", "auto"};
static const char *const units[] = {"second", "minute", "hour", "day", "week", "month", "quarter", "year"};
static const char *const part_names[QJS_INTL_PART_NUMBER_COUNT] = {
    "literal", "element", "integer", "group", "decimal", "fraction", "nan", "infinity",
    "minusSign", "plusSign", "percentSign", "currency", "exponentSeparator", "exponentMinusSign",
    "exponentInteger", "compact", "unit", "approximatelySign"};

typedef struct JSIntlRelativeTimeFormat {
    QJSIntlRelativeFormatter *formatter;
    char *locale, *numbering_system;
    int width, numeric;
} JSIntlRelativeTimeFormat;

typedef struct RelativeCapabilities {
    const QJSIntlRelativeService *service;
    size_t locale;
} RelativeCapabilities;

static QJSIntlStatus system_supported(void *opaque, QJSIntlBytes name, int *out)
{
    RelativeCapabilities *capabilities = opaque;
    return qjs_intl_relative_system_supported(capabilities->service, capabilities->locale, name, out);
}

QJSIntlRelativeService *js_intl_relative_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_relative) {
        const QJSIntlData *data = js_intl_data(ctx);
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlRelativeService *service;

        if (!data)
            return NULL;
        if (js_intl_error(ctx, qjs_intl_relative_open(&allocator, data, &service), "relative initialization"))
            return NULL;
        rt->intl_relative = service;
    }
    return rt->intl_relative;
}

static void relative_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlRelativeTimeFormat *state = JS_GetOpaque(value, JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    if (state) {
        qjs_intl_relative_formatter_close(state->formatter);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state->numbering_system);
        js_free_rt(rt, state);
    }
}

static JSValue relative_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                                    JSValueConst *argv)
{
    JSIntlLocaleList requested = {0};
    JSIntlResolvedKeyword resolved = {0};
    QJSIntlLocaleMatch match;
    QJSIntlRelativeService *service;
    QJSIntlRelativeOptions native;
    const QJSIntlBytes *available;
    QJSIntlBytes option, default_system;
    const char *owned_option = NULL;
    RelativeCapabilities capabilities;
    JSIntlRelativeTimeFormat *state;
    size_t count;
    int width, numeric;
    JSValue object, options = JS_UNDEFINED, result = JS_EXCEPTION;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.RelativeTimeFormat requires new");
    object = js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto done;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, TRUE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0 ||
        js_intl_unicode_type_option(ctx, options, "numberingSystem", &option, &owned_option))
        goto done;
    service = js_intl_relative_service(ctx);
    if (!service || js_intl_error(ctx, qjs_intl_relative_available(service, &available, &count),
                                    "relative locales") ||
        js_intl_resolve_locale_match(ctx, available, count, &requested, &match) ||
        js_intl_error(ctx, qjs_intl_relative_default_system(service, match.available_index,
                                                             &default_system), "numbering system"))
        goto done;
    capabilities = (RelativeCapabilities){service, match.available_index};
    if (js_intl_resolve_locale_key(ctx, available[match.available_index], match.extension,
                                     "nu", default_system, option, system_supported, &capabilities,
                                     &resolved))
        goto done;
    width = js_intl_string_option(ctx, options, "style", widths, countof(widths), 0);
    if (width < 0)
        goto done;
    numeric = js_intl_string_option(ctx, options, "numeric", numerics, countof(numerics), 0);
    if (numeric < 0)
        goto done;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto done;
    state->width = width;
    state->numeric = numeric;
    state->locale = resolved.locale;
    state->numbering_system = resolved.value;
    resolved = (JSIntlResolvedKeyword){0};
    JS_SetOpaque(object, state);
    native = (QJSIntlRelativeOptions){width, numeric, JS_STRING_LEN_MAX};
    if (js_intl_error(ctx, qjs_intl_relative_formatter_open(service, match.available_index,
                               (QJSIntlBytes){state->numbering_system, strlen(state->numbering_system)},
                               &native, &state->formatter), "RelativeTimeFormat"))
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

static JSValue relative_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlRelativeService *service = js_intl_relative_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service || js_intl_error(ctx, qjs_intl_relative_available(service, &available, &count), "relative locales"))
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

static JSValue relative_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlRelativeTimeFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    JSValue result;
    if (!state)
        return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    if (property(ctx, result, "locale", JS_NewString(ctx, state->locale)) < 0 ||
        property(ctx, result, "style", JS_NewString(ctx, widths[state->width])) < 0 ||
        property(ctx, result, "numeric", JS_NewString(ctx, numerics[state->numeric])) < 0 ||
        property(ctx, result, "numberingSystem", JS_NewString(ctx, state->numbering_system)) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}

static JSValue relative_parts(JSContext *ctx, const QJSIntlRelativeFormatted *formatted)
{
    JSValue array;
    size_t i, end = 0;
    if (formatted->part_count > UINT32_MAX)
        return JS_ThrowRangeError(ctx, "too many relative-time parts");
    array = JS_NewArray(ctx);
    if (JS_IsException(array))
        return array;
    for (i = 0; i < formatted->part_count; i++) {
        const QJSIntlRelativePart *span = &formatted->parts[i];
        JSValue part;
        if (span->start != end || span->end < end || span->end > formatted->length ||
            (unsigned int)span->type >= QJS_INTL_PART_NUMBER_COUNT || span->type == QJS_INTL_PART_ELEMENT ||
            (unsigned int)span->unit > QJS_INTL_RELATIVE_UNIT_COUNT)
            goto invalid;
        end = span->end;
        part = JS_NewObject(ctx);
        if (JS_IsException(part))
            goto fail;
        if (property(ctx, part, "type", JS_NewString(ctx, part_names[span->type])) < 0 ||
            property(ctx, part, "value", JS_NewStringUTF16(ctx, formatted->text + span->start,
                                                           span->end - span->start)) < 0 ||
            (span->unit != QJS_INTL_RELATIVE_UNIT_COUNT &&
             property(ctx, part, "unit", JS_NewString(ctx, units[span->unit])) < 0)) {
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
    JS_ThrowInternalError(ctx, "invalid relative-time parts");
fail:
    JS_FreeValue(ctx, array);
    return JS_EXCEPTION;
}

static JSValue relative_format(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv, int parts)
{
    JSIntlRelativeTimeFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlRelativeFormatted formatted = {0};
    const char *unit = NULL;
    JSValue result = JS_EXCEPTION;
    size_t length, index;
    double value;
    if (!state)
        return result;
    if (JS_ToFloat64(ctx, &value, argc ? argv[0] : JS_UNDEFINED))
        goto done;
    unit = JS_ToCStringLen(ctx, &length, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (!unit)
        goto done;
    if (!isfinite(value)) {
        JS_ThrowRangeError(ctx, "relative-time value must be finite");
        goto done;
    }
    for (index = 0; index < countof(units); index++) {
        size_t n = strlen(units[index]);
        if ((length == n || (length == n + 1 && unit[n] == 's')) && !memcmp(unit, units[index], n))
            break;
    }
    if (index == countof(units)) {
        JS_ThrowRangeError(ctx, "invalid relative-time unit");
        goto done;
    }
    if (js_intl_error(ctx, qjs_intl_relative_format(state->formatter, value, (QJSIntlRelativeUnit)index, &formatted), "relative formatting"))
        goto done;
    result = parts ? relative_parts(ctx, &formatted)
                   : JS_NewStringUTF16(ctx, formatted.text, formatted.length);
done:
    JS_FreeCString(ctx, unit);
    qjs_intl_relative_result_clear(&allocator, &formatted);
    return result;
}

static const JSCFunctionListEntry relative_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, relative_supported),
};
static const JSCFunctionListEntry relative_properties[] = {
    JS_CFUNC_MAGIC_DEF("format", 2, relative_format, 0),
    JS_CFUNC_MAGIC_DEF("formatToParts", 2, relative_format, 1),
    JS_CFUNC_DEF("resolvedOptions", 0, relative_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.RelativeTimeFormat", JS_PROP_CONFIGURABLE),
};

int js_intl_init_relative_time_format(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.RelativeTimeFormat",
        .finalizer = relative_finalizer,
    };
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_RELATIVE_TIME_FORMAT) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_RELATIVE_TIME_FORMAT, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_RELATIVE_TIME_FORMAT, "RelativeTimeFormat",
                                     relative_constructor, 0, JS_CFUNC_constructor, 0, JS_UNDEFINED,
                                     relative_statics, countof(relative_statics), relative_properties,
                                     countof(relative_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    return JS_DefinePropertyValueStr(ctx, namespace_object, "RelativeTimeFormat", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
}
