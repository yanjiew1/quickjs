/*
 * QuickJS asynchronous resource management builtins
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */

#include "../internal/atom.h"
#include "../internal/base.h"
#include "../internal/function.h"
#include "../internal/runtime.h"
#include "../internal/object.h"
#include "../internal/function-list.h"
#include "resource-management.h"
#include "promise.h"

typedef struct JSAsyncDisposableStackData {
    JSDisposableStackData base;
    JSContext *realm;
    JSValue resolving_funcs[2];
    BOOL active;
    BOOL waiting;
} JSAsyncDisposableStackData;

static void js_async_dispose_release_realm(JSAsyncDisposableStackData *stack)
{
    JSContext *realm = stack->realm;

    stack->realm = NULL;
    if (realm)
        JS_FreeContext(realm);
}

static void js_async_disposable_stack_finalizer(JSRuntime *rt, JSValue value)
{
    JSAsyncDisposableStackData *stack =
        JS_GetOpaque(value, JS_CLASS_ASYNC_DISPOSABLE_STACK);

    if (!stack)
        return;
    js_disposable_stack_clear(rt, &stack->base);
    JS_FreeValueRT(rt, stack->resolving_funcs[0]);
    JS_FreeValueRT(rt, stack->resolving_funcs[1]);
    js_async_dispose_release_realm(stack);
    js_free_rt(rt, stack);
}

static void js_async_disposable_stack_mark(JSRuntime *rt, JSValueConst value,
                                           JS_MarkFunc *mark_func)
{
    JSAsyncDisposableStackData *stack =
        JS_GetOpaque(value, JS_CLASS_ASYNC_DISPOSABLE_STACK);

    if (!stack)
        return;
    if (stack->realm)
        mark_func(rt, &stack->realm->header);
    js_disposable_stack_mark_data(rt, &stack->base, mark_func);
    JS_MarkValue(rt, stack->resolving_funcs[0], mark_func);
    JS_MarkValue(rt, stack->resolving_funcs[1], mark_func);
}

static void js_async_disposable_stack_initialize(JSValueConst object)
{
    JSAsyncDisposableStackData *stack =
        JS_GetOpaque(object, JS_CLASS_ASYNC_DISPOSABLE_STACK);

    stack->resolving_funcs[0] = JS_UNDEFINED;
    stack->resolving_funcs[1] = JS_UNDEFINED;
}

static JSValue js_async_disposable_stack_constructor(JSContext *ctx,
                                                     JSValueConst new_target,
                                                     int argc,
                                                     JSValueConst *argv)
{
    JSValue object;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    object = js_new_disposable_stack(ctx, new_target,
                                    JS_CLASS_ASYNC_DISPOSABLE_STACK,
                                    sizeof(JSAsyncDisposableStackData));
    if (!JS_IsException(object))
        js_async_disposable_stack_initialize(object);
    return object;
}

static JSValue js_async_disposable_stack_move(JSContext *ctx,
                                              JSValueConst value,
                                              int argc, JSValueConst *argv)
{
    JSValue object = js_disposable_stack_move(ctx, value,
                                              JS_CLASS_ASYNC_DISPOSABLE_STACK,
                                              sizeof(JSAsyncDisposableStackData));

    if (!JS_IsException(object))
        js_async_disposable_stack_initialize(object);
    return object;
}

static void js_async_dispose_settle(JSContext *ctx, JSValueConst *funcs,
                                    JSValueConst value, BOOL reject)
{
    JSValue result = JS_Call(ctx, funcs[reject], JS_UNDEFINED, 1, &value);

    if (JS_IsException(result))
        result = JS_GetException(ctx);
    JS_FreeValue(ctx, result);
}

/* The synchronous fallback ignores its result, including returned promises. */
static JSValue js_async_dispose_sync_wrapper(JSContext *ctx,
                                             JSValueConst value,
                                             int argc, JSValueConst *argv,
                                             int magic, JSValue *data)
{
    JSValue promise, funcs[2], result;
    BOOL reject;

    /* The private intrinsic owns the hidden builtin creation realm. */
    ctx = JS_GetFunctionRealm(ctx, data[1]);

    promise = JS_NewPromiseCapability(ctx, funcs);
    if (JS_IsException(promise))
        return promise;
    result = JS_Call(ctx, data[0], value, 0, NULL);
    reject = JS_IsException(result);
    if (reject) {
        result = JS_GetException(ctx);
    } else {
        JS_FreeValue(ctx, result);
        result = JS_UNDEFINED;
    }
    js_async_dispose_settle(ctx, (JSValueConst *)funcs, result, reject);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    return promise;
}

static JSValue js_get_disposal_method(JSContext *ctx, JSValueConst value,
                                      JSAtom atom)
{
    JSValue method = JS_GetProperty(ctx, value, atom);

    if (JS_IsException(method))
        return method;
    if (JS_IsNull(method)) {
        JS_FreeValue(ctx, method);
        return JS_UNDEFINED;
    }
    if (!JS_IsUndefined(method) && !JS_IsFunction(ctx, method)) {
        JS_FreeValue(ctx, method);
        return JS_ThrowTypeError(ctx, "dispose method is not callable");
    }
    return method;
}

JSValue js_get_async_dispose_method(JSContext *ctx, JSValueConst value)
{
    JSValue method, wrapper;
    JSValueConst data[2];

    method = js_get_disposal_method(ctx, value, JS_ATOM_Symbol_asyncDispose);
    if (!JS_IsUndefined(method))
        return method;
    method = js_get_disposal_method(ctx, value, JS_ATOM_Symbol_dispose);
    if (JS_IsException(method) || JS_IsUndefined(method))
        return method;
    data[0] = method;
    data[1] = ctx->promise_ctor;
    wrapper = JS_NewCFunctionData(ctx, js_async_dispose_sync_wrapper,
                                  0, 0, countof(data), data);
    JS_FreeValue(ctx, method);
    return wrapper;
}

static void js_async_dispose_drain(JSContext *ctx, JSValueConst value,
                                   JSAsyncDisposableStackData *stack,
                                   JSValueConst input, BOOL is_throw);

static void js_async_dispose_finish(JSContext *ctx,
                                    JSAsyncDisposableStackData *stack,
                                    JSValueConst error, BOOL reject)
{
    JSValue funcs[2];

    funcs[0] = stack->resolving_funcs[0];
    funcs[1] = stack->resolving_funcs[1];
    stack->resolving_funcs[0] = JS_UNDEFINED;
    stack->resolving_funcs[1] = JS_UNDEFINED;
    stack->active = FALSE;
    stack->waiting = FALSE;
    js_async_dispose_settle(ctx, (JSValueConst *)funcs, error, reject);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
}

static JSValue js_async_dispose_resume(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int reject, JSValue *data)
{
    JSAsyncDisposableStackData *stack =
        JS_GetOpaque(data[0], JS_CLASS_ASYNC_DISPOSABLE_STACK);

    if (!stack->active || !stack->waiting)
        return JS_UNDEFINED;
    /* Await resumes the first disposeAsync invocation context. */
    ctx = stack->realm;
    stack->waiting = FALSE;
    js_async_dispose_drain(ctx, data[0], stack, argv[0], reject);
    if (!stack->active)
        js_async_dispose_release_realm(stack);
    return JS_UNDEFINED;
}

/* Await uses intrinsic reactions without reading .then or species. */
static int js_async_dispose_await(JSContext *ctx, JSValueConst value,
                                  JSAsyncDisposableStackData *stack,
                                  JSValueConst result)
{
    JSValue promise, handlers[2] = { JS_UNDEFINED, JS_UNDEFINED };
    JSValueConst funcs[2] = { JS_UNDEFINED, JS_UNDEFINED };
    int status = -1;

    stack->waiting = TRUE;
    promise = js_promise_resolve(ctx, ctx->promise_ctor, 1, &result, 0);
    if (JS_IsException(promise))
        goto done;
    handlers[0] = JS_NewCFunctionData(ctx, js_async_dispose_resume,
                                      1, 0, 1, &value);
    if (JS_IsException(handlers[0]))
        goto done;
    handlers[1] = JS_NewCFunctionData(ctx, js_async_dispose_resume,
                                      1, 1, 1, &value);
    if (JS_IsException(handlers[1]))
        goto done;
    status = perform_promise_then(ctx, promise, (JSValueConst *)handlers, funcs);
 done:
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, handlers[0]);
    JS_FreeValue(ctx, handlers[1]);
    if (status < 0)
        stack->waiting = FALSE;
    return status;
}

static void js_async_dispose_drain(JSContext *ctx, JSValueConst value,
                                   JSAsyncDisposableStackData *stack,
                                   JSValueConst input, BOOL is_throw)
{
    JSValue failure = JS_UNDEFINED;

    for (;;) {
        JSValue result;
        int status = js_dispose_resources_step(ctx, stack->base.resources,
                                               input, is_throw, &result);

        JS_FreeValue(ctx, failure);
        failure = JS_UNDEFINED;
        if (status <= 0) {
            if (status < 0)
                failure = JS_GetException(ctx);
            js_async_dispose_finish(ctx, stack, failure, status < 0);
            JS_FreeValue(ctx, failure);
            return;
        }
        status = js_async_dispose_await(ctx, value, stack, result);
        JS_FreeValue(ctx, result);
        if (status == 0)
            return;
        failure = JS_GetException(ctx);
        input = failure;
        is_throw = TRUE;
    }
}

static JSValue js_async_disposable_stack_dispose(JSContext *ctx,
                                                 JSValueConst value,
                                                 int argc,
                                                 JSValueConst *argv)
{
    JSAsyncDisposableStackData *stack;
    JSValue promise, funcs[2], error;

    promise = JS_NewPromiseCapability(ctx, funcs);
    if (JS_IsException(promise))
        return promise;
    stack = JS_GetOpaque2(ctx, value, JS_CLASS_ASYNC_DISPOSABLE_STACK);
    if (!stack || stack->base.disposed) {
        BOOL reject = stack == NULL;
        error = reject ? JS_GetException(ctx) : JS_UNDEFINED;
        js_async_dispose_settle(ctx, (JSValueConst *)funcs, error, reject);
        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, funcs[0]);
        JS_FreeValue(ctx, funcs[1]);
        return promise;
    }
    stack->base.disposed = TRUE;
    stack->realm = JS_DupContext(ctx);
    stack->resolving_funcs[0] = funcs[0];
    stack->resolving_funcs[1] = funcs[1];
    stack->active = TRUE;
    js_async_dispose_drain(ctx, value, stack, JS_UNDEFINED, FALSE);
    if (!stack->active)
        js_async_dispose_release_realm(stack);
    return promise;
}

static const JSCFunctionListEntry js_async_disposable_stack_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("use", 1, js_disposable_stack_use,
                       JS_CLASS_ASYNC_DISPOSABLE_STACK),
    JS_CFUNC_MAGIC_DEF("adopt", 2, js_disposable_stack_adopt,
                       JS_CLASS_ASYNC_DISPOSABLE_STACK),
    JS_CFUNC_MAGIC_DEF("defer", 1, js_disposable_stack_defer,
                       JS_CLASS_ASYNC_DISPOSABLE_STACK),
    JS_CFUNC_DEF("move", 0, js_async_disposable_stack_move),
    JS_CFUNC_DEF("disposeAsync", 0, js_async_disposable_stack_dispose),
    JS_CGETSET_MAGIC_DEF("disposed", js_disposable_stack_get_disposed, NULL,
                         JS_CLASS_ASYNC_DISPOSABLE_STACK),
    JS_ALIAS_DEF("[Symbol.asyncDispose]", "disposeAsync"),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "AsyncDisposableStack",
                       JS_PROP_CONFIGURABLE),
};

int js_init_async_disposable_stack(JSContext *ctx)
{
    static const JSClassDef stack_class = {
        .class_name = "AsyncDisposableStack",
        .finalizer = js_async_disposable_stack_finalizer,
        .gc_mark = js_async_disposable_stack_mark,
    };
    JSValue constructor;

    if (js_init_disposable_resource_list(ctx) < 0)
        return -1;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_ASYNC_DISPOSABLE_STACK) &&
        JS_NewClass(ctx->rt, JS_CLASS_ASYNC_DISPOSABLE_STACK, &stack_class) < 0)
        return -1;
    constructor = JS_NewCConstructor(ctx, JS_CLASS_ASYNC_DISPOSABLE_STACK,
                                     "AsyncDisposableStack",
                                     js_async_disposable_stack_constructor, 0,
                                     JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED, NULL, 0,
                                     js_async_disposable_stack_proto_funcs,
                                     countof(js_async_disposable_stack_proto_funcs),
                                     0);
    if (JS_IsException(constructor))
        return -1;
    JS_FreeValue(ctx, constructor);
    return 0;
}
