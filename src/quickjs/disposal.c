/*
 * QuickJS disposable resource traversal
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */

#include "internal/atom.h"
#include "internal/base.h"
#include "internal/runtime.h"
#include "internal/object.h"
#include "internal/function.h"
#include "internal/c-function.h"
#include "internal/disposal.h"
#include "builtins/error.h"

static int js_init_disposable_resource_list(JSContext *ctx);

typedef enum {
    JS_DISPOSABLE_USE,
    JS_DISPOSABLE_ADOPT,
    JS_DISPOSABLE_DEFER,
} JSDisposableInvocation;

typedef struct JSDisposableResource {
    struct JSDisposableResource *next;
    JSValue value;
    JSValue method;
    JSDisposableInvocation invocation;
    JSDisposalKind kind;
} JSDisposableResource;

typedef enum {
    JS_DISPOSAL_AWAIT_NONE,
    JS_DISPOSAL_AWAIT_METHOD,
    JS_DISPOSAL_AWAIT_BEFORE_SYNC,
    JS_DISPOSAL_AWAIT_END,
} JSDisposalAwaitReason;

typedef struct JSDisposableResourceList {
    JSDisposableResource *head;
    JSDisposableResource *active;
    JSValue error;
    BOOL started;
    BOOL done;
    BOOL has_error;
    BOOL needs_await;
    BOOL has_awaited;
    JSDisposalAwaitReason await_reason;
} JSDisposableResourceList;

static void js_disposable_resource_free(JSRuntime *rt, JSDisposableResource *resource)
{
    JS_FreeValueRT(rt, resource->value);
    JS_FreeValueRT(rt, resource->method);
    js_free_rt(rt, resource);
}

static void js_disposable_resource_chain_free(JSRuntime *rt,
                                              JSDisposableResource *resource)
{
    while (resource) {
        JSDisposableResource *next = resource->next;
        js_disposable_resource_free(rt, resource);
        resource = next;
    }
}

static void js_disposable_resource_chain_mark(JSRuntime *rt,
                                              JSDisposableResource *resource,
                                              JS_MarkFunc *mark_func)
{
    for (; resource; resource = resource->next) {
        JS_MarkValue(rt, resource->value, mark_func);
        JS_MarkValue(rt, resource->method, mark_func);
    }
}

static void js_disposable_resource_list_finalizer(JSRuntime *rt, JSValue value)
{
    JSDisposableResourceList *list =
        JS_GetOpaque(value, JS_CLASS_DISPOSABLE_RESOURCE_LIST);

    if (!list)
        return;
    js_disposable_resource_chain_free(rt, list->head);
    js_disposable_resource_chain_free(rt, list->active);
    JS_FreeValueRT(rt, list->error);
    js_free_rt(rt, list);
}

static void js_disposable_resource_list_mark(JSRuntime *rt, JSValueConst value,
                                             JS_MarkFunc *mark_func)
{
    JSDisposableResourceList *list =
        JS_GetOpaque(value, JS_CLASS_DISPOSABLE_RESOURCE_LIST);

    if (!list)
        return;
    js_disposable_resource_chain_mark(rt, list->head, mark_func);
    js_disposable_resource_chain_mark(rt, list->active, mark_func);
    JS_MarkValue(rt, list->error, mark_func);
}

JSValue js_new_disposable_resource_list(JSContext *ctx)
{
    JSDisposableResourceList *list;
    JSValue value;

    if (js_init_disposable_resource_list(ctx) < 0)
        return JS_EXCEPTION;
    value = JS_NewObjectProtoClass(ctx, JS_NULL,
                                  JS_CLASS_DISPOSABLE_RESOURCE_LIST);
    if (JS_IsException(value))
        return value;
    list = js_mallocz(ctx, sizeof(*list));
    if (!list) {
        JS_FreeValue(ctx, value);
        return JS_EXCEPTION;
    }
    list->error = JS_UNDEFINED;
    JS_SetOpaque(value, list);
    return value;
}

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

static int js_disposable_resource_add(JSContext *ctx,
                                      JSValueConst resources,
                                      JSValueConst value, JSValue method,
                                      int invocation, JSDisposalKind kind)
{
    JSDisposableResourceList *list =
        JS_GetOpaque(resources, JS_CLASS_DISPOSABLE_RESOURCE_LIST);
    JSDisposableResource *resource;

    resource = js_malloc(ctx, sizeof(*resource));
    if (!resource) {
        JS_FreeValue(ctx, method);
        return -1;
    }
    resource->value = JS_DupValue(ctx, value);
    resource->method = method;
    resource->invocation = invocation;
    resource->kind = kind;
    resource->next = list->head;
    list->head = resource;
    return 0;
}

int js_add_disposable_resource(JSContext *ctx,
                               JSValueConst resources, JSValueConst value,
                               JSDisposalKind kind)
{
    BOOL nullish = JS_IsNull(value) || JS_IsUndefined(value);
    JSValue method;

    if (nullish && kind == JS_DISPOSAL_SYNC)
        return 0;
    if (!nullish && !JS_IsObject(value)) {
        JS_ThrowTypeError(ctx, "resource is not an object");
        return -1;
    }
    method = nullish ? JS_UNDEFINED : kind == JS_DISPOSAL_ASYNC ?
        js_get_async_dispose_method(ctx, value) :
        js_get_dispose_method(ctx, value);
    if (JS_IsUndefined(method) && !nullish)
        method = JS_ThrowTypeError(ctx, "resource has no dispose method");
    if (JS_IsException(method))
        return -1;
    return js_disposable_resource_add(ctx, resources,
                                      nullish ? JS_UNDEFINED : value, method,
                                      JS_DISPOSABLE_USE, kind);
}

/* The insertion operations consume method, including on failure. */
int js_adopt_disposable_resource(JSContext *ctx, JSValueConst resources,
                                 JSValueConst value, JSValue method,
                                 JSDisposalKind kind)
{
    return js_disposable_resource_add(ctx, resources, value, method,
                                      JS_DISPOSABLE_ADOPT, kind);
}

int js_defer_disposable_resource(JSContext *ctx, JSValueConst resources,
                                 JSValue method, JSDisposalKind kind)
{
    return js_disposable_resource_add(ctx, resources, JS_UNDEFINED, method,
                                      JS_DISPOSABLE_DEFER, kind);
}

static JSValue js_disposable_resource_call(JSContext *ctx,
                                           const JSDisposableResource *resource)
{
    if (resource->invocation == JS_DISPOSABLE_ADOPT)
        return JS_Call(ctx, resource->method, JS_UNDEFINED, 1,
                       (JSValueConst *)&resource->value);
    return JS_Call(ctx, resource->method,
                   resource->invocation == JS_DISPOSABLE_USE ?
                   resource->value : JS_UNDEFINED, 0, NULL);
}

static void js_disposable_resource_add_error(JSContext *ctx, JSValue *error,
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

int js_dispose_resources_step(JSContext *ctx,
                              JSValueConst resources, JSValueConst input,
                              BOOL is_throw, JSValue *await_value)
{
    JSDisposableResourceList *list =
        JS_GetOpaque(resources, JS_CLASS_DISPOSABLE_RESOURCE_LIST);

    *await_value = JS_UNDEFINED;
    if (!list) {
        JS_ThrowInternalError(ctx, "invalid disposal resource list");
        return -1;
    }
    if (list->done)
        return 0;
    if (!list->started) {
        list->started = TRUE;
        list->active = list->head;
        list->head = NULL;
        if (is_throw) {
            list->error = JS_DupValue(ctx, input);
            list->has_error = TRUE;
        }
    } else if (list->await_reason != JS_DISPOSAL_AWAIT_NONE) {
        if (list->await_reason == JS_DISPOSAL_AWAIT_METHOD)
            list->has_awaited = TRUE;
        list->await_reason = JS_DISPOSAL_AWAIT_NONE;
        if (is_throw)
            js_disposable_resource_add_error(ctx, &list->error,
                                              &list->has_error,
                                              JS_DupValue(ctx, input));
    }

    while (list->active) {
        JSDisposableResource *resource = list->active;
        JSDisposalKind kind = resource->kind;
        JSValue result;

        if (kind == JS_DISPOSAL_SYNC &&
            list->needs_await && !list->has_awaited) {
            list->needs_await = FALSE;
            list->await_reason = JS_DISPOSAL_AWAIT_BEFORE_SYNC;
            return 1;
        }
        list->active = resource->next;
        if (JS_IsUndefined(resource->method)) {
            assert(kind == JS_DISPOSAL_ASYNC);
            list->needs_await = TRUE;
            js_disposable_resource_free(ctx->rt, resource);
            continue;
        }
        result = js_disposable_resource_call(ctx, resource);
        js_disposable_resource_free(ctx->rt, resource);
        if (JS_IsException(result)) {
            js_disposable_resource_add_error(ctx, &list->error,
                                              &list->has_error,
                                              JS_GetException(ctx));
        } else if (kind == JS_DISPOSAL_SYNC) {
            JS_FreeValue(ctx, result);
        } else {
            list->await_reason = JS_DISPOSAL_AWAIT_METHOD;
            *await_value = result;
            return 1;
        }
    }
    if (list->needs_await && !list->has_awaited) {
        list->needs_await = FALSE;
        list->await_reason = JS_DISPOSAL_AWAIT_END;
        return 1;
    }
    list->done = TRUE;
    if (list->has_error) {
        JSValue error = list->error;
        list->error = JS_UNDEFINED;
        list->has_error = FALSE;
        JS_Throw(ctx, error);
        return -1;
    }
    return 0;
}

static int js_init_disposable_resource_list(JSContext *ctx)
{
    static const JSClassDef list_class = {
        .class_name = "DisposableResourceList",
        .finalizer = js_disposable_resource_list_finalizer,
        .gc_mark = js_disposable_resource_list_mark,
    };

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_DISPOSABLE_RESOURCE_LIST))
        return JS_NewClass(ctx->rt, JS_CLASS_DISPOSABLE_RESOURCE_LIST,
                            &list_class);
    return 0;
}
