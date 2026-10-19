/* Intl.ListFormat construction, locale resolution and native ownership.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../../internal/atom.h"
#include "../../internal/object.h"
#include "../../internal/runtime.h"
#include "../intl.h"

typedef struct JSIntlListFormat {
    QJSIntlListFormatter *formatter;
    char *locale;
    int type, style;
} JSIntlListFormat;

static const char *const list_types[] = {"conjunction", "disjunction", "unit"};
static const char *const list_styles[] = {"long", "short", "narrow"};

static QJSIntlListService *list_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_list) {
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlListService *service;
        QJSIntlStatus status = qjs_intl_list_open(&allocator, &service);
        if (js_intl_error(ctx, status, "list initialization"))
            return NULL;
        rt->intl_list = service;
    }
    return rt->intl_list;
}

static void list_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlListFormat *state = JS_GetOpaque(value, JS_CLASS_INTL_LIST_FORMAT);
    if (state) {
        qjs_intl_list_formatter_close(state->formatter);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state);
    }
}

static JSValue list_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                                JSValueConst *argv)
{
    JSIntlLocaleList requested = {0};
    JSValue object, options = JS_UNDEFINED;
    QJSIntlListService *service;
    const QJSIntlBytes *available;
    size_t count, locale;
    int type, style;
    JSIntlListFormat *state;
    QJSIntlStatus status;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.ListFormat requires new");
    object =
        js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_LIST_FORMAT);
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED,
                                         &requested))
        goto fail;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, FALSE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0)
        goto fail;
    service = list_service(ctx);
    if (!service ||
        js_intl_error(ctx, qjs_intl_list_available(service, &available, &count),
                      "list locales") ||
        js_intl_resolve_locale_index(ctx, available, count, &requested, &locale))
        goto fail;
    type = js_intl_string_option(ctx, options, "type", list_types, 3, 0);
    if (type < 0)
        goto fail;
    style = js_intl_string_option(ctx, options, "style", list_styles, 3, 0);
    if (style < 0)
        goto fail;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto fail;
    JS_SetOpaque(object, state);
    state->type = type;
    state->style = style;
    state->locale = js_strndup(ctx, available[locale].data, available[locale].length);
    if (!state->locale)
        goto fail;
    status = qjs_intl_list_formatter_open(service, locale, (QJSIntlListType)type,
                                          (QJSIntlListStyle)style, &state->formatter);
    if (js_intl_error(ctx, status, "ListFormat"))
        goto fail;
    JS_FreeValue(ctx, options);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return object;
fail:
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, options);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return JS_EXCEPTION;
}

static JSValue list_resolved(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv)
{
    JSIntlListFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LIST_FORMAT);
    JSValue result;
    const char *names[] = {"locale", "type", "style"};
    const char *values[3];
    size_t i;
    if (!state)
        return JS_EXCEPTION;
    values[0] = state->locale;
    values[1] = list_types[state->type];
    values[2] = list_styles[state->style];
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    for (i = 0; i < 3; i++) {
        JSValue value = JS_NewString(ctx, values[i]);
        if (JS_IsException(value) ||
            JS_DefinePropertyValueStr(ctx, result, names[i], value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
            JS_FreeValue(ctx, result);
            return JS_EXCEPTION;
        }
    }
    return result;
}

static JSValue list_supported(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv)
{
    QJSIntlListService *service = list_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service ||
        js_intl_error(ctx, qjs_intl_list_available(service, &available, &count),
                      "list locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                  argc > 1 ? argv[1] : JS_UNDEFINED);
}

static const JSCFunctionListEntry list_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, list_supported),
};
static const JSCFunctionListEntry list_properties[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, list_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.ListFormat", JS_PROP_CONFIGURABLE),
};

int js_intl_init_list_format(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.ListFormat",
        .finalizer = list_finalizer,
    };
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_LIST_FORMAT) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_LIST_FORMAT, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(
        ctx, JS_CLASS_INTL_LIST_FORMAT, "ListFormat", list_constructor, 0,
        JS_CFUNC_constructor, 0, JS_UNDEFINED, list_statics, countof(list_statics),
        list_properties, countof(list_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    return JS_DefinePropertyValueStr(ctx, namespace_object, "ListFormat", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE |
                                         JS_PROP_THROW) < 0
               ? -1
               : 0;
}
