/*
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

// Copyright 2025 Google LLC
/* Exercise JSON parsing and serialization without evaluating fuzz input as code. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    JSRuntime *rt;
    JSContext *ctx;
    JSValue value, json;
    char *input;

    if (size == SIZE_MAX)
        return 0;
    input = malloc(size + 1);
    if (!input)
        return 0;
    memcpy(input, data, size);
    input[size] = '\0';
    rt = JS_NewRuntime();
    if (!rt) {
        free(input);
        return 0;
    }
    JS_SetMemoryLimit(rt, 64 * 1024 * 1024);
    JS_SetMaxStackSize(rt, 64 * 1024);
    ctx = JS_NewContext(rt);
    if (ctx) {
        value = JS_ParseJSON(ctx, input, size, "<json-fuzz>");
        if (JS_IsException(value)) {
            JS_FreeValue(ctx, JS_GetException(ctx));
        } else {
            json = JS_JSONStringify(ctx, value, JS_UNDEFINED, JS_UNDEFINED);
            if (JS_IsException(json))
                JS_FreeValue(ctx, JS_GetException(ctx));
            else
                JS_FreeValue(ctx, json);
            JS_FreeValue(ctx, value);
        }
        JS_FreeContext(ctx);
    }
    JS_FreeRuntime(rt);
    free(input);
    return 0;
}
