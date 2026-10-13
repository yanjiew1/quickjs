/* Native Intl.Locale, ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd.
 * Reviewed 2026-10-08. Stable anchors: sec-intl-locale-constructor,
 * sec-properties-of-intl-locale-prototype-object, sec-intl-locale-abstracts. */
#include "locale-private.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)

enum { LOC_CA, LOC_CO, LOC_FW, LOC_HC, LOC_KF, LOC_KN, LOC_NU, LOC_SLOT_COUNT };
static const char *const locale_keys[] = { "ca", "co", "fw", "hc", "kf", "kn", "nu" };
typedef struct JSIntlLocale {
    char *locale;
    char *slots[LOC_SLOT_COUNT];
    BOOL numeric;
} JSIntlLocale;
const char *js_intl_locale_tag(JSValueConst value)
{
    JSIntlLocale *loc = JS_GetOpaque(value, JS_CLASS_INTL_LOCALE);
    return loc ? loc->locale : NULL;
}
static void locale_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlLocale *loc = JS_GetOpaque(value, JS_CLASS_INTL_LOCALE); int i;
    if (!loc) return;
    js_free_rt(rt, loc->locale); for (i = 0; i < LOC_SLOT_COUNT; i++) js_free_rt(rt, loc->slots[i]); js_free_rt(rt, loc);
}
static JSIntlLocale *locale_brand(JSContext *ctx, JSValueConst value)
{
    return JS_GetOpaque2(ctx, value, JS_CLASS_INTL_LOCALE);
}
static int alpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int digit(int c) { return c >= '0' && c <= '9'; }
static int field_valid(const char *s, int kind)
{
    size_t n = strlen(s), i;
    if (kind == 0 && !(n == 2 || n == 3 || (n >= 5 && n <= 8))) return 0;
    if (kind == 1 && n != 4) return 0;
    if (kind == 2 && n != 2 && n != 3) return 0;
    for (i = 0; i < n; i++) if (!(kind == 2 && n == 3 ? digit((unsigned char)s[i]) : alpha((unsigned char)s[i]))) return 0;
    return 1;
}
static int update_field(JSContext *ctx, JSValueConst options, const char *property, char **field, int kind)
{
    char *value; size_t i;
    if (js_intl_get_string_option_alloc(ctx, options, property, &value) < 0) return -1;
    if (!value) return 0;
    if (!field_valid(value, kind)) { js_free(ctx, value); JS_ThrowRangeError(ctx, "invalid Intl.Locale %s", property); return -1; }
    for (i = 0; value[i]; i++) if (value[i] >= 'A' && value[i] <= 'Z') value[i] += 32;
    js_free(ctx, *field); *field = value; return 0;
}
static int update_variants(JSContext *ctx, JSValueConst options, IntlLanguageId *base)
{
    char *variants, *synthetic; size_t n; IntlTag parsed;
    if (js_intl_get_string_option_alloc(ctx, options, "variants", &variants) < 0) return -1;
    if (!variants) return 0;
    n = strlen(variants);
    if (!n) { js_free(ctx, variants); JS_ThrowRangeError(ctx, "invalid Intl.Locale variants"); return -1; }
    if (n > SIZE_MAX - 16) { js_free(ctx, variants); JS_ThrowOutOfMemory(ctx); return -1; }
    /* Prefix by a complete language, script and region so every subsequent
     * token must be a variant. Extensions and private use are rejected. */
    synthetic = js_malloc(ctx, n + 16);
    if (!synthetic) { js_free(ctx, variants); return -1; }
    strcpy(synthetic, "und-Latn-US-"); strcpy(synthetic + 12, variants);
    if (intl_parse_tag(ctx, synthetic, n + 12, &parsed) < 0) { js_free(ctx, synthetic); js_free(ctx, variants); return -1; }
    js_free(ctx, synthetic); js_free(ctx, variants);
    if (parsed.count || parsed.private_use || !parsed.base.variants.count) { intl_tag_free(ctx, &parsed); JS_ThrowRangeError(ctx, "invalid Intl.Locale variants"); return -1; }
    js_intl_locale_list_free(ctx, &base->variants); base->variants = parsed.base.variants;
    memset(&parsed.base.variants, 0, sizeof(parsed.base.variants)); intl_tag_free(ctx, &parsed); return 0;
}
static JSValue locale_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    JSValue object = JS_EXCEPTION, options = JS_UNDEFINED; JSIntlLocale *loc = NULL; IntlTag parsed = { 0 };
    const char *input, *cstr = NULL; size_t length; char *canonical = NULL, *updated = NULL, *value = NULL; int i, index, numeric;
    static const char *const cycles[] = { "h11", "h12", "h23", "h24" };
    static const char *const cases[] = { "upper", "lower", "false" };
    static const char *const weekdays[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat", "sun" };
    JSValueConst tag_value = argc ? argv[0] : JS_UNDEFINED;
    if (JS_IsUndefined(new_target)) return JS_ThrowTypeError(ctx, "Intl.Locale requires new");
    object = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_LOCALE); if (JS_IsException(object)) return object;
    if (!JS_IsString(tag_value) && !JS_IsObject(tag_value)) { JS_ThrowTypeError(ctx, "locale tag must be a string or object"); goto fail; }
    input = js_intl_locale_tag(tag_value);
    if (input) length = strlen(input);
    else { input = cstr = JS_ToCStringLen(ctx, &length, tag_value); if (!input) goto fail; }
    /* Coerce options before checking language grammar, preserving getter and
     * conversion order from the living constructor. */
    options = js_intl_coerce_options(ctx, argc > 1 ? argv[1] : JS_UNDEFINED); if (JS_IsException(options)) goto fail;
    canonical = js_intl_canonicalize_tag(ctx, input, length); if (!canonical) goto fail;
    if (intl_parse_tag(ctx, canonical, strlen(canonical), &parsed) < 0) goto fail;
    if (update_field(ctx, options, "language", &parsed.base.language, 0) < 0 ||
        update_field(ctx, options, "script", &parsed.base.script, 1) < 0 ||
        update_field(ctx, options, "region", &parsed.base.region, 2) < 0 || update_variants(ctx, options, &parsed.base) < 0) goto fail;
    loc = js_mallocz(ctx, sizeof(*loc)); if (!loc) goto fail;
    for (i = LOC_CA; i <= LOC_FW; i++) {
        const char *property = i == LOC_CA ? "calendar" : i == LOC_CO ? "collation" : "firstDayOfWeek";
        if (js_intl_get_string_option_alloc(ctx, options, property, &value) < 0) goto fail;
        if (value) {
            if (i == LOC_FW && strlen(value) == 1 && value[0] >= '0' && value[0] <= '7') {
                int day = value[0] - '0'; js_free(ctx, value); value = js_intl_strdup(ctx, weekdays[day]); if (!value) goto fail;
            }
            if (!js_intl_is_unicode_type(value)) { JS_ThrowRangeError(ctx, "invalid Intl.Locale %s", property); goto fail; }
            loc->slots[i] = intl_canonicalize_uvalue(ctx, locale_keys[i], value); js_free(ctx, value); value = NULL;
            if (!loc->slots[i] || intl_tag_set_keyword(ctx, &parsed, locale_keys[i], loc->slots[i]) < 0) goto fail;
        }
    }
    if (js_intl_get_string_option(ctx, options, "hourCycle", cycles, 4, -1, &index) < 0) goto fail;
    if (index >= 0) { loc->slots[LOC_HC] = js_intl_strdup(ctx, cycles[index]); if (!loc->slots[LOC_HC] || intl_tag_set_keyword(ctx, &parsed, "hc", loc->slots[LOC_HC]) < 0) goto fail; }
    if (js_intl_get_string_option(ctx, options, "caseFirst", cases, 3, -1, &index) < 0) goto fail;
    if (index >= 0) { loc->slots[LOC_KF] = js_intl_strdup(ctx, cases[index]); if (!loc->slots[LOC_KF] || intl_tag_set_keyword(ctx, &parsed, "kf", loc->slots[LOC_KF]) < 0) goto fail; }
    if (js_intl_get_bool_option(ctx, options, "numeric", -1, &numeric) < 0) goto fail;
    if (numeric >= 0) { loc->slots[LOC_KN] = js_intl_strdup(ctx, numeric ? "" : "false"); if (!loc->slots[LOC_KN] || intl_tag_set_keyword(ctx, &parsed, "kn", loc->slots[LOC_KN]) < 0) goto fail; }
    if (js_intl_get_string_option_alloc(ctx, options, "numberingSystem", &value) < 0) goto fail;
    if (value) {
        if (!js_intl_is_unicode_type(value)) { JS_ThrowRangeError(ctx, "invalid Intl.Locale numberingSystem"); goto fail; }
        loc->slots[LOC_NU] = intl_canonicalize_uvalue(ctx, "nu", value); js_free(ctx, value); value = NULL;
        if (!loc->slots[LOC_NU] || intl_tag_set_keyword(ctx, &parsed, "nu", loc->slots[LOC_NU]) < 0) goto fail;
    }
    /* MakeLocaleRecord retains each key value before the final LanguageId
     * canonicalization. CanonicalizeUValue already ran for overrides. */
    for (i = 0; i < LOC_SLOT_COUNT; i++) if (!loc->slots[i]) {
        const char *inherited = intl_tag_keyword(&parsed, locale_keys[i]);
        if (inherited && !(loc->slots[i] = js_intl_strdup(ctx, inherited))) goto fail;
    }
    loc->numeric = loc->slots[LOC_KN] && (!*loc->slots[LOC_KN] || !strcmp(loc->slots[LOC_KN], "true"));
    updated = intl_tag_string(ctx, &parsed); if (!updated) goto fail;
    loc->locale = js_intl_canonicalize_tag(ctx, updated, strlen(updated)); if (!loc->locale) goto fail;
    JS_SetOpaque(object, loc); loc = NULL;
    JS_FreeCString(ctx, cstr); JS_FreeValue(ctx, options); js_free(ctx, canonical); js_free(ctx, updated); intl_tag_free(ctx, &parsed); return object;
fail:
    JS_FreeCString(ctx, cstr); JS_FreeValue(ctx, options); js_free(ctx, canonical); js_free(ctx, updated); js_free(ctx, value); intl_tag_free(ctx, &parsed);
    if (loc) { for (i = 0; i < LOC_SLOT_COUNT; i++) js_free(ctx, loc->slots[i]); js_free(ctx, loc->locale); js_free(ctx, loc); }
    JS_FreeValue(ctx, object); return JS_EXCEPTION;
}
enum { GET_BASE = LOC_SLOT_COUNT, GET_LANGUAGE, GET_SCRIPT, GET_REGION, GET_VARIANTS, GET_NUMERIC };
static JSValue locale_getter(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSIntlLocale *loc = locale_brand(ctx, this_val); IntlTag parsed; const char *value = NULL; JSValue result; char *owned = NULL;
    if (!loc) return JS_EXCEPTION;
    if (magic < LOC_SLOT_COUNT) return loc->slots[magic] ? JS_NewString(ctx, loc->slots[magic]) : JS_UNDEFINED;
    if (magic == GET_NUMERIC) return JS_NewBool(ctx, loc->numeric);
    if (intl_parse_tag(ctx, loc->locale, strlen(loc->locale), &parsed) < 0) return JS_EXCEPTION;
    if (magic == GET_BASE) owned = intl_language_string(ctx, &parsed.base);
    else if (magic == GET_LANGUAGE) value = parsed.base.language;
    else if (magic == GET_SCRIPT && parsed.base.script) {
        owned = js_intl_strdup(ctx, parsed.base.script);
        if (owned) owned[0] -= 32;
    } else if (magic == GET_REGION && parsed.base.region) {
        size_t i; owned = js_intl_strdup(ctx, parsed.base.region);
        if (owned) for (i = 0; owned[i]; i++) if (owned[i] >= 'a' && owned[i] <= 'z') owned[i] -= 32;
    }
    else if (magic == GET_VARIANTS && parsed.base.variants.count) {
        size_t i, length = 0; char *p;
        for (i = 0; i < parsed.base.variants.count; i++) length += strlen(parsed.base.variants.items[i]) + !!i;
        owned = js_malloc(ctx, length + 1);
        if (owned) { p = owned; for (i = 0; i < parsed.base.variants.count; i++) { size_t n = strlen(parsed.base.variants.items[i]); if (i) *p++ = '-'; memcpy(p, parsed.base.variants.items[i], n); p += n; } *p = 0; }
    }
    if ((magic == GET_BASE || (magic == GET_VARIANTS && parsed.base.variants.count) ||
         (magic == GET_SCRIPT && parsed.base.script) || (magic == GET_REGION && parsed.base.region)) && !owned) result = JS_EXCEPTION;
    else result = owned || value ? JS_NewString(ctx, owned ? owned : value) : JS_UNDEFINED;
    js_free(ctx, owned); intl_tag_free(ctx, &parsed); return result;
}
static JSValue locale_to_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlLocale *loc = locale_brand(ctx, this_val); return loc ? JS_NewString(ctx, loc->locale) : JS_EXCEPTION;
}
static char *likely_tag(JSContext *ctx, const char *tag, BOOL minimize)
{
    QJSIntlProvider *provider = js_intl_native_provider(ctx);
    QJSIntlStatus status;
    char *result = NULL;
    if (!provider) return NULL;
    status = minimize ? qjs_intl_locale_minimize(provider,
        (QJSIntlBytes){ tag, strlen(tag) }, &result) :
        qjs_intl_locale_maximize(provider,
        (QJSIntlBytes){ tag, strlen(tag) }, &result);
    if (js_intl_native_error(ctx, status, "likely subtags")) return NULL;
    return result;
}
static JSValue locale_likely(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
    JSIntlLocale *loc = locale_brand(ctx, this_val); char *tag; JSValue argument, result;
    if (!loc) return JS_EXCEPTION;
    tag = likely_tag(ctx, loc->locale, magic); if (!tag) return JS_EXCEPTION;
    argument = JS_NewString(ctx, tag); js_free(ctx, tag); if (JS_IsException(argument)) return argument;
    JSValueConst borrowed_argument = argument;
    result = JS_CallConstructor(ctx, js_intl_constructor(ctx, JS_CLASS_INTL_LOCALE),
                                1, &borrowed_argument);
    JS_FreeValue(ctx, argument);
    return result;
}
enum { INFO_CALENDARS, INFO_COLLATIONS, INFO_HOURS, INFO_NUMBERS,
       INFO_TIMEZONES, INFO_TEXT, INFO_WEEK };
static QJSIntlBytes locale_slot(const char *value)
{
    return (QJSIntlBytes){ value, value ? strlen(value) : 0 };
}
static JSValue locale_info(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv, int magic)
{
    JSIntlLocale *loc = locale_brand(ctx, this_val);
    QJSIntlProvider *provider;
    QJSIntlLocaleInfoRequest request = { 0 };
    QJSIntlLocaleInfoResult native = { 0 };
    QJSIntlLocaleInfoField field;
    QJSIntlStatus status;
    JSValue result = JS_EXCEPTION;
    size_t i;
    if (!loc) return JS_EXCEPTION;
    provider = js_intl_native_provider(ctx);
    if (!provider) return JS_EXCEPTION;
    request.locale = locale_slot(loc->locale);
    request.calendar = locale_slot(loc->slots[LOC_CA]);
    request.collation = locale_slot(loc->slots[LOC_CO]);
    request.hour_cycle = locale_slot(loc->slots[LOC_HC]);
    request.numbering_system = locale_slot(loc->slots[LOC_NU]);
    request.first_day = locale_slot(loc->slots[LOC_FW]);
    /* Field enums are mapped explicitly; no cast between independent owners. */
    switch (magic) {
    case INFO_CALENDARS: field = QJS_INTL_LOCALE_CALENDARS; break;
    case INFO_COLLATIONS: field = QJS_INTL_LOCALE_COLLATIONS; break;
    case INFO_HOURS: field = QJS_INTL_LOCALE_HOUR_CYCLES; break;
    case INFO_NUMBERS: field = QJS_INTL_LOCALE_NUMBERING_SYSTEMS; break;
    case INFO_TIMEZONES: field = QJS_INTL_LOCALE_TIME_ZONES; break;
    case INFO_TEXT: field = QJS_INTL_LOCALE_TEXT_INFO; break;
    case INFO_WEEK: field = QJS_INTL_LOCALE_WEEK_INFO; break;
    default: return JS_ThrowInternalError(ctx, "invalid Locale information field");
    }
    status = qjs_intl_locale_info_get(provider, &request, field, &native);
    if (js_intl_native_error(ctx, status, "Locale information")) goto done;
    if (magic == INFO_TEXT) {
        result = JS_NewObject(ctx);
        if (JS_IsException(result)) goto done;
        if (native.defined) {
            const char *direction = native.direction == QJS_INTL_LOCALE_DIRECTION_LTR
                ? "ltr" : native.direction == QJS_INTL_LOCALE_DIRECTION_RTL ? "rtl" : NULL;
            if (!direction) {
                JS_ThrowInternalError(ctx, "invalid native script direction");
                goto fail;
            }
            if (js_intl_define_string(ctx, result, "direction", direction) < 0)
                goto fail;
        } else if (JS_DefinePropertyValueStr(ctx, result, "direction", JS_UNDEFINED,
                                            JS_PROP_C_W_E) < 0) {
            goto fail;
        }
    } else if (magic == INFO_WEEK) {
        JSValue weekend;
        uint32_t index = 0;
        if (native.first_day < 1 || native.first_day > 7 ||
            (native.weekend_mask & 0x80)) {
            JS_ThrowInternalError(ctx, "invalid native week information");
            goto done;
        }
        result = JS_NewObject(ctx);
        if (JS_IsException(result)) goto done;
        if (js_intl_define_int(ctx, result, "firstDay", native.first_day) < 0)
            goto fail;
        weekend = JS_NewArray(ctx);
        if (JS_IsException(weekend)) goto fail;
        for (i = 0; i < 7; i++) {
            if ((native.weekend_mask & (1u << i)) &&
                JS_DefinePropertyValueUint32(ctx, weekend, index++,
                    JS_NewInt32(ctx, i + 1), JS_PROP_C_W_E) < 0) {
                JS_FreeValue(ctx, weekend);
                goto fail;
            }
        }
        if (JS_DefinePropertyValueStr(ctx, result, "weekend", weekend,
                                      JS_PROP_C_W_E) < 0) goto fail;
    } else if (!native.defined) {
        result = JS_UNDEFINED;
    } else {
        if (native.list.count > INT64_MAX) {
            JS_ThrowOutOfMemory(ctx);
            goto done;
        }
        result = JS_NewArray(ctx);
        if (JS_IsException(result)) goto done;
        for (i = 0; i < native.list.count; i++) {
            QJSIntlBytes item = native.list.items[i];
            JSValue value;
            if (!item.data || item.length > JS_STRING_LEN_MAX) {
                JS_ThrowInternalError(ctx, "invalid native Locale list");
                goto fail;
            }
            value = JS_NewStringLen(ctx, item.data, item.length);
            if (JS_IsException(value) || JS_DefinePropertyValueInt64(ctx, result,
                    i, value, JS_PROP_C_W_E) < 0) goto fail;
        }
    }
    goto done;
fail:
    JS_FreeValue(ctx, result);
    result = JS_EXCEPTION;
done:
    qjs_intl_locale_info_result_clear(provider, &native);
    return result;
}
static const JSCFunctionListEntry locale_prototype_functions[] = {
    JS_CGETSET_MAGIC_DEF("baseName", locale_getter, NULL, GET_BASE),
    JS_CGETSET_MAGIC_DEF("calendar", locale_getter, NULL, LOC_CA),
    JS_CGETSET_MAGIC_DEF("caseFirst", locale_getter, NULL, LOC_KF),
    JS_CGETSET_MAGIC_DEF("collation", locale_getter, NULL, LOC_CO),
    JS_CGETSET_MAGIC_DEF("firstDayOfWeek", locale_getter, NULL, LOC_FW),
    JS_CGETSET_MAGIC_DEF("hourCycle", locale_getter, NULL, LOC_HC),
    JS_CGETSET_MAGIC_DEF("language", locale_getter, NULL, GET_LANGUAGE),
    JS_CGETSET_MAGIC_DEF("numberingSystem", locale_getter, NULL, LOC_NU),
    JS_CGETSET_MAGIC_DEF("numeric", locale_getter, NULL, GET_NUMERIC),
    JS_CGETSET_MAGIC_DEF("region", locale_getter, NULL, GET_REGION),
    JS_CGETSET_MAGIC_DEF("script", locale_getter, NULL, GET_SCRIPT),
    JS_CGETSET_MAGIC_DEF("variants", locale_getter, NULL, GET_VARIANTS),
    JS_CFUNC_DEF("toString", 0, locale_to_string),
    JS_CFUNC_MAGIC_DEF("maximize", 0, locale_likely, FALSE),
    JS_CFUNC_MAGIC_DEF("minimize", 0, locale_likely, TRUE),
    JS_CFUNC_MAGIC_DEF("getCalendars", 0, locale_info, INFO_CALENDARS),
    JS_CFUNC_MAGIC_DEF("getCollations", 0, locale_info, INFO_COLLATIONS),
    JS_CFUNC_MAGIC_DEF("getHourCycles", 0, locale_info, INFO_HOURS),
    JS_CFUNC_MAGIC_DEF("getNumberingSystems", 0, locale_info, INFO_NUMBERS),
    JS_CFUNC_MAGIC_DEF("getTimeZones", 0, locale_info, INFO_TIMEZONES),
    JS_CFUNC_MAGIC_DEF("getTextInfo", 0, locale_info, INFO_TEXT),
    JS_CFUNC_MAGIC_DEF("getWeekInfo", 0, locale_info, INFO_WEEK),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.Locale", JS_PROP_CONFIGURABLE),
};
static const JSClassDef locale_class = { "Intl.Locale", .finalizer = locale_finalizer };
int js_intl_init_locale(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_LOCALE, &locale_class,
        "Locale", locale_constructor, 1, JS_CFUNC_constructor,
        NULL, 0, locale_prototype_functions, countof(locale_prototype_functions));
}
#endif
