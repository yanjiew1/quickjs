/*
 * QuickJS native C function objects
 *
 * Copyright (c) 2017-2026 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 * Copyright (c) 2023-2026 Ben Noordhuis
 * Copyright (c) 2023-2026 Saúl Ibarra Corretgé
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
#include "internal/base.h"
#include "internal/runtime.h"
#include "internal/atom.h"
#include "internal/object.h"
#include "internal/function.h"
#include "internal/c-function.h"
#include "internal/error.h"
#include "internal/iterator.h"

static BOOL js_c_function_name_valid(const char *name)
{
    const char *p;
    BOOL symbol = FALSE;

    if (*name == '\0')
        return TRUE;
    if (!strncmp(name, "get ", 4) || !strncmp(name, "set ", 4))
        name += 4;
    if (!strncmp(name, "[Symbol.", 8)) {
        name += 8;
        symbol = TRUE;
    }
    p = name;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
          *p == '_' || *p == '$'))
        return FALSE;
    p++;
    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
           (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')
        p++;
    return symbol ? p[0] == ']' && p[1] == '\0' : *p == '\0';
}

/* Note: at least 'length' arguments will be readable in 'argv' */
JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                         const char *name,
                         int length, JSCFunctionEnum cproto, int magic,
                         JSValueConst proto_val, int n_fields)
{
    JSValue func_obj;
    JSObject *p;
    JSAtom name_atom;

    if (n_fields > 0) {
        func_obj = JS_NewObjectProtoClassAlloc(ctx, proto_val, JS_CLASS_C_FUNCTION, n_fields);
    } else {
        func_obj = JS_NewObjectProtoClass(ctx, proto_val, JS_CLASS_C_FUNCTION);
    }
    if (JS_IsException(func_obj))
        return func_obj;
    p = JS_VALUE_GET_OBJ(func_obj);
    p->u.cfunc.realm = JS_DupContext(ctx);
    p->u.cfunc.c_function.generic = func;
    p->u.cfunc.length = length;
    p->u.cfunc.cproto = cproto;
    p->u.cfunc.magic = magic;
    p->is_constructor = (cproto == JS_CFUNC_constructor ||
                         cproto == JS_CFUNC_constructor_magic ||
                         cproto == JS_CFUNC_constructor_or_func ||
                         cproto == JS_CFUNC_constructor_or_func_magic ||
                         cproto == JS_CFUNC_constructor_or_func_flags);
    if (!name)
        name = "";
    name_atom = JS_NewAtom(ctx, name);
    if (name_atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    if (js_c_function_name_valid(name))
        *js_c_function_initial_name(p) = JS_DupAtom(ctx, name_atom);
    if (js_function_set_properties(ctx, func_obj, name_atom, length) < 0) {
        JS_FreeAtom(ctx, name_atom);
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    JS_FreeAtom(ctx, name_atom);
    return func_obj;
}

/* Note: at least 'length' arguments will be readable in 'argv' */
JSValue JS_NewCFunction2(JSContext *ctx, JSCFunction *func,
                         const char *name,
                         int length, JSCFunctionEnum cproto, int magic)
{
    return JS_NewCFunction3(ctx, func, name, length, cproto, magic,
                            ctx->function_proto, 0);
}

JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                           JSValueConst this_obj,
                           int argc, JSValueConst *argv, int flags)
{
    JSRuntime *rt = ctx->rt;
    JSContext *previous_realm;
    JSCFunctionType func;
    JSObject *p;
    JSStackFrame sf_s, *sf = &sf_s, *prev_sf;
    JSValue ret_val;
    JSValueConst *arg_buf;
    int arg_count, i;
    JSCFunctionEnum cproto;

    p = JS_VALUE_GET_OBJ(func_obj);
    cproto = p->u.cfunc.cproto;
    arg_count = p->u.cfunc.length;

    /* better to always check stack overflow */
    if (js_check_stack_overflow(rt, sizeof(arg_buf[0]) * arg_count))
        return JS_ThrowStackOverflow(ctx);

    prev_sf = rt->current_stack_frame;
    sf->prev_frame = prev_sf;
    ctx = p->u.cfunc.realm; /* change the current realm */
    previous_realm = rt->current_realm;
    sf->js_mode = 0;
    sf->cur_func = (JSValue)func_obj;
    sf->arg_count = argc;
    arg_buf = argv;

    if (unlikely(argc < arg_count)) {
        /* ensure that at least argc_count arguments are readable */
        arg_buf = alloca(sizeof(arg_buf[0]) * arg_count);
        for(i = 0; i < argc; i++)
            arg_buf[i] = argv[i];
        for(i = argc; i < arg_count; i++)
            arg_buf[i] = JS_UNDEFINED;
        sf->arg_count = arg_count;
    }
    sf->arg_buf = (JSValue*)arg_buf;

    rt->current_stack_frame = sf;
    rt->current_realm = ctx;
    func = p->u.cfunc.c_function;
    switch(cproto) {
    case JS_CFUNC_constructor:
    case JS_CFUNC_constructor_or_func:
        if (!(flags & JS_CALL_FLAG_CONSTRUCTOR)) {
            if (cproto == JS_CFUNC_constructor) {
            not_a_constructor:
                ret_val = JS_ThrowTypeError(ctx, "must be called with new");
                break;
            } else {
                this_obj = JS_UNDEFINED;
            }
        }
        /* here this_obj is new_target */
        /* fall thru */
    case JS_CFUNC_generic:
        ret_val = func.generic(ctx, this_obj, argc, arg_buf);
        break;
    case JS_CFUNC_constructor_or_func_flags:
        ret_val = func.constructor_or_func_flags(ctx, func_obj, this_obj,
                                                 argc, arg_buf, flags);
        break;
    case JS_CFUNC_constructor_magic:
    case JS_CFUNC_constructor_or_func_magic:
        if (!(flags & JS_CALL_FLAG_CONSTRUCTOR)) {
            if (cproto == JS_CFUNC_constructor_magic) {
                goto not_a_constructor;
            } else {
                this_obj = JS_UNDEFINED;
            }
        }
        /* fall thru */
    case JS_CFUNC_generic_magic:
        ret_val = func.generic_magic(ctx, this_obj, argc, arg_buf,
                                     p->u.cfunc.magic);
        break;
    case JS_CFUNC_getter:
        ret_val = func.getter(ctx, this_obj);
        break;
    case JS_CFUNC_setter:
        ret_val = func.setter(ctx, this_obj, arg_buf[0]);
        break;
    case JS_CFUNC_getter_magic:
        ret_val = func.getter_magic(ctx, this_obj, p->u.cfunc.magic);
        break;
    case JS_CFUNC_setter_magic:
        ret_val = func.setter_magic(ctx, this_obj, arg_buf[0], p->u.cfunc.magic);
        break;
    case JS_CFUNC_f_f:
        {
            double d1;

            if (unlikely(JS_ToFloat64(ctx, &d1, arg_buf[0]))) {
                ret_val = JS_EXCEPTION;
                break;
            }
            ret_val = JS_NewFloat64(ctx, func.f_f(d1));
        }
        break;
    case JS_CFUNC_f_f_f:
        {
            double d1, d2;

            if (unlikely(JS_ToFloat64(ctx, &d1, arg_buf[0]))) {
                ret_val = JS_EXCEPTION;
                break;
            }
            if (unlikely(JS_ToFloat64(ctx, &d2, arg_buf[1]))) {
                ret_val = JS_EXCEPTION;
                break;
            }
            ret_val = JS_NewFloat64(ctx, func.f_f_f(d1, d2));
        }
        break;
    case JS_CFUNC_iterator_next:
        {
            int done;
            ret_val = func.iterator_next(ctx, this_obj, argc, arg_buf,
                                         &done, p->u.cfunc.magic);
            if (!JS_IsException(ret_val) && done != 2) {
                ret_val = js_create_iterator_result(ctx, ret_val, done);
            }
        }
        break;
    default:
        abort();
    }

    rt->current_stack_frame = sf->prev_frame;
    rt->current_realm = previous_realm;
    return ret_val;
}

void js_c_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);

    JS_FreeAtomRT(rt, *js_c_function_initial_name(p));
    if (p->u.cfunc.realm)
        JS_FreeContext(p->u.cfunc.realm);
}

void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                        JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);

    if (p->u.cfunc.realm)
        mark_func(rt, &p->u.cfunc.realm->header);
}

static JSValue js_new_c_function_data(JSContext *ctx, JSCFunctionData *func,
                                      const char *name, int length, int magic,
                                      int data_len, JSValueConst *data)
{
    JSCFunctionDataRecord *s;
    JSValue func_obj;
    JSAtom name_atom = JS_ATOM_empty_string;
    int i, ret;

    if (length < 0 || length > UINT8_MAX)
        return JS_ThrowRangeError(ctx, "invalid native function length");
    if (data_len < 0 ||
        (size_t)data_len > (SIZE_MAX - sizeof(*s)) / sizeof(JSValue))
        return JS_ThrowRangeError(ctx, "invalid native capture count");
    if (!func || (data_len && !data))
        return JS_ThrowTypeError(ctx, "invalid native callback or captures");
    func_obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                      JS_CLASS_C_FUNCTION_DATA);
    if (JS_IsException(func_obj))
        return func_obj;
    s = js_malloc(ctx, sizeof(*s) + data_len * sizeof(JSValue));
    if (!s) {
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    s->func = func;
    s->realm = JS_DupContext(ctx);
    s->length = length;
    s->data_len = data_len;
    s->magic = magic;
    for(i = 0; i < data_len; i++)
        s->data[i] = JS_DupValue(ctx, data[i]);
    JS_SetOpaque(func_obj, s);
    if (name && *name) {
        name_atom = JS_NewAtom(ctx, name);
        if (name_atom == JS_ATOM_NULL) {
            JS_FreeValue(ctx, func_obj);
            return JS_EXCEPTION;
        }
    }
    ret = js_function_set_properties(ctx, func_obj, name_atom, length);
    JS_FreeAtom(ctx, name_atom);
    if (ret < 0) {
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    return func_obj;
}

JSValue JS_NewCFunctionData(JSContext *ctx, JSCFunctionData *func,
                           int length, int magic, int data_len,
                           JSValueConst *data)
{
    return js_new_c_function_data(ctx, func, NULL, length, magic, data_len, data);
}

JSValue JS_NewCFunctionData2(JSContext *ctx, JSCFunctionData *func,
                             const char *name, int length, int magic,
                             int data_len, JSValueConst *data)
{
    return js_new_c_function_data(ctx, func, name, length, magic, data_len, data);
}

JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                JSValueConst this_val,
                                int argc, JSValueConst *argv, int flags)
{
    JSRuntime *rt = ctx->rt;
    JSContext *previous_realm;
    JSCFunctionDataRecord *s = JS_GetOpaque(func_obj, JS_CLASS_C_FUNCTION_DATA);
    JSValueConst *arg_buf;
    JSValue result;
    int i;

    ctx = s->realm;
    if (js_check_stack_overflow(rt, sizeof(argv[0]) * s->length))
        return JS_ThrowStackOverflow(ctx);

    if (unlikely(argc < s->length)) {
        arg_buf = alloca(sizeof(arg_buf[0]) * s->length);
        for(i = 0; i < argc; i++)
            arg_buf[i] = argv[i];
        for(i = argc; i < s->length; i++)
            arg_buf[i] = JS_UNDEFINED;
    } else {
        arg_buf = argv;
    }

    previous_realm = rt->current_realm;
    rt->current_realm = ctx;
    result = s->func(ctx, this_val, argc, arg_buf, s->magic, s->data);
    rt->current_realm = previous_realm;
    return result;
}

void js_c_function_data_finalizer(JSRuntime *rt, JSValue val)
{
    JSCFunctionDataRecord *s = JS_GetOpaque(val, JS_CLASS_C_FUNCTION_DATA);
    int i;

    if (s) {
        for(i = 0; i < s->data_len; i++) {
            JS_FreeValueRT(rt, s->data[i]);
        }
        JS_FreeContext(s->realm);
        js_free_rt(rt, s);
    }
}

void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                             JS_MarkFunc *mark_func)
{
    JSCFunctionDataRecord *s = JS_GetOpaque(val, JS_CLASS_C_FUNCTION_DATA);
    int i;

    if (s) {
        for(i = 0; i < s->data_len; i++) {
            JS_MarkValue(rt, s->data[i], mark_func);
        }
        mark_func(rt, &s->realm->header);
    }
}

BOOL JS_IsCFunction(JSValueConst val, JSCFunction *func, int magic)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    if (p->class_id == JS_CLASS_C_FUNCTION)
        return (p->u.cfunc.c_function.generic == func && p->u.cfunc.magic == magic);
    else
        return FALSE;
}

void js_c_closure_finalizer(JSRuntime *rt, JSValue val)
{
    JSCClosureRecord *s = JS_GetOpaque(val, JS_CLASS_C_CLOSURE);

    if (s) {
        if (s->opaque_finalize)
            s->opaque_finalize(s->opaque);
        JS_FreeContext(s->realm);
        js_free_rt(rt, s);
    }
}

void js_c_closure_mark(JSRuntime *rt, JSValueConst val,
                      JS_MarkFunc *mark_func)
{
    JSCClosureRecord *s = JS_GetOpaque(val, JS_CLASS_C_CLOSURE);

    if (s)
        mark_func(rt, &s->realm->header);
}

JSValue js_call_c_closure(JSContext *ctx, JSValueConst func_obj,
                         JSValueConst this_val, int argc,
                         JSValueConst *argv, int flags)
{
    JSRuntime *rt = ctx->rt;
    JSContext *previous_realm;
    JSCClosureRecord *s = JS_GetOpaque(func_obj, JS_CLASS_C_CLOSURE);
    JSStackFrame sf;
    JSValueConst *arg_buf = argv;
    JSValue result;
    int i;

    ctx = s->realm;
    if (js_check_stack_overflow(rt, sizeof(argv[0]) * s->length))
        return JS_ThrowStackOverflow(ctx);
    if (unlikely(argc < s->length)) {
        arg_buf = alloca(sizeof(argv[0]) * s->length);
        for (i = 0; i < argc; i++)
            arg_buf[i] = argv[i];
        for (i = argc; i < s->length; i++)
            arg_buf[i] = JS_UNDEFINED;
    }
    sf.prev_frame = rt->current_stack_frame;
    previous_realm = rt->current_realm;
    sf.cur_func = (JSValue)func_obj;
    sf.arg_count = max_int(argc, s->length);
    sf.arg_buf = (JSValue *)arg_buf;
    sf.js_mode = 0;
    rt->current_stack_frame = &sf;
    rt->current_realm = ctx;
    result = s->func(ctx, this_val, argc, arg_buf, s->magic, s->opaque);
    rt->current_stack_frame = sf.prev_frame;
    rt->current_realm = previous_realm;
    return result;
}

JSValue JS_NewCClosure(JSContext *ctx, JSCClosure *func, const char *name,
                      JSCClosureFinalizerFunc *opaque_finalize,
                      int length, int magic, void *opaque)
{
    JSCClosureRecord *s;
    JSValue function;
    JSAtom name_atom = JS_ATOM_empty_string;
    int ret;

    if (length < 0 || length > UINT16_MAX)
        return JS_ThrowRangeError(ctx, "invalid native closure length");
    if (!func)
        return JS_ThrowTypeError(ctx, "invalid native closure callback");
    function = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                     JS_CLASS_C_CLOSURE);
    if (JS_IsException(function))
        return function;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) {
        JS_FreeValue(ctx, function);
        return JS_EXCEPTION;
    }
    s->func = func;
    s->realm = JS_DupContext(ctx);
    s->length = length;
    s->magic = magic;
    JS_SetOpaque(function, s);
    if (name && *name) {
        name_atom = JS_NewAtom(ctx, name);
        if (name_atom == JS_ATOM_NULL) {
            JS_FreeValue(ctx, function);
            return JS_EXCEPTION;
        }
    }
    ret = js_function_set_properties(ctx, function, name_atom, length);
    JS_FreeAtom(ctx, name_atom);
    if (ret < 0) {
        JS_FreeValue(ctx, function);
        return JS_EXCEPTION;
    }
    /* No fallible initialization remains after ownership transfers. */
    s->opaque = opaque;
    s->opaque_finalize = opaque_finalize;
    return function;
}
