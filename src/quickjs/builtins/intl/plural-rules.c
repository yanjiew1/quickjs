/* Intl.PluralRules binding, observable options, branding and native ownership.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
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
#include "number-options.h"
#include "number-value.h"
#include "../../internal/atom.h"
#include "../../internal/object.h"
#include "../../internal/runtime.h"
#include "../../internal/string.h"
#include "../../../intl/plural/plural.h"

typedef struct JSIntlPluralRules {
    QJSIntlPluralFormatter *formatter;
    char *locale;
    JSIntlDigitOptions digits;
    int type, notation, compact;
} JSIntlPluralRules;

static const char *const types[] = {"cardinal", "ordinal"};
static const char *const notations[] = {"standard", "scientific", "engineering", "compact"};
static const char *const displays[] = {"short", "long"};
static const char *const categories[] = {"zero", "one", "two", "few", "many", "other"};
static const char *const rounding_modes[] = {"ceil",       "floor",     "expand",
                                             "trunc",      "halfCeil",  "halfFloor",
                                             "halfExpand", "halfTrunc", "halfEven"};
static const char *const priorities[] = {"auto", "morePrecision", "lessPrecision"};
static const char *const trailing[] = {"auto", "stripIfInteger"};

static QJSIntlPluralService *plural_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_plural) {
        const QJSIntlData *data = js_intl_data(ctx);
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlPluralService *service;
        QJSIntlStatus status;

        if (!data)
            return NULL;
        status = qjs_intl_plural_open(&allocator, data, &service);
        if (js_intl_error(ctx, status, "plural initialization"))
            return NULL;
        rt->intl_plural = service;
    }
    return rt->intl_plural;
}

static void plural_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlPluralRules *state = JS_GetOpaque(value, JS_CLASS_INTL_PLURAL_RULES);
    if (state) {
        qjs_intl_plural_formatter_close(state->formatter);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state);
    }
}

static JSValue plural_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                                  JSValueConst *argv)
{
    JSIntlLocaleList requested = {0};
    JSValue object, options = JS_UNDEFINED;
    QJSIntlPluralService *service;
    const QJSIntlBytes *available;
    size_t count, locale;
    JSIntlPluralRules *state;
    JSIntlDigitOptions digits;
    QJSIntlPluralOptions native = {0};
    int type, notation, compact;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.PluralRules requires new");
    object = js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_PLURAL_RULES);
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto fail;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, TRUE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0)
        goto fail;
    service = plural_service(ctx);
    if (!service ||
        js_intl_error(ctx, qjs_intl_plural_available(service, &available, &count),
                      "plural locales") ||
        js_intl_resolve_locale_index(ctx, available, count, &requested, &locale))
        goto fail;
    type = js_intl_string_option(ctx, options, "type", types, countof(types), 0);
    if (type < 0)
        goto fail;
    notation = js_intl_string_option(ctx, options, "notation", notations, countof(notations), 0);
    if (notation < 0)
        goto fail;
    compact = js_intl_string_option(ctx, options, "compactDisplay", displays, countof(displays), 0);
    if (compact < 0 ||
        js_intl_set_digit_options(ctx, options, 0, 3, notation == QJS_INTL_PLURAL_COMPACT, &digits))
        goto fail;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto fail;
    JS_SetOpaque(object, state);
    state->type = type;
    state->notation = notation;
    state->compact = compact;
    state->digits = digits;
    state->locale = js_strndup(ctx, available[locale].data, available[locale].length);
    if (!state->locale)
        goto fail;
    native.type = type;
    native.notation = notation;
    native.compact_long = compact;
    native.digits = (QJSIntlDecimalOptions){
        .minimum_integer_digits = digits.minimum_integer_digits,
        .minimum_fraction_digits =
            digits.minimum_fraction_digits < 0 ? 0 : digits.minimum_fraction_digits,
        .maximum_fraction_digits =
            digits.maximum_fraction_digits < 0 ? 3 : digits.maximum_fraction_digits,
        .minimum_significant_digits =
            digits.minimum_significant_digits < 0 ? 1 : digits.minimum_significant_digits,
        .maximum_significant_digits =
            digits.maximum_significant_digits < 0 ? 21 : digits.maximum_significant_digits,
        .rounding_increment = digits.rounding_increment,
        .rounding_mode = digits.rounding_mode,
        .rounding_type = digits.rounding_type,
        .trailing_zero_display = digits.trailing_zero_display,
        .maximum_output_length = JS_STRING_LEN_MAX,
    };
    if (js_intl_error(ctx,
                      qjs_intl_plural_formatter_open(service, locale, &native, &state->formatter),
                      "PluralRules"))
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

static JSValue plural_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlPluralService *service = plural_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service || js_intl_error(ctx, qjs_intl_plural_available(service, &available, &count),
                                  "plural locales"))
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

static JSValue plural_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlPluralRules *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_PLURAL_RULES);
    JSValue result, array;
    unsigned int mask, i, index = 0;
    if (!state)
        return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    if (property(ctx, result, "locale", JS_NewString(ctx, state->locale)) < 0 ||
        property(ctx, result, "type", JS_NewString(ctx, types[state->type])) < 0 ||
        property(ctx, result, "notation", JS_NewString(ctx, notations[state->notation])) < 0 ||
        (state->notation == QJS_INTL_PLURAL_COMPACT &&
         property(ctx, result, "compactDisplay", JS_NewString(ctx, displays[state->compact])) < 0))
        goto fail;
    {
        const struct {
            const char *name;
            int value;
        } digits[] = {
            {"minimumIntegerDigits", state->digits.minimum_integer_digits},
            {"minimumFractionDigits", state->digits.minimum_fraction_digits},
            {"maximumFractionDigits", state->digits.maximum_fraction_digits},
            {"minimumSignificantDigits", state->digits.minimum_significant_digits},
            {"maximumSignificantDigits", state->digits.maximum_significant_digits},
        };
        for (i = 0; i < countof(digits); i++) {
            if (digits[i].value >= 0 &&
                property(ctx, result, digits[i].name, JS_NewInt32(ctx, digits[i].value)) < 0)
                goto fail;
        }
    }
    array = JS_NewArray(ctx);
    if (JS_IsException(array))
        goto fail;
    qjs_intl_plural_categories(state->formatter, &mask);
    for (i = 0; i < countof(categories); i++) {
        if (mask & (1u << i)) {
            JSValue value = JS_NewString(ctx, categories[i]);
            if (JS_IsException(value) ||
                JS_DefinePropertyValueUint32(ctx, array, index++, value,
                                             JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
                JS_FreeValue(ctx, array);
                goto fail;
            }
        }
    }
    if (property(ctx, result, "pluralCategories", array) < 0 ||
        property(ctx, result, "roundingIncrement",
                 JS_NewInt32(ctx, state->digits.rounding_increment)) < 0 ||
        property(ctx, result, "roundingMode",
                 JS_NewString(ctx, rounding_modes[state->digits.rounding_mode])) < 0 ||
        property(ctx, result, "roundingPriority",
                 JS_NewString(ctx, priorities[state->digits.computed_rounding_priority])) < 0 ||
        property(ctx, result, "trailingZeroDisplay",
                 JS_NewString(ctx, trailing[state->digits.trailing_zero_display])) < 0)
        goto fail;
    return result;
fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static JSValue plural_select(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
                             int range)
{
    JSIntlPluralRules *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_PLURAL_RULES);
    QJSIntlNumberDecimal start = {0}, end = {0};
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlPluralCategory category;
    QJSIntlStatus status;
    JSValue result = JS_EXCEPTION;
    if (!state)
        return result;
    if (range && (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1])))
        return JS_ThrowTypeError(ctx, "Intl.PluralRules.selectRange requires endpoints");
    if (js_intl_to_mathematical_value(ctx, argc ? argv[0] : JS_UNDEFINED, &start))
        goto done;
    if (range) {
        if (js_intl_to_mathematical_value(ctx, argv[1], &end))
            goto done;
        status =
            qjs_intl_plural_select_range(state->formatter, &start.value, &end.value, &category);
    } else {
        status = qjs_intl_plural_select(state->formatter, &start.value, &category);
    }
    if (!js_intl_error(ctx, status, "plural selection"))
        result = JS_NewString(ctx, categories[category]);
done:
    qjs_intl_number_decimal_clear(&allocator, &start);
    qjs_intl_number_decimal_clear(&allocator, &end);
    return result;
}

static const JSCFunctionListEntry plural_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, plural_supported),
};
static const JSCFunctionListEntry plural_properties[] = {
    JS_CFUNC_MAGIC_DEF("select", 1, plural_select, 0),
    JS_CFUNC_MAGIC_DEF("selectRange", 2, plural_select, 1),
    JS_CFUNC_DEF("resolvedOptions", 0, plural_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.PluralRules", JS_PROP_CONFIGURABLE),
};

int js_intl_init_plural_rules(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.PluralRules",
        .finalizer = plural_finalizer,
    };
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_PLURAL_RULES) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_PLURAL_RULES, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_PLURAL_RULES, "PluralRules",
                                     plural_constructor, 0, JS_CFUNC_constructor, 0, JS_UNDEFINED,
                                     plural_statics, countof(plural_statics), plural_properties,
                                     countof(plural_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    return JS_DefinePropertyValueStr(ctx, namespace_object, "PluralRules", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0
               ? -1
               : 0;
}
