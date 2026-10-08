/* Native Intl enumeration and named time zones.
 * ECMA402 7ae78cfd, reviewed 2026-10-08; sec-intl.supportedvaluesof,
 * sup-availablenamedtimezoneidentifiers. ICU78.3 / IANA2026a. */
#include "locale-private.h"
#ifdef CONFIG_INTL
#include <unicode/ucal.h>
#include <unicode/ucol.h>
#include <unicode/ucurr.h>
#include <unicode/uenum.h>
#include <unicode/unumsys.h>

char *js_intl_canonicalize_time_zone(JSContext *ctx, const char *identifier, size_t length)
{
    size_t i; const char *known = intl_iana_zone_name(identifier, length); UChar output[128]; int32_t n; UErrorCode status = U_ZERO_ERROR;
    if (!known) { JS_ThrowRangeError(ctx, "invalid named time zone"); return NULL; }
    n = intl_iana_zone_primary(known, output, countof(output), &status);
    if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "IANA time zone identifier"); return NULL; }
    {
        char *result = js_intl_alloc_char(ctx, n);
        if (!result) return NULL;
        for (i = 0; i < (size_t)n; i++) {
            if (output[i] > 127) { js_free(ctx, result); JS_ThrowInternalError(ctx, "non-ASCII IANA time zone"); return NULL; }
            result[i] = output[i];
        }
        result[n] = 0; return result;
    }
}
int js_intl_primary_time_zones(JSContext *ctx, const char *region, JSIntlLocaleList *result)
{
    size_t i; memset(result, 0, sizeof(*result));
    if (region) {
        char country[4]; UEnumeration *e; UErrorCode status = U_ZERO_ERROR;
        const char *name; int32_t length;
        if (strlen(region) > 3) { JS_ThrowInternalError(ctx, "invalid canonical region"); return -1; }
        strcpy(country, region);
        for (i = 0; country[i]; i++) if (country[i] >= 'a' && country[i] <= 'z') country[i] -= 32;
        e = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, country, NULL, &status);
        if (U_FAILURE(status)) { uenum_close(e); return js_intl_icu_error(ctx, status, "regional time zones"); }
        while ((name = uenum_next(e, &length, &status))) {
            char *primary;
            if (!intl_iana_zone_name(name, length)) continue;
            primary = js_intl_canonicalize_time_zone(ctx, name, length);
            if (!primary || js_intl_locale_list_append(ctx, result, primary) < 0) { js_free(ctx, primary); uenum_close(e); goto fail; }
            js_free(ctx, primary);
        }
        uenum_close(e);
        if (js_intl_icu_error(ctx, status, "regional time zones") < 0) goto fail;
        return intl_list_sort_unique(ctx, result);
    }
    for (i = 0; i < intl_iana_zone_count(); i++) {
        const char *name = intl_iana_zone_at(i);
        char *primary = js_intl_canonicalize_time_zone(ctx, name, strlen(name));
        if (!primary) goto fail;
        if (!strcmp(primary, name)) {
            if (js_intl_locale_list_append(ctx, result, primary) < 0) { js_free(ctx, primary); goto fail; }
        }
        js_free(ctx, primary);
    }
    return intl_list_sort_unique(ctx, result);
fail:
    js_intl_locale_list_free(ctx, result); return -1;
}
static int values_from_enum(JSContext *ctx, UEnumeration *e, const char *key, JSIntlLocaleList *list)
{
    UErrorCode status = U_ZERO_ERROR; const char *s; int32_t n;
    if (!e) { JS_ThrowInternalError(ctx, "missing ICU enumeration"); return -1; }
    while ((s = uenum_next(e, &n, &status))) {
        const char *canonical = key ? uloc_toUnicodeLocaleType(key, s) : s;
        if (!canonical) canonical = s;
        if (!strcmp(canonical, "standard") || !strcmp(canonical, "search")) continue;
        if (js_intl_locale_list_append(ctx, list, canonical) < 0) { uenum_close(e); return -1; }
    }
    uenum_close(e); return js_intl_icu_error(ctx, status, "Intl enumeration");
}
int intl_values_list(JSContext *ctx, const char *key, JSIntlLocaleList *list)
{
    UErrorCode status = U_ZERO_ERROR; UEnumeration *e; size_t i; int r;
    memset(list, 0, sizeof(*list));
    if (!strcmp(key, "timeZone")) return js_intl_primary_time_zones(ctx, NULL, list);
    if (!strcmp(key, "unit")) {
        for (i = 0; i < intl_sanctioned_unit_count(); i++) if (js_intl_locale_list_append(ctx, list, intl_sanctioned_unit_at(i)) < 0) goto fail;
    } else if (!strcmp(key, "calendar")) {
        /* AvailableCalendars has exactly the proposal table's types.
         * Canonicalize its two aliases before creating the public list. */
        for (i = 0; i < intl_calendar_type_count(); i++) {
            if (js_intl_locale_list_append(ctx, list, intl_calendar_type_at(i)) < 0)
                goto fail;
        }
    } else if (!strcmp(key, "collation")) {
        e = ucol_getKeywordValues("collation", &status);
        if (U_FAILURE(status)) { uenum_close(e); js_intl_icu_error(ctx, status, "collations"); goto fail; }
        if (values_from_enum(ctx, e, "co", list) < 0) goto fail;
    } else if (!strcmp(key, "currency")) {
        e = ucurr_openISOCurrencies(UCURR_ALL, &status);
        if (U_FAILURE(status)) { uenum_close(e); js_intl_icu_error(ctx, status, "currencies"); goto fail; }
        if (values_from_enum(ctx, e, NULL, list) < 0) goto fail;
    } else if (!strcmp(key, "numberingSystem")) {
        const char *name; int32_t n;
        e = unumsys_openAvailableNames(&status);
        if (U_FAILURE(status)) { uenum_close(e); js_intl_icu_error(ctx, status, "numbering systems"); goto fail; }
        while ((name = uenum_next(e, &n, &status))) {
            UNumberingSystem *system = unumsys_openByName(name, &status);
            if (U_FAILURE(status)) { unumsys_close(system); uenum_close(e); js_intl_icu_error(ctx, status, "numbering system"); goto fail; }
            r = !unumsys_isAlgorithmic(system) && unumsys_getRadix(system) == 10 ? js_intl_locale_list_append(ctx, list, name) : 0;
            unumsys_close(system); if (r < 0) { uenum_close(e); goto fail; }
        }
        uenum_close(e); if (js_intl_icu_error(ctx, status, "numbering systems") < 0) goto fail;
    } else { JS_ThrowRangeError(ctx, "invalid Intl enumeration key"); goto fail; }
    return intl_list_sort_unique(ctx, list);
fail:
    js_intl_locale_list_free(ctx, list); return -1;
}
JSValue js_intl_supported_values_of(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue key_value, result = JS_EXCEPTION; const char *key; size_t length; JSIntlLocaleList list = { 0 };
    key_value = JS_ToString(ctx, argc ? argv[0] : JS_UNDEFINED); if (JS_IsException(key_value)) return key_value;
    key = JS_ToCStringLen(ctx, &length, key_value); JS_FreeValue(ctx, key_value); if (!key) return JS_EXCEPTION;
    if (strlen(key) != length) JS_ThrowRangeError(ctx, "invalid Intl enumeration key");
    else if (intl_values_list(ctx, key, &list) == 0) result = intl_array_from_list(ctx, &list);
    JS_FreeCString(ctx, key); js_intl_locale_list_free(ctx, &list); return result;
}
#endif
