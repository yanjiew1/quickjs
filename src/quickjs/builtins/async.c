/*
 * QuickJS async intrinsic initialization
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#include "async.h"
#include "../internal/function.h"
#include "../internal/runtime.h"
#include "../internal/atom.h"
#include "../internal/object.h"
#include "../internal/generator.h"
#include "../internal/function-list.h"
#include "../internal/vm.h"
#include "../internal/iterator.h"
#include "function.h"
#include "iterator.h"
#include "async-from-sync-iterator.h"
#include "promise.h"

/* AsyncFunction */
static const JSCFunctionListEntry js_async_function_proto_funcs[] = {
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "AsyncFunction", JS_PROP_CONFIGURABLE ),
};

/* AsyncIteratorPrototype */

static JSValue js_async_iterator_dispose_unwrap(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}

static JSValue js_async_iterator_proto_async_dispose(JSContext *ctx,
                                                    JSValueConst this_val,
                                                    int argc,
                                                    JSValueConst *argv)
{
    JSValue promise, resolving_funcs[2], method, result, wrapper, handlers[2];
    JSValue ret;
    BOOL reject = FALSE;
    int status;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return promise;
    method = js_iterator_get_return_method(ctx, this_val);
    if (JS_IsException(method))
        goto reject;
    if (JS_IsUndefined(method)) {
        result = JS_UNDEFINED;
        goto settle;
    }
    result = JS_CallFree(ctx, method, this_val, 0, NULL);
    if (JS_IsException(result))
        goto reject;
    wrapper = js_promise_resolve(ctx, ctx->promise_ctor, 1,
                                (JSValueConst *)&result, 0);
    JS_FreeValue(ctx, result);
    if (JS_IsException(wrapper))
        goto reject;
    handlers[0] = JS_NewCFunction(ctx, js_async_iterator_dispose_unwrap, "", 1);
    handlers[1] = JS_UNDEFINED;
    if (JS_IsException(handlers[0])) {
        JS_FreeValue(ctx, wrapper);
        goto reject;
    }
    status = perform_promise_then(ctx, wrapper, (JSValueConst *)handlers,
                                 (JSValueConst *)resolving_funcs);
    JS_FreeValue(ctx, handlers[0]);
    JS_FreeValue(ctx, wrapper);
    if (status < 0)
        goto reject;
    goto done;

 reject:
    result = JS_GetException(ctx);
    reject = TRUE;
 settle:
    ret = JS_Call(ctx, resolving_funcs[reject], JS_UNDEFINED, 1,
                  (JSValueConst *)&result);
    JS_FreeValue(ctx, result);
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, promise);
        promise = ret;
    } else {
        JS_FreeValue(ctx, ret);
    }
 done:
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return promise;
}

static const JSCFunctionListEntry js_async_iterator_proto_funcs[] = {
    JS_CFUNC_DEF("[Symbol.asyncDispose]", 0, js_async_iterator_proto_async_dispose ),
    JS_CFUNC_DEF("[Symbol.asyncIterator]", 0, js_iterator_proto_iterator ),
};

/* AsyncGeneratorFunction */

static const JSCFunctionListEntry js_async_generator_function_proto_funcs[] = {
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "AsyncGeneratorFunction", JS_PROP_CONFIGURABLE ),
};

/* AsyncGenerator prototype */

static const JSCFunctionListEntry js_async_generator_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("next", 1, js_async_generator_next, GEN_MAGIC_NEXT ),
    JS_CFUNC_MAGIC_DEF("return", 1, js_async_generator_next, GEN_MAGIC_RETURN ),
    JS_CFUNC_MAGIC_DEF("throw", 1, js_async_generator_next, GEN_MAGIC_THROW ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "AsyncGenerator", JS_PROP_CONFIGURABLE ),
};

int js_init_async_intrinsics(JSContext *ctx)
{
    JSValue obj1;
    JSCFunctionType ft;

    /* AsyncFunction */
    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_ASYNC_FUNCTION, "AsyncFunction",
                                     ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_ASYNC,
                                     ctx->function_ctor,
                                     NULL, 0,
                                     js_async_function_proto_funcs, countof(js_async_function_proto_funcs),
                                     JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_READONLY);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    
    /* AsyncIteratorPrototype */
    ctx->async_iterator_proto =
        JS_NewObjectProtoList(ctx,  ctx->class_proto[JS_CLASS_OBJECT],
                              js_async_iterator_proto_funcs,
                              countof(js_async_iterator_proto_funcs));
    if (JS_IsException(ctx->async_iterator_proto))
        return -1;

    /* AsyncFromSyncIteratorPrototype */
    ctx->class_proto[JS_CLASS_ASYNC_FROM_SYNC_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->async_iterator_proto,
                              js_async_from_sync_iterator_proto_funcs,
                              countof(js_async_from_sync_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ASYNC_FROM_SYNC_ITERATOR]))
        return -1;
    
    /* AsyncGeneratorPrototype */
    ctx->class_proto[JS_CLASS_ASYNC_GENERATOR] =
        JS_NewObjectProtoList(ctx, ctx->async_iterator_proto, 
                              js_async_generator_proto_funcs,
                              countof(js_async_generator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ASYNC_GENERATOR]))
        return -1;

    /* AsyncGeneratorFunction */
    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_ASYNC_GENERATOR_FUNCTION, "AsyncGeneratorFunction",
                                     ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_ASYNC_GENERATOR,
                                     ctx->function_ctor,
                                     NULL, 0,
                                     js_async_generator_function_proto_funcs, countof(js_async_generator_function_proto_funcs),
                                     JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_READONLY);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);

    return JS_SetConstructor2(ctx, ctx->class_proto[JS_CLASS_ASYNC_GENERATOR_FUNCTION],
                              ctx->class_proto[JS_CLASS_ASYNC_GENERATOR],
                              JS_PROP_CONFIGURABLE, JS_PROP_CONFIGURABLE);
}
