/* Native DisplayNames frontend. Copyright (c) 2026 Yan-Jie Wang.
 * ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, reviewed 2026-10-09.
 * sec-intl.displaynames, sec-intl.displaynames.prototype.of. */
#include "intl-service-common.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../../../intl/provider-native-display.h"
typedef struct JSIntlDisplayNames {
    char *locale;
    QJSIntlNativeDisplayNames *names;
    QJSIntlAllocator allocator;
    int style, type, fallback, language_display;
} JSIntlDisplayNames;
static const char *const display_types[] = {
    "language", "region", "script", "currency", "calendar", "dateTimeField"
};
static const char *const display_fallback[] = { "code", "none" };
static const char *const display_language[] = { "dialect", "standard" };
static void js_intl_display_finalizer(JSRuntime *rt, JSValue obj)
{
    JSIntlDisplayNames *s = JS_GetOpaque(obj, JS_CLASS_INTL_DISPLAY_NAMES);
    if (s) {
        qjs_intl_native_display_names_close(s->names);
        js_free_rt(rt, s->locale);
        js_free_rt(rt, s);
    }
}
static JSValue js_intl_display_constructor(JSContext *ctx, JSValueConst target,
                                          int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlResolvedLocale resolved = {0};
    JSIntlDisplayNames *s;
    QJSIntlProvider *provider;
    QJSIntlDisplayNamesOptions native;
    QJSIntlStatus status;
    int matcher;
    if (JS_IsUndefined(target))
        return JS_ThrowTypeError(ctx, "Intl.DisplayNames requires new");
    obj = js_intl_new_object(ctx, target, JS_CLASS_INTL_DISPLAY_NAMES);
    if (JS_IsException(obj)) goto fail;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) goto fail;
    JS_SetOpaque(obj, s);
    if (js_intl_service_options(ctx, argc ? argv[0] : JS_UNDEFINED,
            argc > 1 ? argv[1] : JS_UNDEFINED, 2, &requested,
            &options, &matcher) < 0 ||
        js_intl_resolve_locale(ctx, JS_INTL_DISPLAY_NAMES, &requested,
            js_intl_matchers[matcher], NULL, 0, &resolved) < 0 ||
        js_intl_get_string_option(ctx, options, "style", js_intl_styles,
            countof(js_intl_styles), 0, &s->style) < 0 ||
        js_intl_get_string_option(ctx, options, "type", display_types,
            countof(display_types), -1, &s->type) < 0) goto fail;
    if (s->type < 0) {
        JS_ThrowTypeError(ctx, "Intl.DisplayNames requires type");
        goto fail;
    }
    if (js_intl_get_string_option(ctx, options, "fallback", display_fallback,
            countof(display_fallback), 0, &s->fallback) < 0 ||
        js_intl_get_string_option(ctx, options, "languageDisplay", display_language,
            countof(display_language), 0, &s->language_display) < 0) goto fail;
    provider = js_intl_native_provider(ctx);
    if (!provider) goto fail;
    s->allocator = *qjs_intl_native_provider_allocator(provider);
    s->locale = resolved.locale;
    resolved.locale = NULL;
    native.type = (QJSIntlDisplayNamesType)s->type;
    native.style = (QJSIntlDisplayNamesStyle)s->style;
    native.fallback_code = !s->fallback;
    native.language_dialect = !s->language_display;
    status = qjs_intl_native_provider_display_open(provider,
        (QJSIntlBytes){resolved.data_locale, strlen(resolved.data_locale)},
        &native, &s->names);
    if (js_intl_native_error(ctx, status, "DisplayNames")) goto fail;
    JS_FreeValue(ctx, options);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return obj;
fail:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, options);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return JS_EXCEPTION;
}
static JSValue js_intl_display_of(JSContext *ctx, JSValueConst receiver,
                                 int argc, JSValueConst *argv)
{
    JSIntlDisplayNames *s = JS_GetOpaque2(ctx, receiver, JS_CLASS_INTL_DISPLAY_NAMES);
    uint16_t *code = NULL;
    int32_t length;
    QJSIntlDisplayNamesResult out = {0};
    QJSIntlStatus status;
    JSValue result = JS_EXCEPTION;
    if (!s) return result;
    if (js_intl_to_utf16(ctx, argc ? argv[0] : JS_UNDEFINED, &code, &length) < 0)
        goto done;
    status = qjs_intl_native_display_names_of(s->names,
        (QJSIntlUTF16){code, (size_t)length}, &out);
    if (status == QJS_INTL_INVALID_ARGUMENT) {
        JS_ThrowRangeError(ctx, "invalid DisplayNames code");
        goto done;
    }
    if (js_intl_native_error(ctx, status, "DisplayNames.of")) goto done;
    result = out.present ? js_intl_from_utf16(ctx, out.text, out.length)
                         : JS_UNDEFINED;
done:
    js_free(ctx, code);
    qjs_intl_native_display_names_result_clear(&s->allocator, &out);
    return result;
}
static JSValue js_intl_display_resolved(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSIntlDisplayNames *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DISPLAY_NAMES);
    JSValue result;
    if (!s) return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result)) return result;
    if (js_intl_define_string(ctx, result, "locale", s->locale) < 0 ||
        js_intl_define_string(ctx, result, "style", js_intl_styles[s->style]) < 0 ||
        js_intl_define_string(ctx, result, "type", display_types[s->type]) < 0 ||
        js_intl_define_string(ctx, result, "fallback", display_fallback[s->fallback]) < 0 ||
        (s->type == 0 && js_intl_define_string(ctx, result, "languageDisplay",
            display_language[s->language_display]) < 0)) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}
static JSValue js_intl_display_supported(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_DISPLAY_NAMES,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef js_intl_display_class = {
    "Intl.DisplayNames", .finalizer = js_intl_display_finalizer,
};
static const JSCFunctionListEntry js_intl_display_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_display_supported),
};
static const JSCFunctionListEntry js_intl_display_prototype[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_display_resolved),
    JS_CFUNC_DEF("of", 1, js_intl_display_of),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.DisplayNames", JS_PROP_CONFIGURABLE),
};
int js_intl_init_display_names(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_DISPLAY_NAMES,
        &js_intl_display_class, "DisplayNames", js_intl_display_constructor, 2,
        JS_CFUNC_constructor, js_intl_display_static, countof(js_intl_display_static),
        js_intl_display_prototype, countof(js_intl_display_prototype));
}
#endif
