/*
 * QuickJS explicit resource management builtins
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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

#include "../internal/atom.h"
#include "../internal/base.h"
#include "../internal/runtime.h"
#include "../internal/object.h"
#include "../internal/function.h"
#include "../internal/c-function.h"
#include "../internal/function-list.h"
#include "resource-management.h"
#include "../internal/disposal.h"

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
        object = JS_CreateFromConstructor(ctx, new_target, class_id);
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
    if (js_adopt_disposable_resource(ctx, stack->resources, argv[0],
                                   JS_DupValue(ctx, argv[1]),
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
    if (js_defer_disposable_resource(ctx, stack->resources,
                                   JS_DupValue(ctx, argv[0]),
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

int js_init_disposable_stack(JSContext *ctx)
{
    static const JSClassDef stack_class = {
        .class_name = "DisposableStack",
        .finalizer = js_disposable_stack_finalizer,
        .gc_mark = js_disposable_stack_mark,
    };
    JSValue constructor;

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_DISPOSABLE_STACK) &&
        JS_NewClass(ctx->rt, JS_CLASS_DISPOSABLE_STACK, &stack_class) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
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
