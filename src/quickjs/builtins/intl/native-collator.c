/* Native Intl.Collator and ECMA-402 String.prototype.localeCompare.
 * ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd (reviewed 2026-10-09).
 * Native C provider; en/en-US with explicit shared-root sort/search policy.
 */
#include "intl-internal.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../../../intl/provider-native-collator.h"

typedef struct JSIntlCollator {
    JSIntlResolvedLocale locale;
    QJSIntlNativeCollator *collator;
    JSValue bound_compare;
    int usage;
    int sensitivity;
    BOOL ignore_punctuation;
    BOOL numeric;
    int case_first;
} JSIntlCollator;

static const char *const collator_usage[] = { "sort", "search" };
static const char *const collator_matcher[] = { "lookup", "best fit" };
static const char *const collator_sensitivity[] = {
    "base", "accent", "case", "variant"
};
static const char *const collator_case_first[] = { "upper", "lower", "false" };

static void js_intl_collator_free(JSRuntime *rt, JSIntlCollator *s)
{
    int i;
    if (!s)
        return;
    if (s->collator)
        qjs_intl_native_collator_close(s->collator);
    JS_FreeValueRT(rt, s->bound_compare);
    js_free_rt(rt, s->locale.locale);
    js_free_rt(rt, s->locale.data_locale);
    for (i = 0; i < s->locale.key_count; i++)
        js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s);
}

static void js_intl_collator_finalizer(JSRuntime *rt, JSValue value)
{
    js_intl_collator_free(rt, JS_GetOpaque(value, JS_CLASS_INTL_COLLATOR));
}

static void js_intl_collator_mark(JSRuntime *rt, JSValueConst value,
                                  JS_MarkFunc *mark_func)
{
    JSIntlCollator *s = JS_GetOpaque(value, JS_CLASS_INTL_COLLATOR);
    if (!s)
        return;
    JS_MarkValue(rt, s->bound_compare, mark_func);
}

static int js_intl_collator_defaults(JSContext *ctx, JSIntlCollator *s,
                                      int *sensitivity, int *ignore_punctuation)
{
    QJSIntlCollatorOptions defaults;
    QJSIntlProvider *provider = js_intl_native_provider(ctx);
    QJSIntlStatus status;
    if (!provider) return -1;
    status = qjs_intl_native_provider_collator_defaults(provider,
        (QJSIntlBytes){ s->locale.data_locale, strlen(s->locale.data_locale) },
        s->usage ? QJS_INTL_COLLATOR_USAGE_SEARCH : QJS_INTL_COLLATOR_SORT,
        &defaults);
    if (js_intl_native_error(ctx, status, "Collator locale defaults")) return -1;
    /* Explicit enum boundary; sort sensitivity remains variant by ECMA402. */
    switch (defaults.sensitivity) {
    case QJS_INTL_COLLATOR_BASE: *sensitivity = 0; break;
    case QJS_INTL_COLLATOR_ACCENT: *sensitivity = 1; break;
    case QJS_INTL_COLLATOR_CASE: *sensitivity = 2; break;
    case QJS_INTL_COLLATOR_VARIANT: *sensitivity = 3; break;
    default: JS_ThrowInternalError(ctx, "invalid Collator defaults"); return -1;
    }
    if (!s->usage) *sensitivity = 3;
    *ignore_punctuation = defaults.ignore_punctuation;
    return 0;
}

static JSValue js_intl_collator_constructor(JSContext *ctx,
                                            JSValueConst new_target,
                                            int argc, JSValueConst *argv)
{
    JSValue object, options = JS_UNDEFINED;
    JSIntlLocaleList requested = { 0 };
    JSIntlResolutionKey keys[3];
    JSIntlCollator *s = NULL;
    char *collation = NULL;
    QJSIntlProvider *provider;
    QJSIntlStatus status;
    int matcher, numeric, case_first, ignore_punctuation, i;
    int default_sensitivity;

    object = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_COLLATOR);
    if (JS_IsException(object))
        return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    s->bound_compare = JS_UNDEFINED;
    /* sec-intl.collator: these precede usage and do not repeat. */
    if (js_intl_canonicalize_locale_list(ctx, argc > 0 ? argv[0] : JS_UNDEFINED,
                                        &requested) < 0)
        goto fail;
    options = js_intl_coerce_options(ctx, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (JS_IsException(options))
        goto fail;
    if (js_intl_get_string_option(ctx, options, "usage", collator_usage, 2, 0,
                                  &s->usage) < 0 ||
        js_intl_get_string_option(ctx, options, "localeMatcher", collator_matcher,
                                  2, 1, &matcher) < 0 ||
        js_intl_get_string_option_alloc(ctx, options, "collation", &collation) < 0)
        goto fail;
    if (collation && !js_intl_is_unicode_type(collation)) {
        JS_ThrowRangeError(ctx, "invalid collation option");
        goto fail;
    }
    if (js_intl_get_bool_option(ctx, options, "numeric", -1, &numeric) < 0 ||
        js_intl_get_string_option(ctx, options, "caseFirst", collator_case_first,
                                  3, -1, &case_first) < 0)
        goto fail;
    keys[0] = (JSIntlResolutionKey){ "co", collation, FALSE };
    keys[1] = (JSIntlResolutionKey){ "kn", numeric < 0 ? NULL :
                                      numeric ? "true" : "false", FALSE };
    keys[2] = (JSIntlResolutionKey){ "kf", case_first < 0 ? NULL :
                                      collator_case_first[case_first], FALSE };
    if (js_intl_resolve_locale(ctx,
            s->usage ? JS_INTL_COLLATOR_SEARCH : JS_INTL_COLLATOR,
            &requested, collator_matcher[matcher], keys, 3, &s->locale) < 0)
        goto fail;
    s->numeric = s->locale.values[1] && !strcmp(s->locale.values[1], "true");
    s->case_first = 2;
    for (i = 0; i < 2; i++) {
        if (s->locale.values[2] &&
            !strcmp(s->locale.values[2], collator_case_first[i]))
            s->case_first = i;
    }
    if (js_intl_collator_defaults(ctx, s, &default_sensitivity,
                                  &ignore_punctuation) < 0 ||
        js_intl_get_string_option(ctx, options, "sensitivity", collator_sensitivity,
                                  4, default_sensitivity, &s->sensitivity) < 0 ||
        js_intl_get_bool_option(ctx, options, "ignorePunctuation",
                                ignore_punctuation, &ignore_punctuation) < 0)
        goto fail;
    s->ignore_punctuation = ignore_punctuation;
    provider = js_intl_native_provider(ctx);
    if (!provider) goto fail;
    {
        static const QJSIntlCollatorSensitivity sensitivities[] = {
            QJS_INTL_COLLATOR_BASE, QJS_INTL_COLLATOR_ACCENT,
            QJS_INTL_COLLATOR_CASE, QJS_INTL_COLLATOR_VARIANT };
        static const QJSIntlCollatorCaseFirst cases[] = {
            QJS_INTL_COLLATOR_CASE_UPPER, QJS_INTL_COLLATOR_CASE_LOWER,
            QJS_INTL_COLLATOR_CASE_FALSE };
        QJSIntlCollatorOptions native = {
            s->usage ? QJS_INTL_COLLATOR_USAGE_SEARCH : QJS_INTL_COLLATOR_SORT,
            sensitivities[s->sensitivity], cases[s->case_first],
            s->numeric, s->ignore_punctuation };
        /* co LocaleData=[null]: unsupported types are ignored by ResolveLocale. */
        status = qjs_intl_native_provider_collator_open(provider,
            (QJSIntlBytes){ s->locale.data_locale, strlen(s->locale.data_locale) },
            (QJSIntlBytes){ NULL, 0 }, &native, &s->collator);
        if (js_intl_native_error(ctx, status, "Collator open")) goto fail;
    }
    JS_SetOpaque(object, s);
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    js_free(ctx, collation);
    return object;
 fail:
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    js_free(ctx, collation);
    js_intl_collator_free(ctx->rt, s);
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static JSValue js_intl_collator_compare_strings(JSContext *ctx,
                                                JSIntlCollator *s,
                                                JSValueConst x, JSValueConst y)
{
    uint16_t *left = NULL, *right = NULL;
    int32_t left_length, right_length;
    int result = 0;
    QJSIntlStatus status;
    if (js_intl_to_utf16(ctx, x, &left, &left_length) < 0 ||
        js_intl_to_utf16(ctx, y, &right, &right_length) < 0) {
        js_free(ctx, left);
        js_free(ctx, right);
        return JS_EXCEPTION;
    }
    status = qjs_intl_native_collator_compare(s->collator,
        (QJSIntlUTF16){ left, (size_t)left_length },
        (QJSIntlUTF16){ right, (size_t)right_length }, &result);
    js_free(ctx, left);
    js_free(ctx, right);
    if (js_intl_native_error(ctx, status, "Collator.compare")) return JS_EXCEPTION;
    return JS_NewInt32(ctx, result);
}

static JSValue js_intl_collator_bound_compare(JSContext *ctx,
                                              JSValueConst this_value,
                                              int argc, JSValueConst *argv,
                                              int magic, JSValue *data)
{
    JSIntlCollator *s = JS_GetOpaque(data[0], JS_CLASS_INTL_COLLATOR);
    JSValue left, right, result;
    left = JS_ToString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(left))
        return JS_EXCEPTION;
    right = JS_ToString(ctx, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (JS_IsException(right)) {
        JS_FreeValue(ctx, left);
        return JS_EXCEPTION;
    }
    result = js_intl_collator_compare_strings(ctx, s, left, right);
    JS_FreeValue(ctx, left);
    JS_FreeValue(ctx, right);
    return result;
}

static JSValue js_intl_collator_compare_get(JSContext *ctx,
                                            JSValueConst this_value)
{
    JSIntlCollator *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_COLLATOR);
    JSValue function;
    if (!s)
        return JS_EXCEPTION;
    if (JS_IsUndefined(s->bound_compare)) {
        function = js_intl_new_c_function_data(ctx, js_intl_collator_bound_compare,
                                               2, 0, 1, &this_value);
        if (JS_IsException(function))
            return function;
        s->bound_compare = function;
    }
    return JS_DupValue(ctx, s->bound_compare);
}

static JSValue js_intl_collator_resolved_options(JSContext *ctx,
                                                 JSValueConst this_value,
                                                 int argc, JSValueConst *argv)
{
    JSIntlCollator *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_COLLATOR);
    JSValue result;
    if (!s)
        return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    /* Table 3 order is observable. Define data properties, avoiding setters. */
    if (js_intl_define_string(ctx, result, "locale", s->locale.locale) < 0 ||
        js_intl_define_string(ctx, result, "usage", collator_usage[s->usage]) < 0 ||
        js_intl_define_string(ctx, result, "sensitivity",
                              collator_sensitivity[s->sensitivity]) < 0 ||
        js_intl_define_bool(ctx, result, "ignorePunctuation", s->ignore_punctuation) < 0 ||
        js_intl_define_string(ctx, result, "collation",
                              s->locale.values[0] ? s->locale.values[0] : "default") < 0 ||
        js_intl_define_bool(ctx, result, "numeric", s->numeric) < 0 ||
        js_intl_define_string(ctx, result, "caseFirst",
                              collator_case_first[s->case_first]) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}

static JSValue js_intl_collator_supported_locales(JSContext *ctx,
                                                  JSValueConst this_value,
                                                  int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_COLLATOR,
        argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
}

JSValue js_intl_string_locale_compare(JSContext *ctx, JSValueConst this_value,
                                      int argc, JSValueConst *argv)
{
    JSValue left, right, collator, result;
    JSValueConst arguments[2];
    JSIntlCollator *s;
    left = JS_ToStringCheckObject(ctx, this_value);
    if (JS_IsException(left))
        return JS_EXCEPTION;
    right = JS_ToString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(right)) {
        JS_FreeValue(ctx, left);
        return JS_EXCEPTION;
    }
    arguments[0] = argc > 1 ? argv[1] : JS_UNDEFINED;
    arguments[1] = argc > 2 ? argv[2] : JS_UNDEFINED;
    if (js_intl_ensure_service(ctx, JS_INTL_COLLATOR) < 0) {
        JS_FreeValue(ctx, left);
        JS_FreeValue(ctx, right);
        return JS_EXCEPTION;
    }
    collator = js_intl_collator_constructor(ctx, JS_UNDEFINED, 2, arguments);
    if (JS_IsException(collator)) {
        result = JS_EXCEPTION;
    } else {
        s = JS_GetOpaque(collator, JS_CLASS_INTL_COLLATOR);
        result = js_intl_collator_compare_strings(ctx, s, left, right);
        JS_FreeValue(ctx, collator);
    }
    JS_FreeValue(ctx, left);
    JS_FreeValue(ctx, right);
    return result;
}

static const JSClassDef collator_class = {
    "Intl.Collator", .finalizer = js_intl_collator_finalizer,
    .gc_mark = js_intl_collator_mark,
};
static const JSCFunctionListEntry collator_constructor_functions[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_collator_supported_locales),
};
static const JSCFunctionListEntry collator_prototype_functions[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_collator_resolved_options),
    JS_CGETSET_DEF("compare", js_intl_collator_compare_get, NULL),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.Collator", JS_PROP_CONFIGURABLE),
};

int js_intl_init_collator(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_COLLATOR,
        &collator_class, "Collator", js_intl_collator_constructor, 0,
        JS_CFUNC_constructor_or_func, collator_constructor_functions,
        countof(collator_constructor_functions), collator_prototype_functions,
        countof(collator_prototype_functions));
}
#endif /* CONFIG_INTL && CONFIG_INTL_NATIVE */
