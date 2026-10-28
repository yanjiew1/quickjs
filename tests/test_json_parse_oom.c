/* JSON.parse reviver setup allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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

static int test_define_property_value(JSContext *ctx, JSValueConst obj,
                                      JSAtom prop, JSValue val, int flags);
#define JS_DefinePropertyValue test_define_property_value
#include "../src/quickjs/builtins/json.c"
#undef JS_DefinePropertyValue

static int fail_root_property, failures;

static int test_define_property_value(JSContext *ctx, JSValueConst obj,
                                      JSAtom prop, JSValue val, int flags)
{
    if (fail_root_property && prop == JS_ATOM_empty_string) {
        fail_root_property = 0;
        failures++;
        JS_FreeValue(ctx, val);
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    return JS_DefinePropertyValue(ctx, obj, prop, val, flags);
}

static JSValue identity_reviver(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    return JS_DupValue(ctx, argv[1]);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue text, reviver, result, error;
    JSValueConst args[2];

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    text = JS_NewString(ctx, "{\"a\":1}");
    reviver = JS_NewCFunction(ctx, identity_reviver, "reviver", 2);
    assert(!JS_IsException(text) && !JS_IsException(reviver));
    args[0] = text;
    args[1] = reviver;

    /* Force only the root holder's property definition to fail. */
    fail_root_property = 1;
    result = js_json_parse(ctx, JS_UNDEFINED, 2, args);
    assert(JS_IsException(result) && failures == 1);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, reviver);
    JS_FreeValue(ctx, text);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
