/* Observable Intl option access and fixed prefix locale negotiation.
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
#include "../../../intl/locale/matching.h"
#include "../../../intl/locale/identifier.h"
#include "../../../intl/common/locale-grammar.h"
#include "../../internal/object.h"
#include "../../internal/runtime.h"
#include "../intl.h"
#include <math.h>

JSValue js_intl_options_object(JSContext *ctx, JSValueConst value, BOOL coerce)
{
    if (JS_IsUndefined(value))
        return JS_NewObjectProto(ctx, JS_NULL);
    if (coerce)
        return JS_ToObject(ctx, value);
    if (!JS_IsObject(value))
        return JS_ThrowTypeError(ctx, "Intl options must be an object");
    return JS_DupValue(ctx, value);
}

int js_intl_string_option(JSContext *ctx, JSValueConst object, const char *name,
                          const char *const *choices, size_t count, int fallback)
{
    JSValue value = JS_GetPropertyStr(ctx, object, name);
    const char *text;
    size_t length, i;
    int result = -1;
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value))
        return fallback;
    text = JS_ToCStringLen(ctx, &length, value);
    JS_FreeValue(ctx, value);
    if (!text)
        return -1;
    for (i = 0; i < count; i++) {
        if (strlen(choices[i]) == length && !memcmp(text, choices[i], length)) {
            result = i;
            break;
        }
    }
    JS_FreeCString(ctx, text);
    if (result < 0)
        JS_ThrowRangeError(ctx, "invalid Intl %s", name);
    return result;
}

int js_intl_default_number_option(JSContext *ctx, JSValueConst value, int minimum, int maximum,
                                  int fallback, int *out)
{
    double number;
    if (JS_IsUndefined(value)) {
        *out = fallback;
        return 0;
    }
    if (JS_ToFloat64(ctx, &number, value))
        return -1;
    if (isnan(number) || number < minimum || number > maximum) {
        JS_ThrowRangeError(ctx, "Intl numeric option outside range");
        return -1;
    }
    *out = (int)floor(number);
    return 0;
}

int js_intl_get_number_option(JSContext *ctx, JSValueConst options, const char *name, int minimum,
                              int maximum, int fallback, int *out)
{
    JSValue value = JS_GetPropertyStr(ctx, options, name);
    int status;
    if (JS_IsException(value))
        return -1;
    status = js_intl_default_number_option(ctx, value, minimum, maximum, fallback, out);
    JS_FreeValue(ctx, value);
    return status;
}

int js_intl_locale_matcher(JSContext *ctx, JSValueConst options)
{
    static const char *const choices[] = {"lookup", "best fit"};
    return js_intl_string_option(ctx, options, "localeMatcher", choices, 2, 1);
}

static int match_one(JSContext *ctx, const QJSIntlBytes *available, size_t count,
                     const char *requested, QJSIntlLocaleMatch *out)
{
    QJSIntlBytes input = {requested, strlen(requested)};
    return js_intl_error(
        ctx, qjs_intl_lookup_matching_locale(available, count, &input, 1, out),
        "locale matching");
}

int js_intl_resolve_locale_match(JSContext *ctx, const QJSIntlBytes *available,
                                 size_t count, const JSIntlLocaleList *requested,
                                 QJSIntlLocaleMatch *out)
{
    QJSIntlLocaleMatch match;
    const char *locale;
    size_t i;
    memset(out, 0, sizeof(*out));
    /* Best fit currently uses the same prefix algorithm as lookup. This is
       an implementation-defined matching choice, not a capability fallback. */
    for (i = 0; i < requested->count; i++) {
        if (match_one(ctx, available, count, requested->items[i], &match))
            return -1;
        if (match.found) {
            match.requested_index = i;
            *out = match;
            return 0;
        }
    }
    locale = js_intl_default_locale(ctx);
    if (!locale || match_one(ctx, available, count, locale, &match))
        return -1;
    if (!match.found || available[match.available_index].length != strlen(locale) ||
        memcmp(available[match.available_index].data, locale, strlen(locale))) {
        JS_ThrowInternalError(ctx, "Intl service does not admit the default locale");
        return -1;
    }
    match.requested_index = SIZE_MAX;
    match.extension = (QJSIntlBytes){0};
    *out = match;
    return 0;
}

int js_intl_resolve_locale_index(JSContext *ctx, const QJSIntlBytes *available,
                                 size_t count, const JSIntlLocaleList *requested,
                                 size_t *out)
{
    QJSIntlLocaleMatch match;
    if (js_intl_resolve_locale_match(ctx, available, count, requested, &match))
        return -1;
    *out = match.available_index;
    return 0;
}

int js_intl_unicode_type_option(JSContext *ctx, JSValueConst options, const char *name,
                                 QJSIntlBytes *out, const char **owned)
{
    JSValue value;
    const char *text;
    size_t length;
    memset(out, 0, sizeof(*out));
    *owned = NULL;
    value = JS_GetPropertyStr(ctx, options, name);
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value))
        return 0;
    text = JS_ToCStringLen(ctx, &length, value);
    JS_FreeValue(ctx, value);
    if (!text)
        return -1;
    if (!qjs_intl_unicode_type_well_formed(text, length)) {
        JS_FreeCString(ctx, text);
        JS_ThrowRangeError(ctx, "invalid Intl %s", name);
        return -1;
    }
    *out = (QJSIntlBytes){text, length};
    *owned = text;
    return 0;
}

int js_intl_resolve_locale_key(JSContext *ctx, QJSIntlBytes base, QJSIntlBytes extension,
                               const char key[2], QJSIntlBytes default_value, QJSIntlBytes option,
                               JSIntlLocaleValueSupported *supported, void *opaque,
                               JSIntlResolvedKeyword *out)
{
    QJSIntlLocaleService *identity = js_intl_locale_service(ctx);
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlLocaleResolveKey request = {{key[0], key[1]}, default_value, option, 0, supported, opaque};
    QJSIntlResolvedLocale resolved;
    memset(out, 0, sizeof(*out));
    if (!identity || js_intl_error(ctx, qjs_intl_resolve_locale_keys(identity, &allocator,
                         base, extension, &request, 1, &resolved), "locale keyword resolution"))
        return -1;
    out->locale = resolved.locale;
    out->value = resolved.values[0];
    resolved.locale = resolved.values[0] = NULL;
    qjs_intl_resolved_locale_clear(&resolved);
    return 0;
}

JSValue js_intl_filter_locales(JSContext *ctx, const QJSIntlBytes *available,
                               size_t count, JSValueConst locales,
                               JSValueConst options_value)
{
    JSIntlLocaleList requested = {0};
    JSValue options = JS_UNDEFINED, result = JS_EXCEPTION;
    size_t i;
    uint32_t accepted = 0;
    if (js_intl_canonicalize_locale_list(ctx, locales, &requested))
        return JS_EXCEPTION;
    options = js_intl_options_object(ctx, options_value, TRUE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0)
        goto done;
    result = JS_NewArray(ctx);
    if (JS_IsException(result))
        goto done;
    for (i = 0; i < requested.count; i++) {
        QJSIntlLocaleMatch match;
        JSValue value;
        if (match_one(ctx, available, count, requested.items[i], &match))
            goto fail;
        if (!match.found)
            continue;
        if (accepted == UINT32_MAX) {
            JS_ThrowRangeError(ctx, "too many supported locales");
            goto fail;
        }
        value = JS_NewString(ctx, requested.items[i]);
        if (JS_IsException(value) ||
            JS_DefinePropertyValueUint32(ctx, result, accepted++, value,
                                         JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
    }
    goto done;
fail:
    JS_FreeValue(ctx, result);
    result = JS_EXCEPTION;
done:
    JS_FreeValue(ctx, options);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return result;
}
