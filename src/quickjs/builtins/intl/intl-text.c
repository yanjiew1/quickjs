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
#include "intl-text.h"
#include "../../internal/allocator.h"
#include "../../internal/string.h"
#include "../../value/conversion.h"
#ifdef CONFIG_INTL

uint16_t *js_intl_alloc_utf16(JSContext *ctx, int32_t required)
{
    if (required < 0 || required > JS_STRING_LEN_MAX ||
        (size_t)required > SIZE_MAX / sizeof(uint16_t) - 1) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return js_malloc(ctx, ((size_t)required + 1) * sizeof(uint16_t));
}

int js_intl_to_utf16(JSContext *ctx, JSValueConst value,
                   uint16_t **result, int32_t *length)
{
    JSValue string;
    JSString *s;
    uint16_t *buffer;
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
    buffer = js_intl_alloc_utf16(ctx, s->len);
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

JSValue js_intl_from_utf16(JSContext *ctx, const uint16_t *value, int32_t length)
{
    if (length < 0 || length > JS_STRING_LEN_MAX)
        return JS_ThrowOutOfMemory(ctx);
    return js_new_string16_len(ctx, value, length);
}

#endif
