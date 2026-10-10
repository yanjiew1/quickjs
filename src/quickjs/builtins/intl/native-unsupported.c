/* Explicitly unsupported native development services. No ICU fallback.
 * Copyright (c) 2026 Yan-Jie Wang. */
#include "intl-internal.h"
#include "locale-integration.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
static JSValue unsupported(JSContext *ctx, const char *service)
{
    js_intl_native_error(ctx, QJS_INTL_UNSUPPORTED, service);
    return JS_EXCEPTION;
}
static JSValue unsupported_locales(JSContext *ctx, JSValueConst receiver,
                                    int argc, JSValueConst *argv)
{
    return unsupported(ctx, "AvailableLocales for this service");
}
static const JSCFunctionListEntry unsupported_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, unsupported_locales),
};
#define NATIVE_UNSUPPORTED(module, class_id, name, length, proto) \
static JSValue module##_constructor(JSContext *ctx, JSValueConst target, \
                                    int argc, JSValueConst *argv) \
{ \
    return unsupported(ctx, name); \
} \
static const JSClassDef module##_class = { "Intl." name }; \
int js_intl_init_##module(JSContext *ctx, JSValueConst intl) \
{ \
    return js_intl_init_constructor(ctx, intl, class_id, &module##_class, \
        name, module##_constructor, length, proto, unsupported_static, \
        countof(unsupported_static), NULL, 0); \
}
NATIVE_UNSUPPORTED(date_time_format, JS_CLASS_INTL_DATE_TIME_FORMAT, "DateTimeFormat", 0,
                   JS_CFUNC_constructor_or_func)
NATIVE_UNSUPPORTED(plural_rules, JS_CLASS_INTL_PLURAL_RULES, "PluralRules", 0,
                   JS_CFUNC_constructor)
NATIVE_UNSUPPORTED(relative_time_format, JS_CLASS_INTL_RELATIVE_TIME_FORMAT, "RelativeTimeFormat", 0,
                   JS_CFUNC_constructor)
NATIVE_UNSUPPORTED(display_names, JS_CLASS_INTL_DISPLAY_NAMES, "DisplayNames", 2,
                   JS_CFUNC_constructor)
NATIVE_UNSUPPORTED(duration_format, JS_CLASS_INTL_DURATION_FORMAT, "DurationFormat", 0,
                   JS_CFUNC_constructor)
#undef NATIVE_UNSUPPORTED

JSValue js_intl_supported_values_of(JSContext *ctx, JSValueConst receiver,
                                   int argc, JSValueConst *argv)
{
    const char *key;
    size_t n, i;
    static const char *const keys[] = { "calendar", "collation", "currency",
        "numberingSystem", "timeZone", "unit" };
    key = JS_ToCStringLen(ctx, &n, argc ? argv[0] : JS_UNDEFINED);
    if (!key) return JS_EXCEPTION;
    for (i = 0; i < countof(keys); i++) {
        if (strlen(keys[i]) == n && !memcmp(keys[i], key, n)) {
            JS_FreeCString(ctx, key);
            return unsupported(ctx, "supportedValuesOf");
        }
    }
    JS_FreeCString(ctx, key);
    return JS_ThrowRangeError(ctx, "invalid Intl enumeration key");
}
JSValue js_intl_string_locale_case(JSContext *ctx, JSValueConst value,
                                   int argc, JSValueConst *argv, int lower)
{
    JSValue string = JS_ToStringCheckObject(ctx, value);
    if (JS_IsException(string)) return JS_EXCEPTION;
    JS_FreeValue(ctx, string);
    return unsupported(ctx, "locale case conversion");
}
JSValue js_intl_date_format(JSContext *ctx, double time,
                            JSValueConst locales, JSValueConst options,
                            int required, int defaults)
{
    return unsupported(ctx, "DateTimeFormat");
}
#ifdef CONFIG_TEMPORAL
JSValue js_intl_temporal_duration_to_locale_string(JSContext *ctx,
    JSValueConst duration, JSValueConst locales, JSValueConst options)
{
    return unsupported(ctx, "DurationFormat");
}
JSValue js_intl_temporal_to_locale_string(JSContext *ctx, JSValueConst value,
    JSValueConst locales, JSValueConst options)
{
    return unsupported(ctx, "Temporal DateTimeFormat");
}
#endif
#endif
