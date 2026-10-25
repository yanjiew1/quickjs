/* Promise thenable job allocation failure.
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
#include <string.h>
#include "quickjs.h"
#include "pool-padding.h"

static void *nested_padding;
static JSValue nested_error;

static JSValue resolve_inner(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic,
                              JSValue *data)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue result;

    nested_padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    result = JS_Call(ctx, argv[0], JS_UNDEFINED, 1, (JSValueConst *)data);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(JS_IsException(result));
    assert(JS_HasException(ctx));
    nested_error = JS_GetException(ctx);
    return JS_Throw(ctx, JS_DupValue(ctx, nested_error));
}

static void test_nested(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    const char *source = "({ then(resolve) { resolve(2); } })";
    JSValue inner, outer, then, promise, funcs[2], result;
    int jobs = 0;

    inner = JS_Eval(ctx, source, strlen(source), "<nested-promise-oom>",
                    JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(inner));
    outer = JS_NewObject(ctx);
    assert(!JS_IsException(outer));
    then = JS_NewCFunctionData(ctx, resolve_inner, 1, 0, 1,
                              (JSValueConst *)&inner);
    assert(!JS_IsException(then));
    assert(JS_SetPropertyStr(ctx, outer, "then", then) >= 0);
    promise = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(promise));
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 1, (JSValueConst *)&outer);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    while (JS_IsJobPending(rt)) {
        assert(++jobs < 10);
        assert(JS_ExecutePendingJob(rt, NULL) > 0);
    }
    assert(jobs == 1);
    assert(!JS_HasException(ctx));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    result = JS_PromiseResult(ctx, promise);
    /* Under complete exhaustion even allocating the OOM Error can fail. */
    if (JS_IsNull(nested_error)) {
        assert(JS_IsNull(result));
    } else {
        assert(JS_IsObject(nested_error) && JS_IsObject(result));
        assert(JS_VALUE_GET_PTR(result) == JS_VALUE_GET_PTR(nested_error));
    }
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, nested_error);
    nested_error = JS_UNDEFINED;
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, outer);
    JS_FreeValue(ctx, inner);
    test_pool_padding_free(rt, nested_padding);
    nested_padding = NULL;
}

int main(void)
{
    const char *source = "({ then(resolve) { resolve(1); } })";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue promise, funcs[2], thenable, result, error;
    void *padding;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    promise = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(promise));
    thenable = JS_Eval(ctx, source, strlen(source), "<promise-oom>",
                       JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(thenable));
    padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 1,
                     (JSValueConst *)&thenable);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(JS_IsException(result));
    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    assert(!JS_IsJobPending(rt));
    /* The failed attempt must not permanently disable this resolving pair. */
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 1,
                     (JSValueConst *)&thenable);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    assert(JS_ExecutePendingJob(rt, NULL) == 1);
    assert(!JS_IsJobPending(rt));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, thenable);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    JS_FreeValue(ctx, promise);
    test_pool_padding_free(rt, padding);
    test_nested(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
