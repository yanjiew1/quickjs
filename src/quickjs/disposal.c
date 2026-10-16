/*
 * QuickJS shared disposal operations
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */

#include "internal/atom.h"
#include "internal/runtime.h"
#include "internal/function.h"
#include "internal/disposal.h"
#include "builtins/error.h"

void js_async_dispose_settle(JSContext *ctx, JSValueConst *funcs,
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

static JSValue js_get_async_dispose_method(JSContext *ctx, JSValueConst value)
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

static JSValue js_get_dispose_method(JSContext *ctx, JSValueConst value)
{
    JSValue method = JS_GetProperty(ctx, value, JS_ATOM_Symbol_dispose);

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

JSValue js_disposable_resource_method(JSContext *ctx, JSValueConst value,
                                      JSDisposalKind kind)
{
    BOOL nullish = JS_IsNull(value) || JS_IsUndefined(value);
    JSValue method;

    if (!nullish && !JS_IsObject(value))
        return JS_ThrowTypeError(ctx, "resource is not an object");
    method = nullish ? JS_UNDEFINED : kind == JS_DISPOSAL_ASYNC ?
        js_get_async_dispose_method(ctx, value) :
        js_get_dispose_method(ctx, value);
    if (JS_IsUndefined(method) && !nullish)
        method = JS_ThrowTypeError(ctx, "resource has no dispose method");
    return method;
}

void js_disposable_resource_add_error(JSContext *ctx, JSValue *error,
                                      BOOL *has_error, JSValue next_error)
{
    if (*has_error) {
        JSValue combined = js_new_suppressed_error(ctx, next_error, *error);
        JS_FreeValue(ctx, next_error);
        JS_FreeValue(ctx, *error);
        *error = JS_IsException(combined) ? JS_GetException(ctx) : combined;
    } else {
        *error = next_error;
        *has_error = TRUE;
    }
}
