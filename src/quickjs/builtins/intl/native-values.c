/*
 * QuickJS native internationalization support
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#include "intl-internal.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)

char *js_intl_strdup(JSContext *ctx, const char *value)
{
    size_t length = strlen(value);
    char *copy = js_malloc(ctx, length + 1);
    if (copy)
        memcpy(copy, value, length + 1);
    return copy;
}

char *js_intl_alloc_char(JSContext *ctx, int32_t required)
{
    if (required < 0 || required > JS_STRING_LEN_MAX) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return js_malloc(ctx, (size_t)required + 1);
}

int js_intl_native_error(JSContext *ctx, QJSIntlStatus status,
                         const char *operation)
{
    if (status == QJS_INTL_OK) return 0;
    if (status == QJS_INTL_NO_MEMORY || status == QJS_INTL_OVERFLOW)
        JS_ThrowOutOfMemory(ctx);
    else if (status == QJS_INTL_INVALID_ARGUMENT)
        JS_ThrowRangeError(ctx, "invalid native Intl %s input", operation);
    else if (status == QJS_INTL_UNSUPPORTED)
        JS_ThrowInternalError(ctx, "native Intl %s is unsupported", operation);
    else
        JS_ThrowInternalError(ctx, "native Intl %s data is invalid", operation);
    return -1;
}

int js_intl_define_string(JSContext *ctx, JSValueConst object,
                          const char *property, const char *value)
{
    JSValue string = JS_NewString(ctx, value);
    if (JS_IsException(string))
        return -1;
    return JS_DefinePropertyValueStr(ctx, object, property, string,
                                     JS_PROP_C_W_E) < 0 ? -1 : 0;
}

int js_intl_define_int(JSContext *ctx, JSValueConst object,
                       const char *property, int value)
{
    return JS_DefinePropertyValueStr(ctx, object, property,
                                     JS_NewInt32(ctx, value), JS_PROP_C_W_E)
            < 0 ? -1 : 0;
}

int js_intl_define_bool(JSContext *ctx, JSValueConst object,
                        const char *property, BOOL value)
{
    return JS_DefinePropertyValueStr(ctx, object, property,
                                     JS_NewBool(ctx, value), JS_PROP_C_W_E)
            < 0 ? -1 : 0;
}

int js_intl_add_part(JSContext *ctx, JSValueConst parts, uint32_t index,
                     const char *type, JSValueConst value,
                     const char *extra_name, JSValueConst extra)
{
    JSValue part = JS_NewObject(ctx);
    if (JS_IsException(part))
        return -1;
    if (js_intl_define_string(ctx, part, "type", type) < 0 ||
        JS_DefinePropertyValueStr(ctx, part, "value", JS_DupValue(ctx, value),
                                  JS_PROP_C_W_E) < 0 ||
        (extra_name &&
         JS_DefinePropertyValueStr(ctx, part, extra_name,
                                   JS_DupValue(ctx, extra), JS_PROP_C_W_E) < 0)) {
        JS_FreeValue(ctx, part);
        return -1;
    }
    return JS_DefinePropertyValueUint32(ctx, parts, index, part, JS_PROP_C_W_E)
            < 0 ? -1 : 0;
}

void js_intl_locale_list_free(JSContext *ctx, JSIntlLocaleList *list)
{
    size_t i;
    for (i = 0; i < list->count; i++)
        js_free(ctx, list->items[i]);
    js_free(ctx, list->items);
    memset(list, 0, sizeof(*list));
}

int js_intl_locale_list_append(JSContext *ctx, JSIntlLocaleList *list,
                               const char *value)
{
    char *copy = js_intl_strdup(ctx, value);
    if (!copy)
        return -1;
    if (list->count == list->capacity) {
        size_t capacity;
        char **items;
        if (list->capacity > (SIZE_MAX / sizeof(*items) - 4) / 2) {
            js_free(ctx, copy);
            JS_ThrowOutOfMemory(ctx);
            return -1;
        }
        capacity = list->capacity * 2 + 4;
        items = js_realloc(ctx, list->items, capacity * sizeof(*items));
        if (!items) {
            js_free(ctx, copy);
            return -1;
        }
        list->capacity = capacity;
        list->items = items;
    }
    list->items[list->count++] = copy;
    return 0;
}

void js_intl_resolved_locale_free(JSContext *ctx, JSIntlResolvedLocale *locale)
{
    int i;
    js_free(ctx, locale->locale);
    js_free(ctx, locale->data_locale);
    for (i = 0; i < JS_INTL_MAX_RESOLUTION_KEYS; i++)
        js_free(ctx, locale->values[i]);
    memset(locale, 0, sizeof(*locale));
}
#endif
