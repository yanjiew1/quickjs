/*
 * QuickJS native Intl services and tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
/* Intl.DisplayNames, ECMA402 #sec-Intl.DisplayNames.
 * Spec 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-06.
 * Original native C implementation; ICU supplies locale data. */
#include "intl-service-common.h"
#ifdef CONFIG_INTL
#include <unicode/uldnames.h>
#include <unicode/uloc.h>
#include <unicode/udatpg.h>
typedef struct JSIntlDisplayNames {
    char *locale;
    ULocaleDisplayNames *names;
    UDateTimePatternGenerator *generator;
    int style, type, fallback, language_display;
} JSIntlDisplayNames;
static const char *const display_types[] = {
    "language", "region", "script", "currency", "calendar", "dateTimeField"
};
static const char *const display_fallback[] = { "code", "none" };
static const char *const display_language[] = { "dialect", "standard" };
static const char *const display_fields[] = {
    "era", "year", "quarter", "month", "weekOfYear", "weekday", "day",
    "dayPeriod", "hour", "minute", "second", "timeZoneName"
};
static const UDateTimePatternField display_icu_fields[] = {
    UDATPG_ERA_FIELD, UDATPG_YEAR_FIELD, UDATPG_QUARTER_FIELD, UDATPG_MONTH_FIELD,
    UDATPG_WEEK_OF_YEAR_FIELD, UDATPG_WEEKDAY_FIELD, UDATPG_DAY_FIELD,
    UDATPG_DAYPERIOD_FIELD, UDATPG_HOUR_FIELD, UDATPG_MINUTE_FIELD,
    UDATPG_SECOND_FIELD, UDATPG_ZONE_FIELD
};
static void js_intl_display_finalizer(JSRuntime *rt, JSValue obj)
{
    JSIntlDisplayNames *s = JS_GetOpaque(obj, JS_CLASS_INTL_DISPLAY_NAMES);
    if (s) {
        if (s->names) uldn_close(s->names);
        if (s->generator) udatpg_close(s->generator);
        js_free_rt(rt, s->locale);
        js_free_rt(rt, s);
    }
}
static JSValue js_intl_display_constructor(JSContext *ctx, JSValueConst new_target,
                                         int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlResolvedLocale resolved = {0};
    JSIntlDisplayNames *s = NULL;
    UDisplayContext contexts[4];
    UErrorCode status = U_ZERO_ERROR;
    int matcher;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.DisplayNames requires new");
    obj = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_DISPLAY_NAMES);
    if (JS_IsException(obj)) goto fail;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) goto fail;
    JS_SetOpaque(obj, s);
    if (js_intl_service_options(ctx,
            argc > 0 ? argv[0] : JS_UNDEFINED,
            argc > 1 ? argv[1] : JS_UNDEFINED, 2, &requested,
                               &options, &matcher) < 0 ||
        js_intl_resolve_locale(ctx, JS_INTL_DISPLAY_NAMES, &requested,
            js_intl_matchers[matcher], NULL, 0, &resolved) < 0 ||
        js_intl_get_string_option(ctx, options, "style", js_intl_styles,
            countof(js_intl_styles), 0, &s->style) < 0 ||
        js_intl_get_string_option(ctx, options, "type", display_types,
            countof(display_types), -1, &s->type) < 0)
        goto fail;
    if (s->type < 0) {
        JS_ThrowTypeError(ctx, "Intl.DisplayNames requires type");
        goto fail;
    }
    if (js_intl_get_string_option(ctx, options, "fallback", display_fallback,
            countof(display_fallback), 0, &s->fallback) < 0 ||
        js_intl_get_string_option(ctx, options, "languageDisplay", display_language,
            countof(display_language), 0, &s->language_display) < 0)
        goto fail;
    s->locale = resolved.locale;
    resolved.locale = NULL;
    contexts[0] = s->language_display ? UDISPCTX_STANDARD_NAMES : UDISPCTX_DIALECT_NAMES;
    contexts[1] = s->style ? UDISPCTX_LENGTH_SHORT : UDISPCTX_LENGTH_FULL;
    contexts[2] = UDISPCTX_NO_SUBSTITUTE;
    contexts[3] = UDISPCTX_CAPITALIZATION_FOR_STANDALONE;
    s->names = uldn_openForContext(resolved.icu_locale, contexts, 4, &status);
    if (!s->names && U_SUCCESS(status)) {
        JS_ThrowOutOfMemory(ctx);
        goto fail;
    }
    if (s->type == 5) s->generator = udatpg_open(resolved.icu_locale, &status);
    if (js_intl_icu_error(ctx, status, "DisplayNames") < 0) goto fail;
    JS_FreeValue(ctx, options);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, options);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return JS_EXCEPTION;
}
/* A unicode_language_id excludes extensions, extlangs and private use.
   Full structural validation and duplicate variants remain locale-owned. */
static int js_intl_display_language_id(const char *s)
{
    const char *p = s;
    int n, stage = 0, letters, digits;
    while (*p && *p != '-') {
        if (!js_intl_ascii_alpha(*p++)) return 0;
    }
    n = p - s;
    if (!((n >= 2 && n <= 3) || (n >= 5 && n <= 8))) return 0;
    while (*p) {
        s = ++p;
        letters = digits = 1;
        while (*p && *p != '-') {
            if (!js_intl_ascii_alpha(*p)) letters = 0;
            if (!js_intl_ascii_digit(*p)) digits = 0;
            if (!js_intl_ascii_alpha(*p) && !js_intl_ascii_digit(*p)) return 0;
            p++;
        }
        n = p - s;
        if (stage == 0 && n == 4 && letters) stage = 1;
        else if (stage <= 1 && ((n == 2 && letters) || (n == 3 && digits))) stage = 2;
        else if ((n >= 5 && n <= 8) || (n == 4 && js_intl_ascii_digit(s[0]))) stage = 3;
        else return 0;
    }
    return 1;
}
static char *js_intl_display_code(JSContext *ctx, JSValueConst input, int type,
                                 int *field)
{
    UChar *text = NULL;
    int32_t length, i;
    char *code = NULL, *canonical;
    int valid = 0;
    if (js_intl_to_uchar(ctx, input, &text, &length) < 0) return NULL;
    code = js_intl_alloc_char(ctx, length);
    if (!code) goto done;
    for (i = 0; i < length; i++) {
        if (!text[i] || text[i] > 127) goto invalid;
        code[i] = text[i];
    }
    code[length] = 0;
    if (type == 0) {
        if (!js_intl_display_language_id(code)) goto invalid;
        canonical = js_intl_canonicalize_tag(ctx, code, (size_t)length);
        js_free(ctx, code);
        code = canonical;
        goto done;
    }
    if (type == 1) {
        if (length == 2) {
            valid = js_intl_ascii_alpha(code[0]) && js_intl_ascii_alpha(code[1]);
        } else if (length == 3) {
            valid = js_intl_ascii_digit(code[0]) && js_intl_ascii_digit(code[1]) &&
                    js_intl_ascii_digit(code[2]);
        }
    } else if (type == 2) {
        valid = length == 4;
        for (i = 0; i < length; i++) valid &= js_intl_ascii_alpha(code[i]);
    } else if (type == 3) {
        valid = length == 3;
        for (i = 0; i < length; i++) valid &= js_intl_ascii_alpha(code[i]);
    } else if (type == 4) {
        valid = js_intl_unicode_type(code);
    } else {
        for (i = 0; i < countof(display_fields); i++) {
            if (!strcmp(code, display_fields[i])) { valid = 1; *field = i; break; }
        }
    }
    if (!valid) goto invalid;
    if (type < 5) {
        for (i = 0; i < length; i++) {
            if (code[i] >= 'A' && code[i] <= 'Z') code[i] += 'a' - 'A';
            if ((type == 1 || type == 3 || (type == 2 && i == 0)) &&
                    code[i] >= 'a' && code[i] <= 'z') code[i] -= 'a' - 'A';
        }
    }
    goto done;
 invalid:
    JS_ThrowRangeError(ctx, "invalid display name code");
    js_free(ctx, code);
    code = NULL;
 done:
    js_free(ctx, text);
    return code;
}
static int32_t js_intl_display_lookup(JSIntlDisplayNames *s, const char *code,
    const char *icu_code, int field, UChar *text, int32_t capacity, UErrorCode *status)
{
    static const UDateTimePGDisplayWidth widths[] = {
        UDATPG_WIDE, UDATPG_ABBREVIATED, UDATPG_NARROW
    };
    switch (s->type) {
    case 0: return uldn_localeDisplayName(s->names, icu_code, text, capacity, status);
    case 1: return uldn_regionDisplayName(s->names, code, text, capacity, status);
    case 2: return uldn_scriptDisplayName(s->names, code, text, capacity, status);
    case 3: return uldn_keyValueDisplayName(s->names, "currency", code, text, capacity, status);
    case 4: return uldn_keyValueDisplayName(s->names, "calendar", icu_code, text, capacity, status);
    default: return udatpg_getFieldDisplayName(s->generator,
        display_icu_fields[field], widths[s->style], text, capacity, status);
    }
}
static JSValue js_intl_display_of(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSIntlDisplayNames *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DISPLAY_NAMES);
    char *code = NULL, *locale_code = NULL;
    const char *icu_code;
    UChar *text = NULL;
    UErrorCode status = U_ZERO_ERROR;
    int32_t length, actual, i;
    int field = 0, unknown = 0;
    JSValue result = JS_UNDEFINED;
    if (!s) return JS_EXCEPTION;
    code = js_intl_display_code(ctx, argv[0], s->type, &field);
    if (!code) goto fail;
    icu_code = code;
    if (s->type == 0) {
        locale_code = js_intl_locale_to_icu(ctx, code);
        if (!locale_code) goto fail;
        icu_code = locale_code;
    } else if (s->type == 4) {
        const char *legacy = uloc_toLegacyType("ca", code);
        if (legacy) icu_code = legacy;
    }
    length = js_intl_display_lookup(s, code, icu_code, field, NULL, 0, &status);
    if (status == U_ILLEGAL_ARGUMENT_ERROR || status == U_MISSING_RESOURCE_ERROR ||
            (U_SUCCESS(status) && !length)) {
        unknown = 1;
        goto fallback;
    }
    if (status == U_BUFFER_OVERFLOW_ERROR) status = U_ZERO_ERROR;
    if (js_intl_icu_error(ctx, status, "DisplayNames.of") < 0) goto fail;
    text = js_intl_alloc_uchar(ctx, length);
    if (!text) goto fail;
    actual = js_intl_display_lookup(s, code, icu_code, field, text, length + 1, &status);
    if (js_intl_icu_error(ctx, status, "DisplayNames.of") < 0) goto fail;
    /* ICU currency display lookup substitutes the currency code even in
       NO_SUBSTITUTE mode. Canonical fallback remains a JavaScript concern. */
    if (s->type == 3 && actual == strlen(code)) {
        for (i = 0; i < actual && text[i] == code[i]; i++) {}
        unknown = i == actual;
    }
 fallback:
    if (unknown)
        result = s->fallback ? JS_UNDEFINED : JS_NewString(ctx, code);
    else
        result = js_intl_from_uchar(ctx, text, actual);
    goto done;
 fail:
    result = JS_EXCEPTION;
 done:
    js_free(ctx, code);
    js_free(ctx, locale_code);
    js_free(ctx, text);
    return result;
}
static JSValue js_intl_display_resolved(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSIntlDisplayNames *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DISPLAY_NAMES);
    JSValue result;
    if (!s) return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result)) return result;
    if (js_intl_define_string(ctx, result, "locale", s->locale) < 0 ||
        js_intl_define_string(ctx, result, "style", js_intl_styles[s->style]) < 0 ||
        js_intl_define_string(ctx, result, "type", display_types[s->type]) < 0 ||
        js_intl_define_string(ctx, result, "fallback", display_fallback[s->fallback]) < 0 ||
        (s->type == 0 && js_intl_define_string(ctx, result, "languageDisplay",
            display_language[s->language_display]) < 0)) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}
static JSValue js_intl_display_supported(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_DISPLAY_NAMES,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef js_intl_display_class = {
    "Intl.DisplayNames", .finalizer = js_intl_display_finalizer,
};
static const JSCFunctionListEntry js_intl_display_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_display_supported),
};
static const JSCFunctionListEntry js_intl_display_prototype[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_display_resolved),
    JS_CFUNC_DEF("of", 1, js_intl_display_of),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.DisplayNames", JS_PROP_CONFIGURABLE),
};
int js_intl_init_display_names(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_DISPLAY_NAMES,
        &js_intl_display_class, "DisplayNames", js_intl_display_constructor, 2,
        JS_CFUNC_constructor, js_intl_display_static, countof(js_intl_display_static),
        js_intl_display_prototype, countof(js_intl_display_prototype));
}
#endif
