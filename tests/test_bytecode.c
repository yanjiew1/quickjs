/*
 * QuickJS bytecode serialization tests
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
#include <assert.h>

#include "quickjs.h"

static void test_bytecode_roundtrip(void)
{
    static const char source[] = "function saved() { return 42; } saved();";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled, loaded, result;
    uint8_t *buf;
    size_t len;
    int32_t value;
    assert(rt);
    JS_SetStripInfo(rt, 0);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_Eval(ctx, source, sizeof(source) - 1, "roundtrip.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    buf = JS_WriteObject(ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf && len > 0);
    JS_FreeValue(ctx, compiled);
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(ctx, buf);
    assert(!JS_IsException(loaded));
    result = JS_EvalFunction(ctx, loaded);
    assert(!JS_IsException(result));
    assert(JS_ToInt32(ctx, &value, result) == 0 && value == 42);
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_bytecode_roundtrip();
    return 0;
}
