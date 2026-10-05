/*
 * QuickJS fuzz initialization tests
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quickjs.h"
#include "quickjs-libc.h"

/* Entry point implemented by fuzz/fuzz_common.c. */
void test_one_input_init(JSRuntime *rt, JSContext *ctx);

static JSValue eval_source(JSContext *ctx, const char *source,
                           const char *filename, int flags)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), filename, flags);
    return js_std_await(ctx, result);
}

static void check_result(JSContext *ctx, JSValue result)
{
    if (JS_IsException(result)) {
        js_std_dump_error(ctx);
        abort();
    }
    JS_FreeValue(ctx, result);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue result, error, name;
    const char *text;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_one_input_init(rt, ctx);
    result = eval_source(ctx,
                         "typeof std.sprintf === 'function' &&"
                         "typeof os.open === 'function'",
                         "tests/fuzz-globals.js", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result) && JS_ToBool(ctx, result) == 1);
    JS_FreeValue(ctx, result);

    result = eval_source(ctx,
                         "import data from '../examples/message.json'"
                         " with { type: 'json' };"
                         "if (data.x !== 1 || data.tab.length !== 3)"
                         " throw new Error('unexpected JSON module');",
                         "tests/fuzz-json.mjs", JS_EVAL_TYPE_MODULE);
    check_result(ctx, result);

    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    rt = JS_NewRuntime();
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_one_input_init(rt, ctx);

    result = eval_source(ctx,
                         "import data from '../examples/message.json'"
                         " with { unsupported: 'value' };",
                         "tests/fuzz-invalid-attribute.mjs", JS_EVAL_TYPE_MODULE);
    assert(JS_IsException(result));
    error = JS_GetException(ctx);
    name = JS_GetPropertyStr(ctx, error, "name");
    text = JS_ToCString(ctx, name);
    assert(text && strcmp(text, "TypeError") == 0);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, error);

    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
