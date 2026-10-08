/*
 * QuickJS native Intl bound function objects
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
#include "intl-internal.h"
#include "bound-function.h"
#include "../../internal/error.h"

#ifdef CONFIG_INTL
static void js_intl_bound_function_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlBoundFunctionData *s =
        JS_GetOpaque(value, JS_CLASS_INTL_BOUND_FUNCTION);
    int i;

    if (s) {
        for (i = 0; i < s->data_len; i++)
            JS_FreeValueRT(rt, s->data[i]);
        JS_FreeContext(s->realm);
        js_free_rt(rt, s);
    }
}

static void js_intl_bound_function_mark(JSRuntime *rt, JSValueConst value,
                                        JS_MarkFunc *mark_func)
{
    JSIntlBoundFunctionData *s =
        JS_GetOpaque(value, JS_CLASS_INTL_BOUND_FUNCTION);
    int i;

    if (s) {
        mark_func(rt, &s->realm->header);
        for (i = 0; i < s->data_len; i++)
            JS_MarkValue(rt, s->data[i], mark_func);
    }
}

static JSValue js_intl_bound_function_call(JSContext *ctx,
                                           JSValueConst func_obj,
                                           JSValueConst this_val,
                                           int argc, JSValueConst *argv,
                                           int flags)
{
    JSIntlBoundFunctionData *s =
        JS_GetOpaque(func_obj, JS_CLASS_INTL_BOUND_FUNCTION);
    JSStackFrame sf = { 0 };
    JSValueConst *arg_buf = argv;
    JSRuntime *rt;
    JSValue result;
    int i;

    (void)flags;
    if (!s)
        return JS_ThrowTypeError(ctx, "uninitialized Intl bound function");
    ctx = s->realm;
    rt = ctx->rt;
    if (js_check_stack_overflow(rt, sizeof(arg_buf[0]) * s->length))
        return JS_ThrowStackOverflow(ctx);
    if (unlikely(argc < s->length)) {
        arg_buf = alloca(sizeof(arg_buf[0]) * s->length);
        for (i = 0; i < argc; i++)
            arg_buf[i] = argv[i];
        for (i = argc; i < s->length; i++)
            arg_buf[i] = JS_UNDEFINED;
    }

    sf.prev_frame = rt->current_stack_frame;
    sf.cur_func = (JSValue)func_obj;
    sf.arg_count = max_int(argc, s->length);
    sf.arg_buf = (JSValue *)arg_buf;
    rt->current_stack_frame = &sf;
    result = s->func(ctx, this_val, argc, arg_buf, s->magic, s->data);
    rt->current_stack_frame = sf.prev_frame;
    return result;
}

JSValue js_intl_new_c_function_data(JSContext *ctx, JSCFunctionData *func,
                                    int length, int magic, int data_len,
                                    JSValueConst *data)
{
    static const JSClassDef class_def = {
        .class_name = "Function",
        .finalizer = js_intl_bound_function_finalizer,
        .gc_mark = js_intl_bound_function_mark,
        .call = js_intl_bound_function_call,
    };
    JSIntlBoundFunctionData *s;
    JSValue func_obj, name;
    int i;

    /* Private Intl callers use length <= 2 and one captured instance. */
    assert(length >= 0 && length <= UINT8_MAX);
    assert(data_len >= 0 && data_len <= UINT8_MAX);
    if (js_intl_register_class(ctx, JS_CLASS_INTL_BOUND_FUNCTION,
                                &class_def) < 0)
        return JS_EXCEPTION;
    func_obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                      JS_CLASS_INTL_BOUND_FUNCTION);
    if (JS_IsException(func_obj))
        return func_obj;
    s = js_malloc(ctx, sizeof(*s) + data_len * sizeof(*s->data));
    if (!s) {
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    s->realm = JS_DupContext(ctx);
    s->func = func;
    s->length = length;
    s->data_len = data_len;
    s->magic = magic;
    for (i = 0; i < data_len; i++)
        s->data[i] = JS_DupValue(ctx, data[i]);
    JS_SetOpaque(func_obj, s);

    /* CreateBuiltinFunction defines length before name. Both are checked. */
    if (JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_length,
                               JS_NewInt32(ctx, length),
                               JS_PROP_CONFIGURABLE) < 0)
        goto fail;
    name = JS_AtomToString(ctx, JS_ATOM_empty_string);
    if (JS_IsException(name))
        goto fail;
    if (JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name, name,
                               JS_PROP_CONFIGURABLE) < 0)
        goto fail;
    return func_obj;

 fail:
    JS_FreeValue(ctx, func_obj);
    return JS_EXCEPTION;
}
#endif
