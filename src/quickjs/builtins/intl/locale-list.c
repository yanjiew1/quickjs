/* Observable CanonicalizeLocaleList coercions belong to the JS binding.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, 2026-10-19.
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
#include "../../internal/runtime.h"
#include "../../internal/object.h"

void js_intl_locale_list_clear(JSRuntime *rt, JSIntlLocaleList *list)
{
    size_t i;

    for (i = 0; i < list->count; i++)
        js_free_rt(rt, list->items[i]);
    js_free_rt(rt, list->items);
    memset(list, 0, sizeof(*list));
}

static int canonicalize_element(JSContext *ctx, JSValueConst value,
                                JSIntlLocaleList *list)
{
    QJSIntlLocaleService *service;
    QJSIntlStatus status;
    QJSIntlBytes input;
    const char *text, *owned = NULL;
    char *canonical, **items;
    size_t i;

    if (!JS_IsString(value) && !JS_IsObject(value)) {
        JS_ThrowTypeError(ctx, "locale must be a string or object");
        return -1;
    }
    text = js_intl_locale_tag(value);
    if (text) {
        input.length = strlen(text);
    } else {
        text = owned = JS_ToCStringLen(ctx, &input.length, value);
        if (!text)
            return -1;
    }
    input.data = text;
    service = js_intl_locale_service(ctx);
    if (!service) {
        JS_FreeCString(ctx, owned);
        return -1;
    }
    status = qjs_intl_locale_canonicalize(service, input, &canonical);
    JS_FreeCString(ctx, owned);
    if (js_intl_error(ctx, status, "Unicode locale identifier"))
        return -1;
    for (i = 0; i < list->count; i++) {
        if (!strcmp(list->items[i], canonical)) {
            js_free(ctx, canonical);
            return 0;
        }
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 8;

        if (capacity < list->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            js_free(ctx, canonical);
            JS_ThrowOutOfMemory(ctx);
            return -1;
        }
        items = js_realloc(ctx, list->items, capacity * sizeof(*items));
        if (!items) {
            js_free(ctx, canonical);
            return -1;
        }
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = canonical;
    return 0;
}

int js_intl_canonicalize_locale_list(JSContext *ctx, JSValueConst locales,
                                     JSIntlLocaleList *out)
{
    JSValue object;
    int64_t length, index;

    memset(out, 0, sizeof(*out));
    if (JS_IsUndefined(locales))
        return 0;
    if (JS_IsString(locales) || js_intl_locale_tag(locales))
        return canonicalize_element(ctx, locales, out);
    object = JS_ToObject(ctx, locales);
    if (JS_IsException(object))
        return -1;
    if (js_get_length64(ctx, &length, object) < 0)
        goto fail;
    for (index = 0; index < length; index++) {
        JSAtom key = JS_NewAtomInt64(ctx, index);
        JSValue value;
        int present, result;

        if (key == JS_ATOM_NULL)
            goto fail;
        present = JS_HasProperty(ctx, object, key);
        if (present <= 0) {
            JS_FreeAtom(ctx, key);
            if (present < 0)
                goto fail;
            continue;
        }
        value = JS_GetProperty(ctx, object, key);
        JS_FreeAtom(ctx, key);
        if (JS_IsException(value))
            goto fail;
        result = canonicalize_element(ctx, value, out);
        JS_FreeValue(ctx, value);
        if (result)
            goto fail;
    }
    JS_FreeValue(ctx, object);
    return 0;
fail:
    JS_FreeValue(ctx, object);
    js_intl_locale_list_clear(ctx->rt, out);
    return -1;
}

JSValue js_intl_get_canonical_locales(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSIntlLocaleList list;
    JSValue result;
    size_t index;

    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED,
                                         &list))
        return JS_EXCEPTION;
    result = JS_NewArray(ctx);
    if (JS_IsException(result))
        goto done;
    for (index = 0; index < list.count; index++) {
        JSValue text = JS_NewString(ctx, list.items[index]);

        if (JS_IsException(text) ||
            JS_DefinePropertyValueInt64(ctx, result, index, text,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
            JS_FreeValue(ctx, result);
            result = JS_EXCEPTION;
            break;
        }
    }
done:
    js_intl_locale_list_clear(ctx->rt, &list);
    return result;
}
