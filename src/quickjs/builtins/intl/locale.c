/* Native Intl.Locale, ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd.
 * Reviewed 2026-10-08. Stable anchors: sec-intl-locale-constructor,
 * sec-properties-of-intl-locale-prototype-object, sec-intl-locale-abstracts. */
#include "locale-private.h"
#ifdef CONFIG_INTL
#include <unicode/ucal.h>
#include <unicode/ucol.h>
#include <unicode/uenum.h>
#include <unicode/uscript.h>

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
    char *icu = js_intl_locale_to_icu(ctx, tag), *transformed, *result; UErrorCode status = U_ZERO_ERROR; int32_t length;
    if (!icu) return NULL;
    length = minimize ? uloc_minimizeSubtags(icu, NULL, 0, &status) : uloc_addLikelySubtags(icu, NULL, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { js_free(ctx, icu); if (status == U_MEMORY_ALLOCATION_ERROR) { js_intl_icu_error(ctx, status, "likely subtags"); return NULL; } return js_intl_strdup(ctx, tag); }
    transformed = js_intl_alloc_char(ctx, length); if (!transformed) { js_free(ctx, icu); return NULL; }
    status = U_ZERO_ERROR;
    if (minimize) uloc_minimizeSubtags(icu, transformed, length + 1, &status); else uloc_addLikelySubtags(icu, transformed, length + 1, &status);
    js_free(ctx, icu);
    if (U_FAILURE(status)) { js_free(ctx, transformed); if (status == U_MEMORY_ALLOCATION_ERROR) { js_intl_icu_error(ctx, status, "likely subtags"); return NULL; } return js_intl_strdup(ctx, tag); }
    result = js_intl_locale_from_icu(ctx, transformed); js_free(ctx, transformed); return result;
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
static char *subdivision_region(JSContext *ctx, const IntlTag *tag, const char *key)
{
    const char *s = intl_tag_keyword(tag, key); size_t n, prefix, i; char region_tag[8]; char *canonical; IntlTag parsed; char *result;
    if (!s) return NULL;
    n = strlen(s); prefix = n >= 2 && alpha(s[0]) && alpha(s[1]) ? 2 : n >= 3 && digit(s[0]) && digit(s[1]) && digit(s[2]) ? 3 : 0;
    if (!prefix || n <= prefix || n > prefix + 4) return NULL;
    if (n == prefix + 4 && strcmp(s + prefix, "zzzz")) return NULL;
    for (i = prefix; i < n; i++) if (!alpha(s[i]) && !digit(s[i])) return NULL;
    strcpy(region_tag, "und-"); memcpy(region_tag + 4, s, prefix); region_tag[4 + prefix] = 0;
    canonical = js_intl_canonicalize_tag(ctx, region_tag, 4 + prefix); if (!canonical) return NULL;
    if (intl_parse_tag(ctx, canonical, strlen(canonical), &parsed) < 0) { js_free(ctx, canonical); return NULL; }
    result = parsed.base.region ? js_intl_strdup(ctx, parsed.base.region) : NULL; intl_tag_free(ctx, &parsed); js_free(ctx, canonical); return result;
}
static int region_preference(JSContext *ctx, const JSIntlLocale *loc, char **region, char **override, char **language)
{
    IntlTag parsed; char *maximal = NULL; IntlTag max = { 0 };
    *region = *override = *language = NULL;
    if (intl_parse_tag(ctx, loc->locale, strlen(loc->locale), &parsed) < 0) return -1;
    *language = js_intl_strdup(ctx, parsed.base.language); if (!*language) goto fail;
    if (parsed.base.region) *region = js_intl_strdup(ctx, parsed.base.region);
    else *region = subdivision_region(ctx, &parsed, "sd");
    if (JS_HasException(ctx)) goto fail;
    if (!*region) {
        maximal = likely_tag(ctx, loc->locale, FALSE); if (!maximal) goto fail;
        if (intl_parse_tag(ctx, maximal, strlen(maximal), &max) < 0) goto fail;
        *region = js_intl_strdup(ctx, max.base.region ? max.base.region : "001"); if (!*region) goto fail;
    }
    *override = subdivision_region(ctx, &parsed, "rg"); if (JS_HasException(ctx)) goto fail;
    js_free(ctx, maximal); intl_tag_free(ctx, &max); intl_tag_free(ctx, &parsed); return 0;
fail:
    js_free(ctx, maximal); intl_tag_free(ctx, &max); intl_tag_free(ctx, &parsed); js_free(ctx, *region); js_free(ctx, *override); js_free(ctx, *language); *region = *override = *language = NULL; return -1;
}
static UResourceBundle *preference_resource(JSContext *ctx, const char *table, const char *language, const char *region, const char *override)
{
    UResourceBundle *bundle = NULL, *data = NULL, *entry = NULL; UErrorCode status = U_ZERO_ERROR; int i;
    const char *regions[2] = { override ? override : region, override ? region : NULL };
    bundle = ures_openDirect(NULL, "supplementalData", &status); data = ures_getByKey(bundle, table, NULL, &status);
    if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale preference data"); goto done; }
    for (i = 0; i < 2 && regions[i] && !entry; i++) {
        char combined[24], country[4]; size_t j;
        strcpy(country, regions[i]); for (j = 0; country[j]; j++) if (country[j] >= 'a' && country[j] <= 'z') country[j] -= 32;
        snprintf(combined, sizeof(combined), "%s_%s", language, country);
        status = U_ZERO_ERROR; entry = ures_getByKey(data, combined, NULL, &status);
        if (status == U_MISSING_RESOURCE_ERROR) { status = U_ZERO_ERROR; entry = ures_getByKey(data, country, NULL, &status); }
        if (status == U_MISSING_RESOURCE_ERROR) { status = U_ZERO_ERROR; entry = NULL; }
        if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale preference data"); ures_close(entry); entry = NULL; goto done; }
    }
done:
    ures_close(data); ures_close(bundle); return entry;
}
static int list_has_value(const JSIntlLocaleList *list, const char *s)
{
    size_t i; for (i = 0; i < list->count; i++) if (!strcmp(list->items[i], s)) return 1; return 0;
}
static char *resource_text(JSContext *ctx, UResourceBundle *resource, int index)
{
    UErrorCode status = U_ZERO_ERROR; const UChar *s; int32_t n, i; char *r;
    s = ures_getStringByIndex(resource, index, &n, &status);
    if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale preference"); return NULL; }
    r = js_intl_alloc_char(ctx, n); if (!r) return NULL;
    for (i = 0; i < n; i++) { if (s[i] > 127) { js_free(ctx, r); JS_ThrowInternalError(ctx, "non-ASCII locale preference"); return NULL; } r[i] = s[i]; } r[n] = 0; return r;
}
enum { INFO_CALENDARS, INFO_COLLATIONS, INFO_HOURS, INFO_NUMBERS, INFO_TIMEZONES, INFO_TEXT, INFO_WEEK };
static JSValue locale_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
    JSIntlLocale *loc = locale_brand(ctx, this_val); JSIntlLocaleList list = { 0 }, available = { 0 }; IntlTag parsed = { 0 };
    char *region = NULL, *override = NULL, *language = NULL, *icu = NULL; UResourceBundle *resource = NULL; JSValue result = JS_EXCEPTION; UErrorCode status = U_ZERO_ERROR; int i;
    if (!loc) return JS_EXCEPTION;
    if (magic == INFO_CALENDARS && loc->slots[LOC_CA]) { if (js_intl_locale_list_append(ctx, &list, loc->slots[LOC_CA]) < 0) goto done; goto array; }
    if (magic == INFO_COLLATIONS && loc->slots[LOC_CO]) { if (js_intl_locale_list_append(ctx, &list, loc->slots[LOC_CO]) < 0) goto done; goto array; }
    if (magic == INFO_HOURS && loc->slots[LOC_HC]) { if (js_intl_locale_list_append(ctx, &list, loc->slots[LOC_HC]) < 0) goto done; goto array; }
    if (magic == INFO_NUMBERS && loc->slots[LOC_NU]) { if (js_intl_locale_list_append(ctx, &list, loc->slots[LOC_NU]) < 0) goto done; goto array; }
    if (magic == INFO_TIMEZONES) {
        if (intl_parse_tag(ctx, loc->locale, strlen(loc->locale), &parsed) < 0) goto done;
        if (!parsed.base.region) { result = JS_UNDEFINED; goto done; }
        if (js_intl_primary_time_zones(ctx, parsed.base.region, &list) < 0)
            goto done;
        goto array;
    }
    if (magic == INFO_NUMBERS || magic == INFO_COLLATIONS) {
        JSIntlLocaleList requested = { 0 }; JSIntlResolvedLocale resolved = { 0 }; JSIntlResolutionKey key = { magic == INFO_NUMBERS ? "nu" : "co", NULL, FALSE }; int r;
        char *match = intl_lookup_locale(ctx, magic == INFO_NUMBERS ? JS_INTL_NUMBER_FORMAT : JS_INTL_COLLATOR, loc->locale);
        if (!match) {
            if (JS_HasException(ctx)) goto done;
            if (magic == INFO_NUMBERS) { if (js_intl_locale_list_append(ctx, &list, "latn") < 0) goto done; }
            else if (js_intl_locale_list_append(ctx, &list, "emoji") < 0 || js_intl_locale_list_append(ctx, &list, "eor") < 0) goto done;
            goto array;
        }
        js_free(ctx, match);
        if (js_intl_locale_list_append(ctx, &requested, loc->locale) < 0) { js_intl_locale_list_free(ctx, &requested); goto done; }
        r = js_intl_resolve_locale(ctx, magic == INFO_NUMBERS ? JS_INTL_NUMBER_FORMAT : JS_INTL_COLLATOR, &requested, "lookup", &key, 1, &resolved);
        js_intl_locale_list_free(ctx, &requested); if (r < 0) goto done;
        if (magic == INFO_NUMBERS) r = js_intl_locale_list_append(ctx, &list, resolved.values[0]);
        else {
            UEnumeration *e; const char *name; int32_t n;
            e = ucol_getKeywordValuesForLocale("collation", resolved.icu_locale, FALSE, &status);
            if (U_FAILURE(status)) { uenum_close(e); js_intl_icu_error(ctx, status, "locale collations"); r = -1; }
            else { while ((name = uenum_next(e, &n, &status))) {
                const char *canonical = uloc_toUnicodeLocaleType("co", name);
                if (canonical && strcmp(canonical, "standard") && strcmp(canonical, "search") && js_intl_locale_list_append(ctx, &list, canonical) < 0) { r = -1; break; }
            } uenum_close(e); if (js_intl_icu_error(ctx, status, "locale collations") < 0) r = -1; }
            intl_list_sort_unique(ctx, &list);
        }
        js_intl_resolved_locale_free(ctx, &resolved); if (r < 0) goto done; goto array;
    }
    if (magic == INFO_TEXT) {
        char *maximal = NULL; const char *direction = NULL; UScriptCode code; int32_t count;
        if (intl_parse_tag(ctx, loc->locale, strlen(loc->locale), &parsed) < 0) goto done;
        if (!parsed.base.script) {
            intl_tag_free(ctx, &parsed); maximal = likely_tag(ctx, loc->locale, FALSE); if (!maximal) goto done;
            if (intl_parse_tag(ctx, maximal, strlen(maximal), &parsed) < 0) { js_free(ctx, maximal); goto done; } js_free(ctx, maximal);
        }
        if (parsed.base.script) {
            count = uscript_getCode(parsed.base.script, &code, 1, &status);
            if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "script direction"); goto done; }
            if (count == 1 && code != USCRIPT_COMMON &&
                code != USCRIPT_INHERITED && code != USCRIPT_UNKNOWN) {
                UScriptUsage usage = uscript_getUsage(code);
                /* Unknown or unencoded metadata has no text direction. */
                if (usage != USCRIPT_USAGE_UNKNOWN &&
                    usage != USCRIPT_USAGE_NOT_ENCODED)
                    direction = uscript_isRightToLeft(code) ? "rtl" : "ltr";
            }
        }
        result = JS_NewObject(ctx); if (JS_IsException(result)) goto done;
        if ((direction ? js_intl_define_string(ctx, result, "direction", direction) :
            JS_DefinePropertyValueStr(ctx, result, "direction", JS_UNDEFINED, JS_PROP_C_W_E)) < 0) { JS_FreeValue(ctx, result); result = JS_EXCEPTION; } goto done;
    }
    if (region_preference(ctx, loc, &region, &override, &language) < 0) goto done;
    if (magic == INFO_CALENDARS || magic == INFO_HOURS) {
        resource = preference_resource(ctx, magic == INFO_CALENDARS ? "calendarPreferenceData" : "timeData", language, region, override);
        if (JS_HasException(ctx)) goto done;
        if (magic == INFO_CALENDARS) {
            if (intl_values_list(ctx, "calendar", &available) < 0) goto done;
            if (resource) for (i = 0; i < ures_getSize(resource); i++) {
                char *name = resource_text(ctx, resource, i), *canonical;
                if (!name) goto done;
                canonical = intl_canonicalize_uvalue(ctx, "ca", name); js_free(ctx, name); if (!canonical) goto done;
                if (list_has_value(&available, canonical) && !list_has_value(&list, canonical) && js_intl_locale_list_append(ctx, &list, canonical) < 0) { js_free(ctx, canonical); goto done; }
                js_free(ctx, canonical);
            }
            if (!list.count && js_intl_locale_list_append(ctx, &list, "gregory") < 0) goto done;
        } else {
            UResourceBundle *preferred = NULL, *allowed = NULL;
            if (resource) {
                preferred = ures_getByKey(resource, "preferred", NULL, &status); allowed = ures_getByKey(resource, "allowed", NULL, &status);
                if (U_FAILURE(status)) { ures_close(preferred); ures_close(allowed); js_intl_icu_error(ctx, status, "hour cycle preference"); goto done; }
                for (i = -1; i < ures_getSize(allowed); i++) {
                    char *name = resource_text(ctx, i < 0 ? preferred : allowed, i < 0 ? 0 : i); const char *cycle;
                    if (!name) { ures_close(preferred); ures_close(allowed); goto done; }
                    cycle = name[0] == 'h' ? "h12" : name[0] == 'H' ? "h23" : name[0] == 'K' ? "h11" : name[0] == 'k' ? "h24" : NULL;
                    if (cycle && !list_has_value(&list, cycle) && js_intl_locale_list_append(ctx, &list, cycle) < 0) { js_free(ctx, name); ures_close(preferred); ures_close(allowed); goto done; } js_free(ctx, name);
                }
                ures_close(preferred); ures_close(allowed);
            }
            if (!list.count && js_intl_locale_list_append(ctx, &list, "h23") < 0) goto done;
        }
        goto array;
    }
    if (magic == INFO_WEEK) {
        char locale[48]; UCalendar *calendar; JSValue weekend; int first_day, weekday, out = 0;
        const char *country = "001"; size_t j;
        if (override && intl_region_has_week_data(override, &status))
            country = override;
        else if (intl_region_has_week_data(region, &status))
            country = region;
        if (js_intl_icu_error(ctx, status, "week data lookup") < 0)
            goto done;
        snprintf(locale, sizeof(locale), "und_%s@calendar=gregorian", country);
        for (j = 4; locale[j] && locale[j] != '@'; j++) if (locale[j] >= 'a' && locale[j] <= 'z') locale[j] -= 32;
        calendar = ucal_open(NULL, 0, locale, UCAL_GREGORIAN, &status);
        if (U_FAILURE(status)) { ucal_close(calendar); js_intl_icu_error(ctx, status, "week data"); goto done; }
        first_day = ucal_getAttribute(calendar, UCAL_FIRST_DAY_OF_WEEK); first_day = first_day == UCAL_SUNDAY ? 7 : first_day - 1;
        if (loc->slots[LOC_FW]) {
            static const char *const days[] = { "mon", "tue", "wed", "thu", "fri", "sat", "sun" };
            for (i = 0; i < 7; i++) if (!strcmp(loc->slots[LOC_FW], days[i])) first_day = i + 1;
        }
        weekend = JS_NewArray(ctx); if (JS_IsException(weekend)) { ucal_close(calendar); goto done; }
        for (weekday = 1; weekday <= 7; weekday++) {
            UCalendarDaysOfWeek day = weekday == 7 ? UCAL_SUNDAY : weekday + 1;
            UCalendarWeekdayType type = ucal_getDayOfWeekType(calendar, day, &status);
            BOOL include = type == UCAL_WEEKEND;
            if (type == UCAL_WEEKEND_ONSET) include = ucal_getWeekendTransition(calendar, day, &status) < 86400000;
            if (type == UCAL_WEEKEND_CEASE) include = ucal_getWeekendTransition(calendar, day, &status) > 0;
            if (U_FAILURE(status) || (include && JS_DefinePropertyValueUint32(ctx, weekend, out++, JS_NewInt32(ctx, weekday), JS_PROP_C_W_E) < 0)) { if (U_FAILURE(status)) js_intl_icu_error(ctx, status, "weekend data"); JS_FreeValue(ctx, weekend); ucal_close(calendar); goto done; }
        }
        ucal_close(calendar); result = JS_NewObject(ctx); if (JS_IsException(result)) { JS_FreeValue(ctx, weekend); goto done; }
        if (js_intl_define_int(ctx, result, "firstDay", first_day) < 0) { JS_FreeValue(ctx, weekend); JS_FreeValue(ctx, result); result = JS_EXCEPTION; goto done; }
        if (JS_DefinePropertyValueStr(ctx, result, "weekend", weekend, JS_PROP_C_W_E) < 0) { JS_FreeValue(ctx, result); result = JS_EXCEPTION; } goto done;
    }
array:
    result = intl_array_from_list(ctx, &list);
done:
    js_intl_locale_list_free(ctx, &list); js_intl_locale_list_free(ctx, &available); intl_tag_free(ctx, &parsed); js_free(ctx, region); js_free(ctx, override); js_free(ctx, language); js_free(ctx, icu); ures_close(resource); return result;
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
