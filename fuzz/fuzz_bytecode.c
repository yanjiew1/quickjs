/*
 * Copyright 2025 Google LLC
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

#include <stdint.h>
#include <stddef.h>
#include "quickjs.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    JSRuntime *rt;
    JSContext *ctx;
    JSValue obj;

    rt = JS_NewRuntime();
    if (!rt)
        return 0;
    JS_SetMemoryLimit(rt, 64 * 1024 * 1024);
    JS_SetMaxStackSize(rt, 64 * 1024);
    ctx = JS_NewContext(rt);
    if (ctx) {
        /* Deserialization is fuzzed, but arbitrary bytecode is never executed. */
        obj = JS_ReadObject(ctx, data, size, JS_READ_OBJ_BYTECODE);
        if (JS_IsException(obj))
            JS_FreeValue(ctx, JS_GetException(ctx));
        else
            JS_FreeValue(ctx, obj);
        JS_FreeContext(ctx);
    }
    JS_FreeRuntime(rt);
    return 0;
}
