/* Native Intl.Collator and ECMA-402 String.prototype.localeCompare.
 * ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd (2026-10-06).
 * ICU4C C collation API; no process-global locale mutation.
 */
#include "intl-internal.h"
#ifdef CONFIG_INTL
#include <unicode/ucol.h>
#include <unicode/uloc.h>

typedef struct JSIntlCollator {
    JSIntlResolvedLocale locale;
    UCollator *collator;
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
        ucol_close(s->collator);
    JS_FreeValueRT(rt, s->bound_compare);
    js_free_rt(rt, s->locale.locale);
    js_free_rt(rt, s->locale.data_locale);
    js_free_rt(rt, s->locale.icu_locale);
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

static char *js_intl_collator_backend_locale(JSContext *ctx,
                                            const char *locale, int usage)
{
    char *result;
    size_t length = strlen(locale);
    int32_t capacity;
    UErrorCode status = U_ZERO_ERROR;
    if (!usage)
        return js_intl_strdup(ctx, locale);
    /* Replacing an existing keyword cannot need more than this addition. */
    if (length > INT32_MAX - 64) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    capacity = (int32_t)length + 64;
    result = js_malloc(ctx, capacity);
    if (!result)
        return NULL;
    memcpy(result, locale, length + 1);
    uloc_setKeywordValue("collation", "search", result, capacity, &status);
    if (js_intl_icu_error(ctx, status, "Collator search locale") < 0) {
        js_free(ctx, result);
        return NULL;
    }
    return result;
}

static int js_intl_collator_default_sensitivity(const UCollator *collator,
                                                UErrorCode *status)
{
    UCollationStrength strength = ucol_getStrength(collator);
    UColAttributeValue case_level = ucol_getAttribute(collator, UCOL_CASE_LEVEL,
                                                     status);
    if (strength == UCOL_PRIMARY)
        return case_level == UCOL_ON ? 2 : 0;
    if (strength == UCOL_SECONDARY && case_level != UCOL_ON)
        return 1;
    return 3;
}

static int js_intl_collator_defaults(JSContext *ctx, JSIntlCollator *s,
                                      int *sensitivity, int *ignore_punctuation)
{
    char *base = NULL, *locale = NULL;
    UCollator *collator = NULL;
    UErrorCode status = U_ZERO_ERROR;
    int result = -1;
    /* LocaleData is defined per data locale and usage, independently of
       requested Unicode extensions and options selecting a specialization. */
    base = js_intl_locale_to_icu(ctx, s->locale.data_locale);
    if (!base)
        goto done;
    locale = js_intl_collator_backend_locale(ctx, base, s->usage);
    if (!locale)
        goto done;
    collator = ucol_open(locale, &status);
    if (js_intl_icu_error(ctx, status, "Collator locale defaults") < 0)
        goto done;
    if (!collator) {
        JS_ThrowOutOfMemory(ctx);
        goto done;
    }
    *sensitivity = s->usage ?
        js_intl_collator_default_sensitivity(collator, &status) : 3;
    *ignore_punctuation = ucol_getAttribute(collator, UCOL_ALTERNATE_HANDLING,
                                            &status) == UCOL_SHIFTED;
    result = js_intl_icu_error(ctx, status, "Collator locale defaults");
 done:
    if (collator)
        ucol_close(collator);
    js_free(ctx, base);
    js_free(ctx, locale);
    return result;
}

static int js_intl_collator_configure(JSContext *ctx, JSIntlCollator *s)
{
    UErrorCode status = U_ZERO_ERROR;
    UColAttributeValue case_first = UCOL_OFF;
    ucol_setStrength(s->collator,
        s->sensitivity == 0 || s->sensitivity == 2 ? UCOL_PRIMARY :
        s->sensitivity == 1 ? UCOL_SECONDARY : UCOL_TERTIARY);
    ucol_setAttribute(s->collator, UCOL_CASE_LEVEL,
                      s->sensitivity == 2 ? UCOL_ON : UCOL_OFF, &status);
    if (s->case_first == 0)
        case_first = UCOL_UPPER_FIRST;
    else if (s->case_first == 1)
        case_first = UCOL_LOWER_FIRST;
    ucol_setAttribute(s->collator, UCOL_CASE_FIRST, case_first, &status);
    ucol_setAttribute(s->collator, UCOL_NUMERIC_COLLATION,
                      s->numeric ? UCOL_ON : UCOL_OFF, &status);
    ucol_setAttribute(s->collator, UCOL_ALTERNATE_HANDLING,
                      s->ignore_punctuation ? UCOL_SHIFTED : UCOL_NON_IGNORABLE,
                      &status);
    /* SHIFTED must ignore punctuation while continuing to compare symbols. */
    ucol_setMaxVariable(s->collator, UCOL_REORDER_CODE_PUNCTUATION, &status);
    /* ECMA-402 requires canonical equivalence for every sensitivity. */
    ucol_setAttribute(s->collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    return js_intl_icu_error(ctx, status, "Collator attributes");
}

static JSValue js_intl_collator_constructor(JSContext *ctx,
                                            JSValueConst new_target,
                                            int argc, JSValueConst *argv)
{
    JSValue object, options = JS_UNDEFINED;
    JSIntlLocaleList requested = { 0 };
    JSIntlResolutionKey keys[3];
    JSIntlCollator *s = NULL;
    char *collation = NULL, *backend_locale = NULL;
    UErrorCode status = U_ZERO_ERROR;
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
    backend_locale = js_intl_collator_backend_locale(ctx, s->locale.icu_locale,
                                                    s->usage);
    if (!backend_locale)
        goto fail;
    s->collator = ucol_open(backend_locale, &status);
    if (js_intl_icu_error(ctx, status, "Collator open") < 0)
        goto fail;
    if (!s->collator) {
        JS_ThrowOutOfMemory(ctx);
        goto fail;
    }
    if (js_intl_collator_configure(ctx, s) < 0)
        goto fail;
    JS_SetOpaque(object, s);
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    js_free(ctx, collation);
    js_free(ctx, backend_locale);
    return object;
 fail:
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    js_free(ctx, collation);
    js_free(ctx, backend_locale);
    js_intl_collator_free(ctx->rt, s);
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static JSValue js_intl_collator_compare_strings(JSContext *ctx,
                                                JSIntlCollator *s,
                                                JSValueConst x, JSValueConst y)
{
    UChar *left = NULL, *right = NULL;
    int32_t left_length, right_length;
    UCollationResult result;
    /* ICU accepts explicit lengths, including NUL and lone surrogate units. */
    if (js_intl_to_uchar(ctx, x, &left, &left_length) < 0 ||
        js_intl_to_uchar(ctx, y, &right, &right_length) < 0) {
        js_free(ctx, left);
        js_free(ctx, right);
        return JS_EXCEPTION;
    }
    result = ucol_strcoll(s->collator, left, left_length, right, right_length);
    js_free(ctx, left);
    js_free(ctx, right);
    return JS_NewInt32(ctx, result < 0 ? -1 : result > 0 ? 1 : 0);
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
#endif /* CONFIG_INTL */
