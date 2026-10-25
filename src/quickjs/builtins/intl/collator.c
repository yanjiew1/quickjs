/* Intl.Collator JS identity, option ordering and cached comparison ownership.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-22.
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
#include "../../internal/atom.h"
#include "../../internal/function.h"
#include "../../internal/object.h"
#include "../../internal/runtime.h"

static const char *const usages[] = {"sort", "search"};
static const char *const sensitivities[] = {"variant", "base", "accent", "case"};
static const char *const case_firsts[] = {"false", "upper", "lower"};
enum { KEY_CO, KEY_KF, KEY_KN, KEY_COUNT };

typedef struct JSIntlCollator {
    QJSIntlCollator *collator;
    QJSIntlCollationOptions options;
    char *locale, *collation;
    JSValue bound_compare;
} JSIntlCollator;

typedef struct CollationCapabilities {
    const QJSIntlCollationService *service;
    size_t locale;
    QJSIntlCollationUsage usage;
} CollationCapabilities;

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, text ? strlen(text) : 0};
}

QJSIntlCollationService *js_intl_collation_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;

    if (!rt->intl_collation) {
        const QJSIntlData *data = js_intl_data(ctx);
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlCollationService *service;

        if (!data)
            return NULL;
        if (js_intl_error(ctx, qjs_intl_collation_open(&allocator, data, &service), "collation initialization"))
            return NULL;
        rt->intl_collation = service;
    }
    return rt->intl_collation;
}

static QJSIntlStatus co_supported(void *opaque, QJSIntlBytes value, int *out)
{
    CollationCapabilities *capabilities = opaque;

    return qjs_intl_collation_type_supported(capabilities->service, capabilities->locale,
                                             capabilities->usage, value, out);
}

static QJSIntlStatus kf_supported(void *opaque, QJSIntlBytes value, int *out)
{
    size_t i;

    (void)opaque;
    *out = 0;
    for (i = 0; i < countof(case_firsts); i++) {
        if (value.length == strlen(case_firsts[i]) && !memcmp(value.data, case_firsts[i], value.length))
            *out = 1;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus kn_supported(void *opaque, QJSIntlBytes value, int *out)
{
    (void)opaque;
    *out = (value.length == 4 && !memcmp(value.data, "true", 4)) ||
           (value.length == 5 && !memcmp(value.data, "false", 5));
    return QJS_INTL_OK;
}

static int boolean_option(JSContext *ctx, JSValueConst object, const char *name, int fallback, int *out)
{
    JSValue value = JS_GetPropertyStr(ctx, object, name);

    if (JS_IsException(value))
        return -1;
    *out = JS_IsUndefined(value) ? fallback : JS_ToBool(ctx, value);
    JS_FreeValue(ctx, value);
    return 0;
}

static void collator_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlCollator *state = JS_GetOpaque(value, JS_CLASS_INTL_COLLATOR);

    if (state) {
        qjs_intl_collator_close(state->collator);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state->collation);
        JS_FreeValueRT(rt, state->bound_compare);
        js_free_rt(rt, state);
    }
}

static void collator_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark)
{
    JSIntlCollator *state = JS_GetOpaque(value, JS_CLASS_INTL_COLLATOR);

    if (state)
        JS_MarkValue(rt, state->bound_compare, mark);
}

static JSValue collator_constructor(JSContext *ctx, JSValueConst active_function,
                                    JSValueConst this_val, int argc, JSValueConst *argv, int flags)
{
    JSIntlLocaleList requested = {0};
    QJSIntlResolvedLocale resolved = {0};
    QJSIntlLocaleResolveKey keys[KEY_COUNT];
    QJSIntlLocaleMatch match;
    QJSIntlLocaleService *identity;
    QJSIntlCollationService *service;
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlCollationOptions native;
    CollationCapabilities capabilities;
    const QJSIntlBytes *available;
    QJSIntlBytes co_option;
    const char *owned_co = NULL;
    JSIntlCollator *state;
    size_t count;
    int usage, numeric, case_first, sensitivity;
    JSValue object = js_intl_create_from_constructor(ctx,
        (flags & JS_CALL_FLAG_CONSTRUCTOR) ? this_val : active_function, JS_CLASS_INTL_COLLATOR);
    JSValue options = JS_UNDEFINED, result = JS_EXCEPTION;

    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto done;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, TRUE);
    if (JS_IsException(options))
        goto done;

    /* Usage chooses locale data before ResolveOptions, whose observable option
       sequence is localeMatcher, collation, numeric (ToBoolean), caseFirst. */
    usage = js_intl_string_option(ctx, options, "usage", usages, countof(usages), QJS_INTL_COLLATION_SORT);
    if (usage < 0 || js_intl_locale_matcher(ctx, options) < 0 ||
        js_intl_unicode_type_option(ctx, options, "collation", &co_option, &owned_co) ||
        boolean_option(ctx, options, "numeric", -1, &numeric))
        goto done;
    case_first = js_intl_string_option(ctx, options, "caseFirst", case_firsts, countof(case_firsts), -2);
    if (case_first == -1)
        goto done;

    service = js_intl_collation_service(ctx);
    identity = service ? js_intl_locale_service(ctx) : NULL;
    if (!identity || js_intl_error(ctx, qjs_intl_collation_available(service, &available, &count), "collation locales") ||
        js_intl_resolve_locale_match(ctx, available, count, &requested, &match) ||
        js_intl_error(ctx, qjs_intl_collation_defaults(service, match.available_index, usage, &native), "collation defaults"))
        goto done;
    capabilities = (CollationCapabilities){service, match.available_index, usage};
    keys[KEY_CO] = (QJSIntlLocaleResolveKey){{'c', 'o'}, {0}, co_option, 0, co_supported, &capabilities};
    keys[KEY_KF] = (QJSIntlLocaleResolveKey){{'k', 'f'}, bytes(case_firsts[native.case_first]),
        case_first < 0 ? (QJSIntlBytes){0} : bytes(case_firsts[case_first]), 0, kf_supported, NULL};
    keys[KEY_KN] = (QJSIntlLocaleResolveKey){{'k', 'n'}, bytes(native.numeric ? "true" : "false"),
        numeric < 0 ? (QJSIntlBytes){0} : bytes(numeric ? "true" : "false"), 0, kn_supported, NULL};
    if (js_intl_error(ctx, qjs_intl_resolve_locale_keys(identity, &allocator, available[match.available_index],
                         match.extension, keys, KEY_COUNT, &resolved), "collation keyword resolution"))
        goto done;

    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto done;
    state->bound_compare = JS_UNDEFINED;
    state->locale = resolved.locale;
    state->collation = resolved.values[KEY_CO];
    resolved.locale = resolved.values[KEY_CO] = NULL;
    JS_SetOpaque(object, state);

    native.collation = bytes(state->collation);
    native.numeric = !strcmp(resolved.values[KEY_KN], "true");
    native.case_first = !strcmp(resolved.values[KEY_KF], "upper") ? QJS_INTL_COLLATION_UPPER_FIRST :
        !strcmp(resolved.values[KEY_KF], "lower") ? QJS_INTL_COLLATION_LOWER_FIRST : QJS_INTL_COLLATION_CASE_OFF;
    sensitivity = js_intl_string_option(ctx, options, "sensitivity", sensitivities,
                                        countof(sensitivities), native.sensitivity);
    if (sensitivity < 0 || boolean_option(ctx, options, "ignorePunctuation",
                                          native.ignore_punctuation, &native.ignore_punctuation))
        goto done;
    native.sensitivity = sensitivity;
    state->options = native;
    if (js_intl_error(ctx, qjs_intl_collator_open(service, match.available_index, &native, &state->collator), "Collator"))
        goto done;
    result = object;
    object = JS_UNDEFINED;
done:
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, options);
    JS_FreeCString(ctx, owned_co);
    qjs_intl_resolved_locale_clear(&resolved);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return result;
}

/* Both arguments are already strings. Keep JS coercion outside native buffer
   preparation so x and y are observably converted before comparing either. */
static JSValue compare_strings(JSContext *ctx, const JSIntlCollator *state, JSValueConst a, JSValueConst b)
{
    const uint16_t *left = NULL, *right = NULL;
    size_t left_length, right_length;
    JSValue result = JS_EXCEPTION;
    int order;

    left = JS_ToCStringLenUTF16(ctx, &left_length, a);
    if (!left)
        goto done;
    right = JS_ToCStringLenUTF16(ctx, &right_length, b);
    if (!right)
        goto done;
    if (!js_intl_error(ctx, qjs_intl_collator_compare(state->collator, left, left_length,
                           right, right_length, &order), "collation comparison"))
        result = JS_NewInt32(ctx, order);
done:
    JS_FreeCStringUTF16(ctx, left);
    JS_FreeCStringUTF16(ctx, right);
    return result;
}

static JSValue bound_compare(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv, int magic, JSValue *data)
{
    JSContext *realm = JS_GetFunctionRealm(ctx, data[1]);
    JSIntlCollator *state;
    JSValue a, b = JS_UNDEFINED, result = JS_EXCEPTION;

    (void)this_val;
    (void)magic;
    if (!realm)
        return JS_EXCEPTION;
    state = JS_GetOpaque2(realm, data[0], JS_CLASS_INTL_COLLATOR);
    if (!state)
        return JS_EXCEPTION;
    a = JS_ToString(realm, argc ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(a))
        return a;
    b = JS_ToString(realm, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (!JS_IsException(b))
        result = compare_strings(realm, state, a, b);
    JS_FreeValue(realm, a);
    JS_FreeValue(realm, b);
    return result;
}

static JSValue collator_compare_get(JSContext *ctx, JSValueConst this_val)
{
    JSIntlCollator *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_COLLATOR);

    if (!state)
        return JS_EXCEPTION;
    if (JS_IsUndefined(state->bound_compare)) {
        /* CFunctionData uses the caller context; this marked constructor keeps
           the getter's realm for coercion/errors, including borrowed getters. */
        JSValueConst data[] = {this_val, ctx->intl_collator_ctor};
        JSValue function = JS_NewCFunctionData(ctx, bound_compare, 2, 0, countof(data), data);

        if (JS_IsException(function))
            return function;
        state->bound_compare = function;
    }
    return JS_DupValue(ctx, state->bound_compare);
}

JSValue js_intl_compare_strings(JSContext *ctx, JSValueConst a, JSValueConst b, int argc, JSValueConst *argv)
{
    JSValue collator, result;
    JSIntlCollator *state;

    if (js_intl_ensure_intrinsics(ctx))
        return JS_EXCEPTION;
    collator = collator_constructor(ctx, ctx->intl_collator_ctor, JS_UNDEFINED, argc, argv, 0);
    if (JS_IsException(collator))
        return collator;
    state = JS_GetOpaque(collator, JS_CLASS_INTL_COLLATOR);
    result = compare_strings(ctx, state, a, b);
    JS_FreeValue(ctx, collator);
    return result;
}

static int property(JSContext *ctx, JSValueConst object, const char *name, JSValue value)
{
    if (JS_IsException(value))
        return -1;
    return JS_DefinePropertyValueStr(ctx, object, name, value, JS_PROP_C_W_E | JS_PROP_THROW);
}

static JSValue collator_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlCollator *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_COLLATOR);
    JSValue result;

    if (!state)
        return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    if (property(ctx, result, "locale", JS_NewString(ctx, state->locale)) < 0 ||
        property(ctx, result, "usage", JS_NewString(ctx, usages[state->options.usage])) < 0 ||
        property(ctx, result, "sensitivity", JS_NewString(ctx, sensitivities[state->options.sensitivity])) < 0 ||
        property(ctx, result, "ignorePunctuation", JS_NewBool(ctx, state->options.ignore_punctuation)) < 0 ||
        property(ctx, result, "collation", JS_NewString(ctx, state->collation ? state->collation : "default")) < 0 ||
        property(ctx, result, "numeric", JS_NewBool(ctx, state->options.numeric)) < 0 ||
        property(ctx, result, "caseFirst", JS_NewString(ctx, case_firsts[state->options.case_first])) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}

static JSValue collator_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlCollationService *service = js_intl_collation_service(ctx);
    const QJSIntlBytes *available;
    size_t count;

    if (!service || js_intl_error(ctx, qjs_intl_collation_available(service, &available, &count), "collation locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                  argc > 1 ? argv[1] : JS_UNDEFINED);
}

static const JSCFunctionListEntry collator_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, collator_supported),
};
static const JSCFunctionListEntry collator_properties[] = {
    JS_CGETSET_DEF("compare", collator_compare_get, NULL),
    JS_CFUNC_DEF("resolvedOptions", 0, collator_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.Collator", JS_PROP_CONFIGURABLE),
};

int js_intl_init_collator(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.Collator",
        .finalizer = collator_finalizer,
        .gc_mark = collator_mark,
    };
    JSCFunctionType callback = {.constructor_or_func_flags = collator_constructor};
    JSValue constructor;

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_COLLATOR) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_COLLATOR, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_COLLATOR, "Collator", callback.generic, 0,
        JS_CFUNC_constructor_or_func_flags, 0, JS_UNDEFINED, collator_statics, countof(collator_statics),
        collator_properties, countof(collator_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    ctx->intl_collator_ctor = JS_DupValue(ctx, constructor);
    return JS_DefinePropertyValueStr(ctx, namespace_object, "Collator", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
}
