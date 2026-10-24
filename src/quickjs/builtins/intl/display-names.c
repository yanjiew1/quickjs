/* DisplayNames observable options, code canonicalization and native ownership.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../../internal/runtime.h"
#include "../../../intl/common/locale-grammar.h"
#include "../../../intl/datetime/field.h"
#include "../../../intl/locale/identifier.h"

static const char *const types[] = {"language", "region", "script", "currency", "calendar", "dateTimeField"};
static const char *const styles[] = {"long", "short", "narrow"};
static const char *const fallbacks[] = {"code", "none"};
static const char *const languages[] = {"dialect", "standard"};

typedef struct JSIntlDisplayNames {
    QJSIntlDisplayFormatter *formatter;
    char *locale;
    int type, style, fallback, language_display;
} JSIntlDisplayNames;

QJSIntlDisplayService *js_intl_display_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_display) {
        const QJSIntlData *data = js_intl_data(ctx);
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlDisplayService *service;

        if (!data)
            return NULL;
        if (js_intl_error(ctx, qjs_intl_display_open(&allocator, data, &service), "display initialization"))
            return NULL;
        rt->intl_display = service;
    }
    return rt->intl_display;
}

static void display_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlDisplayNames *state = JS_GetOpaque(value, JS_CLASS_INTL_DISPLAY_NAMES);
    if (state) {
        qjs_intl_display_formatter_close(state->formatter);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state);
    }
}

static JSValue display_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                                   JSValueConst *argv)
{
    JSIntlLocaleList requested = {0};
    QJSIntlDisplayService *service;
    QJSIntlDisplayOptions native;
    const QJSIntlBytes *available;
    JSIntlDisplayNames *state;
    size_t count, locale;
    int style, type, fallback, language_display;
    JSValue object, options = JS_UNDEFINED, result = JS_EXCEPTION;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.DisplayNames requires new");
    object = js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_DISPLAY_NAMES);
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto done;
    if (argc < 2 || JS_IsUndefined(argv[1])) {
        JS_ThrowTypeError(ctx, "Intl.DisplayNames requires options");
        goto done;
    }
    options = js_intl_options_object(ctx, argv[1], FALSE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0)
        goto done;
    service = js_intl_display_service(ctx);
    if (!service || js_intl_error(ctx, qjs_intl_display_available(service, &available, &count), "display locales") ||
        js_intl_resolve_locale_index(ctx, available, count, &requested, &locale))
        goto done;
    style = js_intl_string_option(ctx, options, "style", styles, countof(styles), 0);
    if (style < 0)
        goto done;
    type = js_intl_string_option(ctx, options, "type", types, countof(types), -2);
    if (type == -2)
        JS_ThrowTypeError(ctx, "Intl.DisplayNames requires a type");
    if (type < 0)
        goto done;
    fallback = js_intl_string_option(ctx, options, "fallback", fallbacks, countof(fallbacks), 0);
    if (fallback < 0)
        goto done;
    language_display = js_intl_string_option(ctx, options, "languageDisplay", languages, countof(languages), 0);
    if (language_display < 0)
        goto done;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto done;
    JS_SetOpaque(object, state);
    state->style = style;
    state->type = type;
    state->fallback = fallback;
    state->language_display = language_display;
    state->locale = js_strndup(ctx, available[locale].data, available[locale].length);
    if (!state->locale)
        goto done;
    native = (QJSIntlDisplayOptions){type, style, language_display == 0, JS_STRING_LEN_MAX};
    if (js_intl_error(ctx, qjs_intl_display_formatter_open(service, locale, &native, &state->formatter), "DisplayNames"))
        goto done;
    result = object;
    object = JS_UNDEFINED;
done:
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, options);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return result;
}

static JSValue display_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlDisplayService *service = js_intl_display_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service || js_intl_error(ctx, qjs_intl_display_available(service, &available, &count), "display locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                  argc > 1 ? argv[1] : JS_UNDEFINED);
}

static JSValue display_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlDisplayNames *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DISPLAY_NAMES);
    static const char *const keys[] = {"locale", "style", "type", "fallback", "languageDisplay"};
    const char *values[5];
    JSValue object;
    size_t i, count;
    if (!state)
        return JS_EXCEPTION;
    values[0] = state->locale;
    values[1] = styles[state->style];
    values[2] = types[state->type];
    values[3] = fallbacks[state->fallback];
    values[4] = languages[state->language_display];
    count = state->type == QJS_INTL_DISPLAY_LANGUAGE ? 5 : 4;
    object = JS_NewObject(ctx);
    if (JS_IsException(object))
        return object;
    for (i = 0; i < count; i++) {
        JSValue value = JS_NewString(ctx, values[i]);
        if (JS_IsException(value) ||
            JS_DefinePropertyValueStr(ctx, object, keys[i], value, JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
            JS_FreeValue(ctx, object);
            return JS_EXCEPTION;
        }
    }
    return object;
}

/* Preserve CanonicalCodeForDisplayNames identities for API fallback. Calendar
   names use lowercase syntax only; backend source aliases never rewrite it. */
static char *canonical_code(JSContext *ctx, int type, JSValueConst value)
{
    size_t length, i;
    const char *text = JS_ToCStringLen(ctx, &length, value);
    char *result = NULL;
    int valid = 0;
    if (!text)
        return NULL;
    switch (type) {
    case QJS_INTL_DISPLAY_LANGUAGE: {
        QJSIntlLocaleComponents parts;
        QJSIntlLocaleService *service;
        if (!qjs_intl_unicode_locale_well_formed(text, length) ||
            qjs_intl_locale_components((QJSIntlBytes){text, length}, &parts) ||
            parts.base_name.length != length)
            break;
        service = js_intl_locale_service(ctx);
        if (service)
            js_intl_error(ctx, qjs_intl_locale_canonicalize(service, (QJSIntlBytes){text, length}, &result),
                          "language display code");
        goto done;
    }
    case QJS_INTL_DISPLAY_REGION:
        valid = qjs_intl_unicode_subtag_well_formed(QJS_INTL_REGION_SUBTAG, text, length);
        break;
    case QJS_INTL_DISPLAY_SCRIPT:
        valid = qjs_intl_unicode_subtag_well_formed(QJS_INTL_SCRIPT_SUBTAG, text, length);
        break;
    case QJS_INTL_DISPLAY_CURRENCY:
        valid = length == 3;
        for (i = 0; valid && i < length; i++)
            valid = (text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= 'a' && text[i] <= 'z');
        break;
    case QJS_INTL_DISPLAY_CALENDAR:
        valid = qjs_intl_unicode_type_well_formed(text, length);
        break;
    case QJS_INTL_DISPLAY_DATE_TIME_FIELD: {
        QJSIntlDateField field;
        valid = !qjs_intl_date_field_from_code((QJSIntlBytes){text, length}, &field);
        break;
    }
    }
    if (!valid) {
        JS_ThrowRangeError(ctx, "invalid display name code");
        goto done;
    }
    result = js_malloc(ctx, length + 1);
    if (!result)
        goto done;
    for (i = 0; i < length; i++) {
        unsigned int c = (uint8_t)text[i];
        int upper = type == QJS_INTL_DISPLAY_REGION || type == QJS_INTL_DISPLAY_CURRENCY ||
                    (type == QJS_INTL_DISPLAY_SCRIPT && !i);
        if (type != QJS_INTL_DISPLAY_DATE_TIME_FIELD) {
            if (upper && c >= 'a' && c <= 'z')
                c -= 'a' - 'A';
            else if (!upper && c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
        }
        result[i] = c;
    }
    result[length] = '\0';
done:
    JS_FreeCString(ctx, text);
    return result;
}

static JSValue display_of(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlDisplayNames *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DISPLAY_NAMES);
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlNameText formatted = {0};
    JSValue result = JS_EXCEPTION;
    char *code;
    if (!state)
        return result;
    code = canonical_code(ctx, state->type, argc ? argv[0] : JS_UNDEFINED);
    if (!code)
        return result;
    if (!js_intl_error(ctx, qjs_intl_display_format(state->formatter,
                      (QJSIntlBytes){code, strlen(code)}, &formatted), "display name")) {
        if (formatted.found)
            result = JS_NewStringUTF16(ctx, formatted.text, formatted.length);
        else
            result = state->fallback ? JS_UNDEFINED : JS_NewString(ctx, code);
    }
    js_free(ctx, code);
    qjs_intl_name_text_clear(&allocator, &formatted);
    return result;
}

static const JSCFunctionListEntry display_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, display_supported),
};
static const JSCFunctionListEntry display_properties[] = {
    JS_CFUNC_DEF("of", 1, display_of),
    JS_CFUNC_DEF("resolvedOptions", 0, display_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.DisplayNames", JS_PROP_CONFIGURABLE),
};

int js_intl_init_display_names(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.DisplayNames",
        .finalizer = display_finalizer,
    };
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_DISPLAY_NAMES) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_DISPLAY_NAMES, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_DISPLAY_NAMES, "DisplayNames",
                                     display_constructor, 2, JS_CFUNC_constructor, 0, JS_UNDEFINED,
                                     display_statics, countof(display_statics), display_properties,
                                     countof(display_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    return JS_DefinePropertyValueStr(ctx, namespace_object, "DisplayNames", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
}
