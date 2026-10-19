/* Intl.Locale construction and fixed identity operations.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../../internal/runtime.h"
#include "../../internal/object.h"
#include "../../internal/atom.h"
#include "../../../intl/locale/identifier.h"
#include "../../../intl/locale/preferences.h"
#include "../../../intl/common/locale-grammar.h"

enum {
    SLOT_CA,
    SLOT_CO,
    SLOT_FW,
    SLOT_HC,
    SLOT_KF,
    SLOT_KN,
    SLOT_NU,
    SLOT_COUNT
};
static const char keys[SLOT_COUNT][3] = {"ca", "co", "fw", "hc",
                                         "kf", "kn", "nu"};

typedef struct JSIntlLocale {
    char *tag;
    char *slots[SLOT_COUNT];
} JSIntlLocale;

const char *js_intl_locale_tag(JSValueConst value)
{
    JSIntlLocale *locale = JS_GetOpaque(value, JS_CLASS_INTL_LOCALE);
    return locale ? locale->tag : NULL;
}

static void free_locale(JSRuntime *rt, JSIntlLocale *locale)
{
    size_t i;

    if (!locale)
        return;
    js_free_rt(rt, locale->tag);
    for (i = 0; i < SLOT_COUNT; i++)
        js_free_rt(rt, locale->slots[i]);
    js_free_rt(rt, locale);
}

static void locale_finalizer(JSRuntime *rt, JSValue value)
{
    free_locale(rt, JS_GetOpaque(value, JS_CLASS_INTL_LOCALE));
}

static char *copy_slice(JSContext *ctx, QJSIntlBytes value)
{
    char *copy;

    if (value.length == SIZE_MAX) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    copy = js_malloc(ctx, value.length + 1);
    if (!copy)
        return NULL;
    if (value.length)
        memcpy(copy, value.data, value.length);
    copy[value.length] = 0;
    return copy;
}

/* Retain the converted string until construction ends. Validation callers use
   its explicit length, so embedded NUL never becomes a truncated identifier. */
static int string_option(JSContext *ctx, JSValueConst options, const char *name,
                         QJSIntlBytes *out, const char **owned)
{
    JSValue value = JS_GetPropertyStr(ctx, options, name);

    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value)) {
        JS_FreeValue(ctx, value);
        return 0;
    }
    *owned = JS_ToCStringLen(ctx, &out->length, value);
    out->data = *owned;
    JS_FreeValue(ctx, value);
    return out->data ? 0 : -1;
}

static int equals(QJSIntlBytes value, const char *text)
{
    size_t length = strlen(text);
    return value.length == length && !memcmp(value.data, text, length);
}

static int variants_valid(JSContext *ctx, QJSIntlLocaleService *service,
                          QJSIntlBytes variants)
{
    char *synthetic;
    QJSIntlStatus status;
    int well_formed;

    if (!variants.length)
        return 0;
    if (variants.length > SIZE_MAX - 13) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    synthetic = js_malloc(ctx, variants.length + 13);
    if (!synthetic)
        return -1;
    memcpy(synthetic, "und-Latn-US-", 12);
    memcpy(synthetic + 12, variants.data, variants.length);
    synthetic[12 + variants.length] = 0;
    status = qjs_intl_locale_validate(
        service, (QJSIntlBytes){synthetic, variants.length + 12}, &well_formed);
    /* Extensions are well-formed tags but are not a variants override. */
    if (!status && well_formed) {
        QJSIntlLocaleComponents components;
        status = qjs_intl_locale_components(
            (QJSIntlBytes){synthetic, variants.length + 12}, &components);
        well_formed = !status && components.variants.length == variants.length;
    }
    js_free(ctx, synthetic);
    if (js_intl_error(ctx, status, "locale variants"))
        return -1;
    return well_formed;
}

enum {
    OPT_LANGUAGE,
    OPT_SCRIPT,
    OPT_REGION,
    OPT_VARIANTS,
    OPT_CA,
    OPT_CO,
    OPT_FW,
    OPT_HC,
    OPT_KF,
    OPT_KN,
    OPT_NU,
    OPT_COUNT
};
static const char *const option_names[] = {
    "language",  "script",    "region",         "variants",
    "calendar",  "collation", "firstDayOfWeek", "hourCycle",
    "caseFirst", "numeric",   "numberingSystem"};
static const unsigned char slot_options[] = {OPT_CA, OPT_CO, OPT_FW, OPT_HC,
                                             OPT_KF, OPT_KN, OPT_NU};

static int option_valid(JSContext *ctx, QJSIntlLocaleService *service,
                        size_t index, QJSIntlBytes *value)
{
    static const char *const days[] = {"sun", "mon", "tue", "wed",
                                       "thu", "fri", "sat", "sun"};

    if (index <= OPT_REGION)
        return qjs_intl_unicode_subtag_well_formed(
            index == OPT_LANGUAGE ? QJS_INTL_LANGUAGE_SUBTAG
            : index == OPT_SCRIPT ? QJS_INTL_SCRIPT_SUBTAG
                                  : QJS_INTL_REGION_SUBTAG,
            value->data, value->length);
    if (index == OPT_VARIANTS)
        return variants_valid(ctx, service, *value);
    if (index == OPT_FW && value->length == 1 && value->data[0] >= '0' &&
        value->data[0] <= '7') {
        value->data = days[value->data[0] - '0'];
        value->length = 3;
    }
    if (index == OPT_HC)
        return equals(*value, "h11") || equals(*value, "h12") ||
               equals(*value, "h23") || equals(*value, "h24");
    if (index == OPT_KF)
        return equals(*value, "upper") || equals(*value, "lower") ||
               equals(*value, "false");
    return qjs_intl_unicode_type_well_formed(value->data, value->length);
}

static JSValue locale_constructor(JSContext *ctx, JSValueConst new_target,
                                  int argc, JSValueConst *argv)
{
    JSValue object, options = JS_UNDEFINED;
    JSValueConst tag_value = argc ? argv[0] : JS_UNDEFINED;
    QJSIntlBytes input, values[OPT_COUNT] = {{0}};
    const char *owned[OPT_COUNT] = {0}, *input_owned = NULL;
    QJSIntlLocaleService *service;
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlLocaleBaseOptions base;
    QJSIntlLocaleComponents components;
    QJSIntlLocaleKeyword overrides[SLOT_COUNT];
    JSIntlLocale *locale = NULL;
    char *canonical = NULL, *updated = NULL, *composed = NULL;
    QJSIntlStatus status;
    size_t i, count = 0;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.Locale requires new");
    object = js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_LOCALE);
    if (JS_IsException(object))
        return object;
    if (!JS_IsString(tag_value) && !JS_IsObject(tag_value)) {
        JS_ThrowTypeError(ctx, "locale tag must be a string or object");
        goto fail;
    }
    input.data = js_intl_locale_tag(tag_value);
    if (input.data) {
        input.length = strlen(input.data);
    } else {
        input.data = input_owned =
            JS_ToCStringLen(ctx, &input.length, tag_value);
        if (!input.data)
            goto fail;
    }
    options = argc < 2 || JS_IsUndefined(argv[1])
                  ? JS_NewObjectProto(ctx, JS_NULL)
                  : JS_ToObject(ctx, argv[1]);
    if (JS_IsException(options))
        goto fail;
    service = js_intl_locale_service(ctx);
    if (!service)
        goto fail;
    status = qjs_intl_locale_canonicalize(service, input, &canonical);
    if (js_intl_error(ctx, status, "Unicode locale identifier"))
        goto fail;
    /* Getter order and immediate validation are observable. Keep this before
       record composition, and after canonicalizing the original language ID. */
    for (i = 0; i < OPT_COUNT; i++) {
        if (i == OPT_KN) {
            JSValue value = JS_GetPropertyStr(ctx, options, "numeric");
            if (JS_IsException(value))
                goto fail;
            if (!JS_IsUndefined(value)) {
                int numeric = JS_ToBool(ctx, value);
                values[i] = numeric ? (QJSIntlBytes){"true", 4}
                                    : (QJSIntlBytes){"false", 5};
            }
            JS_FreeValue(ctx, value);
        } else {
            int valid;
            if (string_option(ctx, options, option_names[i], &values[i],
                              &owned[i]))
                goto fail;
            if (!values[i].data)
                continue;
            valid = option_valid(ctx, service, i, &values[i]);
            if (valid < 0)
                goto fail;
            if (!valid) {
                JS_ThrowRangeError(ctx, "invalid Intl.Locale %s",
                                   option_names[i]);
                goto fail;
            }
        }
    }
    base = (QJSIntlLocaleBaseOptions){values[OPT_LANGUAGE], values[OPT_SCRIPT],
                                      values[OPT_REGION], values[OPT_VARIANTS]};
    status = qjs_intl_locale_replace_base(
        &allocator, (QJSIntlBytes){canonical, strlen(canonical)}, &base,
        &updated);
    if (js_intl_error(ctx, status, "locale base"))
        goto fail;
    status = qjs_intl_locale_components(
        (QJSIntlBytes){updated, strlen(updated)}, &components);
    if (js_intl_error(ctx, status, "locale components"))
        goto fail;
    locale = js_mallocz(ctx, sizeof(*locale));
    if (!locale)
        goto fail;
    for (i = 0; i < SLOT_COUNT; i++) {
        QJSIntlBytes value = values[slot_options[i]];
        if (value.data) {
            status = qjs_intl_locale_canonicalize_uvalue(
                service, (QJSIntlBytes){keys[i], 2}, value, &locale->slots[i]);
            if (js_intl_error(ctx, status, "locale keyword"))
                goto fail;
            memcpy(overrides[count].key, keys[i], 2);
            overrides[count++].value =
                (QJSIntlBytes){locale->slots[i], strlen(locale->slots[i])};
        } else {
            value = qjs_intl_locale_keyword(&components, keys[i]);
            if (value.data && !(locale->slots[i] = copy_slice(ctx, value)))
                goto fail;
        }
    }
    status = qjs_intl_locale_replace_keywords(
        &allocator, (QJSIntlBytes){updated, strlen(updated)}, overrides, count,
        &composed);
    if (js_intl_error(ctx, status, "locale keywords"))
        goto fail;
    /* MakeLocaleRecord captures slots before whole-tag canonicalization. */
    status = qjs_intl_locale_canonicalize(
        service, (QJSIntlBytes){composed, strlen(composed)}, &locale->tag);
    if (js_intl_error(ctx, status, "Unicode locale identifier"))
        goto fail;
    JS_SetOpaque(object, locale);
    locale = NULL;
    goto done;
fail:
    JS_FreeValue(ctx, object);
    object = JS_EXCEPTION;
done:
    free_locale(ctx->rt, locale);
    JS_FreeCString(ctx, input_owned);
    for (i = 0; i < OPT_COUNT; i++)
        JS_FreeCString(ctx, owned[i]);
    JS_FreeValue(ctx, options);
    js_free(ctx, canonical);
    js_free(ctx, updated);
    js_free(ctx, composed);
    return object;
}

enum {
    GET_BASE = SLOT_COUNT,
    GET_LANGUAGE,
    GET_SCRIPT,
    GET_REGION,
    GET_VARIANTS
};

static JSValue locale_getter(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSIntlLocale *locale = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LOCALE);
    QJSIntlLocaleComponents parts;
    QJSIntlBytes value = {0};

    if (!locale)
        return JS_EXCEPTION;
    if (magic == SLOT_KN)
        return JS_NewBool(ctx, locale->slots[SLOT_KN] &&
                                   (!*locale->slots[SLOT_KN] ||
                                    !strcmp(locale->slots[SLOT_KN], "true")));
    if (magic < SLOT_COUNT)
        return locale->slots[magic] ? JS_NewString(ctx, locale->slots[magic])
                                    : JS_UNDEFINED;
    if (js_intl_error(
            ctx,
            qjs_intl_locale_components(
                (QJSIntlBytes){locale->tag, strlen(locale->tag)}, &parts),
            "locale components"))
        return JS_EXCEPTION;
    switch (magic) {
    case GET_BASE:
        value = parts.base_name;
        break;
    case GET_LANGUAGE:
        value = parts.language;
        break;
    case GET_SCRIPT:
        value = parts.script;
        break;
    case GET_REGION:
        value = parts.region;
        break;
    case GET_VARIANTS:
        value = parts.variants;
        break;
    }
    return value.data ? JS_NewStringLen(ctx, value.data, value.length)
                      : JS_UNDEFINED;
}

static JSValue locale_to_string(JSContext *ctx, JSValueConst this_val, int argc,
                                JSValueConst *argv)
{
    JSIntlLocale *locale = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LOCALE);
    return locale ? JS_NewString(ctx, locale->tag) : JS_EXCEPTION;
}

static JSValue locale_likely(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv, int minimize)
{
    JSIntlLocale *locale = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LOCALE);
    QJSIntlLocaleService *service;
    QJSIntlBytes input;
    QJSIntlStatus status;
    char *tag;
    JSValue argument, result;

    if (!locale)
        return JS_EXCEPTION;
    service = js_intl_locale_service(ctx);
    if (!service)
        return JS_EXCEPTION;
    input = (QJSIntlBytes){locale->tag, strlen(locale->tag)};
    status = minimize ? qjs_intl_locale_minimize(service, input, &tag)
                      : qjs_intl_locale_maximize(service, input, &tag);
    if (js_intl_error(ctx, status, "likely subtags"))
        return JS_EXCEPTION;
    argument = JS_NewString(ctx, tag);
    js_free(ctx, tag);
    if (JS_IsException(argument))
        return argument;
    {
        JSValueConst borrowed_argument = argument;
        result = JS_CallConstructor(ctx, ctx->intl_locale_ctor, 1,
                                    &borrowed_argument);
    }
    JS_FreeValue(ctx, argument);
    return result;
}

enum { INFO_WEEK, INFO_TEXT, INFO_HOURS };

static JSValue locale_info(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv, int magic)
{
    JSIntlLocale *locale = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LOCALE);
    QJSIntlLocaleService *service;
    QJSIntlBytes tag;
    JSValue result, value = JS_UNDEFINED;

    if (!locale)
        return JS_EXCEPTION;
    service = js_intl_locale_service(ctx);
    if (!service)
        return JS_EXCEPTION;
    tag = (QJSIntlBytes){locale->tag, strlen(locale->tag)};
    if (magic == INFO_HOURS) {
        QJSIntlBytes cycles;
        uint32_t count, i;
        size_t start = 0;
        result = JS_NewArray(ctx);
        if (JS_IsException(result))
            return result;
        if (locale->slots[SLOT_HC]) {
            value = JS_NewString(ctx, locale->slots[SLOT_HC]);
            if (JS_IsException(value) ||
                JS_DefinePropertyValueUint32(ctx, result, 0, value,
                                             JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto fail;
            return result;
        }
        if (js_intl_error(ctx,
                          qjs_intl_locale_hour_cycles(service, tag, &cycles, &count),
                          "locale hour cycles"))
            goto fail;
        for (i = 0; i < count; i++) {
            size_t end = start;
            while (end < cycles.length && cycles.data[end] != ' ')
                end++;
            value = JS_NewStringLen(ctx, cycles.data + start, end - start);
            if (JS_IsException(value) ||
                JS_DefinePropertyValueUint32(ctx, result, i, value,
                                             JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto fail;
            start = end + 1;
        }
        return result;
    }
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    if (magic == INFO_TEXT) {
        unsigned int direction;
        if (js_intl_error(ctx, qjs_intl_locale_text_direction(service, tag, &direction),
                          "locale text direction"))
            goto fail;
        if (direction != QJS_INTL_DIRECTION_UNKNOWN)
            value =
                JS_NewString(ctx, direction == QJS_INTL_DIRECTION_RTL ? "rtl" : "ltr");
        if (JS_IsException(value) ||
            JS_DefinePropertyValueStr(ctx, result, "direction", value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
    } else {
        QJSIntlWeekInfo week;
        unsigned int day, index = 0;
        QJSIntlBytes first =
            locale->slots[SLOT_FW]
                ? (QJSIntlBytes){locale->slots[SLOT_FW], strlen(locale->slots[SLOT_FW])}
                : (QJSIntlBytes){NULL, 0};
        if (js_intl_error(ctx, qjs_intl_locale_week_info(service, tag, first, &week),
                          "locale week information"))
            goto fail;
        if (JS_DefinePropertyValueStr(ctx, result, "firstDay",
                                      JS_NewInt32(ctx, week.first_day),
                                      JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
        value = JS_NewArray(ctx);
        if (JS_IsException(value))
            goto fail;
        for (day = 1; day <= 7; day++) {
            if ((week.weekend >> (day - 1)) & 1) {
                if (JS_DefinePropertyValueUint32(ctx, value, index++,
                                                 JS_NewInt32(ctx, day),
                                                 JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
                    JS_FreeValue(ctx, value);
                    goto fail;
                }
            }
        }
        if (JS_DefinePropertyValueStr(ctx, result, "weekend", value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
    }
    return result;
fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static const JSCFunctionListEntry locale_properties[] = {
    JS_CGETSET_MAGIC_DEF("baseName", locale_getter, NULL, GET_BASE),
    JS_CGETSET_MAGIC_DEF("language", locale_getter, NULL, GET_LANGUAGE),
    JS_CGETSET_MAGIC_DEF("script", locale_getter, NULL, GET_SCRIPT),
    JS_CGETSET_MAGIC_DEF("region", locale_getter, NULL, GET_REGION),
    JS_CGETSET_MAGIC_DEF("variants", locale_getter, NULL, GET_VARIANTS),
    JS_CGETSET_MAGIC_DEF("calendar", locale_getter, NULL, SLOT_CA),
    JS_CGETSET_MAGIC_DEF("collation", locale_getter, NULL, SLOT_CO),
    JS_CGETSET_MAGIC_DEF("firstDayOfWeek", locale_getter, NULL, SLOT_FW),
    JS_CGETSET_MAGIC_DEF("hourCycle", locale_getter, NULL, SLOT_HC),
    JS_CGETSET_MAGIC_DEF("caseFirst", locale_getter, NULL, SLOT_KF),
    JS_CGETSET_MAGIC_DEF("numeric", locale_getter, NULL, SLOT_KN),
    JS_CGETSET_MAGIC_DEF("numberingSystem", locale_getter, NULL, SLOT_NU),
    JS_CFUNC_DEF("toString", 0, locale_to_string),
    JS_CFUNC_MAGIC_DEF("maximize", 0, locale_likely, 0),
    JS_CFUNC_MAGIC_DEF("minimize", 0, locale_likely, 1),
    JS_CFUNC_MAGIC_DEF("getWeekInfo", 0, locale_info, INFO_WEEK),
    JS_CFUNC_MAGIC_DEF("getTextInfo", 0, locale_info, INFO_TEXT),
    JS_CFUNC_MAGIC_DEF("getHourCycles", 0, locale_info, INFO_HOURS),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.Locale",
                       JS_PROP_CONFIGURABLE),
};

int js_intl_init_locale(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.Locale",
        .finalizer = locale_finalizer,
    };
    JSValue constructor;

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_LOCALE) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_LOCALE, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(
        ctx, JS_CLASS_INTL_LOCALE, "Locale", locale_constructor, 1,
        JS_CFUNC_constructor, 0, JS_UNDEFINED, NULL, 0, locale_properties,
        countof(locale_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    if (JS_DefinePropertyValueStr(
            ctx, namespace_object, "Locale", JS_DupValue(ctx, constructor),
            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0) {
        JS_FreeValue(ctx, constructor);
        return -1;
    }
    ctx->intl_locale_ctor = constructor;
    return 0;
}
