/* Intl.NumberFormat observable binding and native formatter ownership.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "number-format-options.h"
#include "number-value.h"
#include "../../internal/atom.h"
#include "../../internal/function.h"
#include "../../internal/object.h"
#include "../../internal/runtime.h"
#include "../../internal/string.h"

static const char *const styles[] = {"decimal", "percent", "currency", "unit"};
static const char *const currency_displays[] = {"symbol", "narrowSymbol", "code", "name"};
static const char *const currency_signs[] = {"standard", "accounting"};
static const char *const unit_displays[] = {"short", "narrow", "long"};
static const char *const notations[] = {"standard", "scientific", "engineering", "compact"};
static const char *const compact_displays[] = {"short", "long"};
static const char *const groupings[] = {NULL, "auto", "always", "min2"};
static const char *const signs[] = {"auto", "never", "always", "exceptZero", "negative"};
static const char *const rounding_modes[] = {"ceil", "floor", "expand", "trunc", "halfCeil",
                                             "halfFloor", "halfExpand", "halfTrunc", "halfEven"};
static const char *const priorities[] = {"auto", "morePrecision", "lessPrecision"};
static const char *const trailing[] = {"auto", "stripIfInteger"};
static const char *const part_names[QJS_INTL_PART_COUNT] = {
    "literal", "element", "integer", "group", "decimal", "fraction", "nan", "infinity",
    "minusSign", "plusSign", "percentSign", "currency", "exponentSeparator", "exponentMinusSign",
    "exponentInteger", "compact", "unit", "approximatelySign"};
static const char *const sources[] = {"shared", "startRange", "endRange"};

typedef struct JSIntlNumberFormat {
    QJSIntlNumberFormatter *formatter;
    JSIntlNumberFormatOptions options;
    char *locale, *numbering_system;
    JSValue bound_format;
} JSIntlNumberFormat;

typedef struct NumberCapabilities {
    const QJSIntlNumberService *service;
    size_t locale;
} NumberCapabilities;

static QJSIntlStatus system_supported(void *opaque, QJSIntlBytes value, int *out)
{
    NumberCapabilities *capabilities = opaque;
    return qjs_intl_number_system_supported(capabilities->service, capabilities->locale, value, out);
}

static QJSIntlNumberService *number_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_number) {
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlNumberService *service;
        if (js_intl_error(ctx, qjs_intl_number_open(&allocator, &service), "number initialization"))
            return NULL;
        rt->intl_number = service;
    }
    return rt->intl_number;
}

static void number_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlNumberFormat *state = JS_GetOpaque(value, JS_CLASS_INTL_NUMBER_FORMAT);
    if (state) {
        qjs_intl_number_formatter_close(state->formatter);
        js_free_rt(rt, state->options.unit.unit);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state->numbering_system);
        JS_FreeValueRT(rt, state->bound_format);
        js_free_rt(rt, state);
    }
}

static void number_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark)
{
    JSIntlNumberFormat *state = JS_GetOpaque(value, JS_CLASS_INTL_NUMBER_FORMAT);
    if (state)
        JS_MarkValue(rt, state->bound_format, mark);
}

static QJSIntlNumberOptions native_options(const JSIntlNumberFormatOptions *options)
{
    const JSIntlDigitOptions *digits = &options->digits;
    return (QJSIntlNumberOptions){
        .style = options->unit.style,
        .notation = options->notation,
        .grouping = options->grouping,
        .sign_display = options->sign_display,
        .compact_long = options->compact_long,
        .digits = {
            .minimum_integer_digits = digits->minimum_integer_digits,
            .minimum_fraction_digits =
                digits->minimum_fraction_digits < 0 ? 0 : digits->minimum_fraction_digits,
            .maximum_fraction_digits =
                digits->maximum_fraction_digits < 0 ? 3 : digits->maximum_fraction_digits,
            .minimum_significant_digits =
                digits->minimum_significant_digits < 0 ? 1 : digits->minimum_significant_digits,
            .maximum_significant_digits =
                digits->maximum_significant_digits < 0 ? 21 : digits->maximum_significant_digits,
            .rounding_increment = digits->rounding_increment,
            .rounding_mode = digits->rounding_mode,
            .rounding_type = digits->rounding_type,
            .trailing_zero_display = digits->trailing_zero_display,
            .maximum_output_length = JS_STRING_LEN_MAX,
        },
        .maximum_output_length = JS_STRING_LEN_MAX,
        .currency = {options->unit.currency, options->unit.style == QJS_INTL_NUMBER_CURRENCY ? 3 : 0},
        .currency_display = options->unit.currency_display,
        .currency_sign = options->unit.currency_sign,
        .unit = {options->unit.unit, options->unit.unit ? strlen(options->unit.unit) : 0},
        .unit_display = options->unit.unit_display,
    };
}

static JSValue number_constructor(JSContext *ctx, JSValueConst active_function,
                                   JSValueConst this_val, int argc, JSValueConst *argv, int flags)
{
    JSIntlLocaleList requested = {0};
    JSIntlResolvedKeyword resolved = {0};
    QJSIntlLocaleMatch match;
    QJSIntlNumberService *service;
    const QJSIntlBytes *available;
    QJSIntlBytes option, default_system;
    const char *owned_option = NULL;
    size_t count;
    JSIntlNumberFormat *state;
    QJSIntlNumberOptions native;
    NumberCapabilities capabilities;
    BOOL construct = (flags & JS_CALL_FLAG_CONSTRUCTOR) != 0;
    JSValue object = js_intl_create_from_constructor(ctx, construct ? this_val : active_function,
                                                      JS_CLASS_INTL_NUMBER_FORMAT);
    JSValue options = JS_UNDEFINED, result = JS_EXCEPTION;
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto done;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, TRUE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0 ||
        js_intl_unicode_type_option(ctx, options, "numberingSystem", &option, &owned_option))
        goto done;
    service = number_service(ctx);
    if (!service || js_intl_error(ctx, qjs_intl_number_available(service, &available, &count),
                                    "number locales") ||
        js_intl_resolve_locale_match(ctx, available, count, &requested, &match) ||
        js_intl_error(ctx, qjs_intl_number_default_system(service, match.available_index,
                                                           &default_system), "numbering system"))
        goto done;
    capabilities = (NumberCapabilities){service, match.available_index};
    if (js_intl_resolve_locale_key(ctx, available[match.available_index], match.extension,
                                     "nu", default_system, option, system_supported, &capabilities,
                                     &resolved))
        goto done;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto done;
    state->bound_format = JS_UNDEFINED;
    state->locale = resolved.locale;
    state->numbering_system = resolved.value;
    resolved = (JSIntlResolvedKeyword){0};
    JS_SetOpaque(object, state);
    if (js_intl_number_format_options(ctx, options, service, &state->options))
        goto done;
    native = native_options(&state->options);
    if (js_intl_error(ctx, qjs_intl_number_formatter_open(service, match.available_index,
                                (QJSIntlBytes){state->numbering_system, strlen(state->numbering_system)},
                                &native, &state->formatter), "NumberFormat"))
        goto done;
    if (!construct) {
        int instance = JS_OrdinaryIsInstanceOf(ctx, this_val, ctx->intl_number_ctor);
        if (instance < 0)
            goto done;
        if (instance) {
            JSAtom key = JS_ValueToAtom(ctx, ctx->intl_fallback_symbol);
            int status;
            if (key == JS_ATOM_NULL)
                goto done;
            status = JS_DefinePropertyValue(ctx, this_val, key, object, JS_PROP_THROW);
            object = JS_UNDEFINED; /* DefinePropertyValue consumes the formatter. */
            JS_FreeAtom(ctx, key);
            if (status < 0)
                goto done;
            result = JS_DupValue(ctx, this_val);
            goto done;
        }
    }
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

static JSValue unwrap(JSContext *ctx, JSValueConst value)
{
    int instance;
    JSAtom key;
    JSValue result;
    if (!JS_IsObject(value))
        return JS_ThrowTypeError(ctx, "NumberFormat receiver must be an object");
    if (JS_GetOpaque(value, JS_CLASS_INTL_NUMBER_FORMAT))
        return JS_DupValue(ctx, value);
    instance = JS_OrdinaryIsInstanceOf(ctx, value, ctx->intl_number_ctor);
    if (instance < 0)
        return JS_EXCEPTION;
    if (!instance)
        return JS_DupValue(ctx, value);
    key = JS_ValueToAtom(ctx, ctx->intl_fallback_symbol);
    if (key == JS_ATOM_NULL)
        return JS_EXCEPTION;
    result = JS_GetProperty(ctx, value, key);
    JS_FreeAtom(ctx, key);
    return result;
}

static int property(JSContext *ctx, JSValueConst object, const char *name, JSValue value)
{
    if (JS_IsException(value))
        return -1;
    return JS_DefinePropertyValueStr(ctx, object, name, value, JS_PROP_C_W_E | JS_PROP_THROW);
}

static JSValue number_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue value = unwrap(ctx, this_val), result = JS_EXCEPTION;
    JSIntlNumberFormat *state;
    JSIntlNumberFormatOptions *options;
    JSIntlNumberUnitOptions *unit;
    if (JS_IsException(value))
        return value;
    state = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_NUMBER_FORMAT);
    if (!state)
        goto done;
    options = &state->options;
    unit = &options->unit;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        goto done;
    if (property(ctx, result, "locale", JS_NewString(ctx, state->locale)) < 0 ||
        property(ctx, result, "numberingSystem", JS_NewString(ctx, state->numbering_system)) < 0 ||
        property(ctx, result, "style", JS_NewString(ctx, styles[unit->style])) < 0 ||
        (unit->style == QJS_INTL_NUMBER_CURRENCY &&
         (property(ctx, result, "currency", JS_NewString(ctx, unit->currency)) < 0 ||
          property(ctx, result, "currencyDisplay",
                    JS_NewString(ctx, currency_displays[unit->currency_display])) < 0 ||
          property(ctx, result, "currencySign",
                    JS_NewString(ctx, currency_signs[unit->currency_sign])) < 0)) ||
        (unit->style == QJS_INTL_NUMBER_UNIT &&
         (property(ctx, result, "unit", JS_NewString(ctx, unit->unit)) < 0 ||
          property(ctx, result, "unitDisplay", JS_NewString(ctx, unit_displays[unit->unit_display])) < 0)))
        goto fail;
    {
        const JSIntlDigitOptions *d = &options->digits;
        const struct {
            const char *name;
            int value;
        } digits[] = {
            {"minimumIntegerDigits", d->minimum_integer_digits},
            {"minimumFractionDigits", d->minimum_fraction_digits},
            {"maximumFractionDigits", d->maximum_fraction_digits},
            {"minimumSignificantDigits", d->minimum_significant_digits},
            {"maximumSignificantDigits", d->maximum_significant_digits},
        };
        for (size_t i = 0; i < countof(digits); i++)
            if (digits[i].value >= 0 &&
                property(ctx, result, digits[i].name, JS_NewInt32(ctx, digits[i].value)) < 0)
                goto fail;
    }
    if (property(ctx, result, "useGrouping", options->grouping
                     ? JS_NewString(ctx, groupings[options->grouping]) : JS_FALSE) < 0 ||
        property(ctx, result, "notation", JS_NewString(ctx, notations[options->notation])) < 0 ||
        (options->notation == QJS_INTL_NUMBER_COMPACT &&
         property(ctx, result, "compactDisplay",
                 JS_NewString(ctx, compact_displays[options->compact_long])) < 0) ||
        property(ctx, result, "signDisplay", JS_NewString(ctx, signs[options->sign_display])) < 0 ||
        property(ctx, result, "roundingIncrement",
                 JS_NewInt32(ctx, options->digits.rounding_increment)) < 0 ||
        property(ctx, result, "roundingMode",
                 JS_NewString(ctx, rounding_modes[options->digits.rounding_mode])) < 0 ||
        property(ctx, result, "roundingPriority",
                 JS_NewString(ctx, priorities[options->digits.computed_rounding_priority])) < 0 ||
        property(ctx, result, "trailingZeroDisplay",
                 JS_NewString(ctx, trailing[options->digits.trailing_zero_display])) < 0)
        goto fail;
    goto done;
fail:
    JS_FreeValue(ctx, result);
    result = JS_EXCEPTION;
done:
    JS_FreeValue(ctx, value);
    return result;
}

static JSValue number_parts(JSContext *ctx, const QJSIntlFormatted *ordinary,
                             const QJSIntlFormattedRange *range)
{
    const uint16_t *text = range ? range->text : ordinary->text;
    size_t length = range ? range->length : ordinary->length;
    size_t count = range ? range->part_count : ordinary->part_count;
    size_t end = 0;
    JSValue array;
    if (count > UINT32_MAX)
        return JS_ThrowRangeError(ctx, "too many NumberFormat parts");
    array = JS_NewArray(ctx);
    if (JS_IsException(array))
        return array;
    for (size_t i = 0; i < count; i++) {
        size_t start = range ? range->parts[i].start : ordinary->parts[i].start;
        size_t limit = range ? range->parts[i].end : ordinary->parts[i].end;
        QJSIntlPartType type = range ? range->parts[i].type : ordinary->parts[i].type;
        unsigned int source = range ? range->parts[i].source : 0;
        JSValue part;
        if (start != end || limit < start || limit > length ||
            (unsigned int)type >= QJS_INTL_PART_COUNT || type == QJS_INTL_PART_ELEMENT ||
            source >= countof(sources))
            goto invalid;
        end = limit;
        part = JS_NewObject(ctx);
        if (JS_IsException(part))
            goto fail;
        if (property(ctx, part, "type", JS_NewString(ctx, part_names[type])) < 0 ||
            property(ctx, part, "value", JS_NewStringUTF16(ctx, text ? text + start : NULL,
                                                           limit - start)) < 0 ||
            (range && property(ctx, part, "source", JS_NewString(ctx, sources[source])) < 0)) {
            JS_FreeValue(ctx, part);
            goto fail;
        }
        if (JS_DefinePropertyValueUint32(ctx, array, (uint32_t)i, part, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
    }
    if (end != length)
        goto invalid;
    return array;
invalid:
    JS_ThrowInternalError(ctx, "invalid Intl number parts");
fail:
    JS_FreeValue(ctx, array);
    return JS_EXCEPTION;
}

/* -1 ordinary text (bound format), 0 ordinary parts, 1 range text, 2 range parts. */
static JSValue number_format(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv, int operation)
{
    JSIntlNumberFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_NUMBER_FORMAT);
    QJSIntlNumberDecimal first = {0}, last = {0};
    QJSIntlFormatted ordinary = {0};
    QJSIntlFormattedRange range = {0};
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    JSValue result = JS_EXCEPTION;
    QJSIntlStatus status;
    if (!state)
        return result;
    if (operation > 0 && (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1])))
        return JS_ThrowTypeError(ctx, "NumberFormat range requires endpoints");
    if (js_intl_to_mathematical_value(ctx, argc ? argv[0] : JS_UNDEFINED, &first))
        goto done;
    if (operation > 0) {
        if (js_intl_to_mathematical_value(ctx, argv[1], &last))
            goto done;
        status = qjs_intl_number_format_range(state->formatter, &first.value, &last.value, &range);
        if (js_intl_error(ctx, status, "number range"))
            goto done;
        result = operation == 2 ? number_parts(ctx, NULL, &range)
                                : JS_NewStringUTF16(ctx, range.text, range.length);
    } else {
        status = qjs_intl_number_format(state->formatter, &first.value, &ordinary);
        if (js_intl_error(ctx, status, "number formatting"))
            goto done;
        result = operation == 0 ? number_parts(ctx, &ordinary, NULL)
                                : JS_NewStringUTF16(ctx, ordinary.text, ordinary.length);
    }
done:
    qjs_intl_number_decimal_clear(&allocator, &first);
    qjs_intl_number_decimal_clear(&allocator, &last);
    qjs_intl_number_result_clear(&allocator, &ordinary);
    qjs_intl_number_range_result_clear(&allocator, &range);
    return result;
}

static JSValue bound_format(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv, int magic, JSValue *data)
{
    JSContext *realm;
    (void)this_val;
    (void)magic;
    /* Data callbacks use the caller's context. Retain the getter's intrinsic
       constructor as a marked realm anchor, including for borrowed getters. */
    realm = JS_GetFunctionRealm(ctx, data[1]);
    if (!realm)
        return JS_EXCEPTION;
    return number_format(realm, data[0], argc, argv, -1);
}

JSValue js_intl_format_number(JSContext *ctx, JSValueConst value, int argc, JSValueConst *argv)
{
    JSValue formatter, result;
    if (js_intl_ensure_intrinsics(ctx))
        return JS_EXCEPTION;
    formatter = number_constructor(ctx, ctx->intl_number_ctor, JS_UNDEFINED, argc, argv, 0);
    if (JS_IsException(formatter))
        return formatter;
    result = number_format(ctx, formatter, 1, &value, -1);
    JS_FreeValue(ctx, formatter);
    return result;
}

static JSValue number_format_get(JSContext *ctx, JSValueConst this_val)
{
    JSValue value = unwrap(ctx, this_val), result = JS_EXCEPTION;
    JSIntlNumberFormat *state;
    if (JS_IsException(value))
        return value;
    state = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_NUMBER_FORMAT);
    if (!state)
        goto done;
    if (JS_IsUndefined(state->bound_format)) {
        JSValueConst data[] = {value, ctx->intl_number_ctor};
        JSValue function = JS_NewCFunctionData(ctx, bound_format, 1, 0, countof(data), data);
        if (JS_IsException(function))
            goto done;
        state->bound_format = function;
    }
    result = JS_DupValue(ctx, state->bound_format);
done:
    JS_FreeValue(ctx, value);
    return result;
}

static JSValue number_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlNumberService *service = number_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service || js_intl_error(ctx, qjs_intl_number_available(service, &available, &count),
                                    "number locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                    argc > 1 ? argv[1] : JS_UNDEFINED);
}

static const JSCFunctionListEntry number_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, number_supported),
};
static const JSCFunctionListEntry number_properties[] = {
    JS_CGETSET_DEF("format", number_format_get, NULL),
    JS_CFUNC_MAGIC_DEF("formatToParts", 1, number_format, 0),
    JS_CFUNC_MAGIC_DEF("formatRange", 2, number_format, 1),
    JS_CFUNC_MAGIC_DEF("formatRangeToParts", 2, number_format, 2),
    JS_CFUNC_DEF("resolvedOptions", 0, number_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.NumberFormat", JS_PROP_CONFIGURABLE),
};

int js_intl_init_number_format(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.NumberFormat",
        .finalizer = number_finalizer,
        .gc_mark = number_mark,
    };
    JSCFunctionType callback = {.constructor_or_func_flags = number_constructor};
    JSAtom description;
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_NUMBER_FORMAT) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_NUMBER_FORMAT, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    description = JS_NewAtom(ctx, "IntlLegacyConstructedSymbol");
    if (description == JS_ATOM_NULL)
        return -1;
    ctx->intl_fallback_symbol = JS_NewSymbolFromAtom(ctx, description, JS_ATOM_TYPE_SYMBOL);
    JS_FreeAtom(ctx, description);
    if (JS_IsException(ctx->intl_fallback_symbol))
        return -1;
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_NUMBER_FORMAT, "NumberFormat",
                                       callback.generic, 0, JS_CFUNC_constructor_or_func_flags, 0,
                                       JS_UNDEFINED, number_statics, countof(number_statics),
                                       number_properties, countof(number_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    ctx->intl_number_ctor = JS_DupValue(ctx, constructor);
    return JS_DefinePropertyValueStr(ctx, namespace_object, "NumberFormat", constructor,
                                       JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
}
