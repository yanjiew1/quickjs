/*
 * QuickJS function objects
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#include "internal/gc.h"
#include "internal/atom.h"
#include "internal/string.h"
#include "internal/object.h"
#include "internal/function.h"
#include "builtins/proxy.h"
#include "internal/generator.h"

void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                JSAtom name, int len)
{
    /* ES6 feature non compatible with ES5.1: length is configurable */
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_length, JS_NewInt32(ctx, len),
                           JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name,
                           JS_AtomToString(ctx, name), JS_PROP_CONFIGURABLE);
}

BOOL js_class_has_bytecode(JSClassID class_id)
{
    return (class_id == JS_CLASS_BYTECODE_FUNCTION ||
            class_id == JS_CLASS_GENERATOR_FUNCTION ||
            class_id == JS_CLASS_ASYNC_FUNCTION ||
            class_id == JS_CLASS_ASYNC_GENERATOR_FUNCTION);
}

const uint16_t func_kind_to_class_id[] = {
    [JS_FUNC_NORMAL] = JS_CLASS_BYTECODE_FUNCTION,
    [JS_FUNC_GENERATOR] = JS_CLASS_GENERATOR_FUNCTION,
    [JS_FUNC_ASYNC] = JS_CLASS_ASYNC_FUNCTION,
    [JS_FUNC_ASYNC_GENERATOR] = JS_CLASS_ASYNC_GENERATOR_FUNCTION,
};

/* return NULL without exception if not a function or no bytecode */
JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(val);
    if (!js_class_has_bytecode(p->class_id))
        return NULL;
    return p->u.func.function_bytecode;
}

void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
                               JSValueConst home_obj)
{
    JSObject *p, *p1;
    JSFunctionBytecode *b;

    if (JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)
        return;
    p = JS_VALUE_GET_OBJ(func_obj);
    if (!js_class_has_bytecode(p->class_id))
        return;
    b = p->u.func.function_bytecode;
    if (b->need_home_object) {
        p1 = p->u.func.home_object;
        if (p1) {
            JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
        }
        if (JS_VALUE_GET_TAG(home_obj) == JS_TAG_OBJECT)
            p1 = JS_VALUE_GET_OBJ(JS_DupValue(ctx, home_obj));
        else
            p1 = NULL;
        p->u.func.home_object = p1;
    }
}

JSValue js_get_function_name(JSContext *ctx, JSAtom name)
{
    JSValue name_str;

    name_str = JS_AtomToString(ctx, name);
    if (JS_AtomSymbolHasDescription(ctx, name)) {
        name_str = JS_ConcatString3(ctx, "[", name_str, "]");
    }
    return name_str;
}

/* Modify the name of a method according to the atom and
   'flags'. 'flags' is a bitmask of JS_PROP_HAS_GET and
   JS_PROP_HAS_SET. Also set the home object of the method.
   Return < 0 if exception. */
int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                             JSAtom name, int flags, JSValueConst home_obj)
{
    JSValue name_str;

    name_str = js_get_function_name(ctx, name);
    if (flags & JS_PROP_HAS_GET) {
        name_str = JS_ConcatString3(ctx, "get ", name_str, "");
    } else if (flags & JS_PROP_HAS_SET) {
        name_str = JS_ConcatString3(ctx, "set ", name_str, "");
    }
    if (JS_IsException(name_str))
        return -1;
    if (JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name, name_str,
                               JS_PROP_CONFIGURABLE) < 0)
        return -1;
    js_method_set_home_object(ctx, func_obj, home_obj);
    return 0;
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
                         cproto == JS_CFUNC_constructor_or_func_magic);
    if (!name)
        name = "";
    name_atom = JS_NewAtom(ctx, name);
    if (name_atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, func_obj);
        return JS_EXCEPTION;
    }
    js_function_set_properties(ctx, func_obj, name_atom, length);
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



void js_c_function_data_finalizer(JSRuntime *rt, JSValue val)
{
    JSCFunctionDataRecord *s = JS_GetOpaque(val, JS_CLASS_C_FUNCTION_DATA);
    int i;

    if (s) {
        for(i = 0; i < s->data_len; i++) {
            JS_FreeValueRT(rt, s->data[i]);
        }
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
    }
}

JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                JSValueConst this_val,
                                int argc, JSValueConst *argv, int flags)
{
    JSCFunctionDataRecord *s = JS_GetOpaque(func_obj, JS_CLASS_C_FUNCTION_DATA);
    JSValueConst *arg_buf;
    int i;

    /* XXX: could add the function on the stack for debug */
    if (unlikely(argc < s->length)) {
        arg_buf = alloca(sizeof(arg_buf[0]) * s->length);
        for(i = 0; i < argc; i++)
            arg_buf[i] = argv[i];
        for(i = argc; i < s->length; i++)
            arg_buf[i] = JS_UNDEFINED;
    } else {
        arg_buf = argv;
    }

    return s->func(ctx, this_val, argc, arg_buf, s->magic, s->data);
}

JSValue JS_NewCFunctionData(JSContext *ctx, JSCFunctionData *func,
                            int length, int magic, int data_len,
                            JSValueConst *data)
{
    JSCFunctionDataRecord *s;
    JSValue func_obj;
    int i;

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
    s->length = length;
    s->data_len = data_len;
    s->magic = magic;
    for(i = 0; i < data_len; i++)
        s->data[i] = JS_DupValue(ctx, data[i]);
    JS_SetOpaque(func_obj, s);
    js_function_set_properties(ctx, func_obj,
                               JS_ATOM_empty_string, length);
    return func_obj;
}

void free_var_ref(JSRuntime *rt, JSVarRef *var_ref)
{
    if (var_ref) {
        assert(js_rc(var_ref)->ref_count > 0);
        if (--js_rc(var_ref)->ref_count == 0) {
            if (var_ref->is_detached) {
                JS_FreeValueRT(rt, var_ref->value);
            } else {
                JSStackFrame *sf = var_ref->stack_frame;
                assert(sf->var_refs[var_ref->var_ref_idx] == var_ref);
                sf->var_refs[var_ref->var_ref_idx] = NULL;
                if (sf->js_mode & JS_MODE_ASYNC) {
                    JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
                    async_func_free(rt, async_func);
                }
            }
            remove_gc_object(&var_ref->header);
            js_free_rt(rt, var_ref);
        }
    }
}

void js_c_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);

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

void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p1, *p = JS_VALUE_GET_OBJ(val);
    JSFunctionBytecode *b;
    JSVarRef **var_refs;
    int i;

    p1 = p->u.func.home_object;
    if (p1) {
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, p1));
    }
    b = p->u.func.function_bytecode;
    if (b) {
        var_refs = p->u.func.var_refs;
        if (var_refs) {
            for(i = 0; i < b->closure_var_count; i++)
                free_var_ref(rt, var_refs[i]);
            js_free_rt(rt, var_refs);
        }
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_FUNCTION_BYTECODE, b));
    }
}

void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSVarRef **var_refs = p->u.func.var_refs;
    JSFunctionBytecode *b = p->u.func.function_bytecode;
    int i;

    if (p->u.func.home_object) {
        JS_MarkValue(rt, JS_MKPTR(JS_TAG_OBJECT, p->u.func.home_object),
                     mark_func);
    }
    if (b) {
        if (var_refs) {
            for(i = 0; i < b->closure_var_count; i++) {
                JSVarRef *var_ref = var_refs[i];
                if (var_ref) {
                    mark_func(rt, &var_ref->header);
                }
            }
        }
        /* must mark the function bytecode because template objects may be
           part of a cycle */
        JS_MarkValue(rt, JS_MKPTR(JS_TAG_FUNCTION_BYTECODE, b), mark_func);
    }
}

void js_bound_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSBoundFunction *bf = p->u.bound_function;
    int i;

    JS_FreeValueRT(rt, bf->func_obj);
    JS_FreeValueRT(rt, bf->this_val);
    for(i = 0; i < bf->argc; i++) {
        JS_FreeValueRT(rt, bf->argv[i]);
    }
    js_free_rt(rt, bf);
}

void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSBoundFunction *bf = p->u.bound_function;
    int i;

    JS_MarkValue(rt, bf->func_obj, mark_func);
    JS_MarkValue(rt, bf->this_val, mark_func);
    for(i = 0; i < bf->argc; i++)
        JS_MarkValue(rt, bf->argv[i], mark_func);
}

BOOL JS_IsFunction(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    switch(p->class_id) {
    case JS_CLASS_BYTECODE_FUNCTION:
        return TRUE;
    case JS_CLASS_PROXY:
        return p->u.proxy_data->is_func;
    default:
        return (ctx->rt->class_array[p->class_id].call != NULL);
    }
}

BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic)
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

BOOL JS_IsConstructor(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    return p->is_constructor;
}

BOOL JS_SetConstructorBit(JSContext *ctx, JSValueConst func_obj, BOOL val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(func_obj);
    p->is_constructor = val;
    return TRUE;
}


JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque)
{
    JSValue obj, this_val;
    int ret;

    this_val = JS_MKPTR(JS_TAG_OBJECT, p);
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    set_cycle_flag(ctx, obj);
    set_cycle_flag(ctx, this_val);
    ret = JS_DefinePropertyValue(ctx, obj, JS_ATOM_constructor,
                                 JS_DupValue(ctx, this_val),
                                 JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    if (ret < 0) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

/* warning: the refcount of the context is not incremented. Return
   NULL in case of exception (case of revoked proxy only) */
JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj)
{
    JSObject *p;
    JSContext *realm;

    if (JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)
        return ctx;
    p = JS_VALUE_GET_OBJ(func_obj);
    switch(p->class_id) {
    case JS_CLASS_C_FUNCTION:
        realm = p->u.cfunc.realm;
        break;
    case JS_CLASS_BYTECODE_FUNCTION:
    case JS_CLASS_GENERATOR_FUNCTION:
    case JS_CLASS_ASYNC_FUNCTION:
    case JS_CLASS_ASYNC_GENERATOR_FUNCTION:
        {
            JSFunctionBytecode *b;
            b = p->u.func.function_bytecode;
            realm = b->realm;
        }
        break;
    case JS_CLASS_PROXY:
        {
            JSProxyData *s = p->u.opaque;
            if (!s)
                return ctx;
            if (s->is_revoked) {
                JS_ThrowTypeErrorRevokedProxy(ctx);
                return NULL;
            } else {
                realm = JS_GetFunctionRealm(ctx, s->target);
            }
        }
        break;
    case JS_CLASS_BOUND_FUNCTION:
        {
            JSBoundFunction *bf = p->u.bound_function;
            realm = JS_GetFunctionRealm(ctx, bf->func_obj);
        }
        break;
    default:
        realm = ctx;
        break;
    }
    return realm;
}

JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                            int class_id)
{
    JSValue proto, obj;
    JSContext *realm;

    if (JS_IsUndefined(ctor)) {
        proto = JS_DupValue(ctx, ctx->class_proto[class_id]);
    } else {
        proto = JS_GetProperty(ctx, ctor, JS_ATOM_prototype);
        if (JS_IsException(proto))
            return proto;
        if (!JS_IsObject(proto)) {
            JS_FreeValue(ctx, proto);
            realm = JS_GetFunctionRealm(ctx, ctor);
            if (!realm)
                return JS_EXCEPTION;
            proto = JS_DupValue(ctx, realm->class_proto[class_id]);
        }
    }
    obj = JS_NewObjectProtoClass(ctx, proto, class_id);
    JS_FreeValue(ctx, proto);
    return obj;
}

int check_function(JSContext *ctx, JSValueConst obj)
{
    if (likely(JS_IsFunction(ctx, obj)))
        return 0;
    JS_ThrowTypeError(ctx, "not a function");
    return -1;
}

