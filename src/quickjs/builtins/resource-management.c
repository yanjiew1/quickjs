/*
 * QuickJS explicit resource management builtins
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */

#include "../internal/atom.h"
#include "../internal/base.h"
#include "../internal/runtime.h"
#include "../internal/object.h"
#include "../internal/function.h"
#include "../internal/c-function.h"
#include "../internal/function-list.h"
#include "resource-management.h"
#include "error.h"

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

void js_disposable_stack_clear(JSRuntime *rt, JSDisposableStackData *stack)
{
    JS_FreeValueRT(rt, stack->resources);
    stack->resources = JS_UNDEFINED;
}

void js_disposable_stack_mark_data(JSRuntime *rt,
                                   const JSDisposableStackData *stack,
                                   JS_MarkFunc *mark_func)
{
    JS_MarkValue(rt, stack->resources, mark_func);
}

static void js_disposable_stack_finalizer(JSRuntime *rt, JSValue value)
{
    JSDisposableStackData *stack =
        JS_GetOpaque(value, JS_CLASS_DISPOSABLE_STACK);

    if (!stack)
        return;
    js_disposable_stack_clear(rt, stack);
    js_free_rt(rt, stack);
}

static void js_disposable_stack_mark(JSRuntime *rt, JSValueConst value,
                                     JS_MarkFunc *mark_func)
{
    JSDisposableStackData *stack =
        JS_GetOpaque(value, JS_CLASS_DISPOSABLE_STACK);

    if (stack)
        js_disposable_stack_mark_data(rt, stack, mark_func);
}

static JSDisposableStackData *js_get_disposable_stack(JSContext *ctx,
                                                      JSValueConst value,
                                                      int class_id,
                                                      BOOL pending)
{
    JSDisposableStackData *stack = JS_GetOpaque2(ctx, value, class_id);

    if (stack && pending && stack->disposed) {
        JS_ThrowReferenceError(ctx, "disposable stack is already disposed");
        return NULL;
    }
    return stack;
}

JSValue js_new_disposable_stack(JSContext *ctx, JSValueConst new_target,
                                int class_id, size_t data_size)
{
    JSDisposableStackData *stack;
    JSValue object, resources;

    if (JS_IsUndefined(new_target)) {
        object = JS_NewObjectProtoClass(ctx, ctx->class_proto[class_id],
                                        class_id);
    } else {
        object = js_create_from_ctor(ctx, new_target, class_id);
    }
    if (JS_IsException(object))
        return object;
    resources = js_new_disposable_resource_list(ctx);
    if (JS_IsException(resources)) {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }
    stack = js_mallocz(ctx, data_size);
    if (!stack) {
        JS_FreeValue(ctx, resources);
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }
    stack->resources = resources;
    JS_SetOpaque(object, stack);
    return object;
}

static JSValue js_disposable_stack_constructor(JSContext *ctx,
                                               JSValueConst new_target,
                                               int argc, JSValueConst *argv)
{
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    return js_new_disposable_stack(ctx, new_target, JS_CLASS_DISPOSABLE_STACK,
                                   sizeof(JSDisposableStackData));
}

JSValue js_disposable_stack_get_disposed(JSContext *ctx, JSValueConst value,
                                         int class_id)
{
    JSDisposableStackData *stack =
        js_get_disposable_stack(ctx, value, class_id, FALSE);

    if (!stack)
        return JS_EXCEPTION;
    return JS_NewBool(ctx, stack->disposed);
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

JSValue js_disposable_stack_use(JSContext *ctx, JSValueConst value,
                                int argc, JSValueConst *argv, int class_id)
{
    JSDisposableStackData *stack =
        js_get_disposable_stack(ctx, value, class_id, TRUE);
    JSValue resources;
    int result;

    if (!stack)
        return JS_EXCEPTION;
    /* Method lookup can move the receiver or begin disposing this list. */
    resources = JS_DupValue(ctx, stack->resources);
    result = js_add_disposable_resource(ctx, resources, argv[0],
        class_id == JS_CLASS_ASYNC_DISPOSABLE_STACK ?
        JS_DISPOSAL_ASYNC : JS_DISPOSAL_SYNC);
    JS_FreeValue(ctx, resources);
    if (result < 0)
        return JS_EXCEPTION;
    return JS_DupValue(ctx, argv[0]);
}

JSValue js_disposable_stack_adopt(JSContext *ctx, JSValueConst value,
                                  int argc, JSValueConst *argv, int class_id)
{
    JSDisposableStackData *stack =
        js_get_disposable_stack(ctx, value, class_id, TRUE);

    if (!stack)
        return JS_EXCEPTION;
    if (!JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "disposal callback is not callable");
    if (js_disposable_resource_add(ctx, stack->resources, argv[0],
                                   JS_DupValue(ctx, argv[1]),
                                   JS_DISPOSABLE_ADOPT,
                                   class_id == JS_CLASS_ASYNC_DISPOSABLE_STACK ?
                                   JS_DISPOSAL_ASYNC : JS_DISPOSAL_SYNC) < 0)
        return JS_EXCEPTION;
    return JS_DupValue(ctx, argv[0]);
}

JSValue js_disposable_stack_defer(JSContext *ctx, JSValueConst value,
                                  int argc, JSValueConst *argv, int class_id)
{
    JSDisposableStackData *stack =
        js_get_disposable_stack(ctx, value, class_id, TRUE);

    if (!stack)
        return JS_EXCEPTION;
    if (!JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "disposal callback is not callable");
    if (js_disposable_resource_add(ctx, stack->resources, JS_UNDEFINED,
                                   JS_DupValue(ctx, argv[0]),
                                   JS_DISPOSABLE_DEFER,
                                   class_id == JS_CLASS_ASYNC_DISPOSABLE_STACK ?
                                   JS_DISPOSAL_ASYNC : JS_DISPOSAL_SYNC) < 0)
        return JS_EXCEPTION;
    return JS_UNDEFINED;
}

JSValue js_disposable_stack_move(JSContext *ctx, JSValueConst value,
                                 int class_id, size_t data_size)
{
    JSDisposableStackData *stack =
        js_get_disposable_stack(ctx, value, class_id, TRUE);
    JSDisposableStackData *destination;
    JSValue object;

    if (!stack)
        return JS_EXCEPTION;
    object = js_new_disposable_stack(ctx, JS_UNDEFINED, class_id, data_size);
    if (JS_IsException(object))
        return object;
    destination = JS_GetOpaque(object, class_id);
    /* Allocate both destination and replacement list before changing source. */
    {
        JSValue empty = destination->resources;
        destination->resources = stack->resources;
        stack->resources = empty;
    }
    stack->disposed = TRUE;
    return object;
}

static JSValue js_disposable_stack_move_sync(JSContext *ctx,
                                             JSValueConst value,
                                             int argc, JSValueConst *argv)
{
    return js_disposable_stack_move(ctx, value, JS_CLASS_DISPOSABLE_STACK,
                                    sizeof(JSDisposableStackData));
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

static JSValue js_disposable_stack_dispose(JSContext *ctx, JSValueConst value,
                                           int argc, JSValueConst *argv)
{
    JSDisposableStackData *stack =
        js_get_disposable_stack(ctx, value, JS_CLASS_DISPOSABLE_STACK, FALSE);
    JSValue result;
    int status;

    if (!stack)
        return JS_EXCEPTION;
    if (stack->disposed)
        return JS_UNDEFINED;
    stack->disposed = TRUE;
    status = js_dispose_resources_step(ctx, stack->resources,
                                       JS_UNDEFINED, FALSE, &result);
    assert(status != 1);
    JS_FreeValue(ctx, result);
    return status < 0 ? JS_EXCEPTION : JS_UNDEFINED;
}

static const JSCFunctionListEntry js_disposable_stack_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("use", 1, js_disposable_stack_use,
                       JS_CLASS_DISPOSABLE_STACK),
    JS_CFUNC_MAGIC_DEF("adopt", 2, js_disposable_stack_adopt,
                       JS_CLASS_DISPOSABLE_STACK),
    JS_CFUNC_MAGIC_DEF("defer", 1, js_disposable_stack_defer,
                       JS_CLASS_DISPOSABLE_STACK),
    JS_CFUNC_DEF("move", 0, js_disposable_stack_move_sync),
    JS_CFUNC_DEF("dispose", 0, js_disposable_stack_dispose),
    JS_CGETSET_MAGIC_DEF("disposed", js_disposable_stack_get_disposed, NULL,
                         JS_CLASS_DISPOSABLE_STACK),
    JS_ALIAS_DEF("[Symbol.dispose]", "dispose"),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "DisposableStack",
                       JS_PROP_CONFIGURABLE),
};

int js_init_disposable_resource_list(JSContext *ctx)
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

int js_init_disposable_stack(JSContext *ctx)
{
    static const JSClassDef stack_class = {
        .class_name = "DisposableStack",
        .finalizer = js_disposable_stack_finalizer,
        .gc_mark = js_disposable_stack_mark,
    };
    JSValue constructor;

    if (js_init_disposable_resource_list(ctx) < 0)
        return -1;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_DISPOSABLE_STACK) &&
        JS_NewClass(ctx->rt, JS_CLASS_DISPOSABLE_STACK, &stack_class) < 0)
        return -1;
    constructor = JS_NewCConstructor(ctx, JS_CLASS_DISPOSABLE_STACK,
                                     "DisposableStack",
                                     js_disposable_stack_constructor, 0,
                                     JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED, NULL, 0,
                                     js_disposable_stack_proto_funcs,
                                     countof(js_disposable_stack_proto_funcs), 0);
    if (JS_IsException(constructor))
        return -1;
    JS_FreeValue(ctx, constructor);
    return 0;
}
