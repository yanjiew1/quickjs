/*
 * QuickJS Object Module
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
#include "quickjs-internal.h"

QJS_INTERNAL void js_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_object_data_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_object_data_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_c_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_c_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL void js_bound_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL void gc_decref(JSRuntime *rt);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
QJS_INTERNAL void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
QJS_INTERNAL void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags);
QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h);
QJS_INTERNAL void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);

/* 'props[]' is used to initialized the object properties. The number
   of elements depends on the shape. */
QJS_INTERNAL JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                                     JSProperty *props)
{
    JSObject *p;
    int i;
    
    js_trigger_gc(ctx->rt, sizeof(JSObject));
    p = js_malloc(ctx, sizeof(JSObject));
    if (unlikely(!p))
        goto fail;
    p->class_id = class_id;
    p->is_std_array_prototype = 0;
    p->extensible = TRUE;
    p->free_mark = 0;
    p->is_exotic = 0;
    p->fast_array = 0;
    p->is_constructor = 0;
    p->has_immutable_prototype = 0;
    p->tmp_mark = 0;
    p->is_HTMLDDA = 0;
    p->weakref_count = 0;
    p->u.opaque = NULL;
    p->shape = sh;
    p->prop = js_malloc(ctx, sizeof(JSProperty) * sh->prop_size);
    if (unlikely(!p->prop)) {
        js_free(ctx, p);
    fail:
        if (props) {
            JSShapeProperty *prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                free_property(ctx->rt, &props[i], prs->flags);
                prs++;
            }
        }
        js_free_shape(ctx->rt, sh);
        return JS_EXCEPTION;
    }

    switch(class_id) {
    case JS_CLASS_OBJECT:
        break;
    case JS_CLASS_ARRAY:
        {
            JSProperty *pr;
            p->is_exotic = 1;
            p->fast_array = 1;
            p->u.array.u.values = NULL;
            p->u.array.count = 0;
            p->u.array.u1.size = 0;
            if (!props) {
                /* XXX: remove */
                /* the length property is always the first one */
                if (likely(sh == ctx->array_shape)) {
                    pr = &p->prop[0];
                } else {
                    /* only used for the first array */
                    /* cannot fail */
                    pr = add_property(ctx, p, JS_ATOM_length,
                                      JS_PROP_WRITABLE | JS_PROP_LENGTH);
                }
                pr->u.value = JS_NewInt32(ctx, 0);
            }
        }
        break;
    case JS_CLASS_C_FUNCTION:
        p->prop[0].u.value = JS_UNDEFINED;
        break;
    case JS_CLASS_ARGUMENTS:
    case JS_CLASS_MAPPED_ARGUMENTS:
    case JS_CLASS_UINT8C_ARRAY:
    case JS_CLASS_INT8_ARRAY:
    case JS_CLASS_UINT8_ARRAY:
    case JS_CLASS_INT16_ARRAY:
    case JS_CLASS_UINT16_ARRAY:
    case JS_CLASS_INT32_ARRAY:
    case JS_CLASS_UINT32_ARRAY:
    case JS_CLASS_BIG_INT64_ARRAY:
    case JS_CLASS_BIG_UINT64_ARRAY:
    case JS_CLASS_FLOAT16_ARRAY:
    case JS_CLASS_FLOAT32_ARRAY:
    case JS_CLASS_FLOAT64_ARRAY:
        p->is_exotic = 1;
        p->fast_array = 1;
        p->u.array.u.ptr = NULL;
        p->u.array.count = 0;
        break;
    case JS_CLASS_DATAVIEW:
        p->u.array.u.ptr = NULL;
        p->u.array.count = 0;
        break;
    case JS_CLASS_NUMBER:
    case JS_CLASS_STRING:
    case JS_CLASS_BOOLEAN:
    case JS_CLASS_SYMBOL:
    case JS_CLASS_DATE:
    case JS_CLASS_BIG_INT:
        p->u.object_data = JS_UNDEFINED;
        goto set_exotic;
    case JS_CLASS_REGEXP:
        p->u.regexp.pattern = NULL;
        p->u.regexp.bytecode = NULL;
        break;
    case JS_CLASS_GLOBAL_OBJECT:
        p->u.global_object.uninitialized_vars = JS_UNDEFINED;
        break;
    default:
    set_exotic:
        if (ctx->rt->class_array[class_id].exotic) {
            p->is_exotic = 1;
        }
        break;
    }
    js_rc(p)->ref_count = 1;
    add_gc_object(ctx->rt, &p->header, JS_GC_OBJ_TYPE_JS_OBJECT);
    if (props) {
        for(i = 0; i < sh->prop_count; i++)
            p->prop[i] = props[i];
    }
    return JS_MKPTR(JS_TAG_OBJECT, p);
}

QJS_INTERNAL JSObject *get_proto_obj(JSValueConst proto_val)
{
    if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_OBJECT)
        return NULL;
    else
        return JS_VALUE_GET_OBJ(proto_val);
}

/* WARNING: proto must be an object or JS_NULL */
JSValue JS_NewObjectProtoClass(JSContext *ctx, JSValueConst proto_val,
                               JSClassID class_id)
{
    JSShape *sh;
    JSObject *proto;

    proto = get_proto_obj(proto_val);
    sh = find_hashed_shape_proto(ctx->rt, proto);
    if (likely(sh)) {
        sh = js_dup_shape(sh);
    } else {
        sh = js_new_shape(ctx, proto);
        if (!sh)
            return JS_EXCEPTION;
    }
    return JS_NewObjectFromShape(ctx, sh, class_id, NULL);
}

/* WARNING: the shape is not hashed. It is used for objects where
   factorizing the shape is not relevant (prototypes, constructors) */
QJS_INTERNAL JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props)
{
    JSShape *sh;
    JSObject *proto;
    int hash_size, hash_bits;
    
    if (n_alloc_props <= JS_PROP_INITIAL_SIZE) {
        n_alloc_props = JS_PROP_INITIAL_SIZE;
        hash_size = JS_PROP_INITIAL_HASH_SIZE;
    } else {
        hash_bits = 32 - clz32(n_alloc_props - 1); /* ceil(log2(radix)) */
        hash_size = 1 << hash_bits;
    }
    proto = get_proto_obj(proto_val);
    sh = js_new_shape_nohash(ctx, proto, hash_size, n_alloc_props);
    if (!sh)
        return JS_EXCEPTION;
    return JS_NewObjectFromShape(ctx, sh, class_id, NULL);
}

#if 0
static JSValue JS_GetObjectData(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        switch(p->class_id) {
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
        case JS_CLASS_SYMBOL:
        case JS_CLASS_DATE:
        case JS_CLASS_BIG_INT:
            return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_UNDEFINED;
}
#endif

QJS_INTERNAL int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        switch(p->class_id) {
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
        case JS_CLASS_SYMBOL:
        case JS_CLASS_DATE:
        case JS_CLASS_BIG_INT:
            JS_FreeValue(ctx, p->u.object_data);
            p->u.object_data = val; /* for JS_CLASS_STRING, 'val' must
                                       be JS_TAG_STRING (and not a
                                       rope) */
            return 0;
        }
    }
    JS_FreeValue(ctx, val);
    if (!JS_IsException(obj))
        JS_ThrowTypeError(ctx, "invalid object type");
    return -1;
}

JSValue JS_NewObjectClass(JSContext *ctx, int class_id)
{
    return JS_NewObjectProtoClass(ctx, ctx->class_proto[class_id], class_id);
}

JSValue JS_NewObjectProto(JSContext *ctx, JSValueConst proto)
{
    return JS_NewObjectProtoClass(ctx, proto, JS_CLASS_OBJECT);
}

JSValue JS_NewArray(JSContext *ctx)
{
    return JS_NewObjectFromShape(ctx, js_dup_shape(ctx->array_shape),
                                 JS_CLASS_ARRAY, NULL);
}

JSValue JS_NewObject(JSContext *ctx)
{
    /* inline JS_NewObjectClass(ctx, JS_CLASS_OBJECT); */
    return JS_NewObjectProtoClass(ctx, ctx->class_proto[JS_CLASS_OBJECT], JS_CLASS_OBJECT);
}

QJS_INTERNAL void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                       JSAtom name, int len)
{
    /* ES6 feature non compatible with ES5.1: length is configurable */
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_length, JS_NewInt32(ctx, len),
                           JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name,
                           JS_AtomToString(ctx, name), JS_PROP_CONFIGURABLE);
}

QJS_INTERNAL BOOL js_class_has_bytecode(JSClassID class_id)
{
    return (class_id == JS_CLASS_BYTECODE_FUNCTION ||
            class_id == JS_CLASS_GENERATOR_FUNCTION ||
            class_id == JS_CLASS_ASYNC_FUNCTION ||
            class_id == JS_CLASS_ASYNC_GENERATOR_FUNCTION);
}

/* return NULL without exception if not a function or no bytecode */
QJS_INTERNAL JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(val);
    if (!js_class_has_bytecode(p->class_id))
        return NULL;
    return p->u.func.function_bytecode;
}

QJS_INTERNAL void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
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

QJS_INTERNAL JSValue js_get_function_name(JSContext *ctx, JSAtom name)
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
QJS_INTERNAL int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
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
QJS_INTERNAL JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
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


QJS_INTERNAL void js_c_function_data_finalizer(JSRuntime *rt, JSValue val)
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

QJS_INTERNAL void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
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

QJS_INTERNAL JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
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

QJS_INTERNAL JSContext *js_autoinit_get_realm(JSProperty *pr)
{
    return (JSContext *)(pr->u.init.realm_and_id & ~3);
}

QJS_INTERNAL JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr)
{
    return pr->u.init.realm_and_id & 3;
}

QJS_INTERNAL void js_autoinit_free(JSRuntime *rt, JSProperty *pr)
{
    JS_FreeContext(js_autoinit_get_realm(pr));
}

static void js_autoinit_mark(JSRuntime *rt, JSProperty *pr,
                             JS_MarkFunc *mark_func)
{
    mark_func(rt, &js_autoinit_get_realm(pr)->header);
}

QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags)
{
    if (unlikely(prop_flags & JS_PROP_TMASK)) {
        if ((prop_flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
            if (pr->u.getset.getter)
                JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
            if (pr->u.getset.setter)
                JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
        } else if ((prop_flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            free_var_ref(rt, pr->u.var_ref);
        } else if ((prop_flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
            js_autoinit_free(rt, pr);
        }
    } else {
        JS_FreeValueRT(rt, pr->u.value);
    }
}





/* indicate that the object may be part of a function prototype cycle */
QJS_INTERNAL void set_cycle_flag(JSContext *ctx, JSValueConst obj)
{
}

QJS_INTERNAL void free_var_ref(JSRuntime *rt, JSVarRef *var_ref)
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

QJS_INTERNAL void js_array_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    int i;

    for(i = 0; i < p->u.array.count; i++) {
        JS_FreeValueRT(rt, p->u.array.u.values[i]);
    }
    js_free_rt(rt, p->u.array.u.values);
}

QJS_INTERNAL void js_array_mark(JSRuntime *rt, JSValueConst val,
                          JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    int i;

    for(i = 0; i < p->u.array.count; i++) {
        JS_MarkValue(rt, p->u.array.u.values[i], mark_func);
    }
}

QJS_INTERNAL void js_object_data_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_FreeValueRT(rt, p->u.object_data);
    p->u.object_data = JS_UNDEFINED;
}

QJS_INTERNAL void js_object_data_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_MarkValue(rt, p->u.object_data, mark_func);
}

QJS_INTERNAL void js_c_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);

    if (p->u.cfunc.realm)
        JS_FreeContext(p->u.cfunc.realm);
}

QJS_INTERNAL void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);

    if (p->u.cfunc.realm)
        mark_func(rt, &p->u.cfunc.realm->header);
}

QJS_INTERNAL void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val)
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

QJS_INTERNAL void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
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

QJS_INTERNAL void js_bound_function_finalizer(JSRuntime *rt, JSValue val)
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

QJS_INTERNAL void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
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

QJS_INTERNAL void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSForInIterator *it = p->u.for_in_iterator;
    int i;

    JS_FreeValueRT(rt, it->obj);
    if (!it->is_array) {
        for(i = 0; i < it->atom_count; i++) {
            JS_FreeAtomRT(rt, it->tab_atom[i].atom);
        }
        js_free_rt(rt, it->tab_atom);
    }
    js_free_rt(rt, it);
}

QJS_INTERNAL void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSForInIterator *it = p->u.for_in_iterator;
    JS_MarkValue(rt, it->obj, mark_func);
}

static void free_object(JSRuntime *rt, JSObject *p)
{
    int i;
    JSClassFinalizer *finalizer;
    JSShape *sh;
    JSShapeProperty *pr;

    p->free_mark = 1; /* used to tell the object is invalid when
                         freeing cycles */
    /* free all the fields */
    sh = p->shape;
    pr = get_shape_prop(sh);
    for(i = 0; i < sh->prop_count; i++) {
        free_property(rt, &p->prop[i], pr->flags);
        pr++;
    }
    js_free_rt(rt, p->prop);
    /* as an optimization we destroy the shape immediately without
       putting it in gc_zero_ref_count_list */
    js_free_shape(rt, sh);

    /* fail safe */
    p->shape = NULL;
    p->prop = NULL;

    finalizer = rt->class_array[p->class_id].finalizer;
    if (finalizer)
        (*finalizer)(rt, JS_MKPTR(JS_TAG_OBJECT, p));

    /* fail safe */
    p->class_id = 0;
    p->u.opaque = NULL;
    p->u.func.var_refs = NULL;
    p->u.func.home_object = NULL;

    remove_gc_object(&p->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES) {
        if (js_rc(p)->ref_count == 0 && p->weakref_count == 0) {
            js_free_rt(rt, p);
        } else {
            /* keep the object structure because there are may be
               references to it */
            list_add_tail(&p->header.link, &rt->gc_zero_ref_count_list);
        }
    } else {
        /* keep the object structure in case there are weak references to it */
        if (p->weakref_count == 0) {
            js_free_rt(rt, p);
        } else {
            js_rc(p)->mark = 0; /* reset the mark so that the weakref can be freed */
        }
    }
}

static void free_gc_object(JSRuntime *rt, JSGCObjectHeader *gp)
{
    switch(js_rc(gp)->gc_obj_type) {
    case JS_GC_OBJ_TYPE_JS_OBJECT:
        free_object(rt, (JSObject *)gp);
        break;
    case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        free_function_bytecode(rt, (JSFunctionBytecode *)gp);
        break;
    case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        __async_func_free(rt, (JSAsyncFunctionState *)gp);
        break;
    case JS_GC_OBJ_TYPE_MODULE:
        js_free_module_def(rt, (JSModuleDef *)gp);
        break;
    default:
        abort();
    }
}

QJS_INTERNAL void free_zero_refcount(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    rt->gc_phase = JS_GC_PHASE_DECREF;
    for(;;) {
        el = rt->gc_zero_ref_count_list.next;
        if (el == &rt->gc_zero_ref_count_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->ref_count == 0);
        free_gc_object(rt, p);
    }
    rt->gc_phase = JS_GC_PHASE_NONE;
}

/* called with the ref_count of 'v' reaches zero. */
void __JS_FreeValueRT(JSRuntime *rt, JSValue v)
{
    uint32_t tag = JS_VALUE_GET_TAG(v);

#ifdef DUMP_FREE
    {
        printf("Freeing ");
        if (tag == JS_TAG_OBJECT) {
            JS_DumpObject(rt, JS_VALUE_GET_OBJ(v));
        } else {
            JS_DumpValueShort(rt, v);
            printf("\n");
        }
    }
#endif

    switch(tag) {
    case JS_TAG_STRING:
        {
            JSString *p = JS_VALUE_GET_STRING(v);
            if (p->atom_type) {
                JS_FreeAtomStruct(rt, p);
            } else {
#ifdef DUMP_LEAKS
                list_del(&p->link);
#endif
                js_free_rt(rt, p);
            }
        }
        break;
    case JS_TAG_STRING_ROPE:
        /* Note: recursion is acceptable because the rope depth is bounded */
        {
            JSStringRope *p = JS_VALUE_GET_STRING_ROPE(v);
            JS_FreeValueRT(rt, p->left);
            JS_FreeValueRT(rt, p->right);
            js_free_rt(rt, p);
        }
        break;
    case JS_TAG_OBJECT:
    case JS_TAG_FUNCTION_BYTECODE:
    case JS_TAG_MODULE:
        {
            JSGCObjectHeader *p = JS_VALUE_GET_PTR(v);
            if (rt->gc_phase != JS_GC_PHASE_REMOVE_CYCLES) {
                list_del(&p->link);
                list_add(&p->link, &rt->gc_zero_ref_count_list);
                js_rc(p)->mark = 1; /* indicate that the object is about to be freed */
                if (rt->gc_phase == JS_GC_PHASE_NONE) {
                    free_zero_refcount(rt);
                }
            }
        }
        break;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(v);
            js_free_rt(rt, p);
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(v);
            JS_FreeAtomStruct(rt, p);
        }
        break;
    default:
        abort();
    }
}

void __JS_FreeValue(JSContext *ctx, JSValue v)
{
    __JS_FreeValueRT(ctx->rt, v);
}

/* garbage collection */

static void gc_remove_weak_objects(JSRuntime *rt)
{
    struct list_head *el;

    /* add the freed objects to rt->gc_zero_ref_count_list so that
       rt->weakref_list is not modified while we traverse it */
    rt->gc_phase = JS_GC_PHASE_DECREF; 
        
    list_for_each(el, &rt->weakref_list) {
        JSWeakRefHeader *wh = list_entry(el, JSWeakRefHeader, link);
        switch(wh->weakref_type) {
        case JS_WEAKREF_TYPE_MAP:
            map_delete_weakrefs(rt, wh);
            break;
        case JS_WEAKREF_TYPE_WEAKREF:
            weakref_delete_weakref(rt, wh);
            break;
        case JS_WEAKREF_TYPE_FINREC:
            finrec_delete_weakref(rt, wh);
            break;
        default:
            abort();
        }
    }

    rt->gc_phase = JS_GC_PHASE_NONE;
    /* free the freed objects here. */
    free_zero_refcount(rt);
}

QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type)
{
    js_rc(h)->mark = 0;
    js_rc(h)->gc_obj_type = type;
    list_add_tail(&h->link, &rt->gc_obj_list);
}

QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h)
{
    list_del(&h->link);
}

void JS_MarkValue(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    if (JS_VALUE_HAS_REF_COUNT(val)) {
        switch(JS_VALUE_GET_TAG(val)) {
        case JS_TAG_OBJECT:
        case JS_TAG_FUNCTION_BYTECODE:
        case JS_TAG_MODULE:
            mark_func(rt, JS_VALUE_GET_PTR(val));
            break;
        default:
            break;
        }
    }
}

static void mark_children(JSRuntime *rt, JSGCObjectHeader *gp,
                          JS_MarkFunc *mark_func)
{
    switch(js_rc(gp)->gc_obj_type) {
    case JS_GC_OBJ_TYPE_JS_OBJECT:
        {
            JSObject *p = (JSObject *)gp;
            JSShapeProperty *prs;
            JSShape *sh;
            int i;
            sh = p->shape;
            mark_func(rt, &sh->header);
            /* mark all the fields */
            prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                JSProperty *pr = &p->prop[i];
                if (prs->atom != JS_ATOM_NULL) {
                    if (prs->flags & JS_PROP_TMASK) {
                        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                            if (pr->u.getset.getter)
                                mark_func(rt, &pr->u.getset.getter->header);
                            if (pr->u.getset.setter)
                                mark_func(rt, &pr->u.getset.setter->header);
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                            /* Note: the tag does not matter
                               provided it is a GC object */
                            mark_func(rt, &pr->u.var_ref->header);
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                            js_autoinit_mark(rt, pr, mark_func);
                        }
                    } else {
                        JS_MarkValue(rt, pr->u.value, mark_func);
                    }
                }
                prs++;
            }

            if (p->class_id != JS_CLASS_OBJECT) {
                JSClassGCMark *gc_mark;
                gc_mark = rt->class_array[p->class_id].gc_mark;
                if (gc_mark)
                    gc_mark(rt, JS_MKPTR(JS_TAG_OBJECT, p), mark_func);
            }
        }
        break;
    case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        /* the template objects can be part of a cycle */
        {
            JSFunctionBytecode *b = (JSFunctionBytecode *)gp;
            int i;
            for(i = 0; i < b->cpool_count; i++) {
                JS_MarkValue(rt, b->cpool[i], mark_func);
            }
            if (b->realm)
                mark_func(rt, &b->realm->header);
        }
        break;
    case JS_GC_OBJ_TYPE_VAR_REF:
        {
            JSVarRef *var_ref = (JSVarRef *)gp;
            if (var_ref->is_detached) {
                JS_MarkValue(rt, *var_ref->pvalue, mark_func);
            } else {
                JSStackFrame *sf = var_ref->stack_frame;
                if (sf->js_mode & JS_MODE_ASYNC) {
                    JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
                    mark_func(rt, &async_func->header);
                }
            }
        }
        break;
    case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        {
            JSAsyncFunctionState *s = (JSAsyncFunctionState *)gp;
            JSStackFrame *sf = &s->frame;
            JSValue *sp;

            if (!s->is_completed) {
                JS_MarkValue(rt, sf->cur_func, mark_func);
                JS_MarkValue(rt, s->this_val, mark_func);
                /* sf->cur_sp = NULL if the function is running */
                if (sf->cur_sp) {
                    /* if the function is running, cur_sp is not known so we
                       cannot mark the stack. Marking the variables is not needed
                       because a running function cannot be part of a removable
                       cycle */
                    for(sp = sf->arg_buf; sp < sf->cur_sp; sp++)
                        JS_MarkValue(rt, *sp, mark_func);
                }
            }
            JS_MarkValue(rt, s->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, s->resolving_funcs[1], mark_func);
        }
        break;
    case JS_GC_OBJ_TYPE_SHAPE:
        {
            JSShape *sh = (JSShape *)gp;
            if (sh->proto != NULL) {
                mark_func(rt, &sh->proto->header);
            }
        }
        break;
    case JS_GC_OBJ_TYPE_JS_CONTEXT:
        {
            JSContext *ctx = (JSContext *)gp;
            JS_MarkContext(rt, ctx, mark_func);
        }
        break;
    case JS_GC_OBJ_TYPE_MODULE:
        {
            JSModuleDef *m = (JSModuleDef *)gp;
            js_mark_module_def(rt, m, mark_func);
        }
        break;
    default:
        abort();
    }
}

static void gc_decref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    assert(js_rc(p)->ref_count > 0);
    js_rc(p)->ref_count--;
    if (js_rc(p)->ref_count == 0 && js_rc(p)->mark == 1) {
        list_del(&p->link);
        list_add_tail(&p->link, &rt->tmp_obj_list);
    }
}

QJS_INTERNAL void gc_decref(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;

    init_list_head(&rt->tmp_obj_list);

    /* decrement the refcount of all the children of all the GC
       objects and move the GC objects with zero refcount to
       tmp_obj_list */
    list_for_each_safe(el, el1, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->mark == 0);
        mark_children(rt, p, gc_decref_child);
        js_rc(p)->mark = 1;
        if (js_rc(p)->ref_count == 0) {
            list_del(&p->link);
            list_add_tail(&p->link, &rt->tmp_obj_list);
        }
    }
}

static void gc_scan_incref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    js_rc(p)->ref_count++;
    if (js_rc(p)->ref_count == 1) {
        /* ref_count was 0: remove from tmp_obj_list and add at the
           end of gc_obj_list */
        list_del(&p->link);
        list_add_tail(&p->link, &rt->gc_obj_list);
        js_rc(p)->mark = 0; /* reset the mark for the next GC call */
    }
}

static void gc_scan_incref_child2(JSRuntime *rt, JSGCObjectHeader *p)
{
    js_rc(p)->ref_count++;
}

static void gc_scan(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    /* keep the objects with a refcount > 0 and their children. */
    list_for_each(el, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->ref_count > 0);
        js_rc(p)->mark = 0; /* reset the mark for the next GC call */
        mark_children(rt, p, gc_scan_incref_child);
    }

    /* restore the refcount of the objects to be deleted. */
    list_for_each(el, &rt->tmp_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        mark_children(rt, p, gc_scan_incref_child2);
    }
}

static void gc_free_cycles(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;
#ifdef DUMP_GC_FREE
    BOOL header_done = FALSE;
#endif

    rt->gc_phase = JS_GC_PHASE_REMOVE_CYCLES;

    for(;;) {
        el = rt->tmp_obj_list.next;
        if (el == &rt->tmp_obj_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        /* Only need to free the GC object associated with JS values
           or async functions. The rest will be automatically removed
           because they must be referenced by them. */
        switch(js_rc(p)->gc_obj_type) {
        case JS_GC_OBJ_TYPE_JS_OBJECT:
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        case JS_GC_OBJ_TYPE_MODULE:
#ifdef DUMP_GC_FREE
            if (!header_done) {
                printf("Freeing cycles:\n");
                JS_DumpObjectHeader(rt);
                header_done = TRUE;
            }
            JS_DumpGCObject(rt, p);
#endif
            free_gc_object(rt, p);
            break;
        default:
            list_del(&p->link);
            list_add_tail(&p->link, &rt->gc_zero_ref_count_list);
            break;
        }
    }
    rt->gc_phase = JS_GC_PHASE_NONE;

    list_for_each_safe(el, el1, &rt->gc_zero_ref_count_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_FUNCTION_BYTECODE ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_ASYNC_FUNCTION ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_MODULE);
        if (js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT &&
            ((JSObject *)p)->weakref_count != 0) {
            /* keep the object because there are weak references to it */
            js_rc(p)->mark = 0;
        } else {
            js_free_rt(rt, p);
        }
    }

    init_list_head(&rt->gc_zero_ref_count_list);
}

QJS_INTERNAL void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects)
{
    if (remove_weak_objects) {
        /* free the weakly referenced object or symbol structures, delete
           the associated Map/Set entries and queue the finalization
           registry callbacks. */
        gc_remove_weak_objects(rt);
    }
    
    /* decrement the reference of the children of each object. mark =
       1 after this pass. */
    gc_decref(rt);

    /* keep the GC objects with a non zero refcount and their childs */
    gc_scan(rt);

    /* free the GC objects in a cycle */
    gc_free_cycles(rt);
}

void JS_RunGC(JSRuntime *rt)
{
    JS_RunGCInternal(rt, TRUE);
}

/* Return false if not an object or if the object has already been
   freed (zombie objects are visible in finalizers when freeing
   cycles). */
BOOL JS_IsLiveObject(JSRuntime *rt, JSValueConst obj)
{
    JSObject *p;
    if (!JS_IsObject(obj))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return !p->free_mark;
}

/* Compute memory used by various object types */
/* XXX: poor man's approach to handling multiply referenced objects */
typedef struct JSMemoryUsage_helper {
    double memory_used_count;
    double str_count;
    double str_size;
    int64_t js_func_count;
    double js_func_size;
    int64_t js_func_code_size;
    int64_t js_func_pc2line_count;
    int64_t js_func_pc2line_size;
} JSMemoryUsage_helper;

static void compute_value_size(JSValueConst val, JSMemoryUsage_helper *hp);

static void compute_jsstring_size(JSString *str, JSMemoryUsage_helper *hp)
{
    if (!str->atom_type) {  /* atoms are handled separately */
        double s_ref_count = js_rc(str)->ref_count;
        hp->str_count += 1 / s_ref_count;
        hp->str_size += ((sizeof(*str) + (str->len << str->is_wide_char) +
                          1 - str->is_wide_char) / s_ref_count);
    }
}

static void compute_bytecode_size(JSFunctionBytecode *b, JSMemoryUsage_helper *hp)
{
    int memory_used_count, js_func_size, i;

    memory_used_count = 0;
    js_func_size = offsetof(JSFunctionBytecode, debug);
    if (b->vardefs) {
        js_func_size += (b->arg_count + b->var_count) * sizeof(*b->vardefs);
    }
    if (b->cpool) {
        js_func_size += b->cpool_count * sizeof(*b->cpool);
        for (i = 0; i < b->cpool_count; i++) {
            JSValueConst val = b->cpool[i];
            compute_value_size(val, hp);
        }
    }
    if (b->closure_var) {
        js_func_size += b->closure_var_count * sizeof(*b->closure_var);
    }
    if (!b->read_only_bytecode && b->byte_code_buf) {
        hp->js_func_code_size += b->byte_code_len;
    }
    if (b->has_debug) {
        js_func_size += sizeof(*b) - offsetof(JSFunctionBytecode, debug);
        if (b->debug.source) {
            memory_used_count++;
            js_func_size += b->debug.source_len + 1;
        }
        if (b->debug.pc2line_len) {
            memory_used_count++;
            hp->js_func_pc2line_count += 1;
            hp->js_func_pc2line_size += b->debug.pc2line_len;
        }
    }
    hp->js_func_size += js_func_size;
    hp->js_func_count += 1;
    hp->memory_used_count += memory_used_count;
}

static void compute_value_size(JSValueConst val, JSMemoryUsage_helper *hp)
{
    switch(JS_VALUE_GET_TAG(val)) {
    case JS_TAG_STRING:
        compute_jsstring_size(JS_VALUE_GET_STRING(val), hp);
        break;
    case JS_TAG_BIG_INT:
        /* should track JSBigInt usage */
        break;
    }
}

void JS_ComputeMemoryUsage(JSRuntime *rt, JSMemoryUsage *s)
{
    struct list_head *el, *el1;
    int i;
    JSMemoryUsage_helper mem = { 0 }, *hp = &mem;

    memset(s, 0, sizeof(*s));
    s->malloc_count = rt->malloc_ctx.malloc_state.malloc_count;
    s->malloc_size = rt->malloc_ctx.malloc_state.malloc_size;
    s->malloc_limit = rt->malloc_ctx.malloc_state.malloc_limit;

    s->memory_used_count = 2; /* rt + rt->class_array */
    s->memory_used_size = sizeof(JSRuntime) + sizeof(JSValue) * rt->class_count;

    list_for_each(el, &rt->context_list) {
        JSContext *ctx = list_entry(el, JSContext, link);
        JSShape *sh = ctx->array_shape;
        s->memory_used_count += 2; /* ctx + ctx->class_proto */
        s->memory_used_size += sizeof(JSContext) +
            sizeof(JSValue) * rt->class_count;
        s->binary_object_count += ctx->binary_object_count;
        s->binary_object_size += ctx->binary_object_size;

        /* the hashed shapes are counted separately */
        if (sh && !sh->is_hashed) {
            int hash_size = sh->prop_hash_mask + 1;
            s->shape_count++;
            s->shape_size += get_shape_size(hash_size, sh->prop_size);
        }
        list_for_each(el1, &ctx->loaded_modules) {
            JSModuleDef *m = list_entry(el1, JSModuleDef, link);
            s->memory_used_count += 1;
            s->memory_used_size += sizeof(*m);
            if (m->req_module_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->req_module_entries_count * sizeof(*m->req_module_entries);
            }
            if (m->export_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->export_entries_count * sizeof(*m->export_entries);
                for (i = 0; i < m->export_entries_count; i++) {
                    JSExportEntry *me = &m->export_entries[i];
                    if (me->export_type == JS_EXPORT_TYPE_LOCAL && me->u.local.var_ref) {
                        /* potential multiple count */
                        s->memory_used_count += 1;
                        compute_value_size(me->u.local.var_ref->value, hp);
                    }
                }
            }
            if (m->star_export_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->star_export_entries_count * sizeof(*m->star_export_entries);
            }
            if (m->import_entries) {
                s->memory_used_count += 1;
                s->memory_used_size += m->import_entries_count * sizeof(*m->import_entries);
            }
            compute_value_size(m->module_ns, hp);
            compute_value_size(m->func_obj, hp);
        }
    }

    list_for_each(el, &rt->gc_obj_list) {
        JSGCObjectHeader *gp = list_entry(el, JSGCObjectHeader, link);
        JSObject *p;
        JSShape *sh;
        JSShapeProperty *prs;

        /* XXX: could count the other GC object types too */
        if (js_rc(gp)->gc_obj_type == JS_GC_OBJ_TYPE_FUNCTION_BYTECODE) {
            compute_bytecode_size((JSFunctionBytecode *)gp, hp);
            continue;
        } else if (js_rc(gp)->gc_obj_type != JS_GC_OBJ_TYPE_JS_OBJECT) {
            continue;
        }
        p = (JSObject *)gp;
        sh = p->shape;
        s->obj_count++;
        if (p->prop) {
            s->memory_used_count++;
            s->prop_size += sh->prop_size * sizeof(*p->prop);
            s->prop_count += sh->prop_count;
            prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                JSProperty *pr = &p->prop[i];
                if (prs->atom != JS_ATOM_NULL && !(prs->flags & JS_PROP_TMASK)) {
                    compute_value_size(pr->u.value, hp);
                }
                prs++;
            }
        }
        /* the hashed shapes are counted separately */
        if (!sh->is_hashed) {
            int hash_size = sh->prop_hash_mask + 1;
            s->shape_count++;
            s->shape_size += get_shape_size(hash_size, sh->prop_size);
        }

        switch(p->class_id) {
        case JS_CLASS_ARRAY:             /* u.array | length */
        case JS_CLASS_ARGUMENTS:         /* u.array | length */
            s->array_count++;
            if (p->fast_array) {
                s->fast_array_count++;
                if (p->u.array.u.values) {
                    s->memory_used_count++;
                    s->memory_used_size += p->u.array.count *
                        sizeof(*p->u.array.u.values);
                    s->fast_array_elements += p->u.array.count;
                    for (i = 0; i < p->u.array.count; i++) {
                        compute_value_size(p->u.array.u.values[i], hp);
                    }
                }
            }
            break;
        case JS_CLASS_MAPPED_ARGUMENTS:         /* u.array | length */
            if (p->fast_array) {
                s->fast_array_count++;
                if (p->u.array.u.values) {
                    s->memory_used_count++;
                    s->memory_used_size += p->u.array.count *
                        sizeof(*p->u.array.u.var_refs);
                    s->fast_array_elements += p->u.array.count;
                    for (i = 0; i < p->u.array.count; i++) {
                        compute_value_size(*p->u.array.u.var_refs[i]->pvalue, hp);
                    }
                }
            }
            break;
        case JS_CLASS_NUMBER:            /* u.object_data */
        case JS_CLASS_STRING:            /* u.object_data */
        case JS_CLASS_BOOLEAN:           /* u.object_data */
        case JS_CLASS_SYMBOL:            /* u.object_data */
        case JS_CLASS_DATE:              /* u.object_data */
        case JS_CLASS_BIG_INT:           /* u.object_data */
            compute_value_size(p->u.object_data, hp);
            break;
        case JS_CLASS_C_FUNCTION:        /* u.cfunc */
            s->c_func_count++;
            break;
        case JS_CLASS_BYTECODE_FUNCTION: /* u.func */
            {
                JSFunctionBytecode *b = p->u.func.function_bytecode;
                JSVarRef **var_refs = p->u.func.var_refs;
                /* home_object: object will be accounted for in list scan */
                if (var_refs) {
                    s->memory_used_count++;
                    s->js_func_size += b->closure_var_count * sizeof(*var_refs);
                    for (i = 0; i < b->closure_var_count; i++) {
                        if (var_refs[i]) {
                            double ref_count = js_rc(var_refs[i])->ref_count;
                            s->memory_used_count += 1 / ref_count;
                            s->js_func_size += sizeof(*var_refs[i]) / ref_count;
                            /* handle non object closed values */
                            if (var_refs[i]->pvalue == &var_refs[i]->value) {
                                /* potential multiple count */
                                compute_value_size(var_refs[i]->value, hp);
                            }
                        }
                    }
                }
            }
            break;
        case JS_CLASS_BOUND_FUNCTION:    /* u.bound_function */
            {
                JSBoundFunction *bf = p->u.bound_function;
                /* func_obj and this_val are objects */
                for (i = 0; i < bf->argc; i++) {
                    compute_value_size(bf->argv[i], hp);
                }
                s->memory_used_count += 1;
                s->memory_used_size += sizeof(*bf) + bf->argc * sizeof(*bf->argv);
            }
            break;
        case JS_CLASS_C_FUNCTION_DATA:   /* u.c_function_data_record */
            {
                JSCFunctionDataRecord *fd = p->u.c_function_data_record;
                if (fd) {
                    for (i = 0; i < fd->data_len; i++) {
                        compute_value_size(fd->data[i], hp);
                    }
                    s->memory_used_count += 1;
                    s->memory_used_size += sizeof(*fd) + fd->data_len * sizeof(*fd->data);
                }
            }
            break;
        case JS_CLASS_REGEXP:            /* u.regexp */
            compute_jsstring_size(p->u.regexp.pattern, hp);
            compute_jsstring_size(p->u.regexp.bytecode, hp);
            break;

        case JS_CLASS_FOR_IN_ITERATOR:   /* u.for_in_iterator */
            {
                JSForInIterator *it = p->u.for_in_iterator;
                if (it) {
                    compute_value_size(it->obj, hp);
                    s->memory_used_count += 1;
                    s->memory_used_size += sizeof(*it);
                }
            }
            break;
        case JS_CLASS_ARRAY_BUFFER:      /* u.array_buffer */
        case JS_CLASS_SHARED_ARRAY_BUFFER: /* u.array_buffer */
            {
                JSArrayBuffer *abuf = p->u.array_buffer;
                if (abuf) {
                    s->memory_used_count += 1;
                    s->memory_used_size += sizeof(*abuf);
                    if (abuf->data) {
                        s->memory_used_count += 1;
                        s->memory_used_size += abuf->byte_length;
                    }
                }
            }
            break;
        case JS_CLASS_GENERATOR:         /* u.generator_data */
        case JS_CLASS_UINT8C_ARRAY:      /* u.typed_array / u.array */
        case JS_CLASS_INT8_ARRAY:        /* u.typed_array / u.array */
        case JS_CLASS_UINT8_ARRAY:       /* u.typed_array / u.array */
        case JS_CLASS_INT16_ARRAY:       /* u.typed_array / u.array */
        case JS_CLASS_UINT16_ARRAY:      /* u.typed_array / u.array */
        case JS_CLASS_INT32_ARRAY:       /* u.typed_array / u.array */
        case JS_CLASS_UINT32_ARRAY:      /* u.typed_array / u.array */
        case JS_CLASS_BIG_INT64_ARRAY:   /* u.typed_array / u.array */
        case JS_CLASS_BIG_UINT64_ARRAY:  /* u.typed_array / u.array */
        case JS_CLASS_FLOAT16_ARRAY:     /* u.typed_array / u.array */
        case JS_CLASS_FLOAT32_ARRAY:     /* u.typed_array / u.array */
        case JS_CLASS_FLOAT64_ARRAY:     /* u.typed_array / u.array */
        case JS_CLASS_DATAVIEW:          /* u.typed_array */
        case JS_CLASS_MAP:               /* u.map_state */
        case JS_CLASS_SET:               /* u.map_state */
        case JS_CLASS_WEAKMAP:           /* u.map_state */
        case JS_CLASS_WEAKSET:           /* u.map_state */
        case JS_CLASS_MAP_ITERATOR:      /* u.map_iterator_data */
        case JS_CLASS_SET_ITERATOR:      /* u.map_iterator_data */
        case JS_CLASS_ARRAY_ITERATOR:    /* u.array_iterator_data */
        case JS_CLASS_STRING_ITERATOR:   /* u.array_iterator_data */
        case JS_CLASS_PROXY:             /* u.proxy_data */
        case JS_CLASS_PROMISE:           /* u.promise_data */
        case JS_CLASS_PROMISE_RESOLVE_FUNCTION:  /* u.promise_function_data */
        case JS_CLASS_PROMISE_REJECT_FUNCTION:   /* u.promise_function_data */
        case JS_CLASS_ASYNC_FUNCTION_RESOLVE:    /* u.async_function_data */
        case JS_CLASS_ASYNC_FUNCTION_REJECT:     /* u.async_function_data */
        case JS_CLASS_ASYNC_FROM_SYNC_ITERATOR:  /* u.async_from_sync_iterator_data */
        case JS_CLASS_ASYNC_GENERATOR:   /* u.async_generator_data */
            /* TODO */
        default:
            /* XXX: class definition should have an opaque block size */
            if (p->u.opaque) {
                s->memory_used_count += 1;
            }
            break;
        }
    }
    s->obj_size += s->obj_count * sizeof(JSObject);

    /* hashed shapes */
    s->memory_used_count++; /* rt->shape_hash */
    s->memory_used_size += sizeof(rt->shape_hash[0]) * rt->shape_hash_size;
    for(i = 0; i < rt->shape_hash_size; i++) {
        JSShape *sh;
        for(sh = rt->shape_hash[i]; sh != NULL; sh = sh->shape_hash_next) {
            int hash_size = sh->prop_hash_mask + 1;
            s->shape_count++;
            s->shape_size += get_shape_size(hash_size, sh->prop_size);
        }
    }

    /* atoms */
    s->memory_used_count += 2; /* rt->atom_array, rt->atom_hash */
    s->atom_count = rt->atom_count;
    s->atom_size = sizeof(rt->atom_array[0]) * rt->atom_size +
        sizeof(rt->atom_hash[0]) * rt->atom_hash_size;
    for(i = 0; i < rt->atom_size; i++) {
        JSAtomStruct *p = rt->atom_array[i];
        if (!atom_is_free(p)) {
            s->atom_size += (sizeof(*p) + (p->len << p->is_wide_char) +
                             1 - p->is_wide_char);
        }
    }
    s->str_count = round(mem.str_count);
    s->str_size = round(mem.str_size);
    s->js_func_count = mem.js_func_count;
    s->js_func_size = round(mem.js_func_size);
    s->js_func_code_size = mem.js_func_code_size;
    s->js_func_pc2line_count = mem.js_func_pc2line_count;
    s->js_func_pc2line_size = mem.js_func_pc2line_size;
    s->memory_used_count += round(mem.memory_used_count) +
        s->atom_count + s->str_count +
        s->obj_count + s->shape_count +
        s->js_func_count + s->js_func_pc2line_count;
    s->memory_used_size += s->atom_size + s->str_size +
        s->obj_size + s->prop_size + s->shape_size +
        s->js_func_size + s->js_func_code_size + s->js_func_pc2line_size;
}

void JS_DumpMemoryUsage(FILE *fp, const JSMemoryUsage *s, JSRuntime *rt)
{
    fprintf(fp, "QuickJS memory usage -- " CONFIG_VERSION " version, %d-bit, malloc limit: %"PRId64"\n\n",
            (int)sizeof(void *) * 8, s->malloc_limit);
#if 1
    if (rt) {
        static const struct {
            const char *name;
            size_t size;
        } object_types[] = {
            { "JSRuntime", sizeof(JSRuntime) },
            { "JSContext", sizeof(JSContext) },
            { "JSObject", sizeof(JSObject) },
            { "JSString", sizeof(JSString) },
            { "JSFunctionBytecode", sizeof(JSFunctionBytecode) },
        };
        int i, usage_size_ok = 0;
        for(i = 0; i < countof(object_types); i++) {
            unsigned int size = object_types[i].size;
            void *p = js_malloc_rt(rt, size);
            if (p) {
                unsigned int size1 = js_malloc_usable_size_rt(rt, p);
                if (size1 >= size) {
                    usage_size_ok = 1;
                    fprintf(fp, "  %3u + %-2u  %s\n",
                            size, size1 - size, object_types[i].name);
                }
                js_free_rt(rt, p);
            }
        }
        if (!usage_size_ok) {
            fprintf(fp, "  malloc_usable_size unavailable\n");
        }
        {
            int obj_classes[JS_CLASS_INIT_COUNT + 1] = { 0 };
            int class_id;
            struct list_head *el;
            list_for_each(el, &rt->gc_obj_list) {
                JSGCObjectHeader *gp = list_entry(el, JSGCObjectHeader, link);
                JSObject *p;
                if (js_rc(gp)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT) {
                    p = (JSObject *)gp;
                    obj_classes[min_uint32(p->class_id, JS_CLASS_INIT_COUNT)]++;
                }
            }
            fprintf(fp, "\n" "JSObject classes\n");
            if (obj_classes[0])
                fprintf(fp, "  %5d  %2.0d %s\n", obj_classes[0], 0, "none");
            for (class_id = 1; class_id < JS_CLASS_INIT_COUNT; class_id++) {
                if (obj_classes[class_id] && class_id < rt->class_count) {
                    char buf[ATOM_GET_STR_BUF_SIZE];
                    fprintf(fp, "  %5d  %2.0d %s\n", obj_classes[class_id], class_id,
                            JS_AtomGetStrRT(rt, buf, sizeof(buf), rt->class_array[class_id].class_name));
                }
            }
            if (obj_classes[JS_CLASS_INIT_COUNT])
                fprintf(fp, "  %5d  %2.0d %s\n", obj_classes[JS_CLASS_INIT_COUNT], 0, "other");
        }
        fprintf(fp, "\n");
    }
#endif
    fprintf(fp, "%-20s %8s %8s\n", "NAME", "COUNT", "SIZE");

    if (s->malloc_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per block)\n",
                "memory allocated", s->malloc_count, s->malloc_size,
                (double)s->malloc_size / s->malloc_count);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%d overhead, %0.1f average slack)\n",
                "memory used", s->memory_used_count, s->memory_used_size,
                MALLOC_OVERHEAD, ((double)(s->malloc_size - s->memory_used_size) /
                                  s->memory_used_count));
    }
    if (s->atom_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per atom)\n",
                "atoms", s->atom_count, s->atom_size,
                (double)s->atom_size / s->atom_count);
    }
    if (s->str_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per string)\n",
                "strings", s->str_count, s->str_size,
                (double)s->str_size / s->str_count);
    }
    if (s->obj_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per object)\n",
                "objects", s->obj_count, s->obj_size,
                (double)s->obj_size / s->obj_count);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per object)\n",
                "  properties", s->prop_count, s->prop_size,
                (double)s->prop_count / s->obj_count);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per shape)\n",
                "  shapes", s->shape_count, s->shape_size,
                (double)s->shape_size / s->shape_count);
    }
    if (s->js_func_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"\n",
                "bytecode functions", s->js_func_count, s->js_func_size);
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per function)\n",
                "  bytecode", s->js_func_count, s->js_func_code_size,
                (double)s->js_func_code_size / s->js_func_count);
        if (s->js_func_pc2line_count) {
            fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per function)\n",
                    "  pc2line", s->js_func_pc2line_count,
                    s->js_func_pc2line_size,
                    (double)s->js_func_pc2line_size / s->js_func_pc2line_count);
        }
    }
    if (s->c_func_count) {
        fprintf(fp, "%-20s %8"PRId64"\n", "C functions", s->c_func_count);
    }
    if (s->array_count) {
        fprintf(fp, "%-20s %8"PRId64"\n", "arrays", s->array_count);
        if (s->fast_array_count) {
            fprintf(fp, "%-20s %8"PRId64"\n", "  fast arrays", s->fast_array_count);
            fprintf(fp, "%-20s %8"PRId64" %8"PRId64"  (%0.1f per fast array)\n",
                    "  elements", s->fast_array_elements,
                    s->fast_array_elements * (int)sizeof(JSValue),
                    (double)s->fast_array_elements / s->fast_array_count);
        }
    }
    if (s->binary_object_count) {
        fprintf(fp, "%-20s %8"PRId64" %8"PRId64"\n",
                "binary objects", s->binary_object_count, s->binary_object_size);
    }
}

JSValue JS_GetGlobalObject(JSContext *ctx)
{
    return JS_DupValue(ctx, ctx->global_obj);
}

/* WARNING: obj is freed */
JSValue JS_Throw(JSContext *ctx, JSValue obj)
{
    JSRuntime *rt = ctx->rt;
    JS_FreeValue(ctx, rt->current_exception);
    rt->current_exception = obj;
    rt->current_exception_is_uncatchable = FALSE;
    return JS_EXCEPTION;
}

/* return the pending exception (cannot be called twice). */
JSValue JS_GetException(JSContext *ctx)
{
    JSValue val;
    JSRuntime *rt = ctx->rt;
    val = rt->current_exception;
    rt->current_exception = JS_UNINITIALIZED;
    return val;
}

JS_BOOL JS_HasException(JSContext *ctx)
{
    return !JS_IsUninitialized(ctx->rt->current_exception);
}

QJS_INTERNAL void dbuf_put_leb128(DynBuf *s, uint32_t v)
{
    uint32_t a;
    for(;;) {
        a = v & 0x7f;
        v >>= 7;
        if (v != 0) {
            dbuf_putc(s, a | 0x80);
        } else {
            dbuf_putc(s, a);
            break;
        }
    }
}

QJS_INTERNAL void dbuf_put_sleb128(DynBuf *s, int32_t v1)
{
    uint32_t v = v1;
    dbuf_put_leb128(s, (2 * v) ^ -(v >> 31));
}

QJS_INTERNAL int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end)
{
    const uint8_t *ptr = buf;
    uint32_t v, a, i;
    v = 0;
    for(i = 0; i < 5; i++) {
        if (unlikely(ptr >= buf_end))
            break;
        a = *ptr++;
        v |= (a & 0x7f) << (i * 7);
        if (!(a & 0x80)) {
            *pval = v;
            return ptr - buf;
        }
    }
    *pval = 0;
    return -1;
}

QJS_INTERNAL int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end)
{
    int ret;
    uint32_t val;
    ret = get_leb128(&val, buf, buf_end);
    if (ret < 0) {
        *pval = 0;
        return -1;
    }
    *pval = (val >> 1) ^ -(val & 1);
    return ret;
}

/* use pc_value = -1 to get the position of the function definition */
QJS_INTERNAL int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value, int *pcol_num)
{
    const uint8_t *p_end, *p;
    int new_line_num, line_num, pc, v, ret, new_col_num, col_num;
    uint32_t val;
    unsigned int op;

    if (!b->has_debug || !b->debug.pc2line_buf)
        goto fail; /* function was stripped */

    p = b->debug.pc2line_buf;
    p_end = p + b->debug.pc2line_len;

    /* get the function line and column numbers */
    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    line_num = val + 1;

    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    col_num = val + 1;

    if (pc_value != -1) {
        pc = 0;
        while (p < p_end) {
            op = *p++;
            if (op == 0) {
                ret = get_leb128(&val, p, p_end);
                if (ret < 0)
                    goto fail;
                pc += val;
                p += ret;
                ret = get_sleb128(&v, p, p_end);
                if (ret < 0)
                    goto fail;
                p += ret;
                new_line_num = line_num + v;
            } else {
                op -= PC2LINE_OP_FIRST;
                pc += (op / PC2LINE_RANGE);
                new_line_num = line_num + (op % PC2LINE_RANGE) + PC2LINE_BASE;
            }
            ret = get_sleb128(&v, p, p_end);
            if (ret < 0)
                goto fail;
            p += ret;
            new_col_num = col_num + v;
            
            if (pc_value < pc)
                goto done;
            line_num = new_line_num;
            col_num = new_col_num;
        }
    }
 done:
    *pcol_num = col_num;
    return line_num;
 fail:
    *pcol_num = 0;
    return 0;
}

/* return a string property without executing arbitrary JS code (used
   when dumping the stack trace or in debug print). */
QJS_INTERNAL const char *get_prop_string(JSContext *ctx, JSValueConst obj, JSAtom prop)
{
    JSObject *p;
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;

    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (!prs) {
        /* we look at one level in the prototype to handle the 'name'
           field of the Error objects */
        p = p->shape->proto;
        if (!p)
            return NULL;
        prs = find_own_property(&pr, p, prop);
        if (!prs)
            return NULL;
    }
    
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return NULL;
    val = pr->u.value;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_STRING)
        return NULL;
    return JS_ToCString(ctx, val);
}


/* if filename != NULL, an additional level is added with the filename
   and line number information (used for parse error). */
QJS_INTERNAL void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num, int col_num,
                            int backtrace_flags)
{
    JSStackFrame *sf;
    JSValue str;
    DynBuf dbuf;
    const char *func_name_str;
    const char *str1;
    JSObject *p;

    if (!JS_IsObject(error_obj))
        return; /* protection in the out of memory case */
    
    js_dbuf_init(ctx, &dbuf);
    if (filename) {
        dbuf_printf(&dbuf, "    at %s", filename);
        if (line_num != -1)
            dbuf_printf(&dbuf, ":%d:%d", line_num, col_num);
        dbuf_putc(&dbuf, '\n');
        str = JS_NewString(ctx, filename);
        if (JS_IsException(str))
            return;
        /* Note: SpiderMonkey does that, could update once there is a standard */
        if (JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_fileName, str,
                                   JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0 ||
            JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_lineNumber, JS_NewInt32(ctx, line_num),
                                   JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0 ||
            JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_columnNumber, JS_NewInt32(ctx, col_num),
                                   JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0) {
            return;
        }
    }
    for(sf = ctx->rt->current_stack_frame; sf != NULL; sf = sf->prev_frame) {
        if (sf->js_mode & JS_MODE_BACKTRACE_BARRIER)
            break;
        if (backtrace_flags & JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL) {
            backtrace_flags &= ~JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL;
            continue;
        }
        func_name_str = get_prop_string(ctx, sf->cur_func, JS_ATOM_name);
        if (!func_name_str || func_name_str[0] == '\0')
            str1 = "<anonymous>";
        else
            str1 = func_name_str;
        dbuf_printf(&dbuf, "    at %s", str1);
        JS_FreeCString(ctx, func_name_str);

        p = JS_VALUE_GET_OBJ(sf->cur_func);
        if (js_class_has_bytecode(p->class_id)) {
            JSFunctionBytecode *b;
            const char *atom_str;
            int line_num1, col_num1;

            b = p->u.func.function_bytecode;
            if (b->has_debug) {
                line_num1 = find_line_num(ctx, b,
                                          sf->cur_pc - b->byte_code_buf - 1, &col_num1);
                atom_str = JS_AtomToCString(ctx, b->debug.filename);
                dbuf_printf(&dbuf, " (%s",
                            atom_str ? atom_str : "<null>");
                JS_FreeCString(ctx, atom_str);
                if (line_num1 != 0)
                    dbuf_printf(&dbuf, ":%d:%d", line_num1, col_num1);
                dbuf_putc(&dbuf, ')');
            }
        } else {
            dbuf_printf(&dbuf, " (native)");
        }
        dbuf_putc(&dbuf, '\n');
    }
    dbuf_putc(&dbuf, '\0');
    if (dbuf_error(&dbuf))
        str = JS_NULL;
    else
        str = JS_NewString(ctx, (char *)dbuf.buf);
    dbuf_free(&dbuf);
    JS_DefinePropertyValue(ctx, error_obj, JS_ATOM_stack, str,
                           JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
}

/* Note: it is important that no exception is returned by this function */
QJS_INTERNAL BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != JS_CLASS_ERROR)
        return FALSE;
    if (find_own_property1(p, JS_ATOM_stack))
        return FALSE;
    return TRUE;
}

JSValue JS_NewError(JSContext *ctx)
{
    return JS_NewObjectClass(ctx, JS_CLASS_ERROR);
}

QJS_INTERNAL JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                              const char *fmt, va_list ap, BOOL add_backtrace)
{
    char buf[256];
    JSValue obj, ret;

    vsnprintf(buf, sizeof(buf), fmt, ap);
    obj = JS_NewObjectProtoClass(ctx, ctx->native_error_proto[error_num],
                                 JS_CLASS_ERROR);
    if (unlikely(JS_IsException(obj))) {
        /* out of memory: throw JS_NULL to avoid recursing */
        obj = JS_NULL;
    } else {
        JS_DefinePropertyValue(ctx, obj, JS_ATOM_message,
                               JS_NewString(ctx, buf),
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        if (add_backtrace) {
            build_backtrace(ctx, obj, NULL, 0, 0, 0);
        }
    }
    ret = JS_Throw(ctx, obj);
    return ret;
}

QJS_INTERNAL JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num,
                             const char *fmt, va_list ap)
{
    JSRuntime *rt = ctx->rt;
    JSStackFrame *sf;
    BOOL add_backtrace;

    /* the backtrace is added later if called from a bytecode function */
    sf = rt->current_stack_frame;
    add_backtrace = !rt->in_out_of_memory &&
        (!sf || (JS_GetFunctionBytecode(sf->cur_func) == NULL));
    return JS_ThrowError2(ctx, error_num, fmt, ap, add_backtrace);
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowSyntaxError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_SYNTAX_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowTypeError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_TYPE_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

QJS_INTERNAL int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...)
{
    va_list ap;

    if ((flags & JS_PROP_THROW) ||
        ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
        va_start(ap, fmt);
        JS_ThrowError(ctx, JS_TYPE_ERROR, fmt, ap);
        va_end(ap);
        return -1;
    } else {
        return FALSE;
    }
}

/* never use it directly */
QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowTypeErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowTypeError(ctx, fmt,
                             JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}

/* never use it directly */
QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowSyntaxError(ctx, fmt,
                             JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}

/* %s is replaced by 'atom'. The macro is used so that gcc can check
    the format string. */

QJS_INTERNAL int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom)
{
    if ((flags & JS_PROP_THROW) ||
        ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
        JS_ThrowTypeErrorAtom(ctx, "'%s' is read-only", atom);
        return -1;
    } else {
        return FALSE;
    }
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowReferenceError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_REFERENCE_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowRangeError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_RANGE_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...)
{
    JSValue val;
    va_list ap;

    va_start(ap, fmt);
    val = JS_ThrowError(ctx, JS_INTERNAL_ERROR, fmt, ap);
    va_end(ap);
    return val;
}

JSValue JS_ThrowOutOfMemory(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->in_out_of_memory) {
        rt->in_out_of_memory = TRUE;
        JS_ThrowInternalError(ctx, "out of memory");
        rt->in_out_of_memory = FALSE;
    }
    return JS_EXCEPTION;
}

QJS_INTERNAL JSValue JS_ThrowStackOverflow(JSContext *ctx)
{
    return JS_ThrowInternalError(ctx, "stack overflow");
}

QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "not an object");
}

QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj)
{
    const char *name;
    if (!JS_IsFunction(ctx, func_obj))
        goto fail;
    name = get_prop_string(ctx, func_obj, JS_ATOM_name);
    if (!name) {
    fail:
        return JS_ThrowTypeError(ctx, "not a constructor");
    }
    JS_ThrowTypeError(ctx, "%s is not a constructor", name);
    JS_FreeCString(ctx, name);
    return JS_EXCEPTION;
}

QJS_INTERNAL JSValue JS_ThrowTypeErrorNotASymbol(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "not a symbol");
}

QJS_INTERNAL JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowReferenceError(ctx, "'%s' is not defined",
                                  JS_AtomGetStr(ctx, buf, sizeof(buf), name));
}

QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowReferenceError(ctx, "%s is not initialized",
                                  name == JS_ATOM_NULL ? "lexical variable" :
                                  JS_AtomGetStr(ctx, buf, sizeof(buf), name));
}

QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx,
                                                    JSFunctionBytecode *b,
                                                    int idx, BOOL is_ref)
{
    JSAtom atom = JS_ATOM_NULL;
    if (is_ref) {
        atom = b->closure_var[idx].var_name;
    } else {
        /* not present if the function is stripped and contains no eval() */
        if (b->vardefs)
            atom = b->vardefs[b->arg_count + idx].var_name;
    }
    return JS_ThrowReferenceErrorUninitialized(ctx, atom);
}

QJS_INTERNAL JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id)
{
    JSRuntime *rt = ctx->rt;
    JSAtom name;
    name = rt->class_array[class_id].class_name;
    return JS_ThrowTypeErrorAtom(ctx, "%s object expected", name);
}

QJS_INTERNAL void JS_ThrowInterrupted(JSContext *ctx)
{
    JS_ThrowInternalError(ctx, "interrupted");
    JS_SetUncatchableException(ctx, TRUE);
}

QJS_INTERNAL no_inline __exception int __js_poll_interrupts(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    ctx->interrupt_counter = JS_INTERRUPT_COUNTER_INIT;
    if (rt->interrupt_handler) {
        if (rt->interrupt_handler(rt, rt->interrupt_opaque)) {
            JS_ThrowInterrupted(ctx);
            return -1;
        }
    }
    return 0;
}



