/* Native locale lists and resolution. ECMA402 pin 7ae78cfd, 2026-10-08.
 * Anchors: sec-canonicalizelocalelist, sec-resolvelocale,
 * sec-filterlocales, sec-intl.getcanonicallocales. */
#include "locale-private.h"
#ifdef CONFIG_INTL
#include "../../../intl/libintl.h"
#include <unicode/ubrk.h>
#include <unicode/ucal.h>
#include <unicode/ucol.h>
#include <unicode/udat.h>
#include <unicode/udatpg.h>
#include <unicode/uenum.h>
#include <unicode/unum.h>
#include <unicode/unumsys.h>

static int list_has(const JSIntlLocaleList *l, const char *s)
{
    size_t i; for (i = 0; i < l->count; i++) if (!strcmp(l->items[i], s)) return 1; return 0;
}
static int canonicalize_element(JSContext *ctx, JSValueConst value, JSIntlLocaleList *list)
{
    const char *tag, *cstr = NULL; char *canonical; size_t length; int r = 0;
    if (!JS_IsString(value) && !JS_IsObject(value)) { JS_ThrowTypeError(ctx, "locale must be a string or object"); return -1; }
    tag = js_intl_locale_tag(value);
    if (tag) length = strlen(tag);
    else { tag = cstr = JS_ToCStringLen(ctx, &length, value); if (!tag) return -1; }
    canonical = js_intl_canonicalize_tag(ctx, tag, length); JS_FreeCString(ctx, cstr);
    if (!canonical) return -1;
    if (!list_has(list, canonical)) r = js_intl_locale_list_append(ctx, list, canonical);
    js_free(ctx, canonical); return r;
}
int js_intl_canonicalize_locale_list(JSContext *ctx, JSValueConst locales, JSIntlLocaleList *result)
{
    JSValue object = JS_UNDEFINED, value; int64_t length, k; int present;
    memset(result, 0, sizeof(*result));
    if (JS_IsUndefined(locales)) return 0;
    if (JS_IsString(locales) || js_intl_locale_tag(locales)) return canonicalize_element(ctx, locales, result);
    object = JS_ToObject(ctx, locales); if (JS_IsException(object)) return -1;
    if (js_get_length64(ctx, &length, object) < 0) goto fail;
    for (k = 0; k < length; k++) {
        char name[32]; JSAtom key;
        snprintf(name, sizeof(name), "%lld", (long long)k);
        key = JS_NewAtom(ctx, name); if (key == JS_ATOM_NULL) goto fail;
        present = JS_HasProperty(ctx, object, key);
        if (present < 0) { JS_FreeAtom(ctx, key); goto fail; }
        if (present) {
            value = JS_GetProperty(ctx, object, key); JS_FreeAtom(ctx, key);
            if (JS_IsException(value)) goto fail;
            present = canonicalize_element(ctx, value, result); JS_FreeValue(ctx, value);
            if (present < 0) goto fail;
        } else JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, object); return 0;
fail:
    JS_FreeValue(ctx, object); js_intl_locale_list_free(ctx, result); return -1;
}
char *js_intl_locale_to_icu(JSContext *ctx, const char *tag)
{
    UErrorCode status = U_ZERO_ERROR; int32_t length, parsed; char *result;
    if (strlen(tag) > INT32_MAX) { JS_ThrowOutOfMemory(ctx); return NULL; }
    length = uloc_forLanguageTag(tag, NULL, 0, &parsed, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale conversion"); return NULL; }
    result = js_intl_alloc_char(ctx, length); if (!result) return NULL;
    status = U_ZERO_ERROR; uloc_forLanguageTag(tag, result, length + 1, &parsed, &status);
    if (U_FAILURE(status) || parsed != (int32_t)strlen(tag)) {
        js_free(ctx, result); if (U_FAILURE(status)) js_intl_icu_error(ctx, status, "locale conversion");
        else
            JS_ThrowInternalError(ctx, "ICU did not consume a validated locale");
        return NULL;
    }
    return result;
}
char *js_intl_locale_from_icu(JSContext *ctx, const char *locale)
{
    UErrorCode status = U_ZERO_ERROR; int32_t length; char *tag, *result;
    length = uloc_toLanguageTag(locale, NULL, 0, FALSE, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale conversion"); return NULL; }
    tag = js_intl_alloc_char(ctx, length); if (!tag) return NULL;
    status = U_ZERO_ERROR; uloc_toLanguageTag(locale, tag, length + 1, FALSE, &status);
    if (U_FAILURE(status)) { js_free(ctx, tag); js_intl_icu_error(ctx, status, "locale conversion"); return NULL; }
    result = js_intl_canonicalize_tag(ctx, tag, length); js_free(ctx, tag); return result;
}
static int available_count(JSIntlService service)
{
    switch (service) {
    case JS_INTL_COLLATOR: case JS_INTL_COLLATOR_SEARCH: return ucol_countAvailable();
    case JS_INTL_NUMBER_FORMAT: case JS_INTL_RELATIVE_TIME_FORMAT: case JS_INTL_DURATION_FORMAT: return unum_countAvailable();
    case JS_INTL_DATE_TIME_FORMAT: return udat_countAvailable();
    case JS_INTL_SEGMENTER: return ubrk_countAvailable();
    default: return uloc_countAvailable();
    }
}
static const char *available_at(JSIntlService service, int n)
{
    switch (service) {
    case JS_INTL_COLLATOR: case JS_INTL_COLLATOR_SEARCH: return ucol_getAvailable(n);
    case JS_INTL_NUMBER_FORMAT: case JS_INTL_RELATIVE_TIME_FORMAT: case JS_INTL_DURATION_FORMAT: return unum_getAvailable(n);
    case JS_INTL_DATE_TIME_FORMAT: return udat_getAvailable(n);
    case JS_INTL_SEGMENTER: return ubrk_getAvailable(n);
    default: return uloc_getAvailable(n);
    }
}
static int available_locale_append(JSContext *ctx, JSIntlLocaleList *list,
                                   const char *tag)
{
    IntlTag parsed = { 0 };
    char *canonical = js_intl_strdup(ctx, tag);
    char scriptless[16];
    int result = -1;

    if (!canonical)
        return -1;
    intl_remove_unicode_extension(canonical);
    if (intl_parse_tag(ctx, canonical, strlen(canonical), &parsed) < 0 ||
        js_intl_locale_list_append(ctx, list, canonical) < 0)
        goto done;
    /* Every compound tag has a language fallback, including DefaultLocale. */
    if (strcmp(canonical, parsed.base.language) &&
        js_intl_locale_list_append(ctx, list, parsed.base.language) < 0)
        goto done;
    if (parsed.base.script && parsed.base.region) {
        /* The grammar limits language to eight and region to three bytes. */
        snprintf(scriptless, sizeof(scriptless), "%s-%s",
                 parsed.base.language, parsed.base.region);
        /* The parsed grammar stores subtags in lowercase. */
        for (size_t i = strlen(parsed.base.language) + 1; scriptless[i]; i++) {
            if (scriptless[i] >= 'a' && scriptless[i] <= 'z')
                scriptless[i] -= 'a' - 'A';
        }
        if (js_intl_locale_list_append(ctx, list, scriptless) < 0)
            goto done;
    }
    result = 0;
done:
    intl_tag_free(ctx, &parsed);
    js_free(ctx, canonical);
    return result;
}
static int available_locales(JSContext *ctx, JSIntlService service, JSIntlLocaleList *result)
{
    int i; memset(result, 0, sizeof(*result));
    for (i = 0; i < available_count(service); i++) {
        char *tag = js_intl_locale_from_icu(ctx, available_at(service, i));
        if (!tag) goto fail;
        if (available_locale_append(ctx, result, tag) < 0) { js_free(ctx, tag); goto fail; }
        js_free(ctx, tag);
    }
    /* ECMA402 requires every service's AvailableLocales to contain the
     * context's stable DefaultLocale, including an ICU root default. */
    if (available_locale_append(ctx, result, js_intl_default_locale(ctx)) < 0) goto fail;
    return intl_list_sort_unique(ctx, result);
fail:
    js_intl_locale_list_free(ctx, result); return -1;
}
/* Best fit is implementation-defined. This implementation uses the same
 * deterministic prefix match as lookup, including the singleton rule. */
static char *matching_locale(JSContext *ctx, const JSIntlLocaleList *available, const char *requested)
{
    char *candidate = js_intl_strdup(ctx, requested), *dash;
    if (!candidate) return NULL;
    intl_remove_unicode_extension(candidate);
    while (*candidate) {
        if (list_has(available, candidate)) return candidate;
        dash = strrchr(candidate, '-'); if (!dash) break;
        while (dash - candidate >= 2 && dash[-2] == '-') dash -= 2;
        *dash = 0;
    }
    js_free(ctx, candidate); return NULL;
}
char *intl_lookup_locale(JSContext *ctx, JSIntlService service, const char *requested)
{
    JSIntlLocaleList available = { 0 }; char *result;
    if (available_locales(ctx, service, &available) < 0) return NULL;
    result = matching_locale(ctx, &available, requested);
    js_intl_locale_list_free(ctx, &available); return result;
}
static int enum_types(JSContext *ctx, UEnumeration *enumeration, const char *key, JSIntlLocaleList *list)
{
    UErrorCode status = U_ZERO_ERROR; const char *value; int32_t n;
    if (!enumeration) return -1;
    while ((value = uenum_next(enumeration, &n, &status))) {
        const char *canonical = uloc_toUnicodeLocaleType(key, value);
        if (canonical && strcmp(canonical, "standard") && strcmp(canonical, "search") &&
            !list_has(list, canonical) && js_intl_locale_list_append(ctx, list, canonical) < 0) { uenum_close(enumeration); return -1; }
    }
    uenum_close(enumeration); return js_intl_icu_error(ctx, status, "locale keyword data");
}
static int numbering_systems(JSContext *ctx, JSIntlLocaleList *list)
{
    UErrorCode status = U_ZERO_ERROR; UEnumeration *e = unumsys_openAvailableNames(&status); const char *name; int32_t n;
    if (U_FAILURE(status)) { uenum_close(e); return js_intl_icu_error(ctx, status, "numbering systems"); }
    while ((name = uenum_next(e, &n, &status))) {
        UNumberingSystem *system = unumsys_openByName(name, &status);
        if (U_FAILURE(status)) { unumsys_close(system); uenum_close(e); return js_intl_icu_error(ctx, status, "numbering system"); }
        if (!unumsys_isAlgorithmic(system) && unumsys_getRadix(system) == 10 && js_intl_locale_list_append(ctx, list, name) < 0) { unumsys_close(system); uenum_close(e); return -1; }
        unumsys_close(system);
    }
    uenum_close(e); return js_intl_icu_error(ctx, status, "numbering systems");
}
static int key_data(JSContext *ctx, JSIntlService service, const char *icu,
                    const char *key, JSIntlLocaleList *supported, char **default_value)
{
    UErrorCode status = U_ZERO_ERROR; memset(supported, 0, sizeof(*supported)); *default_value = NULL;
    if (!strcmp(key, "nu")) {
        UNumberingSystem *system = unumsys_open(icu, &status);
        if (U_FAILURE(status)) { unumsys_close(system); return js_intl_icu_error(ctx, status, "default numbering system"); }
        *default_value = js_intl_strdup(ctx, unumsys_getName(system)); unumsys_close(system);
        if (!*default_value) return -1;
        return numbering_systems(ctx, supported);
    }
    if (!strcmp(key, "ca")) {
        UCalendar *calendar;
        const char *type, *canonical;
        size_t i;
        calendar = ucal_open(NULL, 0, icu, UCAL_DEFAULT, &status);
        if (U_FAILURE(status)) {
            ucal_close(calendar);
            return js_intl_icu_error(ctx, status, "default calendar");
        }
        type = ucal_getType(calendar, &status);
        canonical = U_SUCCESS(status) ? uloc_toUnicodeLocaleType("ca", type) : NULL;
        if (U_SUCCESS(status)) {
            const char *known = intl_calendar_type_name(canonical ? canonical : type,
                                                       strlen(canonical ? canonical : type));
            if (known)
                *default_value = js_intl_strdup(ctx, known);
            else {
                /* LocaleData may choose its supported types and default.
                 * Preserve CLDR preference order while restricting this
                 * implementation to the proposal's canonical table. */
                UEnumeration *e = ucal_getKeywordValuesForLocale("calendar", icu,
                                                                TRUE, &status);
                int32_t length;
                if (U_SUCCESS(status)) {
                    while ((type = uenum_next(e, &length, &status))) {
                        canonical = uloc_toUnicodeLocaleType("ca", type);
                        known = intl_calendar_type_name(canonical ? canonical : type,
                                                        strlen(canonical ? canonical : type));
                        if (known) {
                            *default_value = js_intl_strdup(ctx, known);
                            break;
                        }
                    }
                }
                uenum_close(e);
                if (U_SUCCESS(status) && !*default_value && !JS_HasException(ctx))
                    *default_value = js_intl_strdup(ctx, "gregory");
            }
        }
        ucal_close(calendar);
        if (U_FAILURE(status))
            return js_intl_icu_error(ctx, status, "default calendar");
        if (!*default_value)
            return -1;
        for (i = 0; i < intl_calendar_type_count(); i++) {
            if (js_intl_locale_list_append(ctx, supported, intl_calendar_type_at(i)) < 0)
                return -1;
        }
        return 0;
    }
    if (!strcmp(key, "hc")) {
        static const char *const cycles[] = { "h11", "h12", "h23", "h24" };
        UDateTimePatternGenerator *g = udatpg_open(icu, &status); UDateFormatHourCycle cycle; int i;
        if (U_FAILURE(status)) { udatpg_close(g); return js_intl_icu_error(ctx, status, "hour cycle data"); }
        cycle = udatpg_getDefaultHourCycle(g, &status); udatpg_close(g);
        if (U_FAILURE(status)) return js_intl_icu_error(ctx, status, "default hour cycle");
        *default_value = js_intl_strdup(ctx, cycles[cycle]); if (!*default_value) return -1;
        for (i = 0; i < 4; i++) if (js_intl_locale_list_append(ctx, supported, cycles[i]) < 0) return -1;
        return 0;
    }
    if (!strcmp(key, "co") || !strcmp(key, "kn") || !strcmp(key, "kf")) {
        UCollator *c; char *search_locale = NULL;
        if (service == JS_INTL_COLLATOR_SEARCH) {
            size_t size = strlen(icu);
            search_locale = js_malloc(ctx, size + 20);
            if (!search_locale) return -1;
            memcpy(search_locale, icu, size); strcpy(search_locale + size, "@collation=search");
        }
        c = ucol_open(search_locale ? search_locale : icu, &status);
        js_free(ctx, search_locale);
        if (U_FAILURE(status)) { ucol_close(c); return js_intl_icu_error(ctx, status, "collation locale data"); }
        if (!strcmp(key, "co")) {
            UEnumeration *e; ucol_close(c);
            if (service == JS_INTL_COLLATOR_SEARCH) return 0;
            e = ucol_getKeywordValuesForLocale("collation", icu, FALSE, &status);
            if (U_FAILURE(status)) { uenum_close(e); return js_intl_icu_error(ctx, status, "collations"); }
            return enum_types(ctx, e, "co", supported);
        }
        if (!strcmp(key, "kn")) {
            UColAttributeValue v = ucol_getAttribute(c, UCOL_NUMERIC_COLLATION, &status);
            *default_value = js_intl_strdup(ctx, v == UCOL_ON ? "true" : "false");
            if (js_intl_locale_list_append(ctx, supported, "false") < 0 || js_intl_locale_list_append(ctx, supported, "true") < 0) { ucol_close(c); return -1; }
        } else {
            UColAttributeValue v = ucol_getAttribute(c, UCOL_CASE_FIRST, &status);
            *default_value = js_intl_strdup(ctx, v == UCOL_UPPER_FIRST ? "upper" : v == UCOL_LOWER_FIRST ? "lower" : "false");
            if (js_intl_locale_list_append(ctx, supported, "false") < 0 || js_intl_locale_list_append(ctx, supported, "lower") < 0 || js_intl_locale_list_append(ctx, supported, "upper") < 0) { ucol_close(c); return -1; }
        }
        ucol_close(c); if (U_FAILURE(status)) return js_intl_icu_error(ctx, status, "collator defaults");
        return *default_value ? 0 : -1;
    }
    JS_ThrowInternalError(ctx, "unknown Intl resolution key"); return -1;
}
int js_intl_resolve_locale(JSContext *ctx, JSIntlService service,
                           const JSIntlLocaleList *requested, const char *matcher,
                           const JSIntlResolutionKey *keys, int count, JSIntlResolvedLocale *result)
{
    JSIntlLocaleList available = { 0 }; IntlTag request = { 0 }, public_tag = { 0 }, backend_tag = { 0 };
    char *matched = NULL, *base_icu = NULL, *public_string = NULL, *backend_string = NULL; size_t i; int k, r = -1;
    memset(result, 0, sizeof(*result));
    if (count < 0 || count > JS_INTL_MAX_RESOLUTION_KEYS) { JS_ThrowInternalError(ctx, "too many Intl resolution keys"); return -1; }
    (void)matcher;
    if (available_locales(ctx, service, &available) < 0) goto done;
    for (i = 0; i < requested->count; i++) {
        matched = matching_locale(ctx, &available, requested->items[i]);
        if (matched) { if (intl_parse_tag(ctx, requested->items[i], strlen(requested->items[i]), &request) < 0) goto done; break; }
        if (JS_HasException(ctx)) goto done;
    }
    if (!matched) matched = js_intl_strdup(ctx, js_intl_default_locale(ctx));
    if (!matched) goto done;
    result->data_locale = js_intl_strdup(ctx, matched); if (!result->data_locale) goto done;
    base_icu = js_intl_locale_to_icu(ctx, matched); if (!base_icu) goto done;
    if (intl_parse_tag(ctx, matched, strlen(matched), &public_tag) < 0 || intl_parse_tag(ctx, matched, strlen(matched), &backend_tag) < 0) goto done;
    result->key_count = count;
    for (k = 0; k < count; k++) {
        JSIntlLocaleList supported = { 0 }; char *value = NULL, *option = NULL; const char *extension; BOOL retain = FALSE;
        if (key_data(ctx, service, base_icu, keys[k].key, &supported, &value) < 0) { js_intl_locale_list_free(ctx, &supported); js_free(ctx, value); goto done; }
        extension = intl_tag_keyword(&request, keys[k].key);
        if (extension && !keys[k].suppress_extension) {
            const char *candidate = *extension ? extension : "true";
            if (list_has(&supported, candidate)) {
                js_free(ctx, value); value = js_intl_strdup(ctx, candidate); if (!value) { js_intl_locale_list_free(ctx, &supported); goto done; } retain = TRUE;
            }
        }
        if (keys[k].option) {
            option = intl_canonicalize_uvalue(ctx, keys[k].key, keys[k].option);
            if (!option) { js_intl_locale_list_free(ctx, &supported); js_free(ctx, value); goto done; }
            if (!*option) { js_free(ctx, option); option = js_intl_strdup(ctx, "true"); if (!option) { js_intl_locale_list_free(ctx, &supported); js_free(ctx, value); goto done; } }
            if ((!value || strcmp(option, value)) && list_has(&supported, option)) {
                js_free(ctx, value); value = option; option = NULL; retain = FALSE;
            }
            js_free(ctx, option);
        }
        js_intl_locale_list_free(ctx, &supported);
        result->values[k] = value;
        if (retain && intl_tag_set_keyword(ctx, &public_tag, keys[k].key, !strcmp(value, "true") ? "" : value) < 0) goto done;
        if (value && intl_tag_set_keyword(ctx, &backend_tag, keys[k].key, !strcmp(value, "true") ? "" : value) < 0) goto done;
    }
    public_string = intl_tag_string(ctx, &public_tag); backend_string = intl_tag_string(ctx, &backend_tag);
    if (!public_string || !backend_string) goto done;
    result->locale = js_intl_canonicalize_tag(ctx, public_string, strlen(public_string));
    result->icu_locale = js_intl_locale_to_icu(ctx, backend_string);
    if (!result->locale || !result->icu_locale) goto done;
    r = 0;
done:
    js_intl_locale_list_free(ctx, &available); intl_tag_free(ctx, &request); intl_tag_free(ctx, &public_tag); intl_tag_free(ctx, &backend_tag);
    js_free(ctx, matched); js_free(ctx, base_icu); js_free(ctx, public_string); js_free(ctx, backend_string);
    if (r < 0)
        js_intl_resolved_locale_free(ctx, result);
    return r;
}
JSValue js_intl_supported_locales(JSContext *ctx, JSIntlService service, JSValueConst locales, JSValueConst options)
{
    static const char *const matchers[] = { "lookup", "best fit" };
    JSIntlLocaleList requested = { 0 }, available = { 0 }, supported = { 0 }; JSValue object = JS_UNDEFINED, result = JS_EXCEPTION; size_t i; int matcher;
    if (js_intl_canonicalize_locale_list(ctx, locales, &requested) < 0) goto done;
    object = js_intl_coerce_options(ctx, options); if (JS_IsException(object)) goto done;
    if (js_intl_get_string_option(ctx, object, "localeMatcher", matchers, 2, 1, &matcher) < 0) goto done;
    if (available_locales(ctx, service, &available) < 0) goto done;
    for (i = 0; i < requested.count; i++) {
        char *match = matching_locale(ctx, &available, requested.items[i]);
        if (match) { js_free(ctx, match); if (js_intl_locale_list_append(ctx, &supported, requested.items[i]) < 0) goto done; }
        else if (JS_HasException(ctx)) goto done;
    }
    result = intl_array_from_list(ctx, &supported);
done:
    JS_FreeValue(ctx, object); js_intl_locale_list_free(ctx, &requested); js_intl_locale_list_free(ctx, &available); js_intl_locale_list_free(ctx, &supported); return result;
}
JSValue js_intl_get_canonical_locales(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlLocaleList list = { 0 }; JSValue result;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &list) < 0) return JS_EXCEPTION;
    result = intl_array_from_list(ctx, &list); js_intl_locale_list_free(ctx, &list); return result;
}
#endif
