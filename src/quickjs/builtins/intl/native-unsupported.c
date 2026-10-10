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
#endif
