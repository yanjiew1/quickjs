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

JSValue js_intl_coerce_options(JSContext *ctx, JSValueConst options)
{
    if (JS_IsUndefined(options))
        return JS_NewObjectProto(ctx, JS_NULL);
    return JS_ToObject(ctx, options);
}

JSValue js_intl_get_options(JSContext *ctx, JSValueConst options)
{
    if (JS_IsUndefined(options))
        return JS_NewObjectProto(ctx, JS_NULL);
    if (!JS_IsObject(options))
        return JS_ThrowTypeError(ctx, "Intl options must be an object");
    return JS_DupValue(ctx, options);
}

int js_intl_get_string_option(JSContext *ctx, JSValueConst options,
                             const char *property, const char *const *values,
                             int value_count, int fallback, int *result)
{
    JSValue value, string;
    const char *text;
    size_t length;
    int i;

    value = JS_GetPropertyStr(ctx, options, property);
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value)) {
        JS_FreeValue(ctx, value);
        *result = fallback;
        return 0;
    }
    string = JS_ToString(ctx, value);
    JS_FreeValue(ctx, value);
    if (JS_IsException(string))
        return -1;
    text = JS_ToCStringLen(ctx, &length, string);
    JS_FreeValue(ctx, string);
    if (!text)
        return -1;
    for (i = 0; i < value_count; i++) {
        size_t expected = strlen(values[i]);
        if (expected == length && !memcmp(text, values[i], length)) {
            JS_FreeCString(ctx, text);
            *result = i;
            return 0;
        }
    }
    JS_FreeCString(ctx, text);
    JS_ThrowRangeError(ctx, "invalid Intl option %s", property);
    return -1;
}

int js_intl_get_string_option_alloc(JSContext *ctx, JSValueConst options,
                                   const char *property, char **result)
{
    JSValue value, string;
    const char *text;
    size_t length;
    char *copy;

    *result = NULL;
    value = JS_GetPropertyStr(ctx, options, property);
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value)) {
        JS_FreeValue(ctx, value);
        return 0;
    }
    string = JS_ToString(ctx, value);
    JS_FreeValue(ctx, value);
    if (JS_IsException(string))
        return -1;
    text = JS_ToCStringLen(ctx, &length, string);
    JS_FreeValue(ctx, string);
    if (!text)
        return -1;
    /* Every allocating option is an identifier. Preserve invalid embedded NUL
       rather than truncate it before service validation. */
    if (memchr(text, 0, length)) {
        JS_FreeCString(ctx, text);
        JS_ThrowRangeError(ctx, "invalid Intl option %s", property);
        return -1;
    }
    copy = js_malloc(ctx, length + 1);
    if (copy)
        memcpy(copy, text, length + 1);
    JS_FreeCString(ctx, text);
    if (!copy)
        return -1;
    *result = copy;
    return 0;
}

int js_intl_get_bool_option(JSContext *ctx, JSValueConst options,
                           const char *property, int fallback, int *result)
{
    JSValue value;
    int boolean;

    value = JS_GetPropertyStr(ctx, options, property);
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value)) {
        JS_FreeValue(ctx, value);
        *result = fallback;
        return 0;
    }
    boolean = JS_ToBool(ctx, value);
    JS_FreeValue(ctx, value);
    if (boolean < 0)
        return -1;
    *result = boolean;
    return 0;
}

int js_intl_default_number_option(JSContext *ctx, JSValueConst value,
                                 double minimum, double maximum,
                                 int fallback, int *result)
{
    double number;
    if (JS_IsUndefined(value)) {
        *result = fallback;
        return 0;
    }
    if (JS_ToFloat64(ctx, &number, value))
        return -1;
    if (isnan(number) || number < minimum || number > maximum) {
        JS_ThrowRangeError(ctx, "Intl numeric option out of range");
        return -1;
    }
    *result = (int)floor(number);
    return 0;
}

int js_intl_get_number_option(JSContext *ctx, JSValueConst options,
                             const char *property, double minimum,
                             double maximum, int fallback, int *result)
{
    JSValue value;
    int status;
    value = JS_GetPropertyStr(ctx, options, property);
    if (JS_IsException(value))
        return -1;
    status = js_intl_default_number_option(ctx, value, minimum, maximum,
                                          fallback, result);
    JS_FreeValue(ctx, value);
    return status;
}
#endif
