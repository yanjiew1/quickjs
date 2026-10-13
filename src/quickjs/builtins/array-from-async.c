/*
 * QuickJS asynchronous Array construction
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include "array-from-async.h"
#include "array.h"
#include "async-from-sync-iterator.h"
#include "promise.h"
#include "../internal/atom.h"
#include "../internal/error.h"
#include "../internal/number.h"
#include "../internal/object.h"
#include "../internal/runtime.h"
#include "../value/conversion.h"

enum {
    ARRAY_FROM_ASYNC_ARRAY,
    ARRAY_FROM_ASYNC_SOURCE,
    ARRAY_FROM_ASYNC_NEXT,
    ARRAY_FROM_ASYNC_MAPPER,
    ARRAY_FROM_ASYNC_THIS_ARG,
    ARRAY_FROM_ASYNC_RESOLVE,
    ARRAY_FROM_ASYNC_REJECT,
    ARRAY_FROM_ASYNC_ERROR,
    ARRAY_FROM_ASYNC_VALUE_COUNT,
};

typedef enum JSArrayFromAsyncPhase {
    ARRAY_FROM_ASYNC_START,
    ARRAY_FROM_ASYNC_WAIT_NEXT,
    ARRAY_FROM_ASYNC_WAIT_VALUE,
    ARRAY_FROM_ASYNC_WAIT_MAP,
    ARRAY_FROM_ASYNC_WAIT_CLOSE,
    ARRAY_FROM_ASYNC_DONE,
} JSArrayFromAsyncPhase;

typedef struct JSArrayFromAsyncState {
    /* Reactions retain the hidden owner, whose GC edges cover these values. */
    JSValue values[ARRAY_FROM_ASYNC_VALUE_COUNT];
    JSContext *realm;
    int64_t index;
    int64_t length;
    BOOL iterable;
    JSArrayFromAsyncPhase phase;
} JSArrayFromAsyncState;

static void js_array_from_async_finalizer(JSRuntime *rt, JSValue val)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(val, JS_CLASS_ARRAY_FROM_ASYNC);
    int i;

    if (s) {
        for(i = 0; i < ARRAY_FROM_ASYNC_VALUE_COUNT; i++)
            JS_FreeValueRT(rt, s->values[i]);
        JS_FreeContext(s->realm);
        js_free_rt(rt, s);
    }
}

static void js_array_from_async_mark(JSRuntime *rt, JSValueConst val,
                                     JS_MarkFunc *mark_func)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(val, JS_CLASS_ARRAY_FROM_ASYNC);
    int i;

    if (s) {
        mark_func(rt, &s->realm->header);
        for(i = 0; i < ARRAY_FROM_ASYNC_VALUE_COUNT; i++)
            JS_MarkValue(rt, s->values[i], mark_func);
    }
}

static void js_array_from_async_finish(JSContext *ctx,
                                       JSArrayFromAsyncState *s, BOOL reject)
{
    JSValue value, func, ret;
    int i, value_index, func_index;

    value_index = reject ? ARRAY_FROM_ASYNC_ERROR : ARRAY_FROM_ASYNC_ARRAY;
    func_index = reject ? ARRAY_FROM_ASYNC_REJECT : ARRAY_FROM_ASYNC_RESOLVE;
    value = s->values[value_index];
    s->values[value_index] = JS_UNDEFINED;
    func = s->values[func_index];
    s->values[func_index] = JS_UNDEFINED;
    s->phase = ARRAY_FROM_ASYNC_DONE;
    /* Resolution can run an output object's then getter. Drop state first. */
    for(i = 0; i < ARRAY_FROM_ASYNC_VALUE_COUNT; i++) {
        JS_FreeValue(ctx, s->values[i]);
        s->values[i] = JS_UNDEFINED;
    }
    ret = JS_Call(ctx, func, JS_UNDEFINED, 1, (JSValueConst *)&value);
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, value);
    if (JS_IsException(ret))
        JS_FreeValue(ctx, JS_GetException(ctx));
    JS_FreeValue(ctx, ret);
}

static void js_array_from_async_fail(JSContext *ctx, JSValueConst owner,
                                     BOOL close_iterator);
static void js_array_from_async_step(JSContext *ctx, JSValueConst owner);
static JSValue js_array_from_async_get_method(JSContext *ctx, JSValueConst obj,
                                              JSAtom atom);

static JSValue js_array_from_async_resume(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int magic, JSValue *func_data);

/* Consume value. Use intrinsic PromiseResolve/PerformPromiseThen for Await. */
static void js_array_from_async_await(JSContext *ctx, JSValueConst owner,
                                      JSValue value,
                                      JSArrayFromAsyncPhase phase)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(owner, JS_CLASS_ARRAY_FROM_ASYNC);
    JSValue promise, handlers[2];
    JSValueConst resolving_funcs[2] = { JS_UNDEFINED, JS_UNDEFINED };
    int i, ret;

    s->phase = phase;
    promise = js_promise_resolve(ctx, ctx->promise_ctor, 1,
                                 (JSValueConst *)&value, 0);
    JS_FreeValue(ctx, value);
    if (JS_IsException(promise))
        goto fail;
    handlers[0] = JS_UNDEFINED;
    handlers[1] = JS_UNDEFINED;
    for(i = 0; i < 2; i++) {
        handlers[i] = JS_NewCFunctionData(ctx, js_array_from_async_resume,
                                         1, i, 1, &owner);
        if (JS_IsException(handlers[i])) {
            JS_FreeValue(ctx, handlers[0]);
            JS_FreeValue(ctx, promise);
            goto fail;
        }
    }
    ret = perform_promise_then(ctx, promise, (JSValueConst *)handlers,
                               resolving_funcs);
    JS_FreeValue(ctx, handlers[0]);
    JS_FreeValue(ctx, handlers[1]);
    JS_FreeValue(ctx, promise);
    if (!ret)
        return;
 fail:
    if (phase == ARRAY_FROM_ASYNC_WAIT_CLOSE) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        js_array_from_async_finish(ctx, s, TRUE);
    } else {
        js_array_from_async_fail(ctx, owner,
                                 phase == ARRAY_FROM_ASYNC_WAIT_MAP);
    }
}

static void js_array_from_async_fail(JSContext *ctx, JSValueConst owner,
                                     BOOL close_iterator)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(owner, JS_CLASS_ARRAY_FROM_ASYNC);
    JSValue method, ret;

    s->values[ARRAY_FROM_ASYNC_ERROR] = JS_GetException(ctx);
    if (!close_iterator || !s->iterable) {
        js_array_from_async_finish(ctx, s, TRUE);
        return;
    }
    s->phase = ARRAY_FROM_ASYNC_WAIT_CLOSE;
    method = js_array_from_async_get_method(ctx,
                                            s->values[ARRAY_FROM_ASYNC_SOURCE],
                                            JS_ATOM_return);
    if (JS_IsException(method))
        goto close_failed;
    if (JS_IsUndefined(method)) {
        JS_FreeValue(ctx, method);
        js_array_from_async_finish(ctx, s, TRUE);
        return;
    }
    ret = JS_Call(ctx, method, s->values[ARRAY_FROM_ASYNC_SOURCE], 0, NULL);
    JS_FreeValue(ctx, method);
    if (JS_IsException(ret))
        goto close_failed;
    js_array_from_async_await(ctx, owner, ret, ARRAY_FROM_ASYNC_WAIT_CLOSE);
    return;
 close_failed:
    /* AsyncIteratorClose preserves the original throw, including undefined. */
    JS_FreeValue(ctx, JS_GetException(ctx));
    js_array_from_async_finish(ctx, s, TRUE);
}

static void js_array_from_async_complete(JSContext *ctx, JSValueConst owner)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(owner, JS_CLASS_ARRAY_FROM_ASYNC);

    if (JS_SetProperty(ctx, s->values[ARRAY_FROM_ASYNC_ARRAY], JS_ATOM_length,
                       JS_NewInt64(ctx, s->index)) < 0) {
        js_array_from_async_fail(ctx, owner, FALSE);
    } else {
        js_array_from_async_finish(ctx, s, FALSE);
    }
}

/* Consume value, creating an own data property rather than invoking setters. */
static void js_array_from_async_define(JSContext *ctx, JSValueConst owner,
                                       JSValue value)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(owner, JS_CLASS_ARRAY_FROM_ASYNC);

    if (JS_DefinePropertyValueInt64(ctx, s->values[ARRAY_FROM_ASYNC_ARRAY],
                                    s->index, value,
                                    JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
        js_array_from_async_fail(ctx, owner, TRUE);
        return;
    }
    s->index++;
    js_array_from_async_step(ctx, owner);
}

static void js_array_from_async_value(JSContext *ctx, JSValueConst owner,
                                      JSValue value)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(owner, JS_CLASS_ARRAY_FROM_ASYNC);
    JSValue mapped;
    JSValueConst args[2];

    if (JS_IsUndefined(s->values[ARRAY_FROM_ASYNC_MAPPER])) {
        js_array_from_async_define(ctx, owner, value);
        return;
    }
    args[0] = value;
    args[1] = JS_NewInt64(ctx, s->index);
    mapped = JS_Call(ctx, s->values[ARRAY_FROM_ASYNC_MAPPER],
                     s->values[ARRAY_FROM_ASYNC_THIS_ARG], 2, args);
    JS_FreeValue(ctx, value);
    if (JS_IsException(mapped)) {
        js_array_from_async_fail(ctx, owner, TRUE);
        return;
    }
    js_array_from_async_await(ctx, owner, mapped, ARRAY_FROM_ASYNC_WAIT_MAP);
}

static void js_array_from_async_step(JSContext *ctx, JSValueConst owner)
{
    JSArrayFromAsyncState *s = JS_GetOpaque(owner, JS_CLASS_ARRAY_FROM_ASYNC);
    JSValue value;

    if (s->iterable) {
        if (s->index >= MAX_SAFE_INTEGER) {
            JS_ThrowTypeError(ctx, "too many Array.fromAsync elements");
            js_array_from_async_fail(ctx, owner, TRUE);
            return;
        }
        value = JS_Call(ctx, s->values[ARRAY_FROM_ASYNC_NEXT],
                        s->values[ARRAY_FROM_ASYNC_SOURCE], 0, NULL);
        if (JS_IsException(value)) {
            js_array_from_async_fail(ctx, owner, FALSE);
            return;
        }
        js_array_from_async_await(ctx, owner, value,
                                  ARRAY_FROM_ASYNC_WAIT_NEXT);
    } else {
        if (s->index >= s->length) {
            js_array_from_async_complete(ctx, owner);
            return;
        }
        value = JS_GetPropertyInt64(ctx, s->values[ARRAY_FROM_ASYNC_SOURCE],
                                    s->index);
        if (JS_IsException(value)) {
            js_array_from_async_fail(ctx, owner, FALSE);
            return;
        }
        js_array_from_async_await(ctx, owner, value,
                                  ARRAY_FROM_ASYNC_WAIT_VALUE);
    }
}

static JSValue js_array_from_async_resume(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int magic, JSValue *func_data)
{
    JSValueConst owner = func_data[0];
    JSArrayFromAsyncState *s = JS_GetOpaque(owner, JS_CLASS_ARRAY_FROM_ASYNC);
    JSValue value;
    BOOL done;

    /* Await resumes the captured factory context, regardless of job context. */
    ctx = s->realm;
    if (s->phase == ARRAY_FROM_ASYNC_DONE)
        return JS_UNDEFINED;
    if (s->phase == ARRAY_FROM_ASYNC_WAIT_CLOSE) {
        js_array_from_async_finish(ctx, s, TRUE);
        return JS_UNDEFINED;
    }
    if (magic) {
        JS_Throw(ctx, JS_DupValue(ctx, argv[0]));
        js_array_from_async_fail(ctx, owner,
                                 s->phase == ARRAY_FROM_ASYNC_WAIT_MAP);
        return JS_UNDEFINED;
    }
    switch(s->phase) {
    case ARRAY_FROM_ASYNC_WAIT_NEXT:
        if (!JS_IsObject(argv[0])) {
            JS_ThrowTypeErrorNotAnObject(ctx);
            goto fail;
        }
        value = JS_GetProperty(ctx, argv[0], JS_ATOM_done);
        if (JS_IsException(value))
            goto fail;
        done = JS_ToBoolFree(ctx, value);
        if (done) {
            js_array_from_async_complete(ctx, owner);
            break;
        }
        value = JS_GetProperty(ctx, argv[0], JS_ATOM_value);
        if (JS_IsException(value))
            goto fail;
        js_array_from_async_value(ctx, owner, value);
        break;
    case ARRAY_FROM_ASYNC_WAIT_VALUE:
        js_array_from_async_value(ctx, owner, JS_DupValue(ctx, argv[0]));
        break;
    case ARRAY_FROM_ASYNC_WAIT_MAP:
        js_array_from_async_define(ctx, owner, JS_DupValue(ctx, argv[0]));
        break;
    default:
        break;
    }
    return JS_UNDEFINED;
 fail:
    js_array_from_async_fail(ctx, owner, FALSE);
    return JS_UNDEFINED;
}

static JSValue js_array_from_async_get_method(JSContext *ctx, JSValueConst obj,
                                              JSAtom atom)
{
    JSValue method = JS_GetProperty(ctx, obj, atom);

    if (JS_IsException(method) || JS_IsUndefined(method))
        return method;
    if (JS_IsNull(method)) {
        JS_FreeValue(ctx, method);
        return JS_UNDEFINED;
    }
    if (!JS_IsFunction(ctx, method)) {
        JS_FreeValue(ctx, method);
        return JS_ThrowTypeError(ctx, "iterator method is not callable");
    }
    return method;
}

static JSValue js_array_from_async(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSArrayFromAsyncState *s;
    JSValue promise, resolving_funcs[2], owner, method, iterator, value, ret;
    JSValueConst args[1];
    BOOL sync = FALSE;
    int i;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return promise;
    owner = JS_NewObjectProtoClass(ctx, JS_NULL, JS_CLASS_ARRAY_FROM_ASYNC);
    if (JS_IsException(owner))
        goto reject_without_state;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) {
        JS_FreeValue(ctx, owner);
        goto reject_without_state;
    }
    s->realm = JS_DupContext(ctx);
    for(i = 0; i < ARRAY_FROM_ASYNC_VALUE_COUNT; i++)
        s->values[i] = JS_UNDEFINED;
    s->values[ARRAY_FROM_ASYNC_RESOLVE] = resolving_funcs[0];
    s->values[ARRAY_FROM_ASYNC_REJECT] = resolving_funcs[1];
    JS_SetOpaque(owner, s);
    if (argc > 1 && !JS_IsUndefined(argv[1])) {
        if (!JS_IsFunction(ctx, argv[1])) {
            JS_ThrowTypeError(ctx, "mapper is not callable");
            goto fail;
        }
        s->values[ARRAY_FROM_ASYNC_MAPPER] = JS_DupValue(ctx, argv[1]);
        if (argc > 2)
            s->values[ARRAY_FROM_ASYNC_THIS_ARG] = JS_DupValue(ctx, argv[2]);
    }
    method = js_array_from_async_get_method(ctx, argv[0],
                                            JS_ATOM_Symbol_asyncIterator);
    if (JS_IsException(method))
        goto fail;
    if (JS_IsUndefined(method)) {
        method = js_array_from_async_get_method(ctx, argv[0],
                                                JS_ATOM_Symbol_iterator);
        if (JS_IsException(method))
            goto fail;
        sync = TRUE;
    }
    if (!JS_IsUndefined(method)) {
        iterator = JS_GetIterator2(ctx, argv[0], method);
        JS_FreeValue(ctx, method);
        if (JS_IsException(iterator))
            goto fail;
        if (sync) {
            value = JS_CreateAsyncFromSyncIterator(ctx, iterator);
            JS_FreeValue(ctx, iterator);
            iterator = value;
            if (JS_IsException(iterator))
                goto fail;
        }
        s->iterable = TRUE;
        s->values[ARRAY_FROM_ASYNC_SOURCE] = iterator;
        value = JS_GetProperty(ctx, iterator, JS_ATOM_next);
        if (JS_IsException(value))
            goto fail;
        s->values[ARRAY_FROM_ASYNC_NEXT] = value;
        if (JS_IsConstructor(ctx, this_val))
            value = JS_CallConstructor(ctx, this_val, 0, NULL);
        else
            value = JS_NewArray(ctx);
    } else {
        value = JS_ToObject(ctx, argv[0]);
        if (JS_IsException(value))
            goto fail;
        s->values[ARRAY_FROM_ASYNC_SOURCE] = value;
        if (js_get_length64(ctx, &s->length, value))
            goto fail;
        value = JS_NewInt64(ctx, s->length);
        args[0] = value;
        if (JS_IsConstructor(ctx, this_val))
            ret = JS_CallConstructor(ctx, this_val, 1, args);
        else
            ret = js_array_constructor(ctx, JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, value);
        value = ret;
    }
    if (JS_IsException(value))
        goto fail;
    s->values[ARRAY_FROM_ASYNC_ARRAY] = value;
    js_array_from_async_step(ctx, owner);
    JS_FreeValue(ctx, owner);
    return promise;
 fail:
    js_array_from_async_fail(ctx, owner, FALSE);
    JS_FreeValue(ctx, owner);
    return promise;
 reject_without_state:
    value = JS_GetException(ctx);
    ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                   (JSValueConst *)&value);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    if (JS_IsException(ret))
        JS_FreeValue(ctx, JS_GetException(ctx));
    JS_FreeValue(ctx, ret);
    return promise;
}

int js_init_array_from_async(JSContext *ctx)
{
    static const JSClassDef class_def = {
        .class_name = "ArrayFromAsyncState",
        .finalizer = js_array_from_async_finalizer,
        .gc_mark = js_array_from_async_mark,
    };
    static const JSCFunctionListEntry funcs[] = {
        JS_CFUNC_DEF("fromAsync", 1, js_array_from_async),
    };

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_ARRAY_FROM_ASYNC) &&
        JS_NewClass(ctx->rt, JS_CLASS_ARRAY_FROM_ASYNC, &class_def))
        return -1;
    return JS_SetPropertyFunctionList(ctx, ctx->array_ctor,
                                      funcs, countof(funcs));
}
