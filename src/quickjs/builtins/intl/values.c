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
#ifdef CONFIG_INTL

char *js_intl_strdup(JSContext *ctx, const char *value)
{
    size_t length = strlen(value);
    char *copy = js_malloc(ctx, length + 1);
    if (copy)
        memcpy(copy, value, length + 1);
    return copy;
}

UChar *js_intl_alloc_uchar(JSContext *ctx, int32_t required)
{
    if (required < 0 || required > JS_STRING_LEN_MAX ||
        (size_t)required > SIZE_MAX / sizeof(UChar) - 1) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return js_malloc(ctx, ((size_t)required + 1) * sizeof(UChar));
}

char *js_intl_alloc_char(JSContext *ctx, int32_t required)
{
    if (required < 0 || required > JS_STRING_LEN_MAX) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return js_malloc(ctx, (size_t)required + 1);
}

int js_intl_to_uchar(JSContext *ctx, JSValueConst value,
                   UChar **result, int32_t *length)
{
    JSValue string;
    JSString *s;
    UChar *buffer;
    uint32_t i;

    *result = NULL;
    *length = 0;
    string = JS_ToString(ctx, value);
    if (JS_IsException(string))
        return -1;
    if (JS_VALUE_GET_TAG(string) == JS_TAG_STRING_ROPE) {
        string = js_linearize_string_rope(ctx, string);
        if (JS_IsException(string))
            return -1;
    }
    s = JS_VALUE_GET_STRING(string);
    buffer = js_intl_alloc_uchar(ctx, s->len);
    if (!buffer) {
        JS_FreeValue(ctx, string);
        return -1;
    }
    if (s->is_wide_char)
        memcpy(buffer, s->u.str16, (size_t)s->len * sizeof(*buffer));
    else
        for (i = 0; i < s->len; i++)
            buffer[i] = s->u.str8[i];
    buffer[s->len] = 0;
    *length = s->len;
    *result = buffer;
    JS_FreeValue(ctx, string);
    return 0;
}

JSValue js_intl_from_uchar(JSContext *ctx, const UChar *value, int32_t length)
{
    if (length < 0 || length > JS_STRING_LEN_MAX)
        return JS_ThrowOutOfMemory(ctx);
    return js_new_string16_len(ctx, value, length);
}

int js_intl_icu_error(JSContext *ctx, UErrorCode status, const char *operation)
{
    if (U_SUCCESS(status))
        return 0;
    if (status == U_MEMORY_ALLOCATION_ERROR)
        JS_ThrowOutOfMemory(ctx);
    else
        JS_ThrowInternalError(ctx, "ICU %s failed: %s", operation,
                              u_errorName(status));
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

int js_intl_add_part_uchar(JSContext *ctx, JSValueConst parts, uint32_t index,
                          const char *type, const UChar *value, int32_t length,
                          const char *extra_name, JSValueConst extra)
{
    JSValue string = js_intl_from_uchar(ctx, value, length);
    int status;
    if (JS_IsException(string))
        return -1;
    status = js_intl_add_part(ctx, parts, index, type, string,
                              extra_name, extra);
    JS_FreeValue(ctx, string);
    return status;
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
    js_free(ctx, locale->icu_locale);
    for (i = 0; i < JS_INTL_MAX_RESOLUTION_KEYS; i++)
        js_free(ctx, locale->values[i]);
    memset(locale, 0, sizeof(*locale));
}
#endif
