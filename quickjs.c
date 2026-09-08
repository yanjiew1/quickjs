/*
 * QuickJS Javascript Engine
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <fenv.h>
#include <math.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#endif

#include "builtins/js_builtins.h"
#include "compiler/js_opcode.h"
#include "compiler/js_bc.h"
#include "compiler/js_lexer.h"
#include "compiler/js_parser.h"
#include "compiler/js_codegen.h"
#include "value/js_bigint.h"
#include "value/js_value.h"
#include "value/js_string.h"
#include "value/js_atom.h"
#include "runtime/js_malloc.h"
#include "runtime/js_gc.h"
#include "runtime/js_runtime.h"
#include "object/js_shape.h"
#include "object/js_object.h"
#include "object/js_property.h"
#include "runtime/js_error.h"

/* Structs, macros, and basic types are defined in src/quickjs-internal.h */

/* [js_atom_init moved to src/value/js_atom.c] */

/* [OPCodeFormat and enum OPCodeEnum defined in compiler/js_opcode.h] */

/* [JS_InitAtoms declared in js_atom.h] */
/* [__JS_NewAtomInit declared in js_atom.h] */
/* [JS_FreeAtomStruct declared in quickjs-internal.h] */
/* [free_function_bytecode declared in quickjs-internal.h] */
/* [js_call_c_function declared in quickjs-internal.h] */
/* [js_call_bound_function declared in quickjs-internal.h] */
static JSValue JS_CallInternal(JSContext *ctx, JSValueConst func_obj,
                               JSValueConst this_obj, JSValueConst new_target,
                               int argc, JSValue *argv, int flags);
static JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags);
/* [JS_CallFree declared in quickjs-internal.h] */
/* [JS_InvokeFree declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [JS_EvalObject declared in quickjs-internal.h] */
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
/* [JS_DumpAtoms declared in js_atom.h] */
/* [JS_DumpString declared in js_atom.h] */
/* [fwd decl moved to header] */
/* [fwd decl moved to header] */
/* [fwd decl moved to header] */
/* [fwd decl moved to header] */
/* [fwd decl moved to header] */
/* [fwd decl moved to header] */
/* [JS_DumpShapes declared in quickjs-internal.h] */
/* [fwd decl moved to header] */
/* [decl moved to header] */
/* [static void js_array_finalizer(JSRuntime *rt, JSValue val); moved to header] */
/* [static void js_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func); moved to header] */
/* [static void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val); moved to header] */
/* [static void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func); moved to header] */
/* [static void js_object_data_finalizer(JSRuntime *rt, JSValue val); moved to header] */
/* [static void js_object_data_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func); moved to header] */
/* [static void js_c_function_finalizer(JSRuntime *rt, JSValue val); moved to header] */
/* [static void js_c_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func); moved to header] */
/* [js_bytecode_function_finalizer/mark declared in js_builtins.h] */
/* [static void js_bound_function_finalizer(JSRuntime *rt, JSValue val); moved to header] */
/* [static void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func); moved to header] */
/* [static void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val); moved to header] */
/* [static void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func); moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [js_iterator_*_finalizer/mark declared in js_builtins.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [static void js_generator_finalizer(JSRuntime *rt, JSValue obj); moved to header] */
/* [static void js_generator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func); moved to header] */
/* [static void js_global_object_finalizer(JSRuntime *rt, JSValue obj); moved to header] */
/* [static void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func); moved to header] */
/* [js_promise_*_finalizer/mark moved to js_builtin_promise.c] */

/* [HINT defines moved to quickjs-internal.h] */
/* [decl moved to header] */
/* [JS_ToStringFree declared in quickjs-internal.h] */
/* [JS_ToBoolFree declared in quickjs-internal.h] */
/* [JS_ToInt32Free declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [js_new_string8_len declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [gc_decref declared in runtime/js_gc.h] */
/* [JS_NewClass1 declared in runtime/js_runtime.h] */

/* [decl moved to header] */
/* [fwd decl moved to header] */
/* [js_same_value declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [add_property declared in object/js_object.h] */
/* [free_property declared in object/js_object.h] */
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
/* [decl moved to header] */

/* [decl moved to header] */
/* [JS_CreateProperty declared in object/js_property.h] */
/* [js_string_memcmp declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [js_create_var_ref declared in quickjs-internal.h] */
static JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);
/* [__async_func_free declared in quickjs-internal.h] */
static void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
/* [js_generator_function_call declared in quickjs-internal.h] */
/* [js_async_function_resolve_finalizer/mark declared in js_builtins.h] */
static JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                               const char *input, size_t input_len,
                               const char *filename, int flags, int scope_idx);
/* [js_free_module_def declared in quickjs-internal.h] */
/* [js_mark_module_def declared in quickjs-internal.h] */
static JSValue js_import_meta(JSContext *ctx);
static JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
/* [Promise functions declared in js_builtins.h] */
/* [fwd decl moved to header] */
/* [decl moved to header] */
/* [JS_ToNumber inline in quickjs-internal.h] */
/* [decl moved to header] */
/* [JS_NumberIsInteger declared in quickjs-internal.h] */
/* [fwd decl moved to header] */
/* [JS_ToNumberFree declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [JS_AddIntrinsicBasicObjects declared in js_builtins.h] */
/* [js_free_shape declared in quickjs-internal.h] */
/* [js_free_shape_null inline in quickjs-internal.h] */
/* [js_shape_prepare_update declared in object/js_shape.h] */
/* [init_shape_hash declared in quickjs-internal.h] */
/* [js_get_length32 declared in quickjs-internal.h] */
/* [js_get_length64 declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [decl in js_builtins.h] */
/* [static void js_c_function_data_finalizer(JSRuntime *rt, JSValue val); moved to header] */
/* [static void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func); moved to header] */
/* [js_c_function_data_call declared in quickjs-internal.h] */
/* [js_symbol_to_atom declared in js_atom.h] */
/* [add_gc_object and remove_gc_object declared in quickjs-internal.h] */
/* [js_instantiate_prototype declared in quickjs-internal.h] */
/* [js_module_ns_autoinit declared in quickjs-internal.h] */
/* [JS_InstantiateFunctionListItem2 declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [JS_RunGCInternal declared in runtime/js_gc.h] */
/* [decl moved to header] */
/* [js_string_find_invalid_codepoint declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl moved to header] */
/* [js_error_toString in js_builtin_core.c] */
/* [js_global_object_find_uninitialized_var declared in quickjs-internal.h] */
/* [decl moved to header] */


/* [js_arguments_exotic_methods declared in quickjs-internal.h] */
/* [decl moved to header] */
/* [decl in js_builtins.h] */
/* [js_module_ns_exotic_methods declared in quickjs-internal.h] */
/* [JSRuntime and JSContext lifecycle moved to src/runtime/js_runtime.c] */

/* Shape support */

/* [get_shape_size moved to quickjs-internal.h] */

/* [get_shape_prop moved to quickjs-internal.h] */

/* [Shape support moved to src/object/js_shape.c] */

/* [Object allocation & creation moved to src/object/js_object.c] */

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

/* [Array & class finalizers moved to src/object/js_object.c] */

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

/* [Object prototype & finalizers moved to src/object/js_object.c] */

/* [Property access, descriptors & operators moved to src/object/js_property.c] */

static int js_arguments_define_own_property(JSContext *ctx,
                                            JSValueConst this_obj,
                                            JSAtom prop, JSValueConst val,
                                            JSValueConst getter, JSValueConst setter, int flags)
{
    JSObject *p;
    uint32_t idx;
    p = JS_VALUE_GET_OBJ(this_obj);
    /* convert to normal array when redefining an existing numeric field */
    if (p->fast_array && JS_AtomIsArrayIndex(ctx, &idx, prop) &&
        idx < p->u.array.count) {
        if (convert_fast_array_to_array(ctx, p))
            return -1;
    }
    /* run the default define own property */
    return JS_DefineProperty(ctx, this_obj, prop, val, getter, setter,
                             flags | JS_PROP_NO_EXOTIC);
}

const JSClassExoticMethods js_arguments_exotic_methods = {
    .define_own_property = js_arguments_define_own_property,
};

static JSValue js_build_arguments(JSContext *ctx, int argc, JSValueConst *argv)
{
    JSValue val, *tab;
    JSProperty props[3];
    JSObject *p;
    int i;

    props[0].u.value = JS_NewInt32(ctx, argc); /* length */
    props[1].u.value = JS_DupValue(ctx, ctx->array_proto_values); /* Symbol.iterator */
    props[2].u.getset.getter = JS_VALUE_GET_OBJ(JS_DupValue(ctx, ctx->throw_type_error)); /* callee */
    props[2].u.getset.setter = JS_VALUE_GET_OBJ(JS_DupValue(ctx, ctx->throw_type_error)); /* callee */
    
    val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->arguments_shape),
                                JS_CLASS_ARGUMENTS, props);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_OBJ(val);

    /* initialize the fast array part */
    tab = NULL;
    if (argc > 0) {
        tab = js_malloc(ctx, sizeof(tab[0]) * argc);
        if (!tab)
            goto fail;
        for(i = 0; i < argc; i++) {
            tab[i] = JS_DupValue(ctx, argv[i]);
        }
    }
    p->u.array.u.values = tab;
    p->u.array.count = argc;
    return val;
 fail:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000

void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSVarRef **var_refs = p->u.array.u.var_refs;
    int i;
    for(i = 0; i < p->u.array.count; i++)
        free_var_ref(rt, var_refs[i]);
    js_free_rt(rt, var_refs);
}

void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val,
                                     JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSVarRef **var_refs = p->u.array.u.var_refs;
    int i;
    
    for(i = 0; i < p->u.array.count; i++)
        mark_func(rt, &var_refs[i]->header);
}

/* legacy arguments object: add references to the function arguments */
static JSValue js_build_mapped_arguments(JSContext *ctx, int argc,
                                         JSValueConst *argv,
                                         JSStackFrame *sf, int arg_count)
{
    JSValue val;
    JSProperty props[3];
    JSVarRef **tab, *var_ref;
    JSObject *p;
    int i, j;

    props[0].u.value = JS_NewInt32(ctx, argc); /* length */
    props[1].u.value = JS_DupValue(ctx, ctx->array_proto_values); /* Symbol.iterator */
    props[2].u.value = JS_DupValue(ctx, ctx->rt->current_stack_frame->cur_func); /* callee */
    
    val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mapped_arguments_shape),
                                JS_CLASS_MAPPED_ARGUMENTS, props);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_OBJ(val);

    /* initialize the fast array part */
    tab = NULL;
    if (argc > 0) {
        tab = js_malloc(ctx, sizeof(tab[0]) * argc);
        if (!tab)
            goto fail;
        for(i = 0; i < arg_count; i++) {
            var_ref = get_var_ref(ctx, sf, i, TRUE);
            if (!var_ref)
                goto fail1;
            tab[i] = var_ref;
        }
        for(i = arg_count; i < argc; i++) {
            var_ref = js_create_var_ref(ctx, FALSE);
            if (!var_ref) {
            fail1:
                for(j = 0; j < i; j++)
                    free_var_ref(ctx->rt, tab[j]);
                js_free(ctx, tab);
                goto fail;
            }
            var_ref->value = JS_DupValue(ctx, argv[i]);
            tab[i] = var_ref;
        }
    }
    p->u.array.u.var_refs = tab;
    p->u.array.count = argc;
    return val;
 fail:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

static JSValue build_for_in_iterator(JSContext *ctx, JSValue obj)
{
    JSObject *p, *p1;
    JSPropertyEnum *tab_atom;
    int i;
    JSValue enum_obj;
    JSForInIterator *it;
    uint32_t tag, tab_atom_count;

    tag = JS_VALUE_GET_TAG(obj);
    if (tag != JS_TAG_OBJECT && tag != JS_TAG_NULL && tag != JS_TAG_UNDEFINED) {
        obj = JS_ToObjectFree(ctx, obj);
    }

    it = js_malloc(ctx, sizeof(*it));
    if (!it) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    enum_obj = JS_NewObjectProtoClass(ctx, JS_NULL, JS_CLASS_FOR_IN_ITERATOR);
    if (JS_IsException(enum_obj)) {
        js_free(ctx, it);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    it->is_array = FALSE;
    it->obj = obj;
    it->idx = 0;
    it->tab_atom = NULL;
    it->atom_count = 0;
    it->in_prototype_chain = FALSE;
    p1 = JS_VALUE_GET_OBJ(enum_obj);
    p1->u.for_in_iterator = it;

    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED)
        return enum_obj;

    p = JS_VALUE_GET_OBJ(obj);
    if (p->fast_array) {
        JSShape *sh;
        JSShapeProperty *prs;
        /* check that there are no enumerable normal fields */
        sh = p->shape;
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->flags & JS_PROP_ENUMERABLE)
                goto normal_case;
        }
        /* for fast arrays, we only store the number of elements */
        it->is_array = TRUE;
        it->atom_count = p->u.array.count;
    } else {
    normal_case:
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count, p,
                                           JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
            JS_FreeValue(ctx, enum_obj);
            return JS_EXCEPTION;
        }
        it->tab_atom = tab_atom;
        it->atom_count = tab_atom_count;
    }
    return enum_obj;
}

/* obj -> enum_obj */
static __exception int js_for_in_start(JSContext *ctx, JSValue *sp)
{
    sp[-1] = build_for_in_iterator(ctx, sp[-1]);
    if (JS_IsException(sp[-1]))
        return -1;
    return 0;
}

/* return -1 if exception, 0 if slow case, 1 if the enumeration is finished */
static __exception int js_for_in_prepare_prototype_chain_enum(JSContext *ctx,
                                                              JSValueConst enum_obj)
{
    JSObject *p;
    JSForInIterator *it;
    JSPropertyEnum *tab_atom;
    uint32_t tab_atom_count, i;
    JSValue obj1;

    p = JS_VALUE_GET_OBJ(enum_obj);
    it = p->u.for_in_iterator;

    /* check if there are enumerable properties in the prototype chain (fast path) */
    obj1 = JS_DupValue(ctx, it->obj);
    for(;;) {
        obj1 = JS_GetPrototypeFree(ctx, obj1);
        if (JS_IsNull(obj1))
            break;
        if (JS_IsException(obj1))
            goto fail;
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                           JS_VALUE_GET_OBJ(obj1),
                                           JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
            JS_FreeValue(ctx, obj1);
            goto fail;
        }
        JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
        if (tab_atom_count != 0) {
            JS_FreeValue(ctx, obj1);
            goto slow_path;
        }
        /* must check for timeout to avoid infinite loop */
        if (js_poll_interrupts(ctx)) {
            JS_FreeValue(ctx, obj1);
            goto fail;
        }
    }
    JS_FreeValue(ctx, obj1);
    return 1;

 slow_path:
    /* add the visited properties, even if they are not enumerable */
    if (it->is_array) {
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                           JS_VALUE_GET_OBJ(it->obj),
                                           JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
            goto fail;
        }
        it->is_array = FALSE;
        it->tab_atom = tab_atom;
        it->atom_count = tab_atom_count;
    }

    for(i = 0; i < it->atom_count; i++) {
        if (JS_DefinePropertyValue(ctx, enum_obj, it->tab_atom[i].atom, JS_NULL, JS_PROP_ENUMERABLE) < 0)
            goto fail;
    }
    return 0;
 fail:
    return -1;
}

/* enum_obj -> enum_obj value done */
static __exception int js_for_in_next(JSContext *ctx, JSValue *sp)
{
    JSValueConst enum_obj;
    JSObject *p;
    JSAtom prop;
    JSForInIterator *it;
    JSPropertyEnum *tab_atom;
    uint32_t tab_atom_count;
    int ret;

    enum_obj = sp[-1];
    /* fail safe */
    if (JS_VALUE_GET_TAG(enum_obj) != JS_TAG_OBJECT)
        goto done;
    p = JS_VALUE_GET_OBJ(enum_obj);
    if (p->class_id != JS_CLASS_FOR_IN_ITERATOR)
        goto done;
    it = p->u.for_in_iterator;

    for(;;) {
        if (it->idx >= it->atom_count) {
            if (JS_IsNull(it->obj) || JS_IsUndefined(it->obj))
                goto done; /* not an object */
            /* no more property in the current object: look in the prototype */
            if (!it->in_prototype_chain) {
                ret = js_for_in_prepare_prototype_chain_enum(ctx, enum_obj);
                if (ret < 0)
                    return -1;
                if (ret)
                    goto done;
                it->in_prototype_chain = TRUE;
            }
            it->obj = JS_GetPrototypeFree(ctx, it->obj);
            if (JS_IsException(it->obj))
                return -1;
            if (JS_IsNull(it->obj))
                goto done; /* no more prototype */

            /* must check for timeout to avoid infinite loop */
            if (js_poll_interrupts(ctx))
                return -1;

            if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                               JS_VALUE_GET_OBJ(it->obj),
                                               JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
                return -1;
            }
            JS_FreePropertyEnum(ctx, it->tab_atom, it->atom_count);
            it->tab_atom = tab_atom;
            it->atom_count = tab_atom_count;
            it->idx = 0;
        } else {
            if (it->is_array) {
                prop = __JS_AtomFromUInt32(it->idx);
                it->idx++;
            } else {
                BOOL is_enumerable;
                prop = it->tab_atom[it->idx].atom;
                is_enumerable = it->tab_atom[it->idx].is_enumerable;
                it->idx++;
                if (it->in_prototype_chain) {
                    /* slow case: we are in the prototype chain */
                    ret = JS_GetOwnPropertyInternal(ctx, NULL, JS_VALUE_GET_OBJ(enum_obj), prop);
                    if (ret < 0)
                        return ret;
                    if (ret)
                        continue; /* already visited */
                    /* add to the visited property list */
                    if (JS_DefinePropertyValue(ctx, enum_obj, prop, JS_NULL,
                                               JS_PROP_ENUMERABLE) < 0)
                        return -1;
                }
                if (!is_enumerable)
                    continue;
            }
            /* check if the property was deleted */
            ret = JS_GetOwnPropertyInternal(ctx, NULL, JS_VALUE_GET_OBJ(it->obj), prop);
            if (ret < 0)
                return ret;
            if (ret)
                break;
        }
    }
    /* return the property */
    sp[0] = JS_AtomToValue(ctx, prop);
    sp[1] = JS_FALSE;
    return 0;
 done:
    /* return the end */
    sp[0] = JS_UNDEFINED;
    sp[1] = JS_TRUE;
    return 0;
}

JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                               JSValueConst method)
{
    JSValue enum_obj;

    enum_obj = JS_Call(ctx, method, obj, 0, NULL);
    if (JS_IsException(enum_obj))
        return enum_obj;
    if (!JS_IsObject(enum_obj)) {
        JS_FreeValue(ctx, enum_obj);
        return JS_ThrowTypeErrorNotAnObject(ctx);
    }
    return enum_obj;
}

JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async)
{
    JSValue method, ret, sync_iter;

    if (is_async) {
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_asyncIterator);
        if (JS_IsException(method))
            return method;
        if (JS_IsUndefined(method) || JS_IsNull(method)) {
            method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
            if (JS_IsException(method))
                return method;
            sync_iter = JS_GetIterator2(ctx, obj, method);
            JS_FreeValue(ctx, method);
            if (JS_IsException(sync_iter))
                return sync_iter;
            ret = JS_CreateAsyncFromSyncIterator(ctx, sync_iter);
            JS_FreeValue(ctx, sync_iter);
            return ret;
        }
    } else {
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
        if (JS_IsException(method))
            return method;
    }
    if (!JS_IsFunction(ctx, method)) {
        JS_FreeValue(ctx, method);
        return JS_ThrowTypeError(ctx, "value is not iterable");
    }
    ret = JS_GetIterator2(ctx, obj, method);
    JS_FreeValue(ctx, method);
    return ret;
}

/* return *pdone = 2 if the iterator object is not parsed */
JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                                JSValueConst method,
                                int argc, JSValueConst *argv, int *pdone)
{
    JSValue obj;

    /* fast path for the built-in iterators (avoid creating the
       intermediate result object) */
    if (JS_IsObject(method)) {
        JSObject *p = JS_VALUE_GET_OBJ(method);
        if (p->class_id == JS_CLASS_C_FUNCTION &&
            p->u.cfunc.cproto == JS_CFUNC_iterator_next) {
            JSCFunctionType func;
            JSValueConst args[1];

            /* in case the function expects one argument */
            if (argc == 0) {
                args[0] = JS_UNDEFINED;
                argv = args;
            }
            func = p->u.cfunc.c_function;
            return func.iterator_next(ctx, enum_obj, argc, argv,
                                      pdone, p->u.cfunc.magic);
        }
    }
    obj = JS_Call(ctx, method, enum_obj, argc, argv);
    if (JS_IsException(obj))
        goto fail;
    if (!JS_IsObject(obj)) {
        JS_FreeValue(ctx, obj);
        JS_ThrowTypeError(ctx, "iterator must return an object");
        goto fail;
    }
    *pdone = 2;
    return obj;
 fail:
    *pdone = FALSE;
    return JS_EXCEPTION;
}

/* Note: always return JS_UNDEFINED when *pdone = TRUE. */
JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                               JSValueConst method,
                               int argc, JSValueConst *argv, BOOL *pdone)
{
    JSValue obj, value, done_val;
    int done;

    obj = JS_IteratorNext2(ctx, enum_obj, method, argc, argv, &done);
    if (JS_IsException(obj))
        goto fail;
    if (likely(done == 0)) {
        *pdone = FALSE;
        return obj;
    } else if (done != 2) {
        JS_FreeValue(ctx, obj);
        *pdone = TRUE;
        return JS_UNDEFINED;
    } else {
        done_val = JS_GetProperty(ctx, obj, JS_ATOM_done);
        if (JS_IsException(done_val))
            goto fail;
        *pdone = JS_ToBoolFree(ctx, done_val);
        value = JS_UNDEFINED;
        if (!*pdone) {
            value = JS_GetProperty(ctx, obj, JS_ATOM_value);
        }
        JS_FreeValue(ctx, obj);
        return value;
    }
 fail:
    JS_FreeValue(ctx, obj);
    *pdone = FALSE;
    return JS_EXCEPTION;
}

/* return < 0 in case of exception */
int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                            BOOL is_exception_pending)
{
    JSValue method, ret, ex_obj;
    int res;

    if (is_exception_pending) {
        ex_obj = ctx->rt->current_exception;
        ctx->rt->current_exception = JS_UNINITIALIZED;
        res = -1;
    } else {
        ex_obj = JS_UNDEFINED;
        res = 0;
    }
    method = JS_GetProperty(ctx, enum_obj, JS_ATOM_return);
    if (JS_IsException(method)) {
        res = -1;
        goto done;
    }
    if (JS_IsUndefined(method) || JS_IsNull(method)) {
        goto done;
    }
    ret = JS_CallFree(ctx, method, enum_obj, 0, NULL);
    if (!is_exception_pending) {
        if (JS_IsException(ret)) {
            res = -1;
        } else if (!JS_IsObject(ret)) {
            JS_ThrowTypeErrorNotAnObject(ctx);
            res = -1;
        }
    }
    JS_FreeValue(ctx, ret);
 done:
    if (is_exception_pending) {
        JS_Throw(ctx, ex_obj);
    }
    return res;
}

/* obj -> enum_rec (3 slots) */
static __exception int js_for_of_start(JSContext *ctx, JSValue *sp,
                                       BOOL is_async)
{
    JSValue op1, obj, method;
    op1 = sp[-1];
    obj = JS_GetIterator(ctx, op1, is_async);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, op1);
    sp[-1] = obj;
    method = JS_GetProperty(ctx, obj, JS_ATOM_next);
    if (JS_IsException(method))
        return -1;
    sp[0] = method;
    return 0;
}

/* enum_rec [objs] -> enum_rec [objs] value done. There are 'offset'
   objs. If 'done' is true or in case of exception, 'enum_rec' is set
   to undefined. If 'done' is true, 'value' is always set to
   undefined. */
static __exception int js_for_of_next(JSContext *ctx, JSValue *sp, int offset)
{
    JSValue value = JS_UNDEFINED;
    int done = 1;

    if (likely(!JS_IsUndefined(sp[offset]))) {
        value = JS_IteratorNext(ctx, sp[offset], sp[offset + 1], 0, NULL, &done);
        if (JS_IsException(value))
            done = -1;
        if (done) {
            /* value is JS_UNDEFINED or JS_EXCEPTION */
            /* replace the iteration object with undefined */
            JS_FreeValue(ctx, sp[offset]);
            sp[offset] = JS_UNDEFINED;
            if (done < 0) {
                return -1;
            } else {
                JS_FreeValue(ctx, value);
                value = JS_UNDEFINED;
            }
        }
    }
    sp[0] = value;
    sp[1] = JS_NewBool(ctx, done);
    return 0;
}

static __exception int js_for_await_of_next(JSContext *ctx, JSValue *sp)
{
    JSValue obj, iter, next;

    sp[-1] = JS_UNDEFINED; /* disable the catch offset so that
                              exceptions do not close the iterator */
    iter = sp[-3];
    next = sp[-2];
    obj = JS_Call(ctx, next, iter, 0, NULL);
    if (JS_IsException(obj))
        return -1;
    sp[0] = obj;
    return 0;
}

JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                    BOOL *pdone)
{
    JSValue done_val, value;
    BOOL done;
    done_val = JS_GetProperty(ctx, obj, JS_ATOM_done);
    if (JS_IsException(done_val))
        goto fail;
    done = JS_ToBoolFree(ctx, done_val);
    value = JS_GetProperty(ctx, obj, JS_ATOM_value);
    if (JS_IsException(value))
        goto fail;
    *pdone = done;
    return value;
 fail:
    *pdone = FALSE;
    return JS_EXCEPTION;
}

static __exception int js_iterator_get_value_done(JSContext *ctx, JSValue *sp)
{
    JSValue obj, value;
    BOOL done;
    obj = sp[-1];
    if (!JS_IsObject(obj)) {
        JS_ThrowTypeError(ctx, "iterator must return an object");
        return -1;
    }
    value = JS_IteratorGetCompleteValue(ctx, obj, &done);
    if (JS_IsException(value))
        return -1;
    JS_FreeValue(ctx, obj);
    /* put again the catch offset so that exceptions close the
       iterator */
    sp[-2] = JS_NewCatchOffset(ctx, 0); 
    sp[-1] = value;
    sp[0] = JS_NewBool(ctx, done);
    return 0;
}

JSValue js_create_iterator_result(JSContext *ctx,
                                  JSValue val,
                                  BOOL done)
{
    JSValue obj;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, val);
        return obj;
    }
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_value,
                               val, JS_PROP_C_W_E) < 0) {
        goto fail;
    }
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_done,
                               JS_NewBool(ctx, done), JS_PROP_C_W_E) < 0) {
    fail:
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

/* [js_array_iterator_next declared in js_builtins.h] */

/* [decl moved to header] */

/* [js_get_fast_array moved to src/object/js_object.c] */

static __exception int js_append_enumerate(JSContext *ctx, JSValue *sp)
{
    JSValue iterator, enumobj, method, value;
    int is_array_iterator;
    JSValue *arrp;
    uint32_t i, count32, pos;
    JSCFunctionType ft;

    if (JS_VALUE_GET_TAG(sp[-2]) != JS_TAG_INT) {
        JS_ThrowInternalError(ctx, "invalid index for append");
        return -1;
    }

    pos = JS_VALUE_GET_INT(sp[-2]);

    /* XXX: further optimisations:
       - use ctx->array_proto_values?
       - check if array_iterator_prototype next method is built-in and
         avoid constructing actual iterator object?
       - build this into js_for_of_start and use in all `for (x of o)` loops
     */
    iterator = JS_GetProperty(ctx, sp[-1], JS_ATOM_Symbol_iterator);
    if (JS_IsException(iterator))
        return -1;
    ft.generic_magic = js_create_array_iterator;
    is_array_iterator = JS_IsCFunction(ctx, iterator, ft.generic,
                                       JS_ITERATOR_KIND_VALUE);
    JS_FreeValue(ctx, iterator);

    enumobj = JS_GetIterator(ctx, sp[-1], FALSE);
    if (JS_IsException(enumobj))
        return -1;
    method = JS_GetProperty(ctx, enumobj, JS_ATOM_next);
    if (JS_IsException(method)) {
        JS_FreeValue(ctx, enumobj);
        return -1;
    }

    ft.iterator_next = js_array_iterator_next;
    if (is_array_iterator
    &&  JS_IsCFunction(ctx, method, ft.generic, 0)
    &&  js_get_fast_array(ctx, sp[-1], &arrp, &count32)) {
        uint32_t len;
        if (js_get_length32(ctx, &len, sp[-1]))
            goto exception;
        /* if len > count32, the elements >= count32 might be read in
           the prototypes and might have side effects */
        if (len != count32)
            goto general_case;
        /* Handle fast arrays explicitly */
        for (i = 0; i < count32; i++) {
            if (JS_DefinePropertyValueUint32(ctx, sp[-3], pos++,
                                             JS_DupValue(ctx, arrp[i]), JS_PROP_C_W_E) < 0)
                goto exception;
        }
    } else {
    general_case:
        for (;;) {
            BOOL done;
            value = JS_IteratorNext(ctx, enumobj, method, 0, NULL, &done);
            if (JS_IsException(value))
                goto exception;
            if (done) {
                /* value is JS_UNDEFINED */
                break;
            }
            if (JS_DefinePropertyValueUint32(ctx, sp[-3], pos++, value, JS_PROP_C_W_E) < 0)
                goto exception;
        }
    }
    /* Note: could raise an error if too many elements */
    sp[-2] = JS_NewInt32(ctx, pos);
    JS_FreeValue(ctx, enumobj);
    JS_FreeValue(ctx, method);
    return 0;

exception:
    JS_IteratorClose(ctx, enumobj, TRUE);
    JS_FreeValue(ctx, enumobj);
    JS_FreeValue(ctx, method);
    return -1;
}

__exception int JS_CopyDataProperties(JSContext *ctx,
                                      JSValueConst target,
                                      JSValueConst source,
                                      JSValueConst excluded,
                                      BOOL setprop)
{
    JSPropertyEnum *tab_atom;
    JSValue val;
    uint32_t i, tab_atom_count;
    JSObject *p;
    JSObject *pexcl = NULL;
    int ret, gpn_flags;
    JSPropertyDescriptor desc;
    BOOL is_enumerable;

    if (JS_VALUE_GET_TAG(source) != JS_TAG_OBJECT)
        return 0;

    if (JS_VALUE_GET_TAG(excluded) == JS_TAG_OBJECT)
        pexcl = JS_VALUE_GET_OBJ(excluded);

    p = JS_VALUE_GET_OBJ(source);

    gpn_flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_ENUM_ONLY;
    if (p->is_exotic) {
        const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
        /* cannot use JS_GPN_ENUM_ONLY with e.g. proxies because it
           introduces a visible change */
        if (em && em->get_own_property_names) {
            gpn_flags &= ~JS_GPN_ENUM_ONLY;
        }
    }
    if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count, p,
                                       gpn_flags))
        return -1;

    for (i = 0; i < tab_atom_count; i++) {
        if (pexcl) {
            ret = JS_GetOwnPropertyInternal(ctx, NULL, pexcl, tab_atom[i].atom);
            if (ret) {
                if (ret < 0)
                    goto exception;
                continue;
            }
        }
        if (!(gpn_flags & JS_GPN_ENUM_ONLY)) {
            /* test if the property is enumerable */
            ret = JS_GetOwnPropertyInternal(ctx, &desc, p, tab_atom[i].atom);
            if (ret < 0)
                goto exception;
            if (!ret)
                continue;
            is_enumerable = (desc.flags & JS_PROP_ENUMERABLE) != 0;
            js_free_desc(ctx, &desc);
            if (!is_enumerable)
                continue;
        }
        val = JS_GetProperty(ctx, source, tab_atom[i].atom);
        if (JS_IsException(val))
            goto exception;
        if (setprop)
            ret = JS_SetProperty(ctx, target, tab_atom[i].atom, val);
        else
            ret = JS_DefinePropertyValue(ctx, target, tab_atom[i].atom, val,
                                         JS_PROP_C_W_E);
        if (ret < 0)
            goto exception;
    }
    JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
    return 0;
 exception:
    JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
    return -1;
}

/* [JS_GetActiveFunction moved to quickjs-internal.h] */

JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical)
{
    JSVarRef *var_ref;
    var_ref = js_malloc(ctx, sizeof(JSVarRef));
    if (!var_ref)
        return NULL;
    js_rc(var_ref)->ref_count = 1;
    if (is_lexical)
        var_ref->value = JS_UNINITIALIZED;
    else
        var_ref->value = JS_UNDEFINED;
    var_ref->pvalue = &var_ref->value;
    var_ref->is_detached = TRUE;
    var_ref->is_lexical = FALSE;
    var_ref->is_const = FALSE;
    add_gc_object(ctx->rt, &var_ref->header, JS_GC_OBJ_TYPE_VAR_REF);
    return var_ref;
}

static JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg)
{
    JSObject *p;
    JSFunctionBytecode *b;
    JSVarRef *var_ref;
    JSValue *pvalue;
    int var_ref_idx;
    JSBytecodeVarDef *vd;
    
    p = JS_VALUE_GET_OBJ(sf->cur_func);
    b = p->u.func.function_bytecode;
    
    if (is_arg) {
        vd = &b->vardefs[var_idx];
        pvalue = &sf->arg_buf[var_idx];
    } else {
        vd = &b->vardefs[b->arg_count + var_idx];
        pvalue = &sf->var_buf[var_idx];
    }
    assert(vd->is_captured);
    var_ref_idx = vd->var_ref_idx;
    assert(var_ref_idx < b->var_ref_count);
    var_ref = sf->var_refs[var_ref_idx];
    if (var_ref) {
        /* reference to the already created local variable */
        assert(var_ref->pvalue == pvalue);
        js_rc(var_ref)->ref_count++;
        return var_ref;
    }

    /* create a new one */
    var_ref = js_malloc(ctx, sizeof(JSVarRef));
    if (!var_ref)
        return NULL;
    js_rc(var_ref)->ref_count = 1;
    add_gc_object(ctx->rt, &var_ref->header, JS_GC_OBJ_TYPE_VAR_REF);
    var_ref->is_detached = FALSE;
    var_ref->is_lexical = FALSE;
    var_ref->is_const = FALSE;
    var_ref->var_ref_idx = var_ref_idx;
    var_ref->stack_frame = sf;
    sf->var_refs[var_ref_idx] = var_ref;
    if (sf->js_mode & JS_MODE_ASYNC) {
        JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
        /* The stack frame is detached and may be destroyed at any
           time so its reference count must be increased. Calling
           close_var_refs() when destroying the stack frame is not
           possible because it would change the graph between the GC
           objects. Another solution could be to temporarily detach
           the JSVarRef of async functions during the GC. It would
           have the advantage of allowing the release of unused stack
           frames in a cycle. */
        js_rc(async_func)->ref_count++;
    }
    var_ref->pvalue = pvalue;
    return var_ref;
}

void js_global_object_finalizer(JSRuntime *rt, JSValue obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    JS_FreeValueRT(rt, p->u.global_object.uninitialized_vars);
}

void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_MarkValue(rt, p->u.global_object.uninitialized_vars, mark_func);
}

static JSVarRef *js_global_object_get_uninitialized_var(JSContext *ctx, JSObject *p1, 
                                                        JSAtom atom)
{
    JSObject *p = JS_VALUE_GET_OBJ(p1->u.global_object.uninitialized_vars);
    JSShapeProperty *prs;
    JSProperty *pr;
    JSVarRef *var_ref;
    
    prs = find_own_property(&pr, p, atom);
    if (prs) {
        assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
        var_ref = pr->u.var_ref;
        js_rc(var_ref)->ref_count++;
        return var_ref;
    }

    var_ref = js_create_var_ref(ctx, TRUE);
    if (!var_ref)
        return NULL;
    pr = add_property(ctx, p, atom, JS_PROP_C_W_E | JS_PROP_VARREF);
    if (unlikely(!pr)) {
        free_var_ref(ctx->rt, var_ref);
        return NULL;
    }
    pr->u.var_ref = var_ref;
    js_rc(var_ref)->ref_count++;
    return var_ref;
}

/* return a new variable reference. Get it from the uninitialized
   variables if it is present. Return NULL in case of memory error. */
JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                         JSAtom atom, BOOL is_lexical)
{
    JSObject *p1;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSVarRef *var_ref;
    
    p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
    prs = find_own_property(&pr, p1, atom);
    if (prs) {
        assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
        var_ref = pr->u.var_ref;
        js_rc(var_ref)->ref_count++;
        delete_property(ctx, p1, atom);
        if (!is_lexical)
            var_ref->value = JS_UNDEFINED;
    } else {
        var_ref = js_create_var_ref(ctx, is_lexical);
        if (!var_ref)
            return NULL;
    }
    return var_ref;
}

static JSVarRef *js_closure_define_global_var(JSContext *ctx, JSClosureVar *cv,
                                              BOOL is_direct_or_indirect_eval)
{
    JSObject *p, *p1;
    JSShapeProperty *prs;
    int flags;
    JSProperty *pr;
    JSVarRef *var_ref;
    
    if (cv->is_lexical) {
        p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
        flags = JS_PROP_ENUMERABLE | JS_PROP_CONFIGURABLE;
        if (!cv->is_const)
            flags |= JS_PROP_WRITABLE;

        prs = find_own_property(&pr, p, cv->var_name);
        if (prs) {
            assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
            var_ref = pr->u.var_ref;
            js_rc(var_ref)->ref_count++;
            return var_ref;
        }

        /* if there is a corresponding global variable, reuse its
           reference and create a new one for the global variable */
        p1 = JS_VALUE_GET_OBJ(ctx->global_obj);
        prs = find_own_property(&pr, p1, cv->var_name);
        if (prs && (prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            JSVarRef *var_ref1;
            var_ref1 = js_create_var_ref(ctx, FALSE);
            if (!var_ref1)
                return NULL;
            var_ref = pr->u.var_ref;
            var_ref1->value = var_ref->value;
            var_ref->value = JS_UNINITIALIZED;
            pr->u.var_ref = var_ref1;
            goto add_var_ref;
        }
    } else {
        p = JS_VALUE_GET_OBJ(ctx->global_obj);
        flags = JS_PROP_ENUMERABLE | JS_PROP_WRITABLE;
        if (is_direct_or_indirect_eval)
            flags |= JS_PROP_CONFIGURABLE;

    retry:
        prs = find_own_property(&pr, p, cv->var_name);
        if (prs) {
            if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT)) {
                if (JS_AutoInitProperty(ctx, p, cv->var_name, pr, prs))
                    return NULL;
                goto retry;
            } else if ((prs->flags & JS_PROP_TMASK) != JS_PROP_VARREF) {
                var_ref = js_global_object_get_uninitialized_var(ctx, p, cv->var_name);
                if (!var_ref)
                    return NULL;
            } else {
                var_ref = pr->u.var_ref;
                js_rc(var_ref)->ref_count++;
            }
            if (cv->var_kind == JS_VAR_GLOBAL_FUNCTION_DECL &&
                (prs->flags & JS_PROP_CONFIGURABLE)) {
                /* update the property flags if possible when
                   declaring a global function */
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    free_property(ctx->rt, pr, prs->flags);
                    prs->flags = flags | JS_PROP_VARREF;
                    pr->u.var_ref = var_ref;
                    js_rc(var_ref)->ref_count++;
                } else {
                    assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
                    prs->flags = (prs->flags & ~JS_PROP_C_W_E) | flags;
                }
                var_ref->is_const = FALSE;
            }
            return var_ref;
        }
        
        if (!p->extensible) {
            return js_global_object_get_uninitialized_var(ctx, p, cv->var_name);
        }
    }
    
    /* if there is a corresponding uninitialized variable, use it */
    p1 = JS_VALUE_GET_OBJ(ctx->global_obj);
    var_ref = js_global_object_find_uninitialized_var(ctx, p1, cv->var_name, cv->is_lexical);
    if (!var_ref)
        return NULL;
 add_var_ref:
    if (cv->is_lexical) {
        var_ref->is_lexical = TRUE;
        var_ref->is_const = cv->is_const;
    }

    pr = add_property(ctx, p, cv->var_name, flags | JS_PROP_VARREF);
    if (unlikely(!pr)) {
        free_var_ref(ctx->rt, var_ref);
        return NULL;
    }
    pr->u.var_ref = var_ref;
    js_rc(var_ref)->ref_count++;
    return var_ref;
}

static JSVarRef *js_closure_global_var(JSContext *ctx, JSClosureVar *cv)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSVarRef *var_ref;
    
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, cv->var_name);
    if (prs) {
        assert((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
        var_ref = pr->u.var_ref;
        js_rc(var_ref)->ref_count++;
        return var_ref;
    }
    p = JS_VALUE_GET_OBJ(ctx->global_obj);
 redo:
    prs = find_own_property(&pr, p, cv->var_name);
    if (prs) {
        if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT)) {
            /* Instantiate property and retry */
            if (JS_AutoInitProperty(ctx, p, cv->var_name, pr, prs))
                return NULL;
            goto redo;
        }
        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            var_ref = pr->u.var_ref;
            js_rc(var_ref)->ref_count++;
            return var_ref;
        }
    }
    return js_global_object_get_uninitialized_var(ctx, p, cv->var_name);
}

static JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                           JSFunctionBytecode *b,
                           JSVarRef **cur_var_refs,
                           JSStackFrame *sf,
                           BOOL is_eval, JSModuleDef *m)
{
    JSObject *p;
    JSVarRef **var_refs;
    int i;

    p = JS_VALUE_GET_OBJ(func_obj);
    p->u.func.function_bytecode = b;
    p->u.func.home_object = NULL;
    p->u.func.var_refs = NULL;
    if (b->closure_var_count) {
        var_refs = js_mallocz(ctx, sizeof(var_refs[0]) * b->closure_var_count);
        if (!var_refs)
            goto fail;
        p->u.func.var_refs = var_refs;
        if (is_eval) {
            /* first pass to check the global variable definitions */
            for(i = 0; i < b->closure_var_count; i++) {
                JSClosureVar *cv = &b->closure_var[i];
                if (cv->closure_type == JS_CLOSURE_GLOBAL_DECL) {
                    int flags;
                    flags = 0;
                    if (cv->is_lexical)
                        flags |= DEFINE_GLOBAL_LEX_VAR;
                    if (cv->var_kind == JS_VAR_GLOBAL_FUNCTION_DECL)
                        flags |= DEFINE_GLOBAL_FUNC_VAR;
                    if (JS_CheckDefineGlobalVar(ctx, cv->var_name, flags))
                        goto fail;
                }
            }
        }
        for(i = 0; i < b->closure_var_count; i++) {
            JSClosureVar *cv = &b->closure_var[i];
            JSVarRef *var_ref;
            switch(cv->closure_type) {
            case JS_CLOSURE_MODULE_IMPORT:
                /* imported from other modules */
                continue;
            case JS_CLOSURE_MODULE_DECL:
                var_ref = js_create_var_ref(ctx, cv->is_lexical);
                break;
            case JS_CLOSURE_GLOBAL_DECL:
                var_ref = js_closure_define_global_var(ctx, cv, b->is_direct_or_indirect_eval);
                break;
            case JS_CLOSURE_GLOBAL:
                var_ref = js_closure_global_var(ctx, cv);
                break;
            case JS_CLOSURE_LOCAL:
                /* reuse the existing variable reference if it already exists */
                var_ref = get_var_ref(ctx, sf, cv->var_idx, FALSE);
                break;
            case JS_CLOSURE_ARG:
                /* reuse the existing variable reference if it already exists */
                var_ref = get_var_ref(ctx, sf, cv->var_idx, TRUE);
                break;
            case JS_CLOSURE_REF:
            case JS_CLOSURE_GLOBAL_REF:
                var_ref = cur_var_refs[cv->var_idx];
                js_rc(var_ref)->ref_count++;
                break;
            default:
                abort();
            }
            if (!var_ref)
                goto fail;
            var_refs[i] = var_ref;
        }
    }
    return func_obj;
 fail:
    /* bfunc is freed when func_obj is freed */
    JS_FreeValue(ctx, func_obj);
    return JS_EXCEPTION;
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

const uint16_t func_kind_to_class_id[] = {
    [JS_FUNC_NORMAL] = JS_CLASS_BYTECODE_FUNCTION,
    [JS_FUNC_GENERATOR] = JS_CLASS_GENERATOR_FUNCTION,
    [JS_FUNC_ASYNC] = JS_CLASS_ASYNC_FUNCTION,
    [JS_FUNC_ASYNC_GENERATOR] = JS_CLASS_ASYNC_GENERATOR_FUNCTION,
};

static JSValue js_closure(JSContext *ctx, JSValue bfunc,
                          JSVarRef **cur_var_refs,
                          JSStackFrame *sf, BOOL is_eval)
{
    JSFunctionBytecode *b;
    JSValue func_obj;
    JSAtom name_atom;

    b = JS_VALUE_GET_PTR(bfunc);
    func_obj = JS_NewObjectClass(ctx, func_kind_to_class_id[b->func_kind]);
    if (JS_IsException(func_obj)) {
        JS_FreeValue(ctx, bfunc);
        return JS_EXCEPTION;
    }
    func_obj = js_closure2(ctx, func_obj, b, cur_var_refs, sf, is_eval, NULL);
    if (JS_IsException(func_obj)) {
        /* bfunc has been freed */
        goto fail;
    }
    name_atom = b->func_name;
    if (name_atom == JS_ATOM_NULL)
        name_atom = JS_ATOM_empty_string;
    js_function_set_properties(ctx, func_obj, name_atom,
                               b->defined_arg_count);

    if (b->func_kind & JS_FUNC_GENERATOR) {
        JSValue proto;
        int proto_class_id;
        /* generators have a prototype field which is used as
           prototype for the generator object */
        if (b->func_kind == JS_FUNC_ASYNC_GENERATOR)
            proto_class_id = JS_CLASS_ASYNC_GENERATOR;
        else
            proto_class_id = JS_CLASS_GENERATOR;
        proto = JS_NewObjectProto(ctx, ctx->class_proto[proto_class_id]);
        if (JS_IsException(proto))
            goto fail;
        JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_prototype, proto,
                               JS_PROP_WRITABLE);
    } else if (b->has_prototype) {
        /* add the 'prototype' property: delay instantiation to avoid
           creating cycles for every javascript function. The prototype
           object is created on the fly when first accessed */
        JS_SetConstructorBit(ctx, func_obj, TRUE);
        JS_DefineAutoInitProperty(ctx, func_obj, JS_ATOM_prototype,
                                  JS_AUTOINIT_ID_PROTOTYPE, NULL,
                                  JS_PROP_WRITABLE);
    }
    return func_obj;
 fail:
    /* bfunc is freed when func_obj is freed */
    JS_FreeValue(ctx, func_obj);
    return JS_EXCEPTION;
}

/* [JS_DEFINE_CLASS_HAS_HERITAGE defined in compiler/js_opcode.h] */

static int js_op_define_class(JSContext *ctx, JSValue *sp,
                              JSAtom class_name, int class_flags,
                              JSVarRef **cur_var_refs,
                              JSStackFrame *sf, BOOL is_computed_name)
{
    JSValue bfunc, parent_class, proto = JS_UNDEFINED;
    JSValue ctor = JS_UNDEFINED, parent_proto = JS_UNDEFINED;
    JSFunctionBytecode *b;

    parent_class = sp[-2];
    bfunc = sp[-1];

    if (class_flags & JS_DEFINE_CLASS_HAS_HERITAGE) {
        if (JS_IsNull(parent_class)) {
            parent_proto = JS_NULL;
            parent_class = JS_DupValue(ctx, ctx->function_proto);
        } else {
            if (!JS_IsConstructor(ctx, parent_class)) {
                JS_ThrowTypeError(ctx, "parent class must be constructor");
                goto fail;
            }
            parent_proto = JS_GetProperty(ctx, parent_class, JS_ATOM_prototype);
            if (JS_IsException(parent_proto))
                goto fail;
            if (!JS_IsNull(parent_proto) && !JS_IsObject(parent_proto)) {
                JS_ThrowTypeError(ctx, "parent prototype must be an object or null");
                goto fail;
            }
        }
    } else {
        /* parent_class is JS_UNDEFINED in this case */
        parent_proto = JS_DupValue(ctx, ctx->class_proto[JS_CLASS_OBJECT]);
        parent_class = JS_DupValue(ctx, ctx->function_proto);
    }
    proto = JS_NewObjectProto(ctx, parent_proto);
    if (JS_IsException(proto))
        goto fail;

    b = JS_VALUE_GET_PTR(bfunc);
    assert(b->func_kind == JS_FUNC_NORMAL);
    ctor = JS_NewObjectProtoClass(ctx, parent_class,
                                  JS_CLASS_BYTECODE_FUNCTION);
    if (JS_IsException(ctor))
        goto fail;
    ctor = js_closure2(ctx, ctor, b, cur_var_refs, sf, FALSE, NULL);
    bfunc = JS_UNDEFINED;
    if (JS_IsException(ctor))
        goto fail;
    js_method_set_home_object(ctx, ctor, proto);
    JS_SetConstructorBit(ctx, ctor, TRUE);

    JS_DefinePropertyValue(ctx, ctor, JS_ATOM_length,
                           JS_NewInt32(ctx, b->defined_arg_count),
                           JS_PROP_CONFIGURABLE);

    if (is_computed_name) {
        if (JS_DefineObjectNameComputed(ctx, ctor, sp[-3],
                                        JS_PROP_CONFIGURABLE) < 0)
            goto fail;
    } else {
        if (JS_DefineObjectName(ctx, ctor, class_name, JS_PROP_CONFIGURABLE) < 0)
            goto fail;
    }

    /* the constructor property must be first. It can be overriden by
       computed property names */
    if (JS_DefinePropertyValue(ctx, proto, JS_ATOM_constructor,
                               JS_DupValue(ctx, ctor),
                               JS_PROP_CONFIGURABLE |
                               JS_PROP_WRITABLE | JS_PROP_THROW) < 0)
        goto fail;
    /* set the prototype property */
    if (JS_DefinePropertyValue(ctx, ctor, JS_ATOM_prototype,
                               JS_DupValue(ctx, proto), JS_PROP_THROW) < 0)
        goto fail;
    set_cycle_flag(ctx, ctor);
    set_cycle_flag(ctx, proto);

    JS_FreeValue(ctx, parent_proto);
    JS_FreeValue(ctx, parent_class);

    sp[-2] = ctor;
    sp[-1] = proto;
    return 0;
 fail:
    JS_FreeValue(ctx, parent_class);
    JS_FreeValue(ctx, parent_proto);
    JS_FreeValue(ctx, bfunc);
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, ctor);
    sp[-2] = JS_UNDEFINED;
    sp[-1] = JS_UNDEFINED;
    return -1;
}

static void close_var_ref(JSRuntime *rt, JSStackFrame *sf, JSVarRef *var_ref)
{
    if (sf->js_mode & JS_MODE_ASYNC) {
        JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
        async_func_free(rt, async_func);
    }
    var_ref->value = JS_DupValueRT(rt, *var_ref->pvalue);
    var_ref->pvalue = &var_ref->value;
    /* the reference is no longer to a local variable */
    var_ref->is_detached = TRUE;
}

static void close_var_refs(JSRuntime *rt, JSFunctionBytecode *b, JSStackFrame *sf)
{
    JSVarRef *var_ref;
    int i;

    for(i = 0; i < b->var_ref_count; i++) {
        var_ref = sf->var_refs[i];
        if (var_ref)
            close_var_ref(rt, sf, var_ref);
    }
}

static void close_lexical_var(JSContext *ctx, JSFunctionBytecode *b,
                              JSStackFrame *sf, int var_idx)
{
    JSVarRef *var_ref;
    int var_ref_idx;
    
    var_ref_idx = b->vardefs[b->arg_count + var_idx].var_ref_idx;
    var_ref = sf->var_refs[var_ref_idx];
    if (var_ref) {
        close_var_ref(ctx->rt, sf, var_ref);
        sf->var_refs[var_ref_idx] = NULL;
    }
}

#define JS_CALL_FLAG_COPY_ARGV   (1 << 1)
#define JS_CALL_FLAG_GENERATOR   (1 << 2)

JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                           JSValueConst this_obj,
                           int argc, JSValueConst *argv, int flags)
{
    JSRuntime *rt = ctx->rt;
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
    rt->current_stack_frame = sf;
    ctx = p->u.cfunc.realm; /* change the current realm */
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
    return ret_val;
}

JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                               JSValueConst this_obj,
                               int argc, JSValueConst *argv, int flags)
{
    JSObject *p;
    JSBoundFunction *bf;
    JSValueConst *arg_buf, new_target;
    int arg_count, i;

    p = JS_VALUE_GET_OBJ(func_obj);
    bf = p->u.bound_function;
    arg_count = bf->argc + argc;
    if (js_check_stack_overflow(ctx->rt, sizeof(JSValue) * arg_count))
        return JS_ThrowStackOverflow(ctx);
    arg_buf = alloca(sizeof(JSValue) * arg_count);
    for(i = 0; i < bf->argc; i++) {
        arg_buf[i] = bf->argv[i];
    }
    for(i = 0; i < argc; i++) {
        arg_buf[bf->argc + i] = argv[i];
    }
    if (flags & JS_CALL_FLAG_CONSTRUCTOR) {
        new_target = this_obj;
        if (js_same_value(ctx, func_obj, new_target))
            new_target = bf->func_obj;
        return JS_CallConstructor2(ctx, bf->func_obj, new_target,
                                   arg_count, arg_buf);
    } else {
        return JS_Call(ctx, bf->func_obj, bf->this_val,
                       arg_count, arg_buf);
    }
}

/* [OPSpecialObjectEnum defined in compiler/js_opcode.h] */

/* [FUNC_RET_* defined in compiler/js_opcode.h] */

#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
#endif

/* argv[] is modified if (flags & JS_CALL_FLAG_COPY_ARGV) = 0. */
static JSValue JS_CallInternal(JSContext *caller_ctx, JSValueConst func_obj,
                               JSValueConst this_obj, JSValueConst new_target,
                               int argc, JSValue *argv, int flags)
{
    JSRuntime *rt = caller_ctx->rt;
    JSContext *ctx;
    JSObject *p;
    JSFunctionBytecode *b;
    JSStackFrame sf_s, *sf = &sf_s;
    const uint8_t *pc;
    int opcode, arg_allocated_size, i;
    JSValue *local_buf, *stack_buf, *var_buf, *arg_buf, *sp, ret_val, *pval;
    JSVarRef **var_refs;
    size_t alloca_size;

#if !DIRECT_DISPATCH
#define SWITCH(pc)      switch (opcode = *pc++)
#define CASE(op)        case op
#define DEFAULT         default
#define BREAK           break
#else
    static const void * const dispatch_table[256] = {
#define DEF(id, size, n_pop, n_push, f) && case_OP_ ## id,
#if SHORT_OPCODES
#define def(id, size, n_pop, n_push, f)
#else
#define def(id, size, n_pop, n_push, f) && case_default,
#endif
#include "quickjs-opcode.h"
        [ OP_COUNT ... 255 ] = &&case_default
    };
#define SWITCH(pc)      goto *dispatch_table[opcode = *pc++];
#ifdef OPCODE_ASM_LABEL
#define CASE(op)        case_ ## op: asm volatile("label_" #op ":\n.globl label_" #op); dummy_case_ ## op
#else
#define CASE(op)        case_ ## op
#endif
#define DEFAULT         case_default
#define BREAK           SWITCH(pc)
#endif

    if (js_poll_interrupts(caller_ctx))
        return JS_EXCEPTION;
    if (unlikely(JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT)) {
        if (flags & JS_CALL_FLAG_GENERATOR) {
            JSAsyncFunctionState *s = JS_VALUE_GET_PTR(func_obj);
            /* func_obj get contains a pointer to JSFuncAsyncState */
            /* the stack frame is already allocated */
            sf = &s->frame;
            p = JS_VALUE_GET_OBJ(sf->cur_func);
            b = p->u.func.function_bytecode;
            ctx = b->realm;
            var_refs = p->u.func.var_refs;
            local_buf = arg_buf = sf->arg_buf;
            var_buf = sf->var_buf;
            stack_buf = sf->var_buf + b->var_count;
            sp = sf->cur_sp;
            sf->cur_sp = NULL; /* cur_sp is NULL if the function is running */
            pc = sf->cur_pc;
            sf->prev_frame = rt->current_stack_frame;
            rt->current_stack_frame = sf;
            if (s->throw_flag)
                goto exception;
            else
                goto restart;
        } else {
            goto not_a_function;
        }
    }
    p = JS_VALUE_GET_OBJ(func_obj);
    if (unlikely(p->class_id != JS_CLASS_BYTECODE_FUNCTION)) {
        JSClassCall *call_func;
        call_func = rt->class_array[p->class_id].call;
        if (!call_func) {
        not_a_function:
            return JS_ThrowTypeError(caller_ctx, "not a function");
        }
        return call_func(caller_ctx, func_obj, this_obj, argc,
                         (JSValueConst *)argv, flags);
    }
    b = p->u.func.function_bytecode;

    if (unlikely(argc < b->arg_count || (flags & JS_CALL_FLAG_COPY_ARGV))) {
        arg_allocated_size = b->arg_count;
    } else {
        arg_allocated_size = 0;
    }

    alloca_size = sizeof(JSValue) * (arg_allocated_size + b->var_count +
                                     b->stack_size) +
        sizeof(JSVarRef *) * b->var_ref_count;
    if (js_check_stack_overflow(rt, alloca_size))
        return JS_ThrowStackOverflow(caller_ctx);

    sf->js_mode = b->js_mode;
    arg_buf = argv;
    sf->arg_count = argc;
    sf->cur_func = (JSValue)func_obj;
    var_refs = p->u.func.var_refs;

    local_buf = alloca(alloca_size);
    if (unlikely(arg_allocated_size)) {
        int n = min_int(argc, b->arg_count);
        arg_buf = local_buf;
        for(i = 0; i < n; i++)
            arg_buf[i] = JS_DupValue(caller_ctx, argv[i]);
        for(; i < b->arg_count; i++)
            arg_buf[i] = JS_UNDEFINED;
        sf->arg_count = b->arg_count;
    }
    var_buf = local_buf + arg_allocated_size;
    sf->var_buf = var_buf;
    sf->arg_buf = arg_buf;

    for(i = 0; i < b->var_count; i++)
        var_buf[i] = JS_UNDEFINED;

    stack_buf = var_buf + b->var_count;
    sf->var_refs = (JSVarRef **)(stack_buf + b->stack_size);
    for(i = 0; i < b->var_ref_count; i++)
        sf->var_refs[i] = NULL;
    sp = stack_buf;
    pc = b->byte_code_buf;
    sf->prev_frame = rt->current_stack_frame;
    rt->current_stack_frame = sf;
    ctx = b->realm; /* set the current realm */

 restart:
    for(;;) {
        int call_argc;
        JSValue *call_argv;

        SWITCH(pc) {
        CASE(OP_push_i32):
            *sp++ = JS_NewInt32(ctx, get_u32(pc));
            pc += 4;
            BREAK;
        CASE(OP_push_bigint_i32):
            *sp++ = __JS_NewShortBigInt(ctx, (int)get_u32(pc));
            pc += 4;
            BREAK;
        CASE(OP_push_const):
            *sp++ = JS_DupValue(ctx, b->cpool[get_u32(pc)]);
            pc += 4;
            BREAK;
#if SHORT_OPCODES
        CASE(OP_push_minus1):
        CASE(OP_push_0):
        CASE(OP_push_1):
        CASE(OP_push_2):
        CASE(OP_push_3):
        CASE(OP_push_4):
        CASE(OP_push_5):
        CASE(OP_push_6):
        CASE(OP_push_7):
            *sp++ = JS_NewInt32(ctx, opcode - OP_push_0);
            BREAK;
        CASE(OP_push_i8):
            *sp++ = JS_NewInt32(ctx, get_i8(pc));
            pc += 1;
            BREAK;
        CASE(OP_push_i16):
            *sp++ = JS_NewInt32(ctx, get_i16(pc));
            pc += 2;
            BREAK;
        CASE(OP_push_const8):
            *sp++ = JS_DupValue(ctx, b->cpool[*pc++]);
            BREAK;
        CASE(OP_fclosure8):
            *sp++ = js_closure(ctx, JS_DupValue(ctx, b->cpool[*pc++]), var_refs, sf, FALSE);
            if (unlikely(JS_IsException(sp[-1])))
                goto exception;
            BREAK;
        CASE(OP_push_empty_string):
            *sp++ = JS_AtomToString(ctx, JS_ATOM_empty_string);
            BREAK;
#endif
        CASE(OP_push_atom_value):
            *sp++ = JS_AtomToValue(ctx, get_u32(pc));
            pc += 4;
            BREAK;
        CASE(OP_undefined):
            *sp++ = JS_UNDEFINED;
            BREAK;
        CASE(OP_null):
            *sp++ = JS_NULL;
            BREAK;
        CASE(OP_push_this):
            /* OP_push_this is only called at the start of a function */
            {
                JSValue val;
                if (!(b->js_mode & JS_MODE_STRICT)) {
                    uint32_t tag = JS_VALUE_GET_TAG(this_obj);
                    if (likely(tag == JS_TAG_OBJECT))
                        goto normal_this;
                    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED) {
                        val = JS_DupValue(ctx, ctx->global_obj);
                    } else {
                        val = JS_ToObject(ctx, this_obj);
                        if (JS_IsException(val))
                            goto exception;
                    }
                } else {
                normal_this:
                    val = JS_DupValue(ctx, this_obj);
                }
                *sp++ = val;
            }
            BREAK;
        CASE(OP_push_false):
            *sp++ = JS_FALSE;
            BREAK;
        CASE(OP_push_true):
            *sp++ = JS_TRUE;
            BREAK;
        CASE(OP_object):
            *sp++ = JS_NewObject(ctx);
            if (unlikely(JS_IsException(sp[-1])))
                goto exception;
            BREAK;
        CASE(OP_special_object):
            {
                int arg = *pc++;
                switch(arg) {
                case OP_SPECIAL_OBJECT_ARGUMENTS:
                    *sp++ = js_build_arguments(ctx, argc, (JSValueConst *)argv);
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                case OP_SPECIAL_OBJECT_MAPPED_ARGUMENTS:
                    *sp++ = js_build_mapped_arguments(ctx, argc, (JSValueConst *)argv,
                                                      sf, min_int(argc, b->arg_count));
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                case OP_SPECIAL_OBJECT_THIS_FUNC:
                    *sp++ = JS_DupValue(ctx, sf->cur_func);
                    break;
                case OP_SPECIAL_OBJECT_NEW_TARGET:
                    *sp++ = JS_DupValue(ctx, new_target);
                    break;
                case OP_SPECIAL_OBJECT_HOME_OBJECT:
                    {
                        JSObject *p1;
                        p1 = p->u.func.home_object;
                        if (unlikely(!p1))
                            *sp++ = JS_UNDEFINED;
                        else
                            *sp++ = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                    }
                    break;
                case OP_SPECIAL_OBJECT_VAR_OBJECT:
                    *sp++ = JS_NewObjectProto(ctx, JS_NULL);
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                case OP_SPECIAL_OBJECT_IMPORT_META:
                    *sp++ = js_import_meta(ctx);
                    if (unlikely(JS_IsException(sp[-1])))
                        goto exception;
                    break;
                default:
                    abort();
                }
            }
            BREAK;
        CASE(OP_rest):
            {
                int first = get_u16(pc);
                pc += 2;
                first = min_int(first, argc);
                *sp++ = js_create_array(ctx, argc - first, (JSValueConst *)(argv + first));
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
            }
            BREAK;

        CASE(OP_drop):
            JS_FreeValue(ctx, sp[-1]);
            sp--;
            BREAK;
        CASE(OP_nip):
            JS_FreeValue(ctx, sp[-2]);
            sp[-2] = sp[-1];
            sp--;
            BREAK;
        CASE(OP_nip1): /* a b c -> b c */
            JS_FreeValue(ctx, sp[-3]);
            sp[-3] = sp[-2];
            sp[-2] = sp[-1];
            sp--;
            BREAK;
        CASE(OP_dup):
            sp[0] = JS_DupValue(ctx, sp[-1]);
            sp++;
            BREAK;
        CASE(OP_dup2): /* a b -> a b a b */
            sp[0] = JS_DupValue(ctx, sp[-2]);
            sp[1] = JS_DupValue(ctx, sp[-1]);
            sp += 2;
            BREAK;
        CASE(OP_dup3): /* a b c -> a b c a b c */
            sp[0] = JS_DupValue(ctx, sp[-3]);
            sp[1] = JS_DupValue(ctx, sp[-2]);
            sp[2] = JS_DupValue(ctx, sp[-1]);
            sp += 3;
            BREAK;
        CASE(OP_dup1): /* a b -> a a b */
            sp[0] = sp[-1];
            sp[-1] = JS_DupValue(ctx, sp[-2]);
            sp++;
            BREAK;
        CASE(OP_insert2): /* obj a -> a obj a (dup_x1) */
            sp[0] = sp[-1];
            sp[-1] = sp[-2];
            sp[-2] = JS_DupValue(ctx, sp[0]);
            sp++;
            BREAK;
        CASE(OP_insert3): /* obj prop a -> a obj prop a (dup_x2) */
            sp[0] = sp[-1];
            sp[-1] = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = JS_DupValue(ctx, sp[0]);
            sp++;
            BREAK;
        CASE(OP_insert4): /* this obj prop a -> a this obj prop a */
            sp[0] = sp[-1];
            sp[-1] = sp[-2];
            sp[-2] = sp[-3];
            sp[-3] = sp[-4];
            sp[-4] = JS_DupValue(ctx, sp[0]);
            sp++;
            BREAK;
        CASE(OP_perm3): /* obj a b -> a obj b (213) */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = tmp;
            }
            BREAK;
        CASE(OP_rot3l): /* x a b -> a b x (231) */
            {
                JSValue tmp;
                tmp = sp[-3];
                sp[-3] = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_rot4l): /* x a b c -> a b c x */
            {
                JSValue tmp;
                tmp = sp[-4];
                sp[-4] = sp[-3];
                sp[-3] = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_rot5l): /* x a b c d -> a b c d x */
            {
                JSValue tmp;
                tmp = sp[-5];
                sp[-5] = sp[-4];
                sp[-4] = sp[-3];
                sp[-3] = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_rot3r): /* a b x -> x a b (312) */
            {
                JSValue tmp;
                tmp = sp[-1];
                sp[-1] = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = tmp;
            }
            BREAK;
        CASE(OP_perm4): /* obj prop a b -> a obj prop b */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = sp[-4];
                sp[-4] = tmp;
            }
            BREAK;
        CASE(OP_perm5): /* this obj prop a b -> a this obj prop b */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-3];
                sp[-3] = sp[-4];
                sp[-4] = sp[-5];
                sp[-5] = tmp;
            }
            BREAK;
        CASE(OP_swap): /* a b -> b a */
            {
                JSValue tmp;
                tmp = sp[-2];
                sp[-2] = sp[-1];
                sp[-1] = tmp;
            }
            BREAK;
        CASE(OP_swap2): /* a b c d -> c d a b */
            {
                JSValue tmp1, tmp2;
                tmp1 = sp[-4];
                tmp2 = sp[-3];
                sp[-4] = sp[-2];
                sp[-3] = sp[-1];
                sp[-2] = tmp1;
                sp[-1] = tmp2;
            }
            BREAK;

        CASE(OP_fclosure):
            {
                JSValue bfunc = JS_DupValue(ctx, b->cpool[get_u32(pc)]);
                pc += 4;
                *sp++ = js_closure(ctx, bfunc, var_refs, sf, FALSE);
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
            }
            BREAK;
#if SHORT_OPCODES
        CASE(OP_call0):
        CASE(OP_call1):
        CASE(OP_call2):
        CASE(OP_call3):
            call_argc = opcode - OP_call0;
            goto has_call_argc;
#endif
        CASE(OP_call):
        CASE(OP_tail_call):
            {
                call_argc = get_u16(pc);
                pc += 2;
                goto has_call_argc;
            has_call_argc:
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                ret_val = JS_CallInternal(ctx, call_argv[-1], JS_UNDEFINED,
                                          JS_UNDEFINED, call_argc, call_argv, 0);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                if (opcode == OP_tail_call)
                    goto done;
                for(i = -1; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 1;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_call_constructor):
            {
                call_argc = get_u16(pc);
                pc += 2;
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                ret_val = JS_CallConstructorInternal(ctx, call_argv[-2],
                                                     call_argv[-1],
                                                     call_argc, call_argv, 0);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                for(i = -2; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 2;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_call_method):
        CASE(OP_tail_call_method):
            {
                call_argc = get_u16(pc);
                pc += 2;
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                ret_val = JS_CallInternal(ctx, call_argv[-1], call_argv[-2],
                                          JS_UNDEFINED, call_argc, call_argv, 0);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                if (opcode == OP_tail_call_method)
                    goto done;
                for(i = -2; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 2;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_array_from):
            call_argc = get_u16(pc);
            pc += 2;
            ret_val = js_create_array_free(ctx, call_argc, sp - call_argc);
            sp -= call_argc;
            if (unlikely(JS_IsException(ret_val)))
                goto exception;
            *sp++ = ret_val;
            BREAK;

        CASE(OP_apply):
            {
                int magic;
                magic = get_u16(pc);
                pc += 2;
                sf->cur_pc = pc;

                ret_val = js_function_apply(ctx, sp[-3], 2, (JSValueConst *)&sp[-2], magic);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                JS_FreeValue(ctx, sp[-3]);
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-1]);
                sp -= 3;
                *sp++ = ret_val;
            }
            BREAK;
        CASE(OP_return):
            ret_val = *--sp;
            goto done;
        CASE(OP_return_undef):
            ret_val = JS_UNDEFINED;
            goto done;

        CASE(OP_check_ctor_return):
            /* return TRUE if 'this' should be returned */
            if (!JS_IsObject(sp[-1])) {
                if (!JS_IsUndefined(sp[-1])) {
                    JS_ThrowTypeError(caller_ctx, "derived class constructor must return an object or undefined");
                    goto exception;
                }
                sp[0] = JS_TRUE;
            } else {
                sp[0] = JS_FALSE;
            }
            sp++;
            BREAK;
        CASE(OP_check_ctor):
            if (JS_IsUndefined(new_target)) {
            non_ctor_call:
                JS_ThrowTypeError(ctx, "class constructors must be invoked with 'new'");
                goto exception;
            }
            BREAK;
        CASE(OP_init_ctor):
            {
                JSValue super, ret;
                sf->cur_pc = pc;
                if (JS_IsUndefined(new_target))
                    goto non_ctor_call;
                super = JS_GetPrototype(ctx, func_obj);
                if (JS_IsException(super))
                    goto exception;
                ret = JS_CallConstructor2(ctx, super, new_target, argc, (JSValueConst *)argv);
                JS_FreeValue(ctx, super);
                if (JS_IsException(ret))
                    goto exception;
                *sp++ = ret;
            }
            BREAK;
        CASE(OP_check_brand):
            {
                int ret = JS_CheckBrand(ctx, sp[-2], sp[-1]);
                if (ret < 0)
                    goto exception;
                if (!ret) {
                    JS_ThrowTypeError(ctx, "invalid brand on object");
                    goto exception;
                }
            }
            BREAK;
        CASE(OP_add_brand):
            if (JS_AddBrand(ctx, sp[-2], sp[-1]) < 0)
                goto exception;
            JS_FreeValue(ctx, sp[-2]);
            JS_FreeValue(ctx, sp[-1]);
            sp -= 2;
            BREAK;

        CASE(OP_throw):
            JS_Throw(ctx, *--sp);
            goto exception;

        CASE(OP_throw_error):
/* [JS_THROW_* defined in compiler/js_opcode.h] */
            {
                JSAtom atom;
                int type;
                atom = get_u32(pc);
                type = pc[4];
                pc += 5;
                if (type == JS_THROW_VAR_RO)
                    JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, atom);
                else
                if (type == JS_THROW_VAR_REDECL)
                    JS_ThrowSyntaxErrorVarRedeclaration(ctx, atom);
                else
                if (type == JS_THROW_VAR_UNINITIALIZED)
                    JS_ThrowReferenceErrorUninitialized(ctx, atom);
                else
                if (type == JS_THROW_ERROR_DELETE_SUPER)
                    JS_ThrowReferenceError(ctx, "unsupported reference to 'super'");
                else
                if (type == JS_THROW_ERROR_ITERATOR_THROW)
                    JS_ThrowTypeError(ctx, "iterator does not have a throw method");
                else
                    JS_ThrowInternalError(ctx, "invalid throw var type %d", type);
            }
            goto exception;

        CASE(OP_eval):
            {
                JSValueConst obj;
                int scope_idx;
                call_argc = get_u16(pc);
                scope_idx = get_u16(pc + 2) + ARG_SCOPE_END;
                pc += 4;
                call_argv = sp - call_argc;
                sf->cur_pc = pc;
                if (js_same_value(ctx, call_argv[-1], ctx->eval_obj)) {
                    if (call_argc >= 1)
                        obj = call_argv[0];
                    else
                        obj = JS_UNDEFINED;
                    ret_val = JS_EvalObject(ctx, JS_UNDEFINED, obj,
                                            JS_EVAL_TYPE_DIRECT, scope_idx);
                } else {
                    ret_val = JS_CallInternal(ctx, call_argv[-1], JS_UNDEFINED,
                                              JS_UNDEFINED, call_argc, call_argv, 0);
                }
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                for(i = -1; i < call_argc; i++)
                    JS_FreeValue(ctx, call_argv[i]);
                sp -= call_argc + 1;
                *sp++ = ret_val;
            }
            BREAK;
            /* could merge with OP_apply */
        CASE(OP_apply_eval):
            {
                int scope_idx;
                uint32_t len;
                JSValue *tab;
                JSValueConst obj;

                scope_idx = get_u16(pc) + ARG_SCOPE_END;
                pc += 2;
                sf->cur_pc = pc;
                tab = build_arg_list(ctx, &len, sp[-1]);
                if (!tab)
                    goto exception;
                if (js_same_value(ctx, sp[-2], ctx->eval_obj)) {
                    if (len >= 1)
                        obj = tab[0];
                    else
                        obj = JS_UNDEFINED;
                    ret_val = JS_EvalObject(ctx, JS_UNDEFINED, obj,
                                            JS_EVAL_TYPE_DIRECT, scope_idx);
                } else {
                    ret_val = JS_Call(ctx, sp[-2], JS_UNDEFINED, len,
                                      (JSValueConst *)tab);
                }
                free_arg_list(ctx, tab, len);
                if (unlikely(JS_IsException(ret_val)))
                    goto exception;
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-1]);
                sp -= 2;
                *sp++ = ret_val;
            }
            BREAK;

        CASE(OP_regexp):
            {
                sp[-2] = JS_NewRegexp(ctx, sp[-2], sp[-1]);
                sp--;
                if (JS_IsException(sp[-1]))
                    goto exception;
            }
            BREAK;

        CASE(OP_get_super):
            {
                JSValue proto;
                sf->cur_pc = pc;
                proto = JS_GetPrototype(ctx, sp[-1]);
                if (JS_IsException(proto))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = proto;
            }
            BREAK;

        CASE(OP_import):
            {
                JSValue val;
                sf->cur_pc = pc;
                val = js_dynamic_import(ctx, sp[-2], sp[-1]);
                if (JS_IsException(val))
                    goto exception;
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-1]);
                sp--;
                sp[-1] = val;
            }
            BREAK;

        CASE(OP_get_var_undef):
        CASE(OP_get_var):
            {
                int idx;
                JSValue val;
                idx = get_u16(pc);
                pc += 2;
                val = *var_refs[idx]->pvalue;
                if (unlikely(JS_IsUninitialized(val))) {
                    JSClosureVar *cv = &b->closure_var[idx];
                    if (cv->is_lexical) {
                        JS_ThrowReferenceErrorUninitialized(ctx, cv->var_name);
                        goto exception;
                    } else {
                        sf->cur_pc = pc;
                        sp[0] = JS_GetPropertyInternal(ctx, ctx->global_obj,
                                                       cv->var_name,
                                                       ctx->global_obj,
                                                       opcode - OP_get_var_undef);
                        if (JS_IsException(sp[0]))
                            goto exception;
                    }
                } else {
                    sp[0] = JS_DupValue(ctx, val);
                }
                sp++;
            }
            BREAK;

        CASE(OP_put_var):
        CASE(OP_put_var_init):
            {
                int idx, ret;
                JSVarRef *var_ref;
                idx = get_u16(pc);
                pc += 2;
                var_ref = var_refs[idx];
                if (unlikely(JS_IsUninitialized(*var_ref->pvalue) ||
                             var_ref->is_const)) {
                    JSClosureVar *cv = &b->closure_var[idx];
                    if (var_ref->is_lexical) {
                        if (opcode == OP_put_var_init)
                            goto put_var_ok;
                        if (JS_IsUninitialized(*var_ref->pvalue))
                            JS_ThrowReferenceErrorUninitialized(ctx, cv->var_name);
                        else
                            JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, cv->var_name);
                        goto exception;
                    } else {
                        sf->cur_pc = pc;
                        ret = JS_HasProperty(ctx, ctx->global_obj, cv->var_name);
                        if (ret < 0)
                            goto exception;
                        if (ret == 0 && is_strict_mode(ctx)) {
                            JS_ThrowReferenceErrorNotDefined(ctx, cv->var_name);
                            goto exception;
                        }
                        ret = JS_SetPropertyInternal(ctx, ctx->global_obj, cv->var_name, sp[-1],
                                                     ctx->global_obj, JS_PROP_THROW_STRICT);
                        sp--;
                        if (ret < 0)
                            goto exception;
                    }
                } else {
                put_var_ok:
                   set_value(ctx, var_ref->pvalue, sp[-1]);
                   sp--;
                }
            }
            BREAK;
        CASE(OP_get_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                sp[0] = JS_DupValue(ctx, var_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_put_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &var_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &var_buf[idx], JS_DupValue(ctx, sp[-1]));
            }
            BREAK;
        CASE(OP_get_arg):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                sp[0] = JS_DupValue(ctx, arg_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_put_arg):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &arg_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_arg):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &arg_buf[idx], JS_DupValue(ctx, sp[-1]));
            }
            BREAK;

#if SHORT_OPCODES
        CASE(OP_get_loc8): *sp++ = JS_DupValue(ctx, var_buf[*pc++]); BREAK;
        CASE(OP_put_loc8): set_value(ctx, &var_buf[*pc++], *--sp); BREAK;
        CASE(OP_set_loc8): set_value(ctx, &var_buf[*pc++], JS_DupValue(ctx, sp[-1])); BREAK;

        CASE(OP_get_loc0): *sp++ = JS_DupValue(ctx, var_buf[0]); BREAK;
        CASE(OP_get_loc1): *sp++ = JS_DupValue(ctx, var_buf[1]); BREAK;
        CASE(OP_get_loc2): *sp++ = JS_DupValue(ctx, var_buf[2]); BREAK;
        CASE(OP_get_loc3): *sp++ = JS_DupValue(ctx, var_buf[3]); BREAK;
        CASE(OP_put_loc0): set_value(ctx, &var_buf[0], *--sp); BREAK;
        CASE(OP_put_loc1): set_value(ctx, &var_buf[1], *--sp); BREAK;
        CASE(OP_put_loc2): set_value(ctx, &var_buf[2], *--sp); BREAK;
        CASE(OP_put_loc3): set_value(ctx, &var_buf[3], *--sp); BREAK;
        CASE(OP_set_loc0): set_value(ctx, &var_buf[0], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_loc1): set_value(ctx, &var_buf[1], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_loc2): set_value(ctx, &var_buf[2], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_loc3): set_value(ctx, &var_buf[3], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_get_arg0): *sp++ = JS_DupValue(ctx, arg_buf[0]); BREAK;
        CASE(OP_get_arg1): *sp++ = JS_DupValue(ctx, arg_buf[1]); BREAK;
        CASE(OP_get_arg2): *sp++ = JS_DupValue(ctx, arg_buf[2]); BREAK;
        CASE(OP_get_arg3): *sp++ = JS_DupValue(ctx, arg_buf[3]); BREAK;
        CASE(OP_put_arg0): set_value(ctx, &arg_buf[0], *--sp); BREAK;
        CASE(OP_put_arg1): set_value(ctx, &arg_buf[1], *--sp); BREAK;
        CASE(OP_put_arg2): set_value(ctx, &arg_buf[2], *--sp); BREAK;
        CASE(OP_put_arg3): set_value(ctx, &arg_buf[3], *--sp); BREAK;
        CASE(OP_set_arg0): set_value(ctx, &arg_buf[0], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_arg1): set_value(ctx, &arg_buf[1], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_arg2): set_value(ctx, &arg_buf[2], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_arg3): set_value(ctx, &arg_buf[3], JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_get_var_ref0): *sp++ = JS_DupValue(ctx, *var_refs[0]->pvalue); BREAK;
        CASE(OP_get_var_ref1): *sp++ = JS_DupValue(ctx, *var_refs[1]->pvalue); BREAK;
        CASE(OP_get_var_ref2): *sp++ = JS_DupValue(ctx, *var_refs[2]->pvalue); BREAK;
        CASE(OP_get_var_ref3): *sp++ = JS_DupValue(ctx, *var_refs[3]->pvalue); BREAK;
        CASE(OP_put_var_ref0): set_value(ctx, var_refs[0]->pvalue, *--sp); BREAK;
        CASE(OP_put_var_ref1): set_value(ctx, var_refs[1]->pvalue, *--sp); BREAK;
        CASE(OP_put_var_ref2): set_value(ctx, var_refs[2]->pvalue, *--sp); BREAK;
        CASE(OP_put_var_ref3): set_value(ctx, var_refs[3]->pvalue, *--sp); BREAK;
        CASE(OP_set_var_ref0): set_value(ctx, var_refs[0]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_var_ref1): set_value(ctx, var_refs[1]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_var_ref2): set_value(ctx, var_refs[2]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
        CASE(OP_set_var_ref3): set_value(ctx, var_refs[3]->pvalue, JS_DupValue(ctx, sp[-1])); BREAK;
#endif

        CASE(OP_get_var_ref):
            {
                int idx;
                JSValue val;
                idx = get_u16(pc);
                pc += 2;
                val = *var_refs[idx]->pvalue;
                sp[0] = JS_DupValue(ctx, val);
                sp++;
            }
            BREAK;
        CASE(OP_put_var_ref):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_var_ref):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, var_refs[idx]->pvalue, JS_DupValue(ctx, sp[-1]));
            }
            BREAK;
        CASE(OP_get_var_ref_check):
            {
                int idx;
                JSValue val;
                idx = get_u16(pc);
                pc += 2;
                val = *var_refs[idx]->pvalue;
                if (unlikely(JS_IsUninitialized(val))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                    goto exception;
                }
                sp[0] = JS_DupValue(ctx, val);
                sp++;
            }
            BREAK;
        CASE(OP_put_var_ref_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(*var_refs[idx]->pvalue))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                    goto exception;
                }
                set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_put_var_ref_check_init):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(!JS_IsUninitialized(*var_refs[idx]->pvalue))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, TRUE);
                    goto exception;
                }
                set_value(ctx, var_refs[idx]->pvalue, sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_loc_uninitialized):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                set_value(ctx, &var_buf[idx], JS_UNINITIALIZED);
            }
            BREAK;
        CASE(OP_get_loc_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                    goto exception;
                }
                sp[0] = JS_DupValue(ctx, var_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_get_loc_checkthis):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(caller_ctx, b, idx, FALSE);
                    goto exception;
                }
                sp[0] = JS_DupValue(ctx, var_buf[idx]);
                sp++;
            }
            BREAK;
        CASE(OP_put_loc_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                    goto exception;
                }
                set_value(ctx, &var_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_set_loc_check):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceErrorUninitialized2(ctx, b, idx, FALSE);
                    goto exception;
                }
                set_value(ctx, &var_buf[idx], JS_DupValue(ctx, sp[-1]));
            }
            BREAK;
        CASE(OP_put_loc_check_init):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                if (unlikely(!JS_IsUninitialized(var_buf[idx]))) {
                    JS_ThrowReferenceError(ctx, "'this' can be initialized only once");
                    goto exception;
                }
                set_value(ctx, &var_buf[idx], sp[-1]);
                sp--;
            }
            BREAK;
        CASE(OP_close_loc):
            {
                int idx;
                idx = get_u16(pc);
                pc += 2;
                close_lexical_var(ctx, b, sf, idx);
            }
            BREAK;

        CASE(OP_make_loc_ref):
        CASE(OP_make_arg_ref):
        CASE(OP_make_var_ref_ref):
            {
                JSVarRef *var_ref;
                JSProperty *pr;
                JSAtom atom;
                int idx;
                atom = get_u32(pc);
                idx = get_u16(pc + 4);
                pc += 6;
                *sp++ = JS_NewObjectProto(ctx, JS_NULL);
                if (unlikely(JS_IsException(sp[-1])))
                    goto exception;
                if (opcode == OP_make_var_ref_ref) {
                    var_ref = var_refs[idx];
                    js_rc(var_ref)->ref_count++;
                } else {
                    var_ref = get_var_ref(ctx, sf, idx, opcode == OP_make_arg_ref);
                    if (!var_ref)
                        goto exception;
                }
                pr = add_property(ctx, JS_VALUE_GET_OBJ(sp[-1]), atom,
                                  JS_PROP_WRITABLE | JS_PROP_VARREF);
                if (!pr) {
                    free_var_ref(rt, var_ref);
                    goto exception;
                }
                pr->u.var_ref = var_ref;
                *sp++ = JS_AtomToValue(ctx, atom);
            }
            BREAK;
        CASE(OP_make_var_ref):
            {
                JSAtom atom;
                atom = get_u32(pc);
                pc += 4;
                sf->cur_pc = pc;

                if (JS_GetGlobalVarRef(ctx, atom, sp))
                    goto exception;
                sp += 2;
            }
            BREAK;

        CASE(OP_goto):
            pc += (int32_t)get_u32(pc);
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
            BREAK;
#if SHORT_OPCODES
        CASE(OP_goto16):
            pc += (int16_t)get_u16(pc);
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
            BREAK;
        CASE(OP_goto8):
            pc += (int8_t)pc[0];
            if (unlikely(js_poll_interrupts(ctx)))
                goto exception;
            BREAK;
#endif
        CASE(OP_if_true):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 4;
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (res) {
                    pc += (int32_t)get_u32(pc - 4) - 4;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
        CASE(OP_if_false):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 4;
                /* quick and dirty test for JS_TAG_INT, JS_TAG_BOOL, JS_TAG_NULL and JS_TAG_UNDEFINED */
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (!res) {
                    pc += (int32_t)get_u32(pc - 4) - 4;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
#if SHORT_OPCODES
        CASE(OP_if_true8):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 1;
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (res) {
                    pc += (int8_t)pc[-1] - 1;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
        CASE(OP_if_false8):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                pc += 1;
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1);
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp--;
                if (!res) {
                    pc += (int8_t)pc[-1] - 1;
                }
                if (unlikely(js_poll_interrupts(ctx)))
                    goto exception;
            }
            BREAK;
#endif
        CASE(OP_catch):
            {
                int32_t diff;
                diff = get_u32(pc);
                sp[0] = JS_NewCatchOffset(ctx, pc + diff - b->byte_code_buf);
                sp++;
                pc += 4;
            }
            BREAK;
        CASE(OP_gosub):
            {
                int32_t diff;
                diff = get_u32(pc);
                /* XXX: should have a different tag to avoid security flaw */
                sp[0] = JS_NewInt32(ctx, pc + 4 - b->byte_code_buf);
                sp++;
                pc += diff;
            }
            BREAK;
        CASE(OP_ret):
            {
                JSValue op1;
                uint32_t pos;
                op1 = sp[-1];
                if (unlikely(JS_VALUE_GET_TAG(op1) != JS_TAG_INT))
                    goto ret_fail;
                pos = JS_VALUE_GET_INT(op1);
                if (unlikely(pos >= b->byte_code_len)) {
                ret_fail:
                    JS_ThrowInternalError(ctx, "invalid ret value");
                    goto exception;
                }
                sp--;
                pc = b->byte_code_buf + pos;
            }
            BREAK;

        CASE(OP_for_in_start):
            sf->cur_pc = pc;
            if (js_for_in_start(ctx, sp))
                goto exception;
            BREAK;
        CASE(OP_for_in_next):
            sf->cur_pc = pc;
            if (js_for_in_next(ctx, sp))
                goto exception;
            sp += 2;
            BREAK;
        CASE(OP_for_of_start):
            sf->cur_pc = pc;
            if (js_for_of_start(ctx, sp, FALSE))
                goto exception;
            sp += 1;
            *sp++ = JS_NewCatchOffset(ctx, 0);
            BREAK;
        CASE(OP_for_of_next):
            {
                int offset = -3 - pc[0];
                pc += 1;
                sf->cur_pc = pc;
                if (js_for_of_next(ctx, sp, offset))
                    goto exception;
                sp += 2;
            }
            BREAK;
        CASE(OP_for_await_of_next):
            sf->cur_pc = pc;
            if (js_for_await_of_next(ctx, sp))
                goto exception;
            sp++;
            BREAK;
        CASE(OP_for_await_of_start):
            sf->cur_pc = pc;
            if (js_for_of_start(ctx, sp, TRUE))
                goto exception;
            sp += 1;
            *sp++ = JS_NewCatchOffset(ctx, 0);
            BREAK;
        CASE(OP_iterator_get_value_done):
            sf->cur_pc = pc;
            if (js_iterator_get_value_done(ctx, sp))
                goto exception;
            sp += 1;
            BREAK;
        CASE(OP_iterator_check_object):
            if (unlikely(!JS_IsObject(sp[-1]))) {
                JS_ThrowTypeError(ctx, "iterator must return an object");
                goto exception;
            }
            BREAK;

        CASE(OP_iterator_close):
            /* iter_obj next catch_offset -> */
            sp--; /* drop the catch offset to avoid getting caught by exception */
            JS_FreeValue(ctx, sp[-1]); /* drop the next method */
            sp--;
            if (!JS_IsUndefined(sp[-1])) {
                sf->cur_pc = pc;
                if (JS_IteratorClose(ctx, sp[-1], FALSE))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
            }
            sp--;
            BREAK;
        CASE(OP_nip_catch):
            {
                JSValue ret_val;
                /* catch_offset ... ret_val -> ret_eval */
                ret_val = *--sp;
                while (sp > stack_buf &&
                       JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_CATCH_OFFSET) {
                    JS_FreeValue(ctx, *--sp);
                }
                if (unlikely(sp == stack_buf)) {
                    JS_ThrowInternalError(ctx, "nip_catch");
                    JS_FreeValue(ctx, ret_val);
                    goto exception;
                }
                sp[-1] = ret_val;
            }
            BREAK;

        CASE(OP_iterator_next):
            /* stack: iter_obj next catch_offset val */
            {
                JSValue ret;
                sf->cur_pc = pc;
                ret = JS_Call(ctx, sp[-3], sp[-4],
                              1, (JSValueConst *)(sp - 1));
                if (JS_IsException(ret))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret;
            }
            BREAK;

        CASE(OP_iterator_call):
            /* stack: iter_obj next catch_offset val */
            {
                JSValue method, ret;
                BOOL ret_flag;
                int flags;
                flags = *pc++;
                sf->cur_pc = pc;
                method = JS_GetProperty(ctx, sp[-4], (flags & 1) ?
                                        JS_ATOM_throw : JS_ATOM_return);
                if (JS_IsException(method))
                    goto exception;
                if (JS_IsUndefined(method) || JS_IsNull(method)) {
                    ret_flag = TRUE;
                } else {
                    if (flags & 2) {
                        /* no argument */
                        ret = JS_CallFree(ctx, method, sp[-4],
                                          0, NULL);
                    } else {
                        ret = JS_CallFree(ctx, method, sp[-4],
                                          1, (JSValueConst *)(sp - 1));
                    }
                    if (JS_IsException(ret))
                        goto exception;
                    JS_FreeValue(ctx, sp[-1]);
                    sp[-1] = ret;
                    ret_flag = FALSE;
                }
                sp[0] = JS_NewBool(ctx, ret_flag);
                sp += 1;
            }
            BREAK;

        CASE(OP_lnot):
            {
                int res;
                JSValue op1;

                op1 = sp[-1];
                if ((uint32_t)JS_VALUE_GET_TAG(op1) <= JS_TAG_UNDEFINED) {
                    res = JS_VALUE_GET_INT(op1) != 0;
                } else {
                    res = JS_ToBoolFree(ctx, op1);
                }
                sp[-1] = JS_NewBool(ctx, !res);
            }
            BREAK;

#define GET_FIELD_INLINE(name, keep, is_length)                         \
            {                                                           \
                JSValue val, obj;                                       \
                JSAtom atom;                                            \
                JSObject *p;                                            \
                JSProperty *pr;                                         \
                JSShapeProperty *prs;                                   \
                                                                        \
                if (is_length) {                                        \
                    atom = JS_ATOM_length;                              \
                } else {                                                \
                    atom = get_u32(pc);                                 \
                    pc += 4;                                            \
                }                                                       \
                                                                        \
                obj = sp[-1];                                           \
                if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT)) {   \
                    p = JS_VALUE_GET_OBJ(obj);                          \
                    for(;;) {                                           \
                        prs = find_own_property(&pr, p, atom);          \
                        if (prs) {                                      \
                            /* found */                                 \
                            if (unlikely(prs->flags & JS_PROP_TMASK))   \
                                    goto name ## _slow_path;            \
                            val = JS_DupValue(ctx, pr->u.value);        \
                            break;                                      \
                        }                                               \
                        if (unlikely(p->is_exotic)) {                   \
                            /* XXX: should avoid the slow path for arrays \
                               and typed arrays by ensuring that 'prop' is \
                               not numeric */                           \
                            obj = JS_MKPTR(JS_TAG_OBJECT, p);           \
                            goto name ## _slow_path;                    \
                        }                                               \
                        p = p->shape->proto;                            \
                        if (!p) {                                       \
                            val = JS_UNDEFINED;                         \
                            break;                                      \
                        }                                               \
                    }                                                   \
                } else {                                                \
                name ## _slow_path:                                     \
                    sf->cur_pc = pc;                                    \
                    val = JS_GetPropertyInternal(ctx, obj, atom, sp[-1], 0); \
                    if (unlikely(JS_IsException(val)))                  \
                        goto exception;                                 \
                }                                                       \
                if (keep) {                                             \
                    *sp++ = val;                                        \
                } else {                                                \
                    JS_FreeValue(ctx, sp[-1]);                          \
                    sp[-1] = val;                                       \
                }                                                       \
            }

            
        CASE(OP_get_field):
            GET_FIELD_INLINE(get_field, 0, 0);
            BREAK;

        CASE(OP_get_field2):
            GET_FIELD_INLINE(get_field2, 1, 0);
            BREAK;

#if SHORT_OPCODES
        CASE(OP_get_length):
            GET_FIELD_INLINE(get_length, 0, 1);
            BREAK;
#endif
            
        CASE(OP_put_field):
            {
                int ret;
                JSValue obj;
                JSAtom atom;
                JSObject *p;
                JSProperty *pr;
                JSShapeProperty *prs;

                atom = get_u32(pc);
                pc += 4;

                obj = sp[-2];
                if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT)) {
                    p = JS_VALUE_GET_OBJ(obj);
                    prs = find_own_property(&pr, p, atom);
                    if (!prs)
                        goto put_field_slow_path;
                    if (likely((prs->flags & (JS_PROP_TMASK | JS_PROP_WRITABLE |
                                              JS_PROP_LENGTH)) == JS_PROP_WRITABLE)) {
                        /* fast path */
                        set_value(ctx, &pr->u.value, sp[-1]);
                    } else {
                        goto put_field_slow_path;
                    }
                    JS_FreeValue(ctx, obj);
                    sp -= 2;
                } else {
                put_field_slow_path:
                    sf->cur_pc = pc;
                    ret = JS_SetPropertyInternal(ctx, obj, atom, sp[-1], obj,
                                                 JS_PROP_THROW_STRICT);
                    JS_FreeValue(ctx, obj);
                    sp -= 2;
                    if (unlikely(ret < 0))
                        goto exception;
                }
                
            }
            BREAK;

        CASE(OP_private_symbol):
            {
                JSAtom atom;
                JSValue val;

                atom = get_u32(pc);
                pc += 4;
                val = JS_NewSymbolFromAtom(ctx, atom, JS_ATOM_TYPE_PRIVATE);
                if (JS_IsException(val))
                    goto exception;
                *sp++ = val;
            }
            BREAK;

        CASE(OP_get_private_field):
            {
                JSValue val;

                val = JS_GetPrivateField(ctx, sp[-2], sp[-1]);
                JS_FreeValue(ctx, sp[-1]);
                JS_FreeValue(ctx, sp[-2]);
                sp[-2] = val;
                sp--;
                if (unlikely(JS_IsException(val)))
                    goto exception;
            }
            BREAK;

        CASE(OP_put_private_field):
            {
                int ret;
                ret = JS_SetPrivateField(ctx, sp[-3], sp[-1], sp[-2]);
                JS_FreeValue(ctx, sp[-3]);
                JS_FreeValue(ctx, sp[-1]);
                sp -= 3;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_define_private_field):
            {
                int ret;
                ret = JS_DefinePrivateField(ctx, sp[-3], sp[-2], sp[-1]);
                JS_FreeValue(ctx, sp[-2]);
                sp -= 2;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_define_field):
            {
                int ret;
                JSAtom atom;
                atom = get_u32(pc);
                pc += 4;

                ret = JS_DefinePropertyValue(ctx, sp[-2], atom, sp[-1],
                                             JS_PROP_C_W_E | JS_PROP_THROW);
                sp--;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_set_name):
            {
                int ret;
                JSAtom atom;
                atom = get_u32(pc);
                pc += 4;

                ret = JS_DefineObjectName(ctx, sp[-1], atom, JS_PROP_CONFIGURABLE);
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;
        CASE(OP_set_name_computed):
            {
                int ret;
                ret = JS_DefineObjectNameComputed(ctx, sp[-1], sp[-2], JS_PROP_CONFIGURABLE);
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;
        CASE(OP_set_proto):
            {
                JSValue proto;
                sf->cur_pc = pc;
                proto = sp[-1];
                if (JS_IsObject(proto) || JS_IsNull(proto)) {
                    if (JS_SetPrototypeInternal(ctx, sp[-2], proto, TRUE) < 0)
                        goto exception;
                }
                JS_FreeValue(ctx, proto);
                sp--;
            }
            BREAK;
        CASE(OP_set_home_object):
            js_method_set_home_object(ctx, sp[-1], sp[-2]);
            BREAK;
        CASE(OP_define_method):
        CASE(OP_define_method_computed):
            {
                JSValue getter, setter, value;
                JSValueConst obj;
                JSAtom atom;
                int flags, ret, op_flags;
                BOOL is_computed;
/* [OP_DEFINE_METHOD_* defined in compiler/js_opcode.h] */

                is_computed = (opcode == OP_define_method_computed);
                if (is_computed) {
                    atom = JS_ValueToAtom(ctx, sp[-2]);
                    if (unlikely(atom == JS_ATOM_NULL))
                        goto exception;
                    opcode += OP_define_method - OP_define_method_computed;
                } else {
                    atom = get_u32(pc);
                    pc += 4;
                }
                op_flags = *pc++;

                obj = sp[-2 - is_computed];
                flags = JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE |
                    JS_PROP_HAS_ENUMERABLE | JS_PROP_THROW;
                if (op_flags & OP_DEFINE_METHOD_ENUMERABLE)
                    flags |= JS_PROP_ENUMERABLE;
                op_flags &= 3;
                value = JS_UNDEFINED;
                getter = JS_UNDEFINED;
                setter = JS_UNDEFINED;
                if (op_flags == OP_DEFINE_METHOD_METHOD) {
                    value = sp[-1];
                    flags |= JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE;
                } else if (op_flags == OP_DEFINE_METHOD_GETTER) {
                    getter = sp[-1];
                    flags |= JS_PROP_HAS_GET;
                } else {
                    setter = sp[-1];
                    flags |= JS_PROP_HAS_SET;
                }
                ret = js_method_set_properties(ctx, sp[-1], atom, flags, obj);
                if (ret >= 0) {
                    ret = JS_DefineProperty(ctx, obj, atom, value,
                                            getter, setter, flags);
                }
                JS_FreeValue(ctx, sp[-1]);
                if (is_computed) {
                    JS_FreeAtom(ctx, atom);
                    JS_FreeValue(ctx, sp[-2]);
                }
                sp -= 1 + is_computed;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_define_class):
        CASE(OP_define_class_computed):
            {
                int class_flags;
                JSAtom atom;

                atom = get_u32(pc);
                class_flags = pc[4];
                pc += 5;
                if (js_op_define_class(ctx, sp, atom, class_flags,
                                       var_refs, sf,
                                       (opcode == OP_define_class_computed)) < 0)
                    goto exception;
            }
            BREAK;

#define GET_ARRAY_EL_INLINE(name, keep)                                 \
            {                                                           \
                JSValue val, obj, prop;                                 \
                JSObject *p;                                            \
                uint32_t idx;                                           \
                                                                        \
                obj = sp[-2];                                           \
                prop = sp[-1];                                          \
                if (likely(JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT &&    \
                           JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {     \
                    p = JS_VALUE_GET_OBJ(obj);                          \
                    idx = JS_VALUE_GET_INT(prop);                       \
                    if (unlikely(p->class_id != JS_CLASS_ARRAY))        \
                        goto name ## _slow_path;                        \
                    if (unlikely(idx >= p->u.array.count))              \
                        goto name ## _slow_path;                        \
                    val = JS_DupValue(ctx, p->u.array.u.values[idx]);   \
                } else {                                                \
                    name ## _slow_path:                                 \
                    sf->cur_pc = pc;                                    \
                    val = JS_GetPropertyValue(ctx, obj, prop);          \
                    if (unlikely(JS_IsException(val))) {                \
                        if (keep)                                       \
                            sp[-1] = JS_UNDEFINED;                      \
                        else                                            \
                            sp--;                                       \
                        goto exception;                                 \
                    }                                                   \
                }                                                       \
                if (keep) {                                             \
                    sp[-1] = val;                                       \
                } else {                                                \
                    JS_FreeValue(ctx, obj);                             \
                    sp[-2] = val;                                       \
                    sp--;                                               \
                }                                                       \
            }
            
        CASE(OP_get_array_el):
            GET_ARRAY_EL_INLINE(get_array_el, 0);
            BREAK;

        CASE(OP_get_array_el2):
            GET_ARRAY_EL_INLINE(get_array_el2, 1);
            BREAK;

        CASE(OP_get_array_el3):
            {
                JSValue val;
                JSObject *p;
                uint32_t idx;

                if (likely(JS_VALUE_GET_TAG(sp[-2]) == JS_TAG_OBJECT &&
                           JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_INT)) {
                    p = JS_VALUE_GET_OBJ(sp[-2]);
                    idx = JS_VALUE_GET_INT(sp[-1]);
                    if (unlikely(p->class_id != JS_CLASS_ARRAY))
                        goto get_array_el3_slow_path;
                    if (unlikely(idx >= p->u.array.count))
                        goto get_array_el3_slow_path;
                    val = JS_DupValue(ctx, p->u.array.u.values[idx]);
                } else {
                get_array_el3_slow_path:
                    switch (JS_VALUE_GET_TAG(sp[-1])) {
                    case JS_TAG_INT:
                    case JS_TAG_STRING:
                    case JS_TAG_SYMBOL:
                        /* undefined and null are tested in JS_GetPropertyValue() */
                        break;
                    default:
                        /* must be tested before JS_ToPropertyKey */
                        if (unlikely(JS_IsUndefined(sp[-2]) || JS_IsNull(sp[-2]))) {
                            JS_ThrowTypeError(ctx, "value has no property");
                            goto exception;
                        }
                        sf->cur_pc = pc;
                        ret_val = JS_ToPropertyKey(ctx, sp[-1]);
                        if (JS_IsException(ret_val))
                            goto exception;
                        JS_FreeValue(ctx, sp[-1]);
                        sp[-1] = ret_val;
                        break;
                    }
                    sf->cur_pc = pc;
                    val = JS_GetPropertyValue(ctx, sp[-2], JS_DupValue(ctx, sp[-1]));
                    if (unlikely(JS_IsException(val)))
                        goto exception;
                }
                *sp++ = val;
            }
            BREAK;
            
        CASE(OP_get_ref_value):
            {
                JSValue val;
                JSAtom atom;
                int ret;
                
                sf->cur_pc = pc;
                atom = JS_ValueToAtom(ctx, sp[-1]);
                if (atom == JS_ATOM_NULL)
                    goto exception;
                if (unlikely(JS_IsUndefined(sp[-2]))) {
                    JS_ThrowReferenceErrorNotDefined(ctx, atom);
                    JS_FreeAtom(ctx, atom);
                    goto exception;
                }
                ret = JS_HasProperty(ctx, sp[-2], atom);
                if (unlikely(ret <= 0)) {
                    if (ret < 0) {
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    }
                    if (is_strict_mode(ctx)) {
                        JS_ThrowReferenceErrorNotDefined(ctx, atom);
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    } 
                    val = JS_UNDEFINED;
                } else {
                    val = JS_GetProperty(ctx, sp[-2], atom);
                }
                JS_FreeAtom(ctx, atom);
                if (unlikely(JS_IsException(val)))
                    goto exception;
                sp[0] = val;
                sp++;
            }
            BREAK;

        CASE(OP_get_super_value):
            {
                JSValue val;
                JSAtom atom;
                sf->cur_pc = pc;
                atom = JS_ValueToAtom(ctx, sp[-1]);
                if (unlikely(atom == JS_ATOM_NULL))
                    goto exception;
                val = JS_GetPropertyInternal(ctx, sp[-2], atom, sp[-3], FALSE);
                JS_FreeAtom(ctx, atom);
                if (unlikely(JS_IsException(val)))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-3]);
                sp[-3] = val;
                sp -= 2;
            }
            BREAK;

        CASE(OP_put_array_el):
            {
                int ret;
                JSObject *p;
                uint32_t idx;

                if (likely(JS_VALUE_GET_TAG(sp[-3]) == JS_TAG_OBJECT &&
                           JS_VALUE_GET_TAG(sp[-2]) == JS_TAG_INT)) {
                    p = JS_VALUE_GET_OBJ(sp[-3]);
                    idx = JS_VALUE_GET_INT(sp[-2]);
                    if (unlikely(p->class_id != JS_CLASS_ARRAY))
                        goto put_array_el_slow_path;
                    if (unlikely(idx >= (uint32_t)p->u.array.count)) {
                        uint32_t new_len, array_len;
                        if (unlikely(idx != (uint32_t)p->u.array.count ||
                                     !p->fast_array ||
                                     !can_extend_fast_array(p))) {
                            goto put_array_el_slow_path;
                        }
                        if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) != JS_TAG_INT))
                            goto put_array_el_slow_path;
                        /* cannot overflow otherwise the length would not be an integer */
                        new_len = idx + 1;
                        if (unlikely(new_len > p->u.array.u1.size))
                            goto put_array_el_slow_path;
                        array_len = JS_VALUE_GET_INT(p->prop[0].u.value);
                        if (new_len > array_len) {
                            if (unlikely(!(get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE)))
                                goto put_array_el_slow_path;
                            p->prop[0].u.value = JS_NewInt32(ctx, new_len);
                        }
                        p->u.array.count = new_len;
                        p->u.array.u.values[idx] = sp[-1];
                    } else {
                        set_value(ctx, &p->u.array.u.values[idx], sp[-1]);
                    }
                    JS_FreeValue(ctx, sp[-3]);
                    sp -= 3;
                } else {
                put_array_el_slow_path:
                    sf->cur_pc = pc;
                    ret = JS_SetPropertyValue(ctx, sp[-3], sp[-2], sp[-1], JS_PROP_THROW_STRICT);
                    JS_FreeValue(ctx, sp[-3]);
                    sp -= 3;
                    if (unlikely(ret < 0))
                        goto exception;
                }
            }
            BREAK;

        CASE(OP_put_ref_value):
            {
                int ret;
                JSAtom atom;
                sf->cur_pc = pc;
                atom = JS_ValueToAtom(ctx, sp[-2]);
                if (unlikely(atom == JS_ATOM_NULL))
                    goto exception;
                if (unlikely(JS_IsUndefined(sp[-3]))) {
                    if (is_strict_mode(ctx)) {
                        JS_ThrowReferenceErrorNotDefined(ctx, atom);
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    } else {
                        sp[-3] = JS_DupValue(ctx, ctx->global_obj);
                    }
                }
                ret = JS_HasProperty(ctx, sp[-3], atom);
                if (unlikely(ret <= 0)) {
                    if (unlikely(ret < 0)) {
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    }
                    if (is_strict_mode(ctx)) {
                        JS_ThrowReferenceErrorNotDefined(ctx, atom);
                        JS_FreeAtom(ctx, atom);
                        goto exception;
                    }
                }
                ret = JS_SetPropertyInternal(ctx, sp[-3], atom, sp[-1], sp[-3], JS_PROP_THROW_STRICT);
                JS_FreeAtom(ctx, atom);
                JS_FreeValue(ctx, sp[-2]);
                JS_FreeValue(ctx, sp[-3]);
                sp -= 3;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_put_super_value):
            {
                int ret;
                JSAtom atom;
                sf->cur_pc = pc;
                if (JS_VALUE_GET_TAG(sp[-3]) != JS_TAG_OBJECT) {
                    JS_ThrowTypeErrorNotAnObject(ctx);
                    goto exception;
                }
                atom = JS_ValueToAtom(ctx, sp[-2]);
                if (unlikely(atom == JS_ATOM_NULL))
                    goto exception;
                ret = JS_SetPropertyInternal(ctx, sp[-3], atom, sp[-1], sp[-4],
                                             JS_PROP_THROW_STRICT);
                JS_FreeAtom(ctx, atom);
                JS_FreeValue(ctx, sp[-4]);
                JS_FreeValue(ctx, sp[-3]);
                JS_FreeValue(ctx, sp[-2]);
                sp -= 4;
                if (ret < 0)
                    goto exception;
            }
            BREAK;

        CASE(OP_define_array_el):
            {
                int ret;
                ret = JS_DefinePropertyValueValue(ctx, sp[-3], JS_DupValue(ctx, sp[-2]), sp[-1],
                                                  JS_PROP_C_W_E | JS_PROP_THROW);
                sp -= 1;
                if (unlikely(ret < 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_append):    /* array pos enumobj -- array pos */
            {
                sf->cur_pc = pc;
                if (js_append_enumerate(ctx, sp))
                    goto exception;
                JS_FreeValue(ctx, *--sp);
            }
            BREAK;

        CASE(OP_copy_data_properties):    /* target source excludeList */
            {
                /* stack offsets (-1 based):
                   2 bits for target,
                   3 bits for source,
                   2 bits for exclusionList */
                int mask;

                mask = *pc++;
                sf->cur_pc = pc;
                if (JS_CopyDataProperties(ctx, sp[-1 - (mask & 3)],
                                          sp[-1 - ((mask >> 2) & 7)],
                                          sp[-1 - ((mask >> 5) & 7)], 0))
                    goto exception;
            }
            BREAK;

        CASE(OP_add):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int64_t r;
                    r = (int64_t)JS_VALUE_GET_INT(op1) + JS_VALUE_GET_INT(op2);
                    if (unlikely((int)r != r)) {
                        sp[-2] = __JS_NewFloat64(ctx, (double)r);
                    } else {
                        sp[-2] = JS_NewInt32(ctx, r);
                    }
                    sp--;
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                    double d1, d2;
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {
                        d1 = JS_VALUE_GET_FLOAT64(op1);
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                        d1 = JS_VALUE_GET_INT(op1);
                    } else {
                        goto add_slow_case;
                    }
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                        d2 = JS_VALUE_GET_FLOAT64(op2);
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {
                        d2 = JS_VALUE_GET_INT(op2);
                    } else {
                        goto add_slow_case;
                    }
                    sp[-2] = __JS_NewFloat64(ctx, d1 + d2);
                    sp--;
                } else if (JS_IsString(op1) && JS_IsString(op2)) {
                    sp[-2] = JS_ConcatString(ctx, op1, op2);
                    sp--;
                    if (JS_IsException(sp[-1]))
                        goto exception;
                } else {
                add_slow_case:
                    sf->cur_pc = pc;
                    if (js_add_slow(ctx, sp))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_add_loc):
            {
                JSValue op2;
                JSValue *pv;
                int idx;
                idx = *pc;
                pc += 1;

                op2 = sp[-1];
                pv = &var_buf[idx];
                if (likely(JS_VALUE_IS_BOTH_INT(*pv, op2))) {
                    int64_t r;
                    r = (int64_t)JS_VALUE_GET_INT(*pv) + JS_VALUE_GET_INT(op2);
                    if (unlikely((int)r != r)) {
                        *pv = __JS_NewFloat64(ctx, (double)r);
                    } else {
                        *pv = JS_NewInt32(ctx, r);
                    }
                    sp--;
                } else if (JS_VALUE_IS_BOTH_FLOAT(*pv, op2)) {
                    *pv = __JS_NewFloat64(ctx, JS_VALUE_GET_FLOAT64(*pv) +
                                               JS_VALUE_GET_FLOAT64(op2));
                    sp--;
                } else if (JS_VALUE_GET_TAG(*pv) == JS_TAG_STRING &&
                           JS_VALUE_GET_TAG(op2) == JS_TAG_STRING) {
                    sp--;
                    sf->cur_pc = pc;
                    if (JS_ConcatStringInPlace(ctx, JS_VALUE_GET_STRING(*pv), op2)) {
                        JS_FreeValue(ctx, op2);
                    } else {
                        op2 = JS_ConcatString(ctx, JS_DupValue(ctx, *pv), op2);
                        if (JS_IsException(op2))
                            goto exception;
                        set_value(ctx, pv, op2);
                    }
                } else {
                    JSValue ops[2];
                    /* In case of exception, js_add_slow frees ops[0]
                       and ops[1], so we must duplicate *pv */
                    sf->cur_pc = pc;
                    ops[0] = JS_DupValue(ctx, *pv);
                    ops[1] = op2;
                    sp--;
                    if (js_add_slow(ctx, ops + 2))
                        goto exception;
                    set_value(ctx, pv, ops[0]);
                }
            }
            BREAK;
        CASE(OP_sub):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int64_t r;
                    r = (int64_t)JS_VALUE_GET_INT(op1) - JS_VALUE_GET_INT(op2);
                    if (unlikely((int)r != r)) {
                        sp[-2] = __JS_NewFloat64(ctx, (double)r);
                    } else {
                        sp[-2] = JS_NewInt32(ctx, r);
                    }
                    sp--;
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                    double d1, d2;
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {
                        d1 = JS_VALUE_GET_FLOAT64(op1);
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                        d1 = JS_VALUE_GET_INT(op1);
                    } else {
                        goto binary_arith_slow;
                    }
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                        d2 = JS_VALUE_GET_FLOAT64(op2);
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {
                        d2 = JS_VALUE_GET_INT(op2);
                    } else {
                        goto binary_arith_slow;
                    }
                    sp[-2] = __JS_NewFloat64(ctx, d1 - d2);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_mul):
            {
                JSValue op1, op2;
                double d;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int32_t v1, v2;
                    int64_t r;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    r = (int64_t)v1 * v2;
                    if (unlikely((int)r != r)) {
                        d = (double)r;
                        goto mul_fp_res;
                    }
                    /* need to test zero case for -0 result */
                    if (unlikely(r == 0 && (v1 | v2) < 0)) {
                        d = -0.0;
                        goto mul_fp_res;
                    }
                    sp[-2] = JS_NewInt32(ctx, r);
                    sp--;
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                    double d1, d2;
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {
                        d1 = JS_VALUE_GET_FLOAT64(op1);
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                        d1 = JS_VALUE_GET_INT(op1);
                    } else {
                        goto binary_arith_slow;
                    }
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {
                        d2 = JS_VALUE_GET_FLOAT64(op2);
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {
                        d2 = JS_VALUE_GET_INT(op2);
                    } else {
                        goto binary_arith_slow;
                    }
                    d = d1 * d2;
                mul_fp_res:
                    sp[-2] = __JS_NewFloat64(ctx, d);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_div):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int v1, v2;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    sp[-2] = JS_NewFloat64(ctx, (double)v1 / (double)v2);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_mod):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    int v1, v2, r;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    /* We must avoid v2 = 0, v1 = INT32_MIN and v2 =
                       -1 and the cases where the result is -0. */
                    if (unlikely(v1 < 0 || v2 <= 0))
                        goto binary_arith_slow;
                    r = v1 % v2;
                    sp[-2] = JS_NewInt32(ctx, r);
                    sp--;
                } else {
                    goto binary_arith_slow;
                }
            }
            BREAK;
        CASE(OP_pow):
        binary_arith_slow:
            sf->cur_pc = pc;
            if (js_binary_arith_slow(ctx, sp, opcode))
                goto exception;
            sp--;
            BREAK;

        CASE(OP_plus):
            {
                JSValue op1;
                uint32_t tag;
                op1 = sp[-1];
                tag = JS_VALUE_GET_TAG(op1);
                if (tag == JS_TAG_INT || JS_TAG_IS_FLOAT64(tag)) {
                } else if (tag == JS_TAG_NULL || tag == JS_TAG_BOOL) {
                    sp[-1] = JS_NewInt32(ctx, JS_VALUE_GET_INT(op1));
                } else {
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_neg):
            {
                JSValue op1;
                uint32_t tag;
                int val;
                double d;
                op1 = sp[-1];
                tag = JS_VALUE_GET_TAG(op1);
                if (tag == JS_TAG_INT ||
                    tag == JS_TAG_BOOL ||
                    tag == JS_TAG_NULL) {
                    val = JS_VALUE_GET_INT(op1);
                    /* Note: -0 cannot be expressed as integer */
                    if (unlikely(val == 0)) {
                        d = -0.0;
                        goto neg_fp_res;
                    }
                    if (unlikely(val == INT32_MIN)) {
                        d = -(double)val;
                        goto neg_fp_res;
                    }
                    sp[-1] = JS_NewInt32(ctx, -val);
                } else if (JS_TAG_IS_FLOAT64(tag)) {
                    d = -JS_VALUE_GET_FLOAT64(op1);
                neg_fp_res:
                    sp[-1] = __JS_NewFloat64(ctx, d);
                } else {
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_inc):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MAX))
                        goto inc_slow;
                    sp[-1] = JS_NewInt32(ctx, val + 1);
                } else {
                inc_slow:
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_dec):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MIN))
                        goto dec_slow;
                    sp[-1] = JS_NewInt32(ctx, val - 1);
                } else {
                dec_slow:
                    sf->cur_pc = pc;
                    if (js_unary_arith_slow(ctx, sp, opcode))
                        goto exception;
                }
            }
            BREAK;
        CASE(OP_post_inc):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MAX))
                        goto post_inc_slow;
                    sp[0] = JS_NewInt32(ctx, val + 1);
                } else {
                post_inc_slow:
                    sf->cur_pc = pc;
                    if (js_post_inc_slow(ctx, sp, opcode))
                        goto exception;
                }
                sp++;
            }
            BREAK;
        CASE(OP_post_dec):
            {
                JSValue op1;
                int val;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MIN))
                        goto post_dec_slow;
                    sp[0] = JS_NewInt32(ctx, val - 1);
                } else {
                post_dec_slow:
                    sf->cur_pc = pc;
                    if (js_post_inc_slow(ctx, sp, opcode))
                        goto exception;
                }
                sp++;
            }
            BREAK;
        CASE(OP_inc_loc):
            {
                JSValue op1;
                int val;
                int idx;
                idx = *pc;
                pc += 1;

                op1 = var_buf[idx];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MAX))
                        goto inc_loc_slow;
                    var_buf[idx] = JS_NewInt32(ctx, val + 1);
                } else {
                inc_loc_slow:
                    sf->cur_pc = pc;
                    /* must duplicate otherwise the variable value may
                       be destroyed before JS code accesses it */
                    op1 = JS_DupValue(ctx, op1);
                    if (js_unary_arith_slow(ctx, &op1 + 1, OP_inc))
                        goto exception;
                    set_value(ctx, &var_buf[idx], op1);
                }
            }
            BREAK;
        CASE(OP_dec_loc):
            {
                JSValue op1;
                int val;
                int idx;
                idx = *pc;
                pc += 1;

                op1 = var_buf[idx];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    val = JS_VALUE_GET_INT(op1);
                    if (unlikely(val == INT32_MIN))
                        goto dec_loc_slow;
                    var_buf[idx] = JS_NewInt32(ctx, val - 1);
                } else {
                dec_loc_slow:
                    sf->cur_pc = pc;
                    /* must duplicate otherwise the variable value may
                       be destroyed before JS code accesses it */
                    op1 = JS_DupValue(ctx, op1);
                    if (js_unary_arith_slow(ctx, &op1 + 1, OP_dec))
                        goto exception;
                    set_value(ctx, &var_buf[idx], op1);
                }
            }
            BREAK;
        CASE(OP_not):
            {
                JSValue op1;
                op1 = sp[-1];
                if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {
                    sp[-1] = JS_NewInt32(ctx, ~JS_VALUE_GET_INT(op1));
                } else {
                    sf->cur_pc = pc;
                    if (js_not_slow(ctx, sp))
                        goto exception;
                }
            }
            BREAK;

        CASE(OP_shl):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    uint32_t v1, v2;
                    v1 = JS_VALUE_GET_INT(op1);
                    v2 = JS_VALUE_GET_INT(op2);
                    v2 &= 0x1f;
                    sp[-2] = JS_NewInt32(ctx, v1 << v2);
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_shr):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    uint32_t v2;
                    v2 = JS_VALUE_GET_INT(op2);
                    v2 &= 0x1f;
                    sp[-2] = JS_NewUint32(ctx,
                                          (uint32_t)JS_VALUE_GET_INT(op1) >>
                                          v2);
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_shr_slow(ctx, sp))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_sar):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    uint32_t v2;
                    v2 = JS_VALUE_GET_INT(op2);
                    v2 &= 0x1f;
                    sp[-2] = JS_NewInt32(ctx,
                                          (int)JS_VALUE_GET_INT(op1) >> v2);
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_and):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    sp[-2] = JS_NewInt32(ctx,
                                         JS_VALUE_GET_INT(op1) &
                                         JS_VALUE_GET_INT(op2));
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_or):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    sp[-2] = JS_NewInt32(ctx,
                                         JS_VALUE_GET_INT(op1) |
                                         JS_VALUE_GET_INT(op2));
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;
        CASE(OP_xor):
            {
                JSValue op1, op2;
                op1 = sp[-2];
                op2 = sp[-1];
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {
                    sp[-2] = JS_NewInt32(ctx,
                                         JS_VALUE_GET_INT(op1) ^
                                         JS_VALUE_GET_INT(op2));
                    sp--;
                } else {
                    sf->cur_pc = pc;
                    if (js_binary_logic_slow(ctx, sp, opcode))
                        goto exception;
                    sp--;
                }
            }
            BREAK;


#define OP_CMP(opcode, binary_op, slow_call)                            \
            CASE(opcode):                                               \
                {                                                       \
                JSValue op1, op2;                                       \
                op1 = sp[-2];                                           \
                op2 = sp[-1];                                           \
                if (likely(JS_VALUE_IS_BOTH_INT(op1, op2))) {           \
                    sp[-2] = JS_NewBool(ctx, JS_VALUE_GET_INT(op1) binary_op JS_VALUE_GET_INT(op2)); \
                    sp--;                                               \
                } else if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1)) ||  \
                           JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {  \
                    double d1, d2;                                      \
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op1))) {     \
                        d1 = JS_VALUE_GET_FLOAT64(op1);                 \
                    } else if (JS_VALUE_GET_TAG(op1) == JS_TAG_INT) {   \
                        d1 = JS_VALUE_GET_INT(op1);                     \
                    } else {                                            \
                        goto opcode ## _slow_case;                      \
                    }                                                   \
                    if (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(op2))) {     \
                        d2 = JS_VALUE_GET_FLOAT64(op2);                 \
                    } else if (JS_VALUE_GET_TAG(op2) == JS_TAG_INT) {   \
                        d2 = JS_VALUE_GET_INT(op2);                     \
                    } else {                                            \
                        goto opcode ## _slow_case;                      \
                    }                                                   \
                    sp[-2] = JS_NewBool(ctx, d1 binary_op d2);          \
                    sp--;                                               \
                } else {                                                \
                opcode ## _slow_case:                                   \
                    sf->cur_pc = pc;                                    \
                    if (slow_call)                                      \
                        goto exception;                                 \
                    sp--;                                               \
                }                                                       \
                }                                                       \
            BREAK

            OP_CMP(OP_lt, <, js_relational_slow(ctx, sp, opcode));
            OP_CMP(OP_lte, <=, js_relational_slow(ctx, sp, opcode));
            OP_CMP(OP_gt, >, js_relational_slow(ctx, sp, opcode));
            OP_CMP(OP_gte, >=, js_relational_slow(ctx, sp, opcode));

#define OP_CMP_EQ(opcode, inv)                                          \
            CASE(opcode):                                               \
                {                                                       \
                JSValue op1, op2;                                       \
                int res;                                                \
                uint32_t tag1, tag2;                                    \
                op1 = sp[-2];                                           \
                op2 = sp[-1];                                           \
                tag1 = JS_VALUE_GET_TAG(op1);                           \
                tag2 = JS_VALUE_GET_TAG(op2);                           \
                if (likely(tag1 == JS_TAG_INT)) {                       \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_INT(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (JS_TAG_IS_FLOAT64(tag1)) {                   \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (tag1 == JS_TAG_OBJECT) {                     \
                    if (tag2 == JS_TAG_NULL || tag2 == JS_TAG_UNDEFINED) { \
                        JSObject *p = JS_VALUE_GET_OBJ(op1);            \
                        res = p->is_HTMLDDA;                            \
                        JS_FreeValue(ctx, op1);                         \
                    } else if (tag2 == JS_TAG_OBJECT) {                 \
                        res = JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2); \
                        JS_FreeValue(ctx, op1);                         \
                        JS_FreeValue(ctx, op2);                         \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (tag1 == JS_TAG_NULL || tag1 == JS_TAG_UNDEFINED) { \
                    if (tag2 == JS_TAG_NULL || tag2 == JS_TAG_UNDEFINED) { \
                        res = TRUE;                                     \
                    } else if (tag2 == JS_TAG_OBJECT) {                 \
                        JSObject *p = JS_VALUE_GET_OBJ(op2);            \
                        res = p->is_HTMLDDA;                            \
                        JS_FreeValue(ctx, op2);                         \
                    } else {                                            \
                        goto slow_eq ## inv;                            \
                    }                                                   \
                } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) { \
                    res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),   \
                                       JS_VALUE_GET_STRING(op2));       \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                } else {                                                \
                    slow_eq ## inv:                                     \
                    sf->cur_pc = pc;                                    \
                    if (js_eq_slow(ctx, sp, inv))                       \
                        goto exception;                                 \
                    sp--;                                               \
                    goto slow_eq_done ## inv;                           \
                }                                                       \
                sp[-2] = JS_NewBool(ctx, res ^ inv);                    \
                sp--;                                                   \
                slow_eq_done ## inv: ;                                  \
                }                                                       \
            BREAK

            OP_CMP_EQ(OP_eq, 0);
            OP_CMP_EQ(OP_neq, 1);

#define OP_CMP_STRICT_EQ(opcode, inv)                                   \
            CASE(opcode):                                               \
                {                                                       \
                JSValue op1, op2;                                       \
                int res;                                                \
                uint32_t tag1, tag2;                                    \
                op1 = sp[-2];                                           \
                op2 = sp[-1];                                           \
                tag1 = JS_VALUE_GET_TAG(op1);                           \
                tag2 = JS_VALUE_GET_TAG(op2);                           \
                if (likely(tag1 == JS_TAG_INT)) {                       \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_INT(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        JS_FreeValue(ctx, op2);                         \
                        res = FALSE;                                    \
                    }                                                   \
                } else if (JS_TAG_IS_FLOAT64(tag1)) {                   \
                    if (tag2 == JS_TAG_INT) {                           \
                        res = JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_INT(op2); \
                    } else if (JS_TAG_IS_FLOAT64(tag2)) {               \
                        res = (JS_VALUE_GET_FLOAT64(op1) == JS_VALUE_GET_FLOAT64(op2)); \
                    } else {                                            \
                        JS_FreeValue(ctx, op2);                         \
                        res = FALSE;                                    \
                    }                                                   \
                } else if (tag1 == JS_TAG_OBJECT) {                     \
                    if (tag2 == JS_TAG_OBJECT) {                        \
                        res = JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2); \
                    } else {                                            \
                        res = FALSE;                                    \
                    }                                                   \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                } else if (tag1 == JS_TAG_NULL || tag1 == JS_TAG_UNDEFINED) { \
                    res = (tag1 == tag2);                               \
                    JS_FreeValue(ctx, op2);                             \
                } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) { \
                    res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),   \
                                       JS_VALUE_GET_STRING(op2));       \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                } else {                                                \
                    res = js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);   \
                    JS_FreeValue(ctx, op1);                             \
                    JS_FreeValue(ctx, op2);                             \
                }                                                       \
                sp[-2] = JS_NewBool(ctx, res ^ inv);                    \
                sp--;                                                   \
                }                                                       \
            BREAK

            OP_CMP_STRICT_EQ(OP_strict_eq, 0);
            OP_CMP_STRICT_EQ(OP_strict_neq, 1);

        CASE(OP_in):
            sf->cur_pc = pc;
            if (js_operator_in(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_private_in):
            sf->cur_pc = pc;
            if (js_operator_private_in(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_instanceof):
            sf->cur_pc = pc;
            if (js_operator_instanceof(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_typeof):
            {
                JSValue op1;
                JSAtom atom;

                op1 = sp[-1];
                atom = js_operator_typeof(ctx, op1);
                JS_FreeValue(ctx, op1);
                sp[-1] = JS_AtomToString(ctx, atom);
            }
            BREAK;
        CASE(OP_delete):
            sf->cur_pc = pc;
            if (js_operator_delete(ctx, sp))
                goto exception;
            sp--;
            BREAK;
        CASE(OP_delete_var):
            {
                JSAtom atom;
                int ret;

                atom = get_u32(pc);
                pc += 4;
                sf->cur_pc = pc;

                ret = JS_DeleteGlobalVar(ctx, atom);
                if (unlikely(ret < 0))
                    goto exception;
                *sp++ = JS_NewBool(ctx, ret);
            }
            BREAK;

        CASE(OP_to_object):
            if (JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_OBJECT) {
                sf->cur_pc = pc;
                ret_val = JS_ToObject(ctx, sp[-1]);
                if (JS_IsException(ret_val))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret_val;
            }
            BREAK;

        CASE(OP_to_propkey):
            switch (JS_VALUE_GET_TAG(sp[-1])) {
            case JS_TAG_INT:
            case JS_TAG_STRING:
            case JS_TAG_SYMBOL:
                break;
            default:
                sf->cur_pc = pc;
                ret_val = JS_ToPropertyKey(ctx, sp[-1]);
                if (JS_IsException(ret_val))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret_val;
                break;
            }
            BREAK;

#if 0
        CASE(OP_to_string):
            if (JS_VALUE_GET_TAG(sp[-1]) != JS_TAG_STRING) {
                ret_val = JS_ToString(ctx, sp[-1]);
                if (JS_IsException(ret_val))
                    goto exception;
                JS_FreeValue(ctx, sp[-1]);
                sp[-1] = ret_val;
            }
            BREAK;
#endif
        CASE(OP_with_get_var):
        CASE(OP_with_put_var):
        CASE(OP_with_delete_var):
        CASE(OP_with_make_ref):
        CASE(OP_with_get_ref):
            {
                JSAtom atom;
                int32_t diff;
                JSValue obj, val;
                int ret, is_with;
                atom = get_u32(pc);
                diff = get_u32(pc + 4);
                is_with = pc[8];
                pc += 9;
                sf->cur_pc = pc;

                obj = sp[-1];
                ret = JS_HasProperty(ctx, obj, atom);
                if (unlikely(ret < 0))
                    goto exception;
                if (ret) {
                    if (is_with) {
                        ret = js_has_unscopable(ctx, obj, atom);
                        if (unlikely(ret < 0))
                            goto exception;
                        if (ret)
                            goto no_with;
                    }
                    switch (opcode) {
                    case OP_with_get_var:
                        /* in Object Environment Records, GetBindingValue() calls HasProperty() */
                        ret = JS_HasProperty(ctx, obj, atom);
                        if (unlikely(ret <= 0)) {
                            if (ret < 0)
                                goto exception;
                            if (is_strict_mode(ctx)) {
                                JS_ThrowReferenceErrorNotDefined(ctx, atom);
                                goto exception;
                            } 
                            val = JS_UNDEFINED;
                        } else {
                            val = JS_GetProperty(ctx, obj, atom);
                            if (unlikely(JS_IsException(val)))
                                goto exception;
                        }
                        set_value(ctx, &sp[-1], val);
                        break;
                    case OP_with_put_var: /* used e.g. in for in/of */
                        /* in Object Environment Records, SetMutableBinding() calls HasProperty() */
                        ret = JS_HasProperty(ctx, obj, atom);
                        if (unlikely(ret <= 0)) {
                            if (ret < 0)
                                goto exception;
                            if (is_strict_mode(ctx)) {
                                JS_ThrowReferenceErrorNotDefined(ctx, atom);
                                goto exception;
                            } 
                        }
                        ret = JS_SetPropertyInternal(ctx, obj, atom, sp[-2], obj,
                                                     JS_PROP_THROW_STRICT);
                        JS_FreeValue(ctx, sp[-1]);
                        sp -= 2;
                        if (unlikely(ret < 0))
                            goto exception;
                        break;
                    case OP_with_delete_var:
                        ret = JS_DeleteProperty(ctx, obj, atom, 0);
                        if (unlikely(ret < 0))
                            goto exception;
                        JS_FreeValue(ctx, sp[-1]);
                        sp[-1] = JS_NewBool(ctx, ret);
                        break;
                    case OP_with_make_ref:
                        /* produce a pair object/propname on the stack */
                        *sp++ = JS_AtomToValue(ctx, atom);
                        break;
                    case OP_with_get_ref:
                        /* produce a pair object/method on the stack */
                        /* in Object Environment Records, GetBindingValue() calls HasProperty() */
                        ret = JS_HasProperty(ctx, obj, atom);
                        if (unlikely(ret < 0))
                            goto exception;
                        if (!ret) {
                            val = JS_UNDEFINED;
                        } else {
                            val = JS_GetProperty(ctx, obj, atom);
                            if (unlikely(JS_IsException(val)))
                                goto exception;
                        }
                        *sp++ = val;
                        break;
                    }
                    pc += diff - 5;
                } else {
                no_with:
                    /* if not jumping, drop the object argument */
                    JS_FreeValue(ctx, sp[-1]);
                    sp--;
                }
            }
            BREAK;

        CASE(OP_await):
            ret_val = JS_NewInt32(ctx, FUNC_RET_AWAIT);
            goto done_generator;
        CASE(OP_yield):
            ret_val = JS_NewInt32(ctx, FUNC_RET_YIELD);
            goto done_generator;
        CASE(OP_yield_star):
        CASE(OP_async_yield_star):
            ret_val = JS_NewInt32(ctx, FUNC_RET_YIELD_STAR);
            goto done_generator;
        CASE(OP_return_async):
            ret_val = JS_UNDEFINED;
            goto done_generator;
        CASE(OP_initial_yield):
            ret_val = JS_NewInt32(ctx, FUNC_RET_INITIAL_YIELD);
            goto done_generator;

        CASE(OP_nop):
            BREAK;
        CASE(OP_is_undefined_or_null):
            if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_UNDEFINED ||
                JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_NULL) {
                goto set_true;
            } else {
                goto free_and_set_false;
            }
#if SHORT_OPCODES
        CASE(OP_is_undefined):
            if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_UNDEFINED) {
                goto set_true;
            } else {
                goto free_and_set_false;
            }
        CASE(OP_is_null):
            if (JS_VALUE_GET_TAG(sp[-1]) == JS_TAG_NULL) {
                goto set_true;
            } else {
                goto free_and_set_false;
            }
            /* XXX: could merge to a single opcode */
        CASE(OP_typeof_is_undefined):
            /* different from OP_is_undefined because of isHTMLDDA */
            if (js_operator_typeof(ctx, sp[-1]) == JS_ATOM_undefined) {
                goto free_and_set_true;
            } else {
                goto free_and_set_false;
            }
        CASE(OP_typeof_is_function):
            if (js_operator_typeof(ctx, sp[-1]) == JS_ATOM_function) {
                goto free_and_set_true;
            } else {
                goto free_and_set_false;
            }
        free_and_set_true:
            JS_FreeValue(ctx, sp[-1]);
#endif
        set_true:
            sp[-1] = JS_TRUE;
            BREAK;
        free_and_set_false:
            JS_FreeValue(ctx, sp[-1]);
            sp[-1] = JS_FALSE;
            BREAK;
        CASE(OP_invalid):
        DEFAULT:
            JS_ThrowInternalError(ctx, "invalid opcode: pc=%u opcode=0x%02x",
                                  (int)(pc - b->byte_code_buf - 1), opcode);
            goto exception;
        }
    }
 exception:
    if (is_backtrace_needed(ctx, rt->current_exception)) {
        /* add the backtrace information now (it is not done
           before if the exception happens in a bytecode
           operation */
        sf->cur_pc = pc;
        build_backtrace(ctx, rt->current_exception, NULL, 0, 0, 0);
    }
    if (!rt->current_exception_is_uncatchable) {
        while (sp > stack_buf) {
            JSValue val = *--sp;
            JS_FreeValue(ctx, val);
            if (JS_VALUE_GET_TAG(val) == JS_TAG_CATCH_OFFSET) {
                int pos = JS_VALUE_GET_INT(val);
                if (pos == 0) {
                    /* enumerator: close it with a throw */
                    JS_FreeValue(ctx, sp[-1]); /* drop the next method */
                    sp--;
                    JS_IteratorClose(ctx, sp[-1], TRUE);
                } else {
                    *sp++ = rt->current_exception;
                    rt->current_exception = JS_UNINITIALIZED;
                    pc = b->byte_code_buf + pos;
                    goto restart;
                }
            }
        }
    }
    ret_val = JS_EXCEPTION;
    /* the local variables are freed by the caller in the generator
       case. Hence the label 'done' should never be reached in a
       generator function. */
    if (b->func_kind != JS_FUNC_NORMAL) {
    done_generator:
        sf->cur_pc = pc;
        sf->cur_sp = sp;
    } else {
    done:
        if (unlikely(b->var_ref_count != 0)) {
            /* variable references reference the stack: must close them */
            close_var_refs(rt, b, sf);
        }
        /* free the local variables and stack */
        for(pval = local_buf; pval < sp; pval++) {
            JS_FreeValue(ctx, *pval);
        }
    }
    rt->current_stack_frame = sf->prev_frame;
    return ret_val;
}

#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic pop
#endif

JSValue JS_Call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
                int argc, JSValueConst *argv)
{
    return JS_CallInternal(ctx, func_obj, this_obj, JS_UNDEFINED,
                           argc, (JSValue *)argv, JS_CALL_FLAG_COPY_ARGV);
}

JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                    int argc, JSValueConst *argv)
{
    JSValue res = JS_CallInternal(ctx, func_obj, this_obj, JS_UNDEFINED,
                                  argc, (JSValue *)argv, JS_CALL_FLAG_COPY_ARGV);
    JS_FreeValue(ctx, func_obj);
    return res;
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

/* argv[] is modified if (flags & JS_CALL_FLAG_COPY_ARGV) = 0. */
static JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags)
{
    JSObject *p;
    JSFunctionBytecode *b;

    if (js_poll_interrupts(ctx))
        return JS_EXCEPTION;
    flags |= JS_CALL_FLAG_CONSTRUCTOR;
    if (unlikely(JS_VALUE_GET_TAG(func_obj) != JS_TAG_OBJECT))
        goto not_a_function;
    p = JS_VALUE_GET_OBJ(func_obj);
    if (unlikely(!p->is_constructor))
        return JS_ThrowTypeErrorNotAConstructor(ctx, func_obj);
    if (unlikely(p->class_id != JS_CLASS_BYTECODE_FUNCTION)) {
        JSClassCall *call_func;
        call_func = ctx->rt->class_array[p->class_id].call;
        if (!call_func) {
        not_a_function:
            return JS_ThrowTypeError(ctx, "not a function");
        }
        return call_func(ctx, func_obj, new_target, argc,
                         (JSValueConst *)argv, flags);
    }

    b = p->u.func.function_bytecode;
    if (b->is_derived_class_constructor) {
        return JS_CallInternal(ctx, func_obj, JS_UNDEFINED, new_target, argc, argv, flags);
    } else {
        JSValue obj, ret;
        /* legacy constructor behavior */
        obj = js_create_from_ctor(ctx, new_target, JS_CLASS_OBJECT);
        if (JS_IsException(obj))
            return JS_EXCEPTION;
        ret = JS_CallInternal(ctx, func_obj, obj, new_target, argc, argv, flags);
        if (JS_VALUE_GET_TAG(ret) == JS_TAG_OBJECT ||
            JS_IsException(ret)) {
            JS_FreeValue(ctx, obj);
            return ret;
        } else {
            JS_FreeValue(ctx, ret);
            return obj;
        }
    }
}

JSValue JS_CallConstructor2(JSContext *ctx, JSValueConst func_obj,
                            JSValueConst new_target,
                            int argc, JSValueConst *argv)
{
    return JS_CallConstructorInternal(ctx, func_obj, new_target,
                                      argc, (JSValue *)argv,
                                      JS_CALL_FLAG_COPY_ARGV);
}

JSValue JS_CallConstructor(JSContext *ctx, JSValueConst func_obj,
                           int argc, JSValueConst *argv)
{
    return JS_CallConstructorInternal(ctx, func_obj, func_obj,
                                      argc, (JSValue *)argv,
                                      JS_CALL_FLAG_COPY_ARGV);
}

JSValue JS_Invoke(JSContext *ctx, JSValueConst this_val, JSAtom atom,
                  int argc, JSValueConst *argv)
{
    JSValue func_obj;
    func_obj = JS_GetProperty(ctx, this_val, atom);
    if (JS_IsException(func_obj))
        return func_obj;
    return JS_CallFree(ctx, func_obj, this_val, argc, argv);
}

JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                      int argc, JSValueConst *argv)
{
    JSValue res = JS_Invoke(ctx, this_val, atom, argc, argv);
    JS_FreeValue(ctx, this_val);
    return res;
}

/* JSAsyncFunctionState (used by generator and async functions) */
static JSAsyncFunctionState *async_func_init(JSContext *ctx,
                                             JSValueConst func_obj, JSValueConst this_obj,
                                             int argc, JSValueConst *argv)
{
    JSAsyncFunctionState *s;
    JSObject *p;
    JSFunctionBytecode *b;
    JSStackFrame *sf;
    int i, arg_buf_len, n;

    p = JS_VALUE_GET_OBJ(func_obj);
    b = p->u.func.function_bytecode;
    arg_buf_len = max_int(b->arg_count, argc);
    s = js_malloc(ctx, sizeof(*s) + sizeof(JSValue) * (arg_buf_len + b->var_count + b->stack_size) + sizeof(JSVarRef *) * b->var_ref_count);
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    js_rc(s)->ref_count = 1;
    add_gc_object(ctx->rt, &s->header, JS_GC_OBJ_TYPE_ASYNC_FUNCTION);

    sf = &s->frame;
    sf->js_mode = b->js_mode | JS_MODE_ASYNC;
    sf->cur_pc = b->byte_code_buf;
    sf->arg_buf = (JSValue *)(s + 1);
    sf->cur_func = JS_DupValue(ctx, func_obj);
    s->this_val = JS_DupValue(ctx, this_obj);
    s->argc = argc;
    sf->arg_count = arg_buf_len;
    sf->var_buf = sf->arg_buf + arg_buf_len;
    sf->cur_sp = sf->var_buf + b->var_count;
    sf->var_refs = (JSVarRef **)(sf->cur_sp + b->stack_size);
    for(i = 0; i < b->var_ref_count; i++)
        sf->var_refs[i] = NULL;
    for(i = 0; i < argc; i++)
        sf->arg_buf[i] = JS_DupValue(ctx, argv[i]);
    n = arg_buf_len + b->var_count;
    for(i = argc; i < n; i++)
        sf->arg_buf[i] = JS_UNDEFINED;
    s->resolving_funcs[0] = JS_UNDEFINED;
    s->resolving_funcs[1] = JS_UNDEFINED;
    s->is_completed = FALSE;
    return s;
}

static void async_func_free_frame(JSRuntime *rt, JSAsyncFunctionState *s)
{
    JSStackFrame *sf = &s->frame;
    JSValue *sp;

    /* cannot free the function if it is running */
    assert(sf->cur_sp != NULL);
    for(sp = sf->arg_buf; sp < sf->cur_sp; sp++) {
        JS_FreeValueRT(rt, *sp);
    }
    JS_FreeValueRT(rt, sf->cur_func);
    JS_FreeValueRT(rt, s->this_val);
}

static JSValue async_func_resume(JSContext *ctx, JSAsyncFunctionState *s)
{
    JSRuntime *rt = ctx->rt;
    JSStackFrame *sf = &s->frame;
    JSValue func_obj, ret;

    assert(!s->is_completed);
    if (js_check_stack_overflow(ctx->rt, 0)) {
        ret = JS_ThrowStackOverflow(ctx);
    } else {
        /* the tag does not matter provided it is not an object */
        func_obj = JS_MKPTR(JS_TAG_INT, s);
        ret = JS_CallInternal(ctx, func_obj, s->this_val, JS_UNDEFINED,
                              s->argc, sf->arg_buf, JS_CALL_FLAG_GENERATOR);
    }
    if (JS_IsException(ret) || JS_IsUndefined(ret)) {
        JSObject *p;
        JSFunctionBytecode *b;
        
        p = JS_VALUE_GET_OBJ(sf->cur_func);
        b = p->u.func.function_bytecode;
        
        if (JS_IsUndefined(ret)) {
            ret = sf->cur_sp[-1];
            sf->cur_sp[-1] = JS_UNDEFINED;
        }
        /* end of execution */
        s->is_completed = TRUE;

        /* close the closure variables. */
        close_var_refs(rt, b, sf);
        
        async_func_free_frame(rt, s);
    }
    return ret;
}

void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s)
{
    /* cannot close the closure variables here because it would
       potentially modify the object graph */
    if (!s->is_completed) {
        async_func_free_frame(rt, s);
    }

    JS_FreeValueRT(rt, s->resolving_funcs[0]);
    JS_FreeValueRT(rt, s->resolving_funcs[1]);

    remove_gc_object(&s->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES && js_rc(s)->ref_count != 0) {
        list_add_tail(&s->header.link, &rt->gc_zero_ref_count_list);
    } else {
        js_free_rt(rt, s);
    }
}

static void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s)
{
    if (--js_rc(s)->ref_count == 0) {
        if (rt->gc_phase != JS_GC_PHASE_REMOVE_CYCLES) {
            list_del(&s->header.link);
            list_add(&s->header.link, &rt->gc_zero_ref_count_list);
            if (rt->gc_phase == JS_GC_PHASE_NONE) {
                free_zero_refcount(rt);
            }
        }
    }
}

/* Generators */

typedef enum JSGeneratorStateEnum {
    JS_GENERATOR_STATE_SUSPENDED_START,
    JS_GENERATOR_STATE_SUSPENDED_YIELD,
    JS_GENERATOR_STATE_SUSPENDED_YIELD_STAR,
    JS_GENERATOR_STATE_EXECUTING,
    JS_GENERATOR_STATE_COMPLETED,
} JSGeneratorStateEnum;

typedef struct JSGeneratorData {
    JSGeneratorStateEnum state;
    JSAsyncFunctionState *func_state;
} JSGeneratorData;

static void free_generator_stack_rt(JSRuntime *rt, JSGeneratorData *s)
{
    if (s->state == JS_GENERATOR_STATE_COMPLETED)
        return;
    if (s->func_state) {
        async_func_free(rt, s->func_state);
        s->func_state = NULL;
    }
    s->state = JS_GENERATOR_STATE_COMPLETED;
}

void js_generator_finalizer(JSRuntime *rt, JSValue obj)
{
    JSGeneratorData *s = JS_GetOpaque(obj, JS_CLASS_GENERATOR);

    if (s) {
        free_generator_stack_rt(rt, s);
        js_free_rt(rt, s);
    }
}

static void free_generator_stack(JSContext *ctx, JSGeneratorData *s)
{
    free_generator_stack_rt(ctx->rt, s);
}

void js_generator_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSGeneratorData *s = p->u.generator_data;

    if (!s || !s->func_state)
        return;
    mark_func(rt, &s->func_state->header);
}

/* XXX: use enum */
/* [GEN_MAGIC_* moved to quickjs-internal.h] */

JSValue js_generator_next(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv,
                          BOOL *pdone, int magic)
{
    JSGeneratorData *s = JS_GetOpaque(this_val, JS_CLASS_GENERATOR);
    JSStackFrame *sf;
    JSValue ret, func_ret;

    *pdone = TRUE;
    if (!s)
        return JS_ThrowTypeError(ctx, "not a generator");
    switch(s->state) {
    default:
    case JS_GENERATOR_STATE_SUSPENDED_START:
        sf = &s->func_state->frame;
        if (magic == GEN_MAGIC_NEXT) {
            goto exec_no_arg;
        } else {
            free_generator_stack(ctx, s);
            goto done;
        }
        break;
    case JS_GENERATOR_STATE_SUSPENDED_YIELD_STAR:
    case JS_GENERATOR_STATE_SUSPENDED_YIELD:
        sf = &s->func_state->frame;
        /* cur_sp[-1] was set to JS_UNDEFINED in the previous call */
        ret = JS_DupValue(ctx, argv[0]);
        if (magic == GEN_MAGIC_THROW &&
            s->state == JS_GENERATOR_STATE_SUSPENDED_YIELD) {
            JS_Throw(ctx, ret);
            s->func_state->throw_flag = TRUE;
        } else {
            sf->cur_sp[-1] = ret;
            sf->cur_sp[0] = JS_NewInt32(ctx, magic);
            sf->cur_sp++;
        exec_no_arg:
            s->func_state->throw_flag = FALSE;
        }
        s->state = JS_GENERATOR_STATE_EXECUTING;
        func_ret = async_func_resume(ctx, s->func_state);
        s->state = JS_GENERATOR_STATE_SUSPENDED_YIELD;
        if (s->func_state->is_completed) {
            /* finalize the execution in case of exception or normal return */
            free_generator_stack(ctx, s);
            return func_ret;
        } else {
            assert(JS_VALUE_GET_TAG(func_ret) == JS_TAG_INT);
            /* get the returned yield value at the top of the stack */
            ret = sf->cur_sp[-1];
            sf->cur_sp[-1] = JS_UNDEFINED;
            if (JS_VALUE_GET_INT(func_ret) == FUNC_RET_YIELD_STAR) {
                s->state = JS_GENERATOR_STATE_SUSPENDED_YIELD_STAR;
                /* return (value, done) object */
                *pdone = 2;
            } else {
                *pdone = FALSE;
            }
        }
        break;
    case JS_GENERATOR_STATE_COMPLETED:
    done:
        /* execution is finished */
        switch(magic) {
        default:
        case GEN_MAGIC_NEXT:
            ret = JS_UNDEFINED;
            break;
        case GEN_MAGIC_RETURN:
            ret = JS_DupValue(ctx, argv[0]);
            break;
        case GEN_MAGIC_THROW:
            ret = JS_Throw(ctx, JS_DupValue(ctx, argv[0]));
            break;
        }
        break;
    case JS_GENERATOR_STATE_EXECUTING:
        ret = JS_ThrowTypeError(ctx, "cannot invoke a running generator");
        break;
    }
    return ret;
}

JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                   JSValueConst this_obj,
                                   int argc, JSValueConst *argv,
                                   int flags)
{
    JSValue obj, func_ret;
    JSGeneratorData *s;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    s->state = JS_GENERATOR_STATE_SUSPENDED_START;
    s->func_state = async_func_init(ctx, func_obj, this_obj, argc, argv);
    if (!s->func_state) {
        s->state = JS_GENERATOR_STATE_COMPLETED;
        goto fail;
    }

    /* execute the function up to 'OP_initial_yield' */
    func_ret = async_func_resume(ctx, s->func_state);
    if (JS_IsException(func_ret))
        goto fail;
    JS_FreeValue(ctx, func_ret);

    obj = js_create_from_ctor(ctx, func_obj, JS_CLASS_GENERATOR);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, s);
    return obj;
 fail:
    free_generator_stack_rt(ctx->rt, s);
    js_free(ctx, s);
    return JS_EXCEPTION;
}

/* AsyncFunction */

void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSAsyncFunctionState *s = p->u.async_function_data;
    if (s) {
        async_func_free(rt, s);
    }
}

void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSAsyncFunctionState *s = p->u.async_function_data;
    if (s) {
        mark_func(rt, &s->header);
    }
}

static int js_async_function_resolve_create(JSContext *ctx,
                                            JSAsyncFunctionState *s,
                                            JSValue *resolving_funcs)
{
    int i;
    JSObject *p;

    for(i = 0; i < 2; i++) {
        resolving_funcs[i] =
            JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                   JS_CLASS_ASYNC_FUNCTION_RESOLVE + i);
        if (JS_IsException(resolving_funcs[i])) {
            if (i == 1)
                JS_FreeValue(ctx, resolving_funcs[0]);
            return -1;
        }
        p = JS_VALUE_GET_OBJ(resolving_funcs[i]);
        js_rc(s)->ref_count++;
        p->u.async_function_data = s;
    }
    return 0;
}

static void js_async_function_resume(JSContext *ctx, JSAsyncFunctionState *s)
{
    JSValue func_ret, ret2;

    func_ret = async_func_resume(ctx, s);
    if (s->is_completed) {
        if (JS_IsException(func_ret)) {
            JSValue error;
        fail:
            error = JS_GetException(ctx);
            ret2 = JS_Call(ctx, s->resolving_funcs[1], JS_UNDEFINED,
                           1, (JSValueConst *)&error);
            JS_FreeValue(ctx, error);
            JS_FreeValue(ctx, ret2); /* XXX: what to do if exception ? */
        } else {
            /* normal return */
            ret2 = JS_Call(ctx, s->resolving_funcs[0], JS_UNDEFINED,
                           1, (JSValueConst *)&func_ret);
            JS_FreeValue(ctx, func_ret);
            JS_FreeValue(ctx, ret2); /* XXX: what to do if exception ? */
        }
    } else {
        JSValue value, promise, resolving_funcs[2], resolving_funcs1[2];
        int i, res;

        value = s->frame.cur_sp[-1];
        s->frame.cur_sp[-1] = JS_UNDEFINED;

        /* await */
        JS_FreeValue(ctx, func_ret); /* not used */
        promise = js_promise_resolve(ctx, ctx->promise_ctor,
                                     1, (JSValueConst *)&value, 0);
        JS_FreeValue(ctx, value);
        if (JS_IsException(promise))
            goto fail;
        if (js_async_function_resolve_create(ctx, s, resolving_funcs)) {
            JS_FreeValue(ctx, promise);
            goto fail;
        }

        /* Note: no need to create 'thrownawayCapability' as in
           the spec */
        for(i = 0; i < 2; i++)
            resolving_funcs1[i] = JS_UNDEFINED;
        res = perform_promise_then(ctx, promise,
                                   (JSValueConst *)resolving_funcs,
                                   (JSValueConst *)resolving_funcs1);
        JS_FreeValue(ctx, promise);
        for(i = 0; i < 2; i++)
            JS_FreeValue(ctx, resolving_funcs[i]);
        if (res)
            goto fail;
    }
}

JSValue js_async_function_resolve_call(JSContext *ctx,
                                              JSValueConst func_obj,
                                              JSValueConst this_obj,
                                              int argc, JSValueConst *argv,
                                              int flags)
{
    JSObject *p = JS_VALUE_GET_OBJ(func_obj);
    JSAsyncFunctionState *s = p->u.async_function_data;
    BOOL is_reject = p->class_id - JS_CLASS_ASYNC_FUNCTION_RESOLVE;
    JSValueConst arg;

    if (argc > 0)
        arg = argv[0];
    else
        arg = JS_UNDEFINED;
    s->throw_flag = is_reject;
    if (is_reject) {
        JS_Throw(ctx, JS_DupValue(ctx, arg));
    } else {
        /* return value of await */
        s->frame.cur_sp[-1] = JS_DupValue(ctx, arg);
    }
    js_async_function_resume(ctx, s);
    return JS_UNDEFINED;
}

JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags)
{
    JSValue promise;
    JSAsyncFunctionState *s;

    s = async_func_init(ctx, func_obj, this_obj, argc, argv);
    if (!s)
        return JS_EXCEPTION;

    promise = JS_NewPromiseCapability(ctx, s->resolving_funcs);
    if (JS_IsException(promise)) {
        async_func_free(ctx->rt, s);
        return JS_EXCEPTION;
    }

    js_async_function_resume(ctx, s);

    async_func_free(ctx->rt, s);

    return promise;
}

/* AsyncGenerator */

typedef enum JSAsyncGeneratorStateEnum {
    JS_ASYNC_GENERATOR_STATE_SUSPENDED_START,
    JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD,
    JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD_STAR,
    JS_ASYNC_GENERATOR_STATE_EXECUTING,
    JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN,
    JS_ASYNC_GENERATOR_STATE_COMPLETED,
} JSAsyncGeneratorStateEnum;

typedef struct JSAsyncGeneratorRequest {
    struct list_head link;
    /* completion */
    int completion_type; /* GEN_MAGIC_x */
    JSValue result;
    /* promise capability */
    JSValue promise;
    JSValue resolving_funcs[2];
} JSAsyncGeneratorRequest;

typedef struct JSAsyncGeneratorData {
    JSObject *generator; /* back pointer to the object (const) */
    JSAsyncGeneratorStateEnum state;
    /* func_state is NULL is state AWAITING_RETURN and COMPLETED */
    JSAsyncFunctionState *func_state;
    struct list_head queue; /* list of JSAsyncGeneratorRequest.link */
} JSAsyncGeneratorData;

static void js_async_generator_free(JSRuntime *rt,
                                    JSAsyncGeneratorData *s)
{
    struct list_head *el, *el1;
    JSAsyncGeneratorRequest *req;

    list_for_each_safe(el, el1, &s->queue) {
        req = list_entry(el, JSAsyncGeneratorRequest, link);
        JS_FreeValueRT(rt, req->result);
        JS_FreeValueRT(rt, req->promise);
        JS_FreeValueRT(rt, req->resolving_funcs[0]);
        JS_FreeValueRT(rt, req->resolving_funcs[1]);
        js_free_rt(rt, req);
    }
    if (s->func_state)
        async_func_free(rt, s->func_state);
    js_free_rt(rt, s);
}

void js_async_generator_finalizer(JSRuntime *rt, JSValue obj)
{
    JSAsyncGeneratorData *s = JS_GetOpaque(obj, JS_CLASS_ASYNC_GENERATOR);

    if (s) {
        js_async_generator_free(rt, s);
    }
}

void js_async_generator_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func)
{
    JSAsyncGeneratorData *s = JS_GetOpaque(val, JS_CLASS_ASYNC_GENERATOR);
    struct list_head *el;
    JSAsyncGeneratorRequest *req;
    if (s) {
        list_for_each(el, &s->queue) {
            req = list_entry(el, JSAsyncGeneratorRequest, link);
            JS_MarkValue(rt, req->result, mark_func);
            JS_MarkValue(rt, req->promise, mark_func);
            JS_MarkValue(rt, req->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, req->resolving_funcs[1], mark_func);
        }
        if (s->func_state) {
            mark_func(rt, &s->func_state->header);
        }
    }
}

static JSValue js_async_generator_resolve_function(JSContext *ctx,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int magic, JSValue *func_data);

static int js_async_generator_resolve_function_create(JSContext *ctx,
                                                      JSValueConst generator,
                                                      JSValue *resolving_funcs,
                                                      BOOL is_resume_next)
{
    int i;
    JSValue func;

    for(i = 0; i < 2; i++) {
        func = JS_NewCFunctionData(ctx, js_async_generator_resolve_function, 1,
                                   i + is_resume_next * 2, 1, &generator);
        if (JS_IsException(func)) {
            if (i == 1)
                JS_FreeValue(ctx, resolving_funcs[0]);
            return -1;
        }
        resolving_funcs[i] = func;
    }
    return 0;
}

static int js_async_generator_await(JSContext *ctx,
                                    JSAsyncGeneratorData *s,
                                    JSValueConst value)
{
    JSValue promise, resolving_funcs[2], resolving_funcs1[2];
    int i, res;

    promise = js_promise_resolve(ctx, ctx->promise_ctor,
                                 1, &value, 0);
    if (JS_IsException(promise))
        goto fail;

    if (js_async_generator_resolve_function_create(ctx, JS_MKPTR(JS_TAG_OBJECT, s->generator),
                                                   resolving_funcs, FALSE)) {
        JS_FreeValue(ctx, promise);
        goto fail;
    }

    /* Note: no need to create 'thrownawayCapability' as in
       the spec */
    for(i = 0; i < 2; i++)
        resolving_funcs1[i] = JS_UNDEFINED;
    res = perform_promise_then(ctx, promise,
                               (JSValueConst *)resolving_funcs,
                               (JSValueConst *)resolving_funcs1);
    JS_FreeValue(ctx, promise);
    for(i = 0; i < 2; i++)
        JS_FreeValue(ctx, resolving_funcs[i]);
    if (res)
        goto fail;
    return 0;
 fail:
    return -1;
}

static void js_async_generator_resolve_or_reject(JSContext *ctx,
                                                 JSAsyncGeneratorData *s,
                                                 JSValueConst result,
                                                 int is_reject)
{
    JSAsyncGeneratorRequest *next;
    JSValue ret;

    next = list_entry(s->queue.next, JSAsyncGeneratorRequest, link);
    list_del(&next->link);
    ret = JS_Call(ctx, next->resolving_funcs[is_reject], JS_UNDEFINED, 1,
                  &result);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, next->result);
    JS_FreeValue(ctx, next->promise);
    JS_FreeValue(ctx, next->resolving_funcs[0]);
    JS_FreeValue(ctx, next->resolving_funcs[1]);
    js_free(ctx, next);
}

static void js_async_generator_resolve(JSContext *ctx,
                                       JSAsyncGeneratorData *s,
                                       JSValueConst value,
                                       BOOL done)
{
    JSValue result;
    result = js_create_iterator_result(ctx, JS_DupValue(ctx, value), done);
    /* XXX: better exception handling ? */
    js_async_generator_resolve_or_reject(ctx, s, result, 0);
    JS_FreeValue(ctx, result);
 }

static void js_async_generator_reject(JSContext *ctx,
                                       JSAsyncGeneratorData *s,
                                       JSValueConst exception)
{
    js_async_generator_resolve_or_reject(ctx, s, exception, 1);
}

static void js_async_generator_complete(JSContext *ctx,
                                        JSAsyncGeneratorData *s)
{
    if (s->state != JS_ASYNC_GENERATOR_STATE_COMPLETED) {
        s->state = JS_ASYNC_GENERATOR_STATE_COMPLETED;
        async_func_free(ctx->rt, s->func_state);
        s->func_state = NULL;
    }
}

static int js_async_generator_completed_return(JSContext *ctx,
                                               JSAsyncGeneratorData *s,
                                               JSValueConst value)
{
    JSValue promise, resolving_funcs[2], resolving_funcs1[2];
    int res;

    // Can fail looking up JS_ATOM_constructor when is_reject==0.
    promise = js_promise_resolve(ctx, ctx->promise_ctor, 1, &value,
                                 /*is_reject*/0);
    // A poisoned .constructor property is observable and the resulting
    // exception should be delivered to the catch handler.
    if (JS_IsException(promise)) {
        JSValue err = JS_GetException(ctx);
        promise = js_promise_resolve(ctx, ctx->promise_ctor, 1, (JSValueConst *)&err,
                                     /*is_reject*/1);
        JS_FreeValue(ctx, err);
        if (JS_IsException(promise))
            return -1;
    }
    if (js_async_generator_resolve_function_create(ctx,
                                                   JS_MKPTR(JS_TAG_OBJECT, s->generator),
                                                   resolving_funcs1,
                                                   TRUE)) {
        JS_FreeValue(ctx, promise);
        return -1;
    }
    resolving_funcs[0] = JS_UNDEFINED;
    resolving_funcs[1] = JS_UNDEFINED;
    res = perform_promise_then(ctx, promise,
                               (JSValueConst *)resolving_funcs1,
                               (JSValueConst *)resolving_funcs);
    JS_FreeValue(ctx, resolving_funcs1[0]);
    JS_FreeValue(ctx, resolving_funcs1[1]);
    JS_FreeValue(ctx, promise);
    return res;
}

static void js_async_generator_resume_next(JSContext *ctx,
                                           JSAsyncGeneratorData *s)
{
    JSAsyncGeneratorRequest *next;
    JSValue func_ret, value;

    for(;;) {
        if (list_empty(&s->queue))
            break;
        next = list_entry(s->queue.next, JSAsyncGeneratorRequest, link);
        switch(s->state) {
        case JS_ASYNC_GENERATOR_STATE_EXECUTING:
            /* only happens when restarting execution after await() */
            goto resume_exec;
        case JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN:
            goto done;
        case JS_ASYNC_GENERATOR_STATE_SUSPENDED_START:
            if (next->completion_type == GEN_MAGIC_NEXT) {
                goto exec_no_arg;
            } else {
                js_async_generator_complete(ctx, s);
            }
            break;
        case JS_ASYNC_GENERATOR_STATE_COMPLETED:
            if (next->completion_type == GEN_MAGIC_NEXT) {
                js_async_generator_resolve(ctx, s, JS_UNDEFINED, TRUE);
            } else if (next->completion_type == GEN_MAGIC_RETURN) {
                s->state = JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN;
                js_async_generator_completed_return(ctx, s, next->result);
            } else {
                js_async_generator_reject(ctx, s, next->result);
            }
            goto done;
        case JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD:
        case JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD_STAR:
            value = JS_DupValue(ctx, next->result);
            if (next->completion_type == GEN_MAGIC_THROW &&
                s->state == JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD) {
                JS_Throw(ctx, value);
                s->func_state->throw_flag = TRUE;
            } else {
                /* 'yield' returns a value. 'yield *' also returns a value
                   in case the 'throw' method is called */
                s->func_state->frame.cur_sp[-1] = value;
                s->func_state->frame.cur_sp[0] =
                    JS_NewInt32(ctx, next->completion_type);
                s->func_state->frame.cur_sp++;
            exec_no_arg:
                s->func_state->throw_flag = FALSE;
            }
            s->state = JS_ASYNC_GENERATOR_STATE_EXECUTING;
        resume_exec:
            func_ret = async_func_resume(ctx, s->func_state);
            if (s->func_state->is_completed) {
                if (JS_IsException(func_ret)) {
                    value = JS_GetException(ctx);
                    js_async_generator_complete(ctx, s);
                    js_async_generator_reject(ctx, s, value);
                    JS_FreeValue(ctx, value);
                } else {
                    /* end of function */
                    js_async_generator_complete(ctx, s);
                    js_async_generator_resolve(ctx, s, func_ret, TRUE);
                    JS_FreeValue(ctx, func_ret);
                }
            } else {
                int func_ret_code, ret;
                assert(JS_VALUE_GET_TAG(func_ret) == JS_TAG_INT);
                func_ret_code = JS_VALUE_GET_INT(func_ret);
                value = s->func_state->frame.cur_sp[-1];
                s->func_state->frame.cur_sp[-1] = JS_UNDEFINED;
                switch(func_ret_code) {
                case FUNC_RET_YIELD:
                case FUNC_RET_YIELD_STAR:
                    if (func_ret_code == FUNC_RET_YIELD_STAR)
                        s->state = JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD_STAR;
                    else
                        s->state = JS_ASYNC_GENERATOR_STATE_SUSPENDED_YIELD;
                    js_async_generator_resolve(ctx, s, value, FALSE);
                    JS_FreeValue(ctx, value);
                    break;
                case FUNC_RET_AWAIT:
                    ret = js_async_generator_await(ctx, s, value);
                    JS_FreeValue(ctx, value);
                    if (ret < 0) {
                        /* exception: throw it */
                        s->func_state->throw_flag = TRUE;
                        goto resume_exec;
                    }
                    goto done;
                default:
                    abort();
                }
            }
            break;
        default:
            abort();
        }
    }
 done: ;
}

static JSValue js_async_generator_resolve_function(JSContext *ctx,
                                                   JSValueConst this_obj,
                                                   int argc, JSValueConst *argv,
                                                   int magic, JSValue *func_data)
{
    BOOL is_reject = magic & 1;
    JSAsyncGeneratorData *s = JS_GetOpaque(func_data[0], JS_CLASS_ASYNC_GENERATOR);
    JSValueConst arg = argv[0];

    /* XXX: what if s == NULL */

    if (magic >= 2) {
        /* resume next case in AWAITING_RETURN state */
        assert(s->state == JS_ASYNC_GENERATOR_STATE_AWAITING_RETURN ||
               s->state == JS_ASYNC_GENERATOR_STATE_COMPLETED);
        s->state = JS_ASYNC_GENERATOR_STATE_COMPLETED;
        if (is_reject) {
            js_async_generator_reject(ctx, s, arg);
        } else {
            js_async_generator_resolve(ctx, s, arg, TRUE);
        }
    } else if (s->state == JS_ASYNC_GENERATOR_STATE_EXECUTING) {
        /* restart function execution after await() */
        s->func_state->throw_flag = is_reject;
        if (is_reject) {
            JS_Throw(ctx, JS_DupValue(ctx, arg));
        } else {
            /* return value of await */
            s->func_state->frame.cur_sp[-1] = JS_DupValue(ctx, arg);
        }
        js_async_generator_resume_next(ctx, s);
    }
    return JS_UNDEFINED;
}

/* magic = GEN_MAGIC_x */
JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int magic)
{
    JSAsyncGeneratorData *s = JS_GetOpaque(this_val, JS_CLASS_ASYNC_GENERATOR);
    JSValue promise, resolving_funcs[2];
    JSAsyncGeneratorRequest *req;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;
    if (!s) {
        JSValue err, res2;
        JS_ThrowTypeError(ctx, "not an AsyncGenerator object");
        err = JS_GetException(ctx);
        res2 = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                       1, (JSValueConst *)&err);
        JS_FreeValue(ctx, err);
        JS_FreeValue(ctx, res2);
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        return promise;
    }
    req = js_mallocz(ctx, sizeof(*req));
    if (!req)
        goto fail;
    req->completion_type = magic;
    req->result = JS_DupValue(ctx, argv[0]);
    req->promise = JS_DupValue(ctx, promise);
    req->resolving_funcs[0] = resolving_funcs[0];
    req->resolving_funcs[1] = resolving_funcs[1];
    list_add_tail(&req->link, &s->queue);
    if (s->state != JS_ASYNC_GENERATOR_STATE_EXECUTING) {
        js_async_generator_resume_next(ctx, s);
    }
    return promise;
 fail:
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_EXCEPTION;
}

JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                                JSValueConst this_obj,
                                                int argc, JSValueConst *argv,
                                                int flags)
{
    JSValue obj, func_ret;
    JSAsyncGeneratorData *s;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    s->state = JS_ASYNC_GENERATOR_STATE_SUSPENDED_START;
    init_list_head(&s->queue);
    s->func_state = async_func_init(ctx, func_obj, this_obj, argc, argv);
    if (!s->func_state)
        goto fail;
    /* execute the function up to 'OP_initial_yield' (no yield nor
       await are possible) */
    func_ret = async_func_resume(ctx, s->func_state);
    if (JS_IsException(func_ret))
        goto fail;
    JS_FreeValue(ctx, func_ret);

    obj = js_create_from_ctor(ctx, func_obj, JS_CLASS_ASYNC_GENERATOR);
    if (JS_IsException(obj))
        goto fail;
    s->generator = JS_VALUE_GET_OBJ(obj);
    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_async_generator_free(ctx->rt, s);
    return JS_EXCEPTION;
}

/* [Parser structures and tokens moved to src/compiler/js_lexer.h and js_parser.h] */

/* [JSOpCode defined in compiler/js_opcode.h] */

/* [Codegen Section A moved to src/compiler/js_codegen.c] */

/* [Parser Part 1 moved to src/compiler/js_parser.c] */


/* 'name' is freed. The module is referenced by 'ctx->loaded_modules' */
JSModuleDef *js_new_module_def(JSContext *ctx, JSAtom name)
{
    JSModuleDef *m;
    m = js_mallocz(ctx, sizeof(*m));
    if (!m) {
        JS_FreeAtom(ctx, name);
        return NULL;
    }
    js_rc(m)->ref_count = 1;
    add_gc_object(ctx->rt, &m->header, JS_GC_OBJ_TYPE_MODULE);
    m->module_name = name;
    m->module_ns = JS_UNDEFINED;
    m->func_obj = JS_UNDEFINED;
    m->eval_exception = JS_UNDEFINED;
    m->meta_obj = JS_UNDEFINED;
    m->promise = JS_UNDEFINED;
    m->resolving_funcs[0] = JS_UNDEFINED;
    m->resolving_funcs[1] = JS_UNDEFINED;
    m->private_value = JS_UNDEFINED;
    list_add_tail(&m->link, &ctx->loaded_modules);
    return m;
}

void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                        JS_MarkFunc *mark_func)
{
    int i;

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        JS_MarkValue(rt, rme->attributes, mark_func);
    }
    
    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (me->export_type == JS_EXPORT_TYPE_LOCAL &&
            me->u.local.var_ref) {
            mark_func(rt, &me->u.local.var_ref->header);
        }
    }

    JS_MarkValue(rt, m->module_ns, mark_func);
    JS_MarkValue(rt, m->func_obj, mark_func);
    JS_MarkValue(rt, m->eval_exception, mark_func);
    JS_MarkValue(rt, m->meta_obj, mark_func);
    JS_MarkValue(rt, m->promise, mark_func);
    JS_MarkValue(rt, m->resolving_funcs[0], mark_func);
    JS_MarkValue(rt, m->resolving_funcs[1], mark_func);
    JS_MarkValue(rt, m->private_value, mark_func);
}

void js_free_module_def(JSRuntime *rt, JSModuleDef *m)
{
    int i;

    JS_FreeAtomRT(rt, m->module_name);

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        JS_FreeAtomRT(rt, rme->module_name);
        JS_FreeValueRT(rt, rme->attributes);
    }
    js_free_rt(rt, m->req_module_entries);

    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (me->export_type == JS_EXPORT_TYPE_LOCAL)
            free_var_ref(rt, me->u.local.var_ref);
        JS_FreeAtomRT(rt, me->export_name);
        JS_FreeAtomRT(rt, me->local_name);
    }
    js_free_rt(rt, m->export_entries);

    js_free_rt(rt, m->star_export_entries);

    for(i = 0; i < m->import_entries_count; i++) {
        JSImportEntry *mi = &m->import_entries[i];
        JS_FreeAtomRT(rt, mi->import_name);
    }
    js_free_rt(rt, m->import_entries);
    js_free_rt(rt, m->async_parent_modules);

    JS_FreeValueRT(rt, m->module_ns);
    JS_FreeValueRT(rt, m->func_obj);
    JS_FreeValueRT(rt, m->eval_exception);
    JS_FreeValueRT(rt, m->meta_obj);
    JS_FreeValueRT(rt, m->promise);
    JS_FreeValueRT(rt, m->resolving_funcs[0]);
    JS_FreeValueRT(rt, m->resolving_funcs[1]);
    JS_FreeValueRT(rt, m->private_value);
    /* during the GC the finalizers are called in an arbitrary
       order so the module may no longer be referenced by the JSContext list */
    if (m->link.next) {
        list_del(&m->link);
    }
    remove_gc_object(&m->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES && js_rc(m)->ref_count != 0) {
        list_add_tail(&m->header.link, &rt->gc_zero_ref_count_list);
    } else {
        js_free_rt(rt, m);
    }
}

int add_req_module_entry(JSContext *ctx, JSModuleDef *m,
                                JSAtom module_name)
{
    JSReqModuleEntry *rme;

    if (js_resize_array(ctx, (void **)&m->req_module_entries,
                        sizeof(JSReqModuleEntry),
                        &m->req_module_entries_size,
                        m->req_module_entries_count + 1))
        return -1;
    rme = &m->req_module_entries[m->req_module_entries_count++];
    rme->module_name = JS_DupAtom(ctx, module_name);
    rme->module = NULL;
    rme->attributes = JS_UNDEFINED;
    return m->req_module_entries_count - 1;
}

static JSExportEntry *find_export_entry(JSContext *ctx, JSModuleDef *m,
                                        JSAtom export_name)
{
    JSExportEntry *me;
    int i;
    for(i = 0; i < m->export_entries_count; i++) {
        me = &m->export_entries[i];
        if (me->export_name == export_name)
            return me;
    }
    return NULL;
}

static JSExportEntry *add_export_entry2(JSContext *ctx,
                                        JSParseState *s, JSModuleDef *m,
                                       JSAtom local_name, JSAtom export_name,
                                       JSExportTypeEnum export_type)
{
    JSExportEntry *me;

    if (find_export_entry(ctx, m, export_name)) {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        if (s) {
            js_parse_error(s, "duplicate exported name '%s'",
                           JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name));
        } else {
            JS_ThrowSyntaxErrorAtom(ctx, "duplicate exported name '%s'", export_name);
        }
        return NULL;
    }

    if (js_resize_array(ctx, (void **)&m->export_entries,
                        sizeof(JSExportEntry),
                        &m->export_entries_size,
                        m->export_entries_count + 1))
        return NULL;
    me = &m->export_entries[m->export_entries_count++];
    memset(me, 0, sizeof(*me));
    me->local_name = JS_DupAtom(ctx, local_name);
    me->export_name = JS_DupAtom(ctx, export_name);
    me->export_type = export_type;
    return me;
}

JSExportEntry *add_export_entry(JSParseState *s, JSModuleDef *m,
                                       JSAtom local_name, JSAtom export_name,
                                       JSExportTypeEnum export_type)
{
    return add_export_entry2(s->ctx, s, m, local_name, export_name,
                             export_type);
}

int add_star_export_entry(JSContext *ctx, JSModuleDef *m,
                                 int req_module_idx)
{
    JSStarExportEntry *se;

    if (js_resize_array(ctx, (void **)&m->star_export_entries,
                        sizeof(JSStarExportEntry),
                        &m->star_export_entries_size,
                        m->star_export_entries_count + 1))
        return -1;
    se = &m->star_export_entries[m->star_export_entries_count++];
    se->req_module_idx = req_module_idx;
    return 0;
}

/* create a C module */
JSModuleDef *JS_NewCModule(JSContext *ctx, const char *name_str,
                           JSModuleInitFunc *func)
{
    JSModuleDef *m;
    JSAtom name;
    name = JS_NewAtom(ctx, name_str);
    if (name == JS_ATOM_NULL)
        return NULL;
    m = js_new_module_def(ctx, name);
    if (!m)
        return NULL;
    m->init_func = func;
    return m;
}

int JS_AddModuleExport(JSContext *ctx, JSModuleDef *m, const char *export_name)
{
    JSExportEntry *me;
    JSAtom name;
    name = JS_NewAtom(ctx, export_name);
    if (name == JS_ATOM_NULL)
        return -1;
    me = add_export_entry2(ctx, NULL, m, JS_ATOM_NULL, name,
                           JS_EXPORT_TYPE_LOCAL);
    JS_FreeAtom(ctx, name);
    if (!me)
        return -1;
    else
        return 0;
}

int JS_SetModuleExport(JSContext *ctx, JSModuleDef *m, const char *export_name,
                       JSValue val)
{
    JSExportEntry *me;
    JSAtom name;
    name = JS_NewAtom(ctx, export_name);
    if (name == JS_ATOM_NULL)
        goto fail;
    me = find_export_entry(ctx, m, name);
    JS_FreeAtom(ctx, name);
    if (!me)
        goto fail;
    set_value(ctx, me->u.local.var_ref->pvalue, val);
    return 0;
 fail:
    JS_FreeValue(ctx, val);
    return -1;
}

int JS_SetModulePrivateValue(JSContext *ctx, JSModuleDef *m, JSValue val)
{
    set_value(ctx, &m->private_value, val);
    return 0;
}

JSValue JS_GetModulePrivateValue(JSContext *ctx, JSModuleDef *m)
{
    return JS_DupValue(ctx, m->private_value);
}

void JS_SetModuleLoaderFunc(JSRuntime *rt,
                            JSModuleNormalizeFunc *module_normalize,
                            JSModuleLoaderFunc *module_loader, void *opaque)
{
    rt->module_normalize_func = module_normalize;
    rt->module_loader_has_attr = FALSE;
    rt->u.module_loader_func = module_loader;
    rt->module_check_attrs = NULL;
    rt->module_loader_opaque = opaque;
}

void JS_SetModuleLoaderFunc2(JSRuntime *rt,
                             JSModuleNormalizeFunc *module_normalize,
                             JSModuleLoaderFunc2 *module_loader,
                             JSModuleCheckSupportedImportAttributes *module_check_attrs,
                             void *opaque)
{
    rt->module_normalize_func = module_normalize;
    rt->module_loader_has_attr = TRUE;
    rt->u.module_loader_func2 = module_loader;
    rt->module_check_attrs = module_check_attrs;
    rt->module_loader_opaque = opaque;
}

/* default module filename normalizer */
static char *js_default_module_normalize_name(JSContext *ctx,
                                              const char *base_name,
                                              const char *name)
{
    char *filename, *p;
    const char *r;
    int cap;
    int len;

    if (name[0] != '.') {
        /* if no initial dot, the module name is not modified */
        return js_strdup(ctx, name);
    }

    p = strrchr(base_name, '/');
    if (p)
        len = p - base_name;
    else
        len = 0;

    cap = len + strlen(name) + 1 + 1;
    filename = js_malloc(ctx, cap);
    if (!filename)
        return NULL;
    memcpy(filename, base_name, len);
    filename[len] = '\0';

    /* we only normalize the leading '..' or '.' */
    r = name;
    for(;;) {
        if (r[0] == '.' && r[1] == '/') {
            r += 2;
        } else if (r[0] == '.' && r[1] == '.' && r[2] == '/') {
            /* remove the last path element of filename, except if "."
               or ".." */
            if (filename[0] == '\0')
                break;
            p = strrchr(filename, '/');
            if (!p)
                p = filename;
            else
                p++;
            if (!strcmp(p, ".") || !strcmp(p, ".."))
                break;
            if (p > filename)
                p--;
            *p = '\0';
            r += 3;
        } else {
            break;
        }
    }
    if (filename[0] != '\0')
        pstrcat(filename, cap, "/");
    pstrcat(filename, cap, r);
    //    printf("normalize: %s %s -> %s\n", base_name, name, filename);
    return filename;
}

static JSModuleDef *js_find_loaded_module(JSContext *ctx, JSAtom name)
{
    struct list_head *el;
    JSModuleDef *m;

    /* first look at the loaded modules */
    list_for_each(el, &ctx->loaded_modules) {
        m = list_entry(el, JSModuleDef, link);
        if (m->module_name == name)
            return m;
    }
    return NULL;
}

/* return NULL in case of exception (e.g. module could not be loaded) */
static JSModuleDef *js_host_resolve_imported_module(JSContext *ctx,
                                                    const char *base_cname,
                                                    const char *cname1,
                                                    JSValueConst attributes)
{
    JSRuntime *rt = ctx->rt;
    JSModuleDef *m;
    char *cname;
    JSAtom module_name;

    if (!rt->module_normalize_func) {
        cname = js_default_module_normalize_name(ctx, base_cname, cname1);
    } else {
        cname = rt->module_normalize_func(ctx, base_cname, cname1,
                                          rt->module_loader_opaque);
    }
    if (!cname)
        return NULL;

    module_name = JS_NewAtom(ctx, cname);
    if (module_name == JS_ATOM_NULL) {
        js_free(ctx, cname);
        return NULL;
    }

    /* first look at the loaded modules */
    m = js_find_loaded_module(ctx, module_name);
    if (m) {
        js_free(ctx, cname);
        JS_FreeAtom(ctx, module_name);
        return m;
    }

    JS_FreeAtom(ctx, module_name);

    /* load the module */
    if (!rt->u.module_loader_func) {
        /* XXX: use a syntax error ? */
        JS_ThrowReferenceError(ctx, "could not load module '%s'",
                               cname);
        js_free(ctx, cname);
        return NULL;
    }
    if (rt->module_loader_has_attr) {
        m = rt->u.module_loader_func2(ctx, cname, rt->module_loader_opaque, attributes);
    } else {
        m = rt->u.module_loader_func(ctx, cname, rt->module_loader_opaque);
    }
    js_free(ctx, cname);
    return m;
}

static JSModuleDef *js_host_resolve_imported_module_atom(JSContext *ctx,
                                                         JSAtom base_module_name,
                                                         JSAtom module_name1,
                                                         JSValueConst attributes)
{
    const char *base_cname, *cname;
    JSModuleDef *m;

    base_cname = JS_AtomToCString(ctx, base_module_name);
    if (!base_cname)
        return NULL;
    cname = JS_AtomToCString(ctx, module_name1);
    if (!cname) {
        JS_FreeCString(ctx, base_cname);
        return NULL;
    }
    m = js_host_resolve_imported_module(ctx, base_cname, cname, attributes);
    JS_FreeCString(ctx, base_cname);
    JS_FreeCString(ctx, cname);
    return m;
}

typedef struct JSResolveEntry {
    JSModuleDef *module;
    JSAtom name;
} JSResolveEntry;

typedef struct JSResolveState {
    JSResolveEntry *array;
    int size;
    int count;
} JSResolveState;

static int find_resolve_entry(JSResolveState *s,
                              JSModuleDef *m, JSAtom name)
{
    int i;
    for(i = 0; i < s->count; i++) {
        JSResolveEntry *re = &s->array[i];
        if (re->module == m && re->name == name)
            return i;
    }
    return -1;
}

static int add_resolve_entry(JSContext *ctx, JSResolveState *s,
                             JSModuleDef *m, JSAtom name)
{
    JSResolveEntry *re;

    if (js_resize_array(ctx, (void **)&s->array,
                        sizeof(JSResolveEntry),
                        &s->size, s->count + 1))
        return -1;
    re = &s->array[s->count++];
    re->module = m;
    re->name = JS_DupAtom(ctx, name);
    return 0;
}

typedef enum JSResolveResultEnum {
    JS_RESOLVE_RES_EXCEPTION = -1, /* memory alloc error */
    JS_RESOLVE_RES_FOUND = 0,
    JS_RESOLVE_RES_NOT_FOUND,
    JS_RESOLVE_RES_CIRCULAR,
    JS_RESOLVE_RES_AMBIGUOUS,
} JSResolveResultEnum;

static JSResolveResultEnum js_resolve_export1(JSContext *ctx,
                                              JSModuleDef **pmodule,
                                              JSExportEntry **pme,
                                              JSModuleDef *m,
                                              JSAtom export_name,
                                              JSResolveState *s)
{
    JSExportEntry *me;

    *pmodule = NULL;
    *pme = NULL;
    if (find_resolve_entry(s, m, export_name) >= 0)
        return JS_RESOLVE_RES_CIRCULAR;
    if (add_resolve_entry(ctx, s, m, export_name) < 0)
        return JS_RESOLVE_RES_EXCEPTION;
    me = find_export_entry(ctx, m, export_name);
    if (me) {
        if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
            /* local export */
            *pmodule = m;
            *pme = me;
            return JS_RESOLVE_RES_FOUND;
        } else {
            /* indirect export */
            JSModuleDef *m1;
            m1 = m->req_module_entries[me->u.req_module_idx].module;
            if (me->local_name == JS_ATOM__star_) {
                /* export ns from */
                *pmodule = m;
                *pme = me;
                return JS_RESOLVE_RES_FOUND;
            } else {
                return js_resolve_export1(ctx, pmodule, pme, m1,
                                          me->local_name, s);
            }
        }
    } else {
        if (export_name != JS_ATOM_default) {
            /* not found in direct or indirect exports: try star exports */
            int i;

            for(i = 0; i < m->star_export_entries_count; i++) {
                JSStarExportEntry *se = &m->star_export_entries[i];
                JSModuleDef *m1, *res_m;
                JSExportEntry *res_me;
                JSResolveResultEnum ret;

                m1 = m->req_module_entries[se->req_module_idx].module;
                ret = js_resolve_export1(ctx, &res_m, &res_me, m1,
                                         export_name, s);
                if (ret == JS_RESOLVE_RES_AMBIGUOUS ||
                    ret == JS_RESOLVE_RES_EXCEPTION) {
                    return ret;
                } else if (ret == JS_RESOLVE_RES_FOUND) {
                    if (*pme != NULL) {
                        if (*pmodule != res_m ||
                            res_me->local_name != (*pme)->local_name) {
                            *pmodule = NULL;
                            *pme = NULL;
                            return JS_RESOLVE_RES_AMBIGUOUS;
                        }
                    } else {
                        *pmodule = res_m;
                        *pme = res_me;
                    }
                }
            }
            if (*pme != NULL)
                return JS_RESOLVE_RES_FOUND;
        }
        return JS_RESOLVE_RES_NOT_FOUND;
    }
}

/* If the return value is JS_RESOLVE_RES_FOUND, return the module
  (*pmodule) and the corresponding local export entry
  (*pme). Otherwise return (NULL, NULL) */
static JSResolveResultEnum js_resolve_export(JSContext *ctx,
                                             JSModuleDef **pmodule,
                                             JSExportEntry **pme,
                                             JSModuleDef *m,
                                             JSAtom export_name)
{
    JSResolveState ss, *s = &ss;
    int i;
    JSResolveResultEnum ret;

    s->array = NULL;
    s->size = 0;
    s->count = 0;

    ret = js_resolve_export1(ctx, pmodule, pme, m, export_name, s);

    for(i = 0; i < s->count; i++)
        JS_FreeAtom(ctx, s->array[i].name);
    js_free(ctx, s->array);

    return ret;
}

static void js_resolve_export_throw_error(JSContext *ctx,
                                          JSResolveResultEnum res,
                                          JSModuleDef *m, JSAtom export_name)
{
    char buf1[ATOM_GET_STR_BUF_SIZE];
    char buf2[ATOM_GET_STR_BUF_SIZE];
    switch(res) {
    case JS_RESOLVE_RES_EXCEPTION:
        break;
    default:
    case JS_RESOLVE_RES_NOT_FOUND:
        JS_ThrowSyntaxError(ctx, "Could not find export '%s' in module '%s'",
                            JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name),
                            JS_AtomGetStr(ctx, buf2, sizeof(buf2), m->module_name));
        break;
    case JS_RESOLVE_RES_CIRCULAR:
        JS_ThrowSyntaxError(ctx, "circular reference when looking for export '%s' in module '%s'",
                            JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name),
                            JS_AtomGetStr(ctx, buf2, sizeof(buf2), m->module_name));
        break;
    case JS_RESOLVE_RES_AMBIGUOUS:
        JS_ThrowSyntaxError(ctx, "export '%s' in module '%s' is ambiguous",
                            JS_AtomGetStr(ctx, buf1, sizeof(buf1), export_name),
                            JS_AtomGetStr(ctx, buf2, sizeof(buf2), m->module_name));
        break;
    }
}


typedef enum {
    EXPORTED_NAME_AMBIGUOUS,
    EXPORTED_NAME_NORMAL,
    EXPORTED_NAME_DELAYED,
} ExportedNameEntryEnum;

typedef struct ExportedNameEntry {
    JSAtom export_name;
    ExportedNameEntryEnum export_type;
    union {
        JSExportEntry *me; /* using when the list is built */
        JSVarRef *var_ref; /* EXPORTED_NAME_NORMAL */
    } u;
} ExportedNameEntry;

typedef struct GetExportNamesState {
    JSModuleDef **modules;
    int modules_size;
    int modules_count;

    ExportedNameEntry *exported_names;
    int exported_names_size;
    int exported_names_count;
} GetExportNamesState;

static int find_exported_name(GetExportNamesState *s, JSAtom name)
{
    int i;
    for(i = 0; i < s->exported_names_count; i++) {
        if (s->exported_names[i].export_name == name)
            return i;
    }
    return -1;
}

static __exception int get_exported_names(JSContext *ctx,
                                          GetExportNamesState *s,
                                          JSModuleDef *m, BOOL from_star)
{
    ExportedNameEntry *en;
    int i, j;

    /* check circular reference */
    for(i = 0; i < s->modules_count; i++) {
        if (s->modules[i] == m)
            return 0;
    }
    if (js_resize_array(ctx, (void **)&s->modules, sizeof(s->modules[0]),
                        &s->modules_size, s->modules_count + 1))
        return -1;
    s->modules[s->modules_count++] = m;

    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (from_star && me->export_name == JS_ATOM_default)
            continue;
        j = find_exported_name(s, me->export_name);
        if (j < 0) {
            if (js_resize_array(ctx, (void **)&s->exported_names, sizeof(s->exported_names[0]),
                                &s->exported_names_size,
                                s->exported_names_count + 1))
                return -1;
            en = &s->exported_names[s->exported_names_count++];
            en->export_name = me->export_name;
            /* avoid a second lookup for simple module exports */
            if (from_star || me->export_type != JS_EXPORT_TYPE_LOCAL)
                en->u.me = NULL;
            else
                en->u.me = me;
        } else {
            en = &s->exported_names[j];
            en->u.me = NULL;
        }
    }
    for(i = 0; i < m->star_export_entries_count; i++) {
        JSStarExportEntry *se = &m->star_export_entries[i];
        JSModuleDef *m1;
        m1 = m->req_module_entries[se->req_module_idx].module;
        if (get_exported_names(ctx, s, m1, TRUE))
            return -1;
    }
    return 0;
}

/* Unfortunately, the spec gives a different behavior from GetOwnProperty ! */
static int js_module_ns_has(JSContext *ctx, JSValueConst obj, JSAtom atom)
{
    return (find_own_property1(JS_VALUE_GET_OBJ(obj), atom) != NULL);
}

const JSClassExoticMethods js_module_ns_exotic_methods = {
    .has_property = js_module_ns_has,
};

static int exported_names_cmp(const void *p1, const void *p2, void *opaque)
{
    JSContext *ctx = opaque;
    const ExportedNameEntry *me1 = p1;
    const ExportedNameEntry *me2 = p2;
    JSValue str1, str2;
    int ret;

    /* XXX: should avoid allocation memory in atom comparison */
    str1 = JS_AtomToString(ctx, me1->export_name);
    str2 = JS_AtomToString(ctx, me2->export_name);
    if (JS_IsException(str1) || JS_IsException(str2)) {
        /* XXX: raise an error ? */
        ret = 0;
    } else {
        ret = js_string_compare(ctx, JS_VALUE_GET_STRING(str1),
                                JS_VALUE_GET_STRING(str2));
    }
    JS_FreeValue(ctx, str1);
    JS_FreeValue(ctx, str2);
    return ret;
}

JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                     void *opaque)
{
    JSModuleDef *m = opaque;
    JSResolveResultEnum res;
    JSExportEntry *res_me;
    JSModuleDef *res_m;
    JSVarRef *var_ref;

    res = js_resolve_export(ctx, &res_m, &res_me, m, atom);
    if (res != JS_RESOLVE_RES_FOUND) {
        /* fail safe: normally no error should happen here except for memory */
        js_resolve_export_throw_error(ctx, res, m, atom);
        return JS_EXCEPTION;
    }
    if (res_me->local_name == JS_ATOM__star_) {
        return JS_GetModuleNamespace(ctx, res_m->req_module_entries[res_me->u.req_module_idx].module);
    } else {
        if (res_me->u.local.var_ref) {
            var_ref = res_me->u.local.var_ref;
        } else {
            JSObject *p1 = JS_VALUE_GET_OBJ(res_m->func_obj);
            var_ref = p1->u.func.var_refs[res_me->u.local.var_idx];
        }
        /* WARNING: a varref is returned as a string ! */
        return JS_MKPTR(JS_TAG_STRING, var_ref);
    }
}

static JSValue js_build_module_ns(JSContext *ctx, JSModuleDef *m)
{
    JSValue obj;
    JSObject *p;
    GetExportNamesState s_s, *s = &s_s;
    int i, ret;
    JSProperty *pr;

    obj = JS_NewObjectClass(ctx, JS_CLASS_MODULE_NS);
    if (JS_IsException(obj))
        return obj;
    p = JS_VALUE_GET_OBJ(obj);

    memset(s, 0, sizeof(*s));
    ret = get_exported_names(ctx, s, m, FALSE);
    js_free(ctx, s->modules);
    if (ret)
        goto fail;

    /* Resolve the exported names. The ambiguous exports are removed */
    for(i = 0; i < s->exported_names_count; i++) {
        ExportedNameEntry *en = &s->exported_names[i];
        JSResolveResultEnum res;
        JSExportEntry *res_me;
        JSModuleDef *res_m;

        if (en->u.me) {
            res_me = en->u.me; /* fast case: no resolution needed */
            res_m = m;
            res = JS_RESOLVE_RES_FOUND;
        } else {
            res = js_resolve_export(ctx, &res_m, &res_me, m,
                                    en->export_name);
        }
        if (res != JS_RESOLVE_RES_FOUND) {
            if (res != JS_RESOLVE_RES_AMBIGUOUS) {
                js_resolve_export_throw_error(ctx, res, m, en->export_name);
                goto fail;
            }
            en->export_type = EXPORTED_NAME_AMBIGUOUS;
        } else {
            if (res_me->local_name == JS_ATOM__star_) {
                en->export_type = EXPORTED_NAME_DELAYED;
            } else {
                if (res_me->u.local.var_ref) {
                    en->u.var_ref = res_me->u.local.var_ref;
                } else {
                    JSObject *p1 = JS_VALUE_GET_OBJ(res_m->func_obj);
                    en->u.var_ref = p1->u.func.var_refs[res_me->u.local.var_idx];
                }
                if (en->u.var_ref == NULL)
                    en->export_type = EXPORTED_NAME_DELAYED;
                else
                    en->export_type = EXPORTED_NAME_NORMAL;
            }
        }
    }

    /* sort the exported names */
    rqsort(s->exported_names, s->exported_names_count,
           sizeof(s->exported_names[0]), exported_names_cmp, ctx);

    for(i = 0; i < s->exported_names_count; i++) {
        ExportedNameEntry *en = &s->exported_names[i];
        switch(en->export_type) {
        case EXPORTED_NAME_NORMAL:
            {
                JSVarRef *var_ref = en->u.var_ref;
                pr = add_property(ctx, p, en->export_name,
                                  JS_PROP_ENUMERABLE | JS_PROP_WRITABLE |
                                  JS_PROP_VARREF);
                if (!pr)
                    goto fail;
                js_rc(var_ref)->ref_count++;
                pr->u.var_ref = var_ref;
            }
            break;
        case EXPORTED_NAME_DELAYED:
            /* the exported namespace or reference may depend on
               circular references, so we resolve it lazily */
            if (JS_DefineAutoInitProperty(ctx, obj,
                                          en->export_name,
                                          JS_AUTOINIT_ID_MODULE_NS,
                                          m, JS_PROP_ENUMERABLE | JS_PROP_WRITABLE) < 0)
                goto fail;
            break;
        default:
            break;
        }
    }

    js_free(ctx, s->exported_names);

    JS_DefinePropertyValue(ctx, obj, JS_ATOM_Symbol_toStringTag,
                           JS_AtomToString(ctx, JS_ATOM_Module),
                           0);

    p->extensible = FALSE;
    return obj;
 fail:
    js_free(ctx, s->exported_names);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

JSValue JS_GetModuleNamespace(JSContext *ctx, JSModuleDef *m)
{
    if (JS_IsUndefined(m->module_ns)) {
        JSValue val;
        val = js_build_module_ns(ctx, m);
        if (JS_IsException(val))
            return JS_EXCEPTION;
        m->module_ns = val;
    }
    return JS_DupValue(ctx, m->module_ns);
}

/* Load all the required modules for module 'm' */
static int js_resolve_module(JSContext *ctx, JSModuleDef *m)
{
    int i;
    JSModuleDef *m1;

    if (m->resolved)
        return 0;
#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("resolving module '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif
    m->resolved = TRUE;
    /* resolve each requested module */
    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        m1 = js_host_resolve_imported_module_atom(ctx, m->module_name,
                                                  rme->module_name,
                                                  rme->attributes);
        if (!m1)
            return -1;
        rme->module = m1;
        /* already done in js_host_resolve_imported_module() except if
           the module was loaded with JS_EvalBinary() */
        if (js_resolve_module(ctx, m1) < 0)
            return -1;
    }
    return 0;
}

/* Create the <eval> function associated with the module */
static int js_create_module_bytecode_function(JSContext *ctx, JSModuleDef *m)
{
    JSFunctionBytecode *b;
    JSValue func_obj, bfunc;

    bfunc = m->func_obj;
    func_obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                      JS_CLASS_BYTECODE_FUNCTION);

    if (JS_IsException(func_obj))
        return -1;
    m->func_obj = func_obj;
    b = JS_VALUE_GET_PTR(bfunc);
    func_obj = js_closure2(ctx, func_obj, b, NULL, NULL, TRUE, m);
    if (JS_IsException(func_obj)) {
        m->func_obj = JS_UNDEFINED; /* XXX: keep it ? */
        JS_FreeValue(ctx, func_obj);
        return -1;
    }
    return 0;
}

/* must be done before js_link_module() because of cyclic references */
static int js_create_module_function(JSContext *ctx, JSModuleDef *m)
{
    BOOL is_c_module;
    int i;
    JSVarRef *var_ref;

    if (m->func_created)
        return 0;

    is_c_module = (m->init_func != NULL);

    if (is_c_module) {
        /* initialize the exported variables */
        for(i = 0; i < m->export_entries_count; i++) {
            JSExportEntry *me = &m->export_entries[i];
            if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
                var_ref = js_create_var_ref(ctx, FALSE);
                if (!var_ref)
                    return -1;
                me->u.local.var_ref = var_ref;
            }
        }
    } else {
        if (js_create_module_bytecode_function(ctx, m))
            return -1;
    }
    m->func_created = TRUE;

    /* do it on the dependencies */

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        if (js_create_module_function(ctx, rme->module) < 0)
            return -1;
    }

    return 0;
}


/* Prepare a module to be executed by resolving all the imported
   variables. */
static int js_inner_module_linking(JSContext *ctx, JSModuleDef *m,
                                   JSModuleDef **pstack_top, int index)
{
    int i;
    JSImportEntry *mi;
    JSModuleDef *m1;
    JSVarRef **var_refs, *var_ref;
    JSObject *p;
    BOOL is_c_module;
    JSValue ret_val;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        return -1;
    }

#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("js_inner_module_linking '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif

    if (m->status == JS_MODULE_STATUS_LINKING ||
        m->status == JS_MODULE_STATUS_LINKED ||
        m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED)
        return index;

    assert(m->status == JS_MODULE_STATUS_UNLINKED);
    m->status = JS_MODULE_STATUS_LINKING;
    m->dfs_index = index;
    m->dfs_ancestor_index = index;
    index++;
    /* push 'm' on stack */
    m->stack_prev = *pstack_top;
    *pstack_top = m;

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        m1 = rme->module;
        index = js_inner_module_linking(ctx, m1, pstack_top, index);
        if (index < 0)
            goto fail;
        assert(m1->status == JS_MODULE_STATUS_LINKING ||
               m1->status == JS_MODULE_STATUS_LINKED ||
               m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m1->status == JS_MODULE_STATUS_EVALUATED);
        if (m1->status == JS_MODULE_STATUS_LINKING) {
            m->dfs_ancestor_index = min_int(m->dfs_ancestor_index,
                                            m1->dfs_ancestor_index);
        }
    }

#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("instantiating module '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif
    /* check the indirect exports */
    for(i = 0; i < m->export_entries_count; i++) {
        JSExportEntry *me = &m->export_entries[i];
        if (me->export_type == JS_EXPORT_TYPE_INDIRECT &&
            me->local_name != JS_ATOM__star_) {
            JSResolveResultEnum ret;
            JSExportEntry *res_me;
            JSModuleDef *res_m, *m1;
            m1 = m->req_module_entries[me->u.req_module_idx].module;
            ret = js_resolve_export(ctx, &res_m, &res_me, m1, me->local_name);
            if (ret != JS_RESOLVE_RES_FOUND) {
                js_resolve_export_throw_error(ctx, ret, m, me->export_name);
                goto fail;
            }
        }
    }

#ifdef DUMP_MODULE_RESOLVE
    {
        printf("exported bindings:\n");
        for(i = 0; i < m->export_entries_count; i++) {
            JSExportEntry *me = &m->export_entries[i];
            printf(" name="); print_atom(ctx, me->export_name);
            printf(" local="); print_atom(ctx, me->local_name);
            printf(" type=%d idx=%d\n", me->export_type, me->u.local.var_idx);
        }
    }
#endif

    is_c_module = (m->init_func != NULL);

    if (!is_c_module) {
        p = JS_VALUE_GET_OBJ(m->func_obj);
        var_refs = p->u.func.var_refs;

        for(i = 0; i < m->import_entries_count; i++) {
            mi = &m->import_entries[i];
#ifdef DUMP_MODULE_RESOLVE
            printf("import var_idx=%d name=", mi->var_idx);
            print_atom(ctx, mi->import_name);
            printf(": ");
#endif
            m1 = m->req_module_entries[mi->req_module_idx].module;
            if (mi->is_star) {
                JSValue val;
                /* name space import */
                val = JS_GetModuleNamespace(ctx, m1);
                if (JS_IsException(val))
                    goto fail;
                set_value(ctx, &var_refs[mi->var_idx]->value, val);
#ifdef DUMP_MODULE_RESOLVE
                printf("namespace\n");
#endif
            } else {
                JSResolveResultEnum ret;
                JSExportEntry *res_me;
                JSModuleDef *res_m;
                JSObject *p1;

                ret = js_resolve_export(ctx, &res_m,
                                        &res_me, m1, mi->import_name);
                if (ret != JS_RESOLVE_RES_FOUND) {
                    js_resolve_export_throw_error(ctx, ret, m1, mi->import_name);
                    goto fail;
                }
                if (res_me->local_name == JS_ATOM__star_) {
                    JSValue val;
                    JSModuleDef *m2;
                    /* name space import from */
                    m2 = res_m->req_module_entries[res_me->u.req_module_idx].module;
                    val = JS_GetModuleNamespace(ctx, m2);
                    if (JS_IsException(val))
                        goto fail;
                    var_ref = js_create_var_ref(ctx, TRUE);
                    if (!var_ref) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                    set_value(ctx, &var_ref->value, val);
                    var_refs[mi->var_idx] = var_ref;
#ifdef DUMP_MODULE_RESOLVE
                    printf("namespace from\n");
#endif
                } else {
                    var_ref = res_me->u.local.var_ref;
                    if (!var_ref) {
                        p1 = JS_VALUE_GET_OBJ(res_m->func_obj);
                        var_ref = p1->u.func.var_refs[res_me->u.local.var_idx];
                    }
                    js_rc(var_ref)->ref_count++;
                    var_refs[mi->var_idx] = var_ref;
#ifdef DUMP_MODULE_RESOLVE
                    printf("local export (var_ref=%p)\n", var_ref);
#endif
                }
            }
        }

        /* keep the exported variables in the module export entries (they
           are used when the eval function is deleted and cannot be
           initialized before in case imports are exported) */
        for(i = 0; i < m->export_entries_count; i++) {
            JSExportEntry *me = &m->export_entries[i];
            if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
                var_ref = var_refs[me->u.local.var_idx];
                js_rc(var_ref)->ref_count++;
                me->u.local.var_ref = var_ref;
            }
        }

        /* initialize the global variables */
        ret_val = JS_Call(ctx, m->func_obj, JS_TRUE, 0, NULL);
        if (JS_IsException(ret_val))
            goto fail;
        JS_FreeValue(ctx, ret_val);
    }

    assert(m->dfs_ancestor_index <= m->dfs_index);
    if (m->dfs_index == m->dfs_ancestor_index) {
        for(;;) {
            /* pop m1 from stack */
            m1 = *pstack_top;
            *pstack_top = m1->stack_prev;
            m1->status = JS_MODULE_STATUS_LINKED;
            if (m1 == m)
                break;
        }
    }

#ifdef DUMP_MODULE_RESOLVE
    printf("js_inner_module_linking done\n");
#endif
    return index;
 fail:
    return -1;
}

/* Prepare a module to be executed by resolving all the imported
   variables. */
static int js_link_module(JSContext *ctx, JSModuleDef *m)
{
    JSModuleDef *stack_top, *m1;

#ifdef DUMP_MODULE_RESOLVE
    {
        char buf1[ATOM_GET_STR_BUF_SIZE];
        printf("js_link_module '%s':\n", JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name));
    }
#endif
    assert(m->status == JS_MODULE_STATUS_UNLINKED ||
           m->status == JS_MODULE_STATUS_LINKED ||
           m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
           m->status == JS_MODULE_STATUS_EVALUATED);
    stack_top = NULL;
    if (js_inner_module_linking(ctx, m, &stack_top, 0) < 0) {
        while (stack_top != NULL) {
            m1 = stack_top;
            assert(m1->status == JS_MODULE_STATUS_LINKING);
            m1->status = JS_MODULE_STATUS_UNLINKED;
            stack_top = m1->stack_prev;
        }
        return -1;
    }
    assert(stack_top == NULL);
    assert(m->status == JS_MODULE_STATUS_LINKED ||
           m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
           m->status == JS_MODULE_STATUS_EVALUATED);
    return 0;
}

/* return JS_ATOM_NULL if the name cannot be found. Only works with
   not striped bytecode functions. */
JSAtom JS_GetScriptOrModuleName(JSContext *ctx, int n_stack_levels)
{
    JSStackFrame *sf;
    JSFunctionBytecode *b;
    JSObject *p;
    /* XXX: currently we just use the filename of the englobing
       function from the debug info. May need to add a ScriptOrModule
       info in JSFunctionBytecode. */
    sf = ctx->rt->current_stack_frame;
    if (!sf)
        return JS_ATOM_NULL;
    while (n_stack_levels-- > 0) {
        sf = sf->prev_frame;
        if (!sf)
            return JS_ATOM_NULL;
    }
    for(;;) {
        if (JS_VALUE_GET_TAG(sf->cur_func) != JS_TAG_OBJECT)
            return JS_ATOM_NULL;
        p = JS_VALUE_GET_OBJ(sf->cur_func);
        if (!js_class_has_bytecode(p->class_id))
            return JS_ATOM_NULL;
        b = p->u.func.function_bytecode;
        if (!b->is_direct_or_indirect_eval) {
            if (!b->has_debug)
                return JS_ATOM_NULL;
            return JS_DupAtom(ctx, b->debug.filename);
        } else {
            sf = sf->prev_frame;
            if (!sf)
                return JS_ATOM_NULL;
        }
    }
}

JSAtom JS_GetModuleName(JSContext *ctx, JSModuleDef *m)
{
    return JS_DupAtom(ctx, m->module_name);
}

JSValue JS_GetImportMeta(JSContext *ctx, JSModuleDef *m)
{
    JSValue obj;
    /* allocate meta_obj only if requested to save memory */
    obj = m->meta_obj;
    if (JS_IsUndefined(obj)) {
        obj = JS_NewObjectProto(ctx, JS_NULL);
        if (JS_IsException(obj))
            return JS_EXCEPTION;
        m->meta_obj = obj;
    }
    return JS_DupValue(ctx, obj);
}

static JSValue js_import_meta(JSContext *ctx)
{
    JSAtom filename;
    JSModuleDef *m;

    filename = JS_GetScriptOrModuleName(ctx, 0);
    if (filename == JS_ATOM_NULL)
        goto fail;

    /* XXX: inefficient, need to add a module or script pointer in
       JSFunctionBytecode */
    m = js_find_loaded_module(ctx, filename);
    JS_FreeAtom(ctx, filename);
    if (!m) {
    fail:
        JS_ThrowTypeError(ctx, "import.meta not supported in this context");
        return JS_EXCEPTION;
    }
    return JS_GetImportMeta(ctx, m);
}

JSValue JS_NewModuleValue(JSContext *ctx, JSModuleDef *m)
{
    return JS_DupValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
}

static JSValue js_load_module_rejected(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSValueConst *resolving_funcs = (JSValueConst *)func_data;
    JSValueConst error;
    JSValue ret;

    /* XXX: check if the test is necessary */
    if (argc >= 1)
        error = argv[0];
    else
        error = JS_UNDEFINED;
    ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                  1, &error);
    JS_FreeValue(ctx, ret);
    return JS_UNDEFINED;
}

static JSValue js_load_module_fulfilled(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSValueConst *resolving_funcs = (JSValueConst *)func_data;
    JSModuleDef *m = JS_VALUE_GET_PTR(func_data[2]);
    JSValue ret, ns;

    /* return the module namespace */
    ns = JS_GetModuleNamespace(ctx, m);
    if (JS_IsException(ns)) {
        JSValue err = JS_GetException(ctx);
        js_load_module_rejected(ctx, JS_UNDEFINED, 1, (JSValueConst *)&err, 0, func_data);
        return JS_UNDEFINED;
    }
    ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED,
                   1, (JSValueConst *)&ns);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, ns);
    return JS_UNDEFINED;
}

static void JS_LoadModuleInternal(JSContext *ctx, const char *basename,
                                  const char *filename,
                                  JSValueConst *resolving_funcs,
                                  JSValueConst attributes)
{
    JSValue evaluate_promise;
    JSModuleDef *m;
    JSValue ret, err, func_obj, evaluate_resolving_funcs[2];
    JSValueConst func_data[3];

    m = js_host_resolve_imported_module(ctx, basename, filename, attributes);
    if (!m)
        goto fail;

    if (js_resolve_module(ctx, m) < 0) {
        js_free_modules(ctx, JS_FREE_MODULE_NOT_RESOLVED);
        goto fail;
    }

    /* Evaluate the module code */
    func_obj = JS_NewModuleValue(ctx, m);
    evaluate_promise = JS_EvalFunction(ctx, func_obj);
    if (JS_IsException(evaluate_promise)) {
    fail:
        err = JS_GetException(ctx);
        ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                      1, (JSValueConst *)&err);
        JS_FreeValue(ctx, ret); /* XXX: what to do if exception ? */
        JS_FreeValue(ctx, err);
        return;
    }

    func_obj = JS_NewModuleValue(ctx, m);
    func_data[0] = resolving_funcs[0];
    func_data[1] = resolving_funcs[1];
    func_data[2] = func_obj;
    evaluate_resolving_funcs[0] = JS_NewCFunctionData(ctx, js_load_module_fulfilled, 0, 0, 3, func_data);
    evaluate_resolving_funcs[1] = JS_NewCFunctionData(ctx, js_load_module_rejected, 0, 0, 3, func_data);
    JS_FreeValue(ctx, func_obj);
    ret = js_promise_then(ctx, evaluate_promise, 2, (JSValueConst *)evaluate_resolving_funcs);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, evaluate_resolving_funcs[0]);
    JS_FreeValue(ctx, evaluate_resolving_funcs[1]);
    JS_FreeValue(ctx, evaluate_promise);
}

/* Return a promise or an exception in case of memory error. Used by
   os.Worker() */
JSValue JS_LoadModule(JSContext *ctx, const char *basename,
                      const char *filename)
{
    JSValue promise, resolving_funcs[2];

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;
    JS_LoadModuleInternal(ctx, basename, filename,
                          (JSValueConst *)resolving_funcs, JS_UNDEFINED);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return promise;
}

static JSValue js_dynamic_import_job(JSContext *ctx,
                                     int argc, JSValueConst *argv)
{
    JSValueConst *resolving_funcs = argv;
    JSValueConst basename_val = argv[2];
    JSValueConst specifier = argv[3];
    JSValueConst attributes = argv[4];
    const char *basename = NULL, *filename;
    JSValue ret, err;

    if (!JS_IsString(basename_val)) {
        JS_ThrowTypeError(ctx, "no function filename for import()");
        goto exception;
    }
    basename = JS_ToCString(ctx, basename_val);
    if (!basename)
        goto exception;

    filename = JS_ToCString(ctx, specifier);
    if (!filename)
        goto exception;

    JS_LoadModuleInternal(ctx, basename, filename,
                          resolving_funcs, attributes);
    JS_FreeCString(ctx, filename);
    JS_FreeCString(ctx, basename);
    return JS_UNDEFINED;
 exception:
    err = JS_GetException(ctx);
    ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                   1, (JSValueConst *)&err);
    JS_FreeValue(ctx, ret); /* XXX: what to do if exception ? */
    JS_FreeValue(ctx, err);
    JS_FreeCString(ctx, basename);
    return JS_UNDEFINED;
}

static JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options)
{
    JSAtom basename;
    JSValue promise, resolving_funcs[2], basename_val, err, ret;
    JSValue specifier_str = JS_UNDEFINED, attributes = JS_UNDEFINED, attributes_obj = JS_UNDEFINED;
    JSValueConst args[5];

    basename = JS_GetScriptOrModuleName(ctx, 0);
    if (basename == JS_ATOM_NULL)
        basename_val = JS_NULL;
    else
        basename_val = JS_AtomToValue(ctx, basename);
    JS_FreeAtom(ctx, basename);
    if (JS_IsException(basename_val))
        return basename_val;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) {
        JS_FreeValue(ctx, basename_val);
        return promise;
    }

    /* the string conversion must occur here */
    specifier_str = JS_ToString(ctx, specifier);
    if (JS_IsException(specifier_str))
        goto exception;
    
    if (!JS_IsUndefined(options)) {
        if (!JS_IsObject(options)) {
            JS_ThrowTypeError(ctx, "options must be an object");
            goto exception;
        }
        attributes_obj = JS_GetProperty(ctx, options, JS_ATOM_with);
        if (JS_IsException(attributes_obj))
            goto exception;
        if (!JS_IsUndefined(attributes_obj)) {
            JSPropertyEnum *atoms;
            uint32_t atoms_len, i;
            JSValue val;
            
            if (!JS_IsObject(attributes_obj)) {
                JS_ThrowTypeError(ctx, "options.with must be an object");
                goto exception;
            }
            attributes = JS_NewObjectProto(ctx, JS_NULL);
            if (JS_GetOwnPropertyNamesInternal(ctx, &atoms, &atoms_len, JS_VALUE_GET_OBJ(attributes_obj),
                                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
                goto exception;
            }
            for(i = 0; i < atoms_len; i++) {
                val = JS_GetProperty(ctx, attributes_obj, atoms[i].atom);
                if (JS_IsException(val))
                    goto exception1;
                if (!JS_IsString(val)) {
                    JS_FreeValue(ctx, val);
                    JS_ThrowTypeError(ctx, "module attribute values must be strings");
                    goto exception1;
                }
                if (JS_DefinePropertyValue(ctx, attributes,  atoms[i].atom, val,
                                           JS_PROP_C_W_E) < 0) {
                exception1:
                    JS_FreePropertyEnum(ctx, atoms, atoms_len);
                    goto exception;
                }
            }
            JS_FreePropertyEnum(ctx, atoms, atoms_len);
            if (ctx->rt->module_check_attrs &&
                ctx->rt->module_check_attrs(ctx, ctx->rt->module_loader_opaque, attributes) < 0) {
                goto exception;
            }
            JS_FreeValue(ctx, attributes_obj);
        }
    }

    args[0] = resolving_funcs[0];
    args[1] = resolving_funcs[1];
    args[2] = basename_val;
    args[3] = specifier_str;
    args[4] = attributes;
    
    /* cannot run JS_LoadModuleInternal synchronously because it would
       cause an unexpected recursion in js_evaluate_module() */
    JS_EnqueueJob(ctx, js_dynamic_import_job, 5, args);
 done:
    JS_FreeValue(ctx, basename_val);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, specifier_str);
    JS_FreeValue(ctx, attributes);
    return promise;
 exception:
    JS_FreeValue(ctx, attributes_obj);
    err = JS_GetException(ctx);
    ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED,
                   1, (JSValueConst *)&err);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, err);
    goto done;
}

static void js_set_module_evaluated(JSContext *ctx, JSModuleDef *m)
{
    m->status = JS_MODULE_STATUS_EVALUATED;
    if (!JS_IsUndefined(m->promise)) {
        JSValue value, ret_val;
        assert(m->cycle_root == m);
        value = JS_UNDEFINED;
        ret_val = JS_Call(ctx, m->resolving_funcs[0], JS_UNDEFINED,
                          1, (JSValueConst *)&value);
        JS_FreeValue(ctx, ret_val);
    }
}

typedef struct {
    JSModuleDef **tab;
    int count;
    int size;
} ExecModuleList;

/* XXX: slow. Could use a linked list instead of ExecModuleList */
static BOOL find_in_exec_module_list(ExecModuleList *exec_list, JSModuleDef *m)
{
    int i;
    for(i = 0; i < exec_list->count; i++) {
        if (exec_list->tab[i] == m)
            return TRUE;
    }
    return FALSE;
}

static int gather_available_ancestors(JSContext *ctx, JSModuleDef *module,
                                      ExecModuleList *exec_list)
{
    int i;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        return -1;
    }
    for(i = 0; i < module->async_parent_modules_count; i++) {
        JSModuleDef *m = module->async_parent_modules[i];
        if (!find_in_exec_module_list(exec_list, m) &&
            !m->cycle_root->eval_has_exception) {
            assert(m->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
            assert(!m->eval_has_exception);
            assert(m->async_evaluation);
            assert(m->pending_async_dependencies > 0);
            m->pending_async_dependencies--;
            if (m->pending_async_dependencies == 0) {
                if (js_resize_array(ctx, (void **)&exec_list->tab, sizeof(exec_list->tab[0]), &exec_list->size, exec_list->count + 1)) {
                    return -1;
                }
                exec_list->tab[exec_list->count++] = m;
                if (!m->has_tla) {
                    if (gather_available_ancestors(ctx, m, exec_list))
                        return -1;
                }
            }
        }
    }
    return 0;
}

static int exec_module_list_cmp(const void *p1, const void *p2, void *opaque)
{
    JSModuleDef *m1 = *(JSModuleDef **)p1;
    JSModuleDef *m2 = *(JSModuleDef **)p2;
    return (m1->async_evaluation_timestamp > m2->async_evaluation_timestamp) -
        (m1->async_evaluation_timestamp < m2->async_evaluation_timestamp);
}

static int js_execute_async_module(JSContext *ctx, JSModuleDef *m);
static int js_execute_sync_module(JSContext *ctx, JSModuleDef *m,
                                  JSValue *pvalue);
#ifdef DUMP_MODULE_EXEC
static void js_dump_module(JSContext *ctx, const char *str, JSModuleDef *m)
{
    char buf1[ATOM_GET_STR_BUF_SIZE];
    static const char *module_status_str[] = { "unlinked", "linking", "linked", "evaluating", "evaluating_async", "evaluated" };
    printf("%s: %s status=%s\n", str, JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name), module_status_str[m->status]);
}
#endif

static JSValue js_async_module_execution_rejected(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSModuleDef *module = JS_VALUE_GET_PTR(func_data[0]);
    JSValueConst error = argv[0];
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, module);
#endif
    if (js_check_stack_overflow(ctx->rt, 0))
        return JS_ThrowStackOverflow(ctx);

    if (module->status == JS_MODULE_STATUS_EVALUATED) {
        assert(module->eval_has_exception);
        return JS_UNDEFINED;
    }

    assert(module->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
    assert(!module->eval_has_exception);
    assert(module->async_evaluation);

    module->eval_has_exception = TRUE;
    module->eval_exception = JS_DupValue(ctx, error);
    module->status = JS_MODULE_STATUS_EVALUATED;
    module->async_evaluation = FALSE;

    if (!JS_IsUndefined(module->promise)) {
        JSValue ret_val;
        assert(module->cycle_root == module);
        ret_val = JS_Call(ctx, module->resolving_funcs[1], JS_UNDEFINED,
                          1, &error);
        JS_FreeValue(ctx, ret_val);
    }

    for(i = 0; i < module->async_parent_modules_count; i++) {
        JSModuleDef *m = module->async_parent_modules[i];
        JSValue m_obj = JS_NewModuleValue(ctx, m);
        js_async_module_execution_rejected(ctx, JS_UNDEFINED, 1, &error, 0,
                                           &m_obj);
        JS_FreeValue(ctx, m_obj);
    }
    return JS_UNDEFINED;
}

static JSValue js_async_module_execution_fulfilled(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSModuleDef *module = JS_VALUE_GET_PTR(func_data[0]);
    ExecModuleList exec_list_s, *exec_list = &exec_list_s;
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, module);
#endif
    if (module->status == JS_MODULE_STATUS_EVALUATED) {
        assert(module->eval_has_exception);
        return JS_UNDEFINED;
    }
    assert(module->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
    assert(!module->eval_has_exception);
    assert(module->async_evaluation);
    module->async_evaluation = FALSE;
    js_set_module_evaluated(ctx, module);

    exec_list->tab = NULL;
    exec_list->count = 0;
    exec_list->size = 0;

    if (gather_available_ancestors(ctx, module, exec_list) < 0) {
        js_free(ctx, exec_list->tab);
        return JS_EXCEPTION;
    }

    /* sort by increasing async_evaluation timestamp */
    rqsort(exec_list->tab, exec_list->count, sizeof(exec_list->tab[0]),
           exec_module_list_cmp, NULL);

    for(i = 0; i < exec_list->count; i++) {
        JSModuleDef *m = exec_list->tab[i];
#ifdef DUMP_MODULE_EXEC
        printf("  %d/%d", i, exec_list->count); js_dump_module(ctx, "", m);
#endif
        if (m->status == JS_MODULE_STATUS_EVALUATED) {
            assert(m->eval_has_exception);
        } else if (m->has_tla) {
            js_execute_async_module(ctx, m);
        } else {
            JSValue error;
            if (js_execute_sync_module(ctx, m, &error) < 0) {
                JSValue m_obj = JS_NewModuleValue(ctx, m);
                js_async_module_execution_rejected(ctx, JS_UNDEFINED,
                                                   1, (JSValueConst *)&error, 0,
                                                   &m_obj);
                JS_FreeValue(ctx, m_obj);
                JS_FreeValue(ctx, error);
            } else {
                m->async_evaluation = FALSE;
                js_set_module_evaluated(ctx, m);
            }
        }
    }
    js_free(ctx, exec_list->tab);
    return JS_UNDEFINED;
}

static int js_execute_async_module(JSContext *ctx, JSModuleDef *m)
{
    JSValue promise, m_obj;
    JSValue resolve_funcs[2], ret_val;
#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    promise = js_async_function_call(ctx, m->func_obj, JS_UNDEFINED, 0, NULL, 0);
    if (JS_IsException(promise))
        return -1;
    m_obj = JS_NewModuleValue(ctx, m);
    resolve_funcs[0] = JS_NewCFunctionData(ctx, js_async_module_execution_fulfilled, 0, 0, 1, (JSValueConst *)&m_obj);
    resolve_funcs[1] = JS_NewCFunctionData(ctx, js_async_module_execution_rejected, 0, 0, 1, (JSValueConst *)&m_obj);
    ret_val = js_promise_then(ctx, promise, 2, (JSValueConst *)resolve_funcs);
    JS_FreeValue(ctx, ret_val);
    JS_FreeValue(ctx, m_obj);
    JS_FreeValue(ctx, resolve_funcs[0]);
    JS_FreeValue(ctx, resolve_funcs[1]);
    JS_FreeValue(ctx, promise);
    return 0;
}

/* return < 0 in case of exception. *pvalue contains the exception. */
static int js_execute_sync_module(JSContext *ctx, JSModuleDef *m,
                                  JSValue *pvalue)
{
#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    if (m->init_func) {
        /* C module init : no asynchronous execution */
        if (m->init_func(ctx, m) < 0)
            goto fail;
    } else {
        JSValue promise;
        JSPromiseStateEnum state;

        promise = js_async_function_call(ctx, m->func_obj, JS_UNDEFINED, 0, NULL, 0);
        if (JS_IsException(promise))
            goto fail;
        state = JS_PromiseState(ctx, promise);
        if (state == JS_PROMISE_FULFILLED) {
            JS_FreeValue(ctx, promise);
        } else if (state == JS_PROMISE_REJECTED) {
            *pvalue = JS_PromiseResult(ctx, promise);
            JS_FreeValue(ctx, promise);
            return -1;
        } else {
            JS_FreeValue(ctx, promise);
            JS_ThrowTypeError(ctx, "promise is pending");
        fail:
            *pvalue = JS_GetException(ctx);
            return -1;
        }
    }
    *pvalue = JS_UNDEFINED;
    return 0;
}

/* spec: InnerModuleEvaluation. Return (index, JS_UNDEFINED) or (-1,
   exception) */
static int js_inner_module_evaluation(JSContext *ctx, JSModuleDef *m,
                                      int index, JSModuleDef **pstack_top,
                                      JSValue *pvalue)
{
    JSModuleDef *m1;
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        *pvalue = JS_GetException(ctx);
        return -1;
    }

    if (m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED) {
        if (m->eval_has_exception) {
            *pvalue = JS_DupValue(ctx, m->eval_exception);
            return -1;
        } else {
            *pvalue = JS_UNDEFINED;
            return index;
        }
    }
    if (m->status == JS_MODULE_STATUS_EVALUATING) {
        *pvalue = JS_UNDEFINED;
        return index;
    }
    assert(m->status == JS_MODULE_STATUS_LINKED);

    m->status = JS_MODULE_STATUS_EVALUATING;
    m->dfs_index = index;
    m->dfs_ancestor_index = index;
    m->pending_async_dependencies = 0;
    index++;
    /* push 'm' on stack */
    m->stack_prev = *pstack_top;
    *pstack_top = m;

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        m1 = rme->module;
        index = js_inner_module_evaluation(ctx, m1, index, pstack_top, pvalue);
        if (index < 0)
            return -1;
        assert(m1->status == JS_MODULE_STATUS_EVALUATING ||
               m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m1->status == JS_MODULE_STATUS_EVALUATED);
        if (m1->status == JS_MODULE_STATUS_EVALUATING) {
            m->dfs_ancestor_index = min_int(m->dfs_ancestor_index,
                                            m1->dfs_ancestor_index);
        } else {
            m1 = m1->cycle_root;
            assert(m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
                   m1->status == JS_MODULE_STATUS_EVALUATED);
            if (m1->eval_has_exception) {
                *pvalue = JS_DupValue(ctx, m1->eval_exception);
                return -1;
            }
        }
        if (m1->async_evaluation) {
            m->pending_async_dependencies++;
            if (js_resize_array(ctx, (void **)&m1->async_parent_modules, sizeof(m1->async_parent_modules[0]), &m1->async_parent_modules_size, m1->async_parent_modules_count + 1)) {
                *pvalue = JS_GetException(ctx);
                return -1;
            }
            m1->async_parent_modules[m1->async_parent_modules_count++] = m;
        }
    }

    if (m->pending_async_dependencies > 0) {
        assert(!m->async_evaluation);
        m->async_evaluation = TRUE;
        m->async_evaluation_timestamp =
            ctx->rt->module_async_evaluation_next_timestamp++;
    } else if (m->has_tla) {
        assert(!m->async_evaluation);
        m->async_evaluation = TRUE;
        m->async_evaluation_timestamp =
            ctx->rt->module_async_evaluation_next_timestamp++;
        js_execute_async_module(ctx, m);
    } else {
        if (js_execute_sync_module(ctx, m, pvalue) < 0)
            return -1;
    }

    assert(m->dfs_ancestor_index <= m->dfs_index);
    if (m->dfs_index == m->dfs_ancestor_index) {
        for(;;) {
            /* pop m1 from stack */
            m1 = *pstack_top;
            *pstack_top = m1->stack_prev;
            if (!m1->async_evaluation) {
                m1->status = JS_MODULE_STATUS_EVALUATED;
            } else {
                m1->status = JS_MODULE_STATUS_EVALUATING_ASYNC;
            }
            /* spec bug: cycle_root must be assigned before the test */
            m1->cycle_root = m;
            if (m1 == m)
                break;
        }
    }
    *pvalue = JS_UNDEFINED;
    return index;
}

/* Run the <eval> function of the module and of all its requested
   modules. Return a promise or an exception. */
static JSValue js_evaluate_module(JSContext *ctx, JSModuleDef *m)
{
    JSModuleDef *m1, *stack_top;
    JSValue ret_val, result;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    assert(m->status == JS_MODULE_STATUS_LINKED ||
           m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
           m->status == JS_MODULE_STATUS_EVALUATED);
    if (m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED) {
        m = m->cycle_root;
    }
    /* a promise may be created only on the cycle_root of a cycle */
    if (!JS_IsUndefined(m->promise))
        return JS_DupValue(ctx, m->promise);
    m->promise = JS_NewPromiseCapability(ctx, m->resolving_funcs);
    if (JS_IsException(m->promise))
        return JS_EXCEPTION;

    stack_top = NULL;
    if (js_inner_module_evaluation(ctx, m, 0, &stack_top, &result) < 0) {
        while (stack_top != NULL) {
            m1 = stack_top;
            assert(m1->status == JS_MODULE_STATUS_EVALUATING);
            m1->status = JS_MODULE_STATUS_EVALUATED;
            m1->eval_has_exception = TRUE;
            m1->eval_exception = JS_DupValue(ctx, result);
            m1->cycle_root = m; /* spec bug: should be present */
            stack_top = m1->stack_prev;
        }
        JS_FreeValue(ctx, result);
        assert(m->status == JS_MODULE_STATUS_EVALUATED);
        assert(m->eval_has_exception);
        ret_val = JS_Call(ctx, m->resolving_funcs[1], JS_UNDEFINED,
                          1, (JSValueConst *)&m->eval_exception);
        JS_FreeValue(ctx, ret_val);
    } else {
#ifdef DUMP_MODULE_EXEC
        js_dump_module(ctx, "  done", m);
#endif
        assert(m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m->status == JS_MODULE_STATUS_EVALUATED);
        assert(!m->eval_has_exception);
        if (!m->async_evaluation) {
            JSValue value;
            assert(m->status == JS_MODULE_STATUS_EVALUATED);
            value = JS_UNDEFINED;
            ret_val = JS_Call(ctx, m->resolving_funcs[0], JS_UNDEFINED,
                              1, (JSValueConst *)&value);
            JS_FreeValue(ctx, ret_val);
        }
        assert(stack_top == NULL);
    }
    return JS_DupValue(ctx, m->promise);
}

/* [Parser Part 2 moved to src/compiler/js_parser.c] */


/* [Codegen Section B moved to src/compiler/js_codegen.c] */

void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b)
{
    int i;

#if 0
    {
        char buf[ATOM_GET_STR_BUF_SIZE];
        printf("freeing %s\n",
               JS_AtomGetStrRT(rt, buf, sizeof(buf), b->func_name));
    }
#endif
    if (b->byte_code_buf)
        free_bytecode_atoms(rt, b->byte_code_buf, b->byte_code_len, TRUE);

    if (b->vardefs) {
        for(i = 0; i < b->arg_count + b->var_count; i++) {
            JS_FreeAtomRT(rt, b->vardefs[i].var_name);
        }
    }
    for(i = 0; i < b->cpool_count; i++)
        JS_FreeValueRT(rt, b->cpool[i]);

    for(i = 0; i < b->closure_var_count; i++) {
        JSClosureVar *cv = &b->closure_var[i];
        JS_FreeAtomRT(rt, cv->var_name);
    }
    if (b->realm)
        JS_FreeContext(b->realm);

    JS_FreeAtomRT(rt, b->func_name);
    if (b->has_debug) {
        JS_FreeAtomRT(rt, b->debug.filename);
        js_free_rt(rt, b->debug.pc2line_buf);
        js_free_rt(rt, b->debug.source);
    }

    remove_gc_object(&b->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES && js_rc(b)->ref_count != 0) {
        list_add_tail(&b->header.link, &rt->gc_zero_ref_count_list);
    } else {
        js_free_rt(rt, b);
    }
}

/* [Parser Part 3 moved to src/compiler/js_parser.c] */


/* [js_parse_init moved to src/compiler/js_lexer.c] */


static JSValue JS_EvalFunctionInternal(JSContext *ctx, JSValue fun_obj,
                                       JSValueConst this_obj,
                                       JSVarRef **var_refs, JSStackFrame *sf)
{
    JSValue ret_val;
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(fun_obj);
    if (tag == JS_TAG_FUNCTION_BYTECODE) {
        fun_obj = js_closure(ctx, fun_obj, var_refs, sf, TRUE);
        if (JS_IsException(fun_obj))
            return JS_EXCEPTION;
        ret_val = JS_CallFree(ctx, fun_obj, this_obj, 0, NULL);
    } else if (tag == JS_TAG_MODULE) {
        JSModuleDef *m;
        m = JS_VALUE_GET_PTR(fun_obj);
        /* the module refcount should be >= 2 */
        JS_FreeValue(ctx, fun_obj);
        if (js_create_module_function(ctx, m) < 0)
            goto fail;
        if (js_link_module(ctx, m) < 0)
            goto fail;
        ret_val = js_evaluate_module(ctx, m);
        if (JS_IsException(ret_val)) {
        fail:
            return JS_EXCEPTION;
        }
    } else {
        JS_FreeValue(ctx, fun_obj);
        ret_val = JS_ThrowTypeError(ctx, "bytecode function expected");
    }
    return ret_val;
}

JSValue JS_EvalFunction(JSContext *ctx, JSValue fun_obj)
{
    return JS_EvalFunctionInternal(ctx, fun_obj, ctx->global_obj, NULL, NULL);
}

/* 'input' must be zero terminated i.e. input[input_len] = '\0'. */
JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                          const char *input, size_t input_len,
                          const char *filename, int flags, int scope_idx)
{
    JSParseState s1, *s = &s1;
    int err, js_mode, eval_type;
    JSValue fun_obj, ret_val;
    JSStackFrame *sf;
    JSVarRef **var_refs;
    JSFunctionBytecode *b;
    JSFunctionDef *fd;
    JSModuleDef *m;

    js_parse_init(ctx, s, input, input_len, filename);
    skip_shebang(&s->buf_ptr, s->buf_end);

    eval_type = flags & JS_EVAL_TYPE_MASK;
    m = NULL;
    if (eval_type == JS_EVAL_TYPE_DIRECT) {
        JSObject *p;
        sf = ctx->rt->current_stack_frame;
        assert(sf != NULL);
        assert(JS_VALUE_GET_TAG(sf->cur_func) == JS_TAG_OBJECT);
        p = JS_VALUE_GET_OBJ(sf->cur_func);
        assert(js_class_has_bytecode(p->class_id));
        b = p->u.func.function_bytecode;
        var_refs = p->u.func.var_refs;
        js_mode = b->js_mode;
    } else {
        sf = NULL;
        b = NULL;
        var_refs = NULL;
        js_mode = 0;
        if (flags & JS_EVAL_FLAG_STRICT)
            js_mode |= JS_MODE_STRICT;
        if (eval_type == JS_EVAL_TYPE_MODULE) {
            JSAtom module_name = JS_NewAtom(ctx, filename);
            if (module_name == JS_ATOM_NULL)
                return JS_EXCEPTION;
            m = js_new_module_def(ctx, module_name);
            if (!m)
                return JS_EXCEPTION;
            js_mode |= JS_MODE_STRICT;
        }
    }
    fd = js_new_function_def(ctx, NULL, TRUE, FALSE, filename,
                             s->buf_start, &s->get_line_col_cache);
    if (!fd)
        goto fail1;
    s->cur_func = fd;
    fd->eval_type = eval_type;
    fd->has_this_binding = (eval_type != JS_EVAL_TYPE_DIRECT);
    if (eval_type == JS_EVAL_TYPE_DIRECT) {
        fd->new_target_allowed = b->new_target_allowed;
        fd->super_call_allowed = b->super_call_allowed;
        fd->super_allowed = b->super_allowed;
        fd->arguments_allowed = b->arguments_allowed;
    } else {
        fd->new_target_allowed = FALSE;
        fd->super_call_allowed = FALSE;
        fd->super_allowed = FALSE;
        fd->arguments_allowed = TRUE;
    }
    fd->js_mode = js_mode;
    fd->func_name = JS_DupAtom(ctx, JS_ATOM__eval_);
    if (b) {
        if (add_closure_variables(ctx, fd, b, scope_idx))
            goto fail;
    }
    fd->module = m;
    if (m != NULL || (flags & JS_EVAL_FLAG_ASYNC)) {
        fd->in_function_body = TRUE;
        fd->func_kind = JS_FUNC_ASYNC;
    }
    s->is_module = (m != NULL);
    s->allow_html_comments = !s->is_module;

    push_scope(s); /* body scope */
    fd->body_scope = fd->scope_level;

    err = js_parse_program(s);
    if (err) {
    fail:
        free_token(s, &s->token);
        js_free_function_def(ctx, fd);
        goto fail1;
    }

    if (m != NULL)
        m->has_tla = fd->has_await;

    /* create the function object and all the enclosed functions */
    fun_obj = js_create_function(ctx, fd);
    if (JS_IsException(fun_obj))
        goto fail1;
    /* Could add a flag to avoid resolution if necessary */
    if (m) {
        m->func_obj = fun_obj;
        if (js_resolve_module(ctx, m) < 0)
            goto fail1;
        fun_obj = JS_NewModuleValue(ctx, m);
    }
    if (flags & JS_EVAL_FLAG_COMPILE_ONLY) {
        ret_val = fun_obj;
    } else {
        ret_val = JS_EvalFunctionInternal(ctx, fun_obj, this_obj, var_refs, sf);
    }
    return ret_val;
 fail1:
    /* XXX: should free all the unresolved dependencies */
    if (m)
        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
    return JS_EXCEPTION;
}

/* the indirection is needed to make 'eval' optional */
static JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                               const char *input, size_t input_len,
                               const char *filename, int flags, int scope_idx)
{
    BOOL backtrace_barrier = ((flags & JS_EVAL_FLAG_BACKTRACE_BARRIER) != 0);
    int saved_js_mode = 0;
    JSValue ret;
    
    if (unlikely(!ctx->eval_internal)) {
        return JS_ThrowTypeError(ctx, "eval is not supported");
    }
    if (backtrace_barrier && ctx->rt->current_stack_frame) {
        saved_js_mode = ctx->rt->current_stack_frame->js_mode;
        ctx->rt->current_stack_frame->js_mode |= JS_MODE_BACKTRACE_BARRIER;
    }
    ret = ctx->eval_internal(ctx, this_obj, input, input_len, filename,
                             flags, scope_idx);
    if (backtrace_barrier && ctx->rt->current_stack_frame)
        ctx->rt->current_stack_frame->js_mode = saved_js_mode;
    return ret;
}

JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                      JSValueConst val, int flags, int scope_idx)
{
    JSValue ret;
    const char *str;
    size_t len;

    if (!JS_IsString(val))
        return JS_DupValue(ctx, val);
    str = JS_ToCStringLen(ctx, &len, val);
    if (!str)
        return JS_EXCEPTION;
    ret = JS_EvalInternal(ctx, this_obj, str, len, "<input>", flags, scope_idx);
    JS_FreeCString(ctx, str);
    return ret;
}

JSValue JS_EvalThis(JSContext *ctx, JSValueConst this_obj,
                    const char *input, size_t input_len,
                    const char *filename, int eval_flags)
{
    int eval_type = eval_flags & JS_EVAL_TYPE_MASK;
    JSValue ret;

    assert(eval_type == JS_EVAL_TYPE_GLOBAL ||
           eval_type == JS_EVAL_TYPE_MODULE);
    ret = JS_EvalInternal(ctx, this_obj, input, input_len, filename,
                          eval_flags, -1);
    return ret;
}

JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags)
{
    return JS_EvalThis(ctx, ctx->global_obj, input, input_len, filename,
                       eval_flags);
}

int JS_ResolveModule(JSContext *ctx, JSValueConst obj)
{
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
        JSModuleDef *m = JS_VALUE_GET_PTR(obj);
        if (js_resolve_module(ctx, m) < 0) {
            js_free_modules(ctx, JS_FREE_MODULE_NOT_RESOLVED);
            return -1;
        }
    }
    return 0;
}

/* [Bytecode Serialization moved to src/compiler/js_bc.c] */


/*******************************************************************/
/* runtime functions & objects */

/* [js_string_constructor in js_builtin_string.c] */
/* [js_boolean_constructor in js_builtin_core.c] */
/* [js_number_constructor declared in js_builtins.h] */

/* [check_function moved to src/quickjs-internal.h] */

/* check_exception_free is inline in src/quickjs-internal.h */

static JSAtom find_atom(JSContext *ctx, const char *name)
{
    JSAtom atom;
    int len;

    if (*name == '[') {
        name++;
        len = strlen(name) - 1;
        /* We assume 8 bit non null strings, which is the case for these
           symbols */
        for(atom = JS_ATOM_Symbol_toPrimitive; atom < JS_ATOM_END; atom++) {
            JSAtomStruct *p = ctx->rt->atom_array[atom];
            JSString *str = p;
            if (str->len == len && !memcmp(str->u.str8, name, len))
                return JS_DupAtom(ctx, atom);
        }
        abort();
    } else {
        atom = JS_NewAtom(ctx, name);
    }
    return atom;
}

JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                                     const JSCFunctionListEntry *fields, int n_fields)
{
    JSValue obj;
    obj = JS_NewObjectProtoClassAlloc(ctx, proto, JS_CLASS_OBJECT, n_fields);
    if (JS_IsException(obj))
        return obj;
    if (JS_SetPropertyFunctionList(ctx, obj, fields, n_fields)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque)
{
    const JSCFunctionListEntry *e = opaque;
    JSValue val, proto;

    switch(e->def_type) {
    case JS_DEF_CFUNC:
        val = JS_NewCFunction2(ctx, e->u.func.cfunc.generic,
                               e->name, e->u.func.length, e->u.func.cproto, e->magic);
        break;
    case JS_DEF_PROP_STRING:
        val = JS_NewAtomString(ctx, e->u.str);
        break;
    case JS_DEF_OBJECT:
        /* XXX: could add a flag */
        if (atom == JS_ATOM_Symbol_unscopables)
            proto = JS_NULL;
        else
            proto = ctx->class_proto[JS_CLASS_OBJECT];
        val = JS_NewObjectProtoList(ctx, proto,
                                    e->u.prop_list.tab, e->u.prop_list.len);
        break;
    default:
        abort();
    }
    return val;
}

static int JS_InstantiateFunctionListItem(JSContext *ctx, JSValueConst obj,
                                          JSAtom atom,
                                          const JSCFunctionListEntry *e)
{
    JSValue val;
    int prop_flags = e->prop_flags;

    switch(e->def_type) {
    case JS_DEF_ALIAS: /* using autoinit for aliases is not safe */
        {
            JSAtom atom1 = find_atom(ctx, e->u.alias.name);
            switch (e->u.alias.base) {
            case -1:
                val = JS_GetProperty(ctx, obj, atom1);
                break;
            case 0:
                val = JS_GetProperty(ctx, ctx->global_obj, atom1);
                break;
            case 1:
                val = JS_GetProperty(ctx, ctx->class_proto[JS_CLASS_ARRAY], atom1);
                break;
            default:
                abort();
            }
            JS_FreeAtom(ctx, atom1);
            if (JS_IsException(val))
                return -1;
            if (atom == JS_ATOM_Symbol_toPrimitive) {
                /* Symbol.toPrimitive functions are not writable */
                prop_flags = JS_PROP_CONFIGURABLE;
            } else if (atom == JS_ATOM_Symbol_hasInstance) {
                /* Function.prototype[Symbol.hasInstance] is not writable nor configurable */
                prop_flags = 0;
            }
        }
        break;
    case JS_DEF_CFUNC:
        if (atom == JS_ATOM_Symbol_toPrimitive) {
            /* Symbol.toPrimitive functions are not writable */
            prop_flags = JS_PROP_CONFIGURABLE;
        } else if (atom == JS_ATOM_Symbol_hasInstance) {
            /* Function.prototype[Symbol.hasInstance] is not writable nor configurable */
            prop_flags = 0;
        }
        if (JS_DefineAutoInitProperty(ctx, obj, atom, JS_AUTOINIT_ID_PROP,
                                      (void *)e, prop_flags) < 0)
            return -1;
        return 0;
    case JS_DEF_CGETSET: /* XXX: use autoinit again ? */
    case JS_DEF_CGETSET_MAGIC:
        {
            JSValue getter, setter;
            char buf[64];

            getter = JS_UNDEFINED;
            if (e->u.getset.get.generic) {
                snprintf(buf, sizeof(buf), "get %s", e->name);
                getter = JS_NewCFunction2(ctx, e->u.getset.get.generic,
                                          buf, 0, e->def_type == JS_DEF_CGETSET_MAGIC ? JS_CFUNC_getter_magic : JS_CFUNC_getter,
                                          e->magic);
                if (JS_IsException(getter))
                    return -1;
            }
            setter = JS_UNDEFINED;
            if (e->u.getset.set.generic) {
                snprintf(buf, sizeof(buf), "set %s", e->name);
                setter = JS_NewCFunction2(ctx, e->u.getset.set.generic,
                                          buf, 1, e->def_type == JS_DEF_CGETSET_MAGIC ? JS_CFUNC_setter_magic : JS_CFUNC_setter,
                                          e->magic);
                if (JS_IsException(setter)) {
                    JS_FreeValue(ctx, getter);
                    return -1;
                }
            }
            if (JS_DefinePropertyGetSet(ctx, obj, atom, getter, setter, prop_flags) < 0)
                return -1;
            return 0;
        }
        break;
    case JS_DEF_PROP_INT32:
        val = JS_NewInt32(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_INT64:
        val = JS_NewInt64(ctx, e->u.i64);
        break;
    case JS_DEF_PROP_DOUBLE:
        val = __JS_NewFloat64(ctx, e->u.f64);
        break;
    case JS_DEF_PROP_UNDEFINED:
        val = JS_UNDEFINED;
        break;
    case JS_DEF_PROP_ATOM:
        val = JS_AtomToValue(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_BOOL:
        val = JS_NewBool(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_STRING:
    case JS_DEF_OBJECT:
        if (JS_DefineAutoInitProperty(ctx, obj, atom, JS_AUTOINIT_ID_PROP,
                                      (void *)e, prop_flags) < 0)
            return -1;
        return 0;
    default:
        abort();
    }
    if (JS_DefinePropertyValue(ctx, obj, atom, val, prop_flags) < 0)
        return -1;
    return 0;
}

int JS_SetPropertyFunctionList(JSContext *ctx, JSValueConst obj,
                               const JSCFunctionListEntry *tab, int len)
{
    int i, ret;

    for (i = 0; i < len; i++) {
        const JSCFunctionListEntry *e = &tab[i];
        JSAtom atom = find_atom(ctx, e->name);
        if (atom == JS_ATOM_NULL)
            return -1;
        ret = JS_InstantiateFunctionListItem(ctx, obj, atom, e);
        JS_FreeAtom(ctx, atom);
        if (ret)
            return -1;
    }
    return 0;
}

int JS_AddModuleExportList(JSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len)
{
    int i;
    for(i = 0; i < len; i++) {
        if (JS_AddModuleExport(ctx, m, tab[i].name))
            return -1;
    }
    return 0;
}

int JS_SetModuleExportList(JSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len)
{
    int i;
    JSValue val;

    for(i = 0; i < len; i++) {
        const JSCFunctionListEntry *e = &tab[i];
        switch(e->def_type) {
        case JS_DEF_CFUNC:
            val = JS_NewCFunction2(ctx, e->u.func.cfunc.generic,
                                   e->name, e->u.func.length, e->u.func.cproto, e->magic);
            break;
        case JS_DEF_PROP_STRING:
            val = JS_NewString(ctx, e->u.str);
            break;
        case JS_DEF_PROP_INT32:
            val = JS_NewInt32(ctx, e->u.i32);
            break;
        case JS_DEF_PROP_INT64:
            val = JS_NewInt64(ctx, e->u.i64);
            break;
        case JS_DEF_PROP_DOUBLE:
            val = __JS_NewFloat64(ctx, e->u.f64);
            break;
        case JS_DEF_OBJECT:
            val = JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_OBJECT],
                                        e->u.prop_list.tab, e->u.prop_list.len);
            break;
        default:
            abort();
        }
        if (JS_SetModuleExport(ctx, m, e->name, val))
            return -1;
    }
    return 0;
}

/* Note: 'func_obj' is not necessarily a constructor */
int JS_SetConstructor2(JSContext *ctx,
                              JSValueConst func_obj,
                              JSValueConst proto,
                              int proto_flags, int ctor_flags)
{
    if (JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_prototype,
                               JS_DupValue(ctx, proto), proto_flags) < 0)
        return -1;
    if (JS_DefinePropertyValue(ctx, proto, JS_ATOM_constructor,
                               JS_DupValue(ctx, func_obj),
                               ctor_flags) < 0)
        return -1;
    set_cycle_flag(ctx, func_obj);
    set_cycle_flag(ctx, proto);
    return 0;
}

/* return 0 if OK, -1 if exception */
int JS_SetConstructor(JSContext *ctx, JSValueConst func_obj,
                      JSValueConst proto)
{
    return JS_SetConstructor2(ctx, func_obj, proto,
                              0, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
}

/* [JS_NEW_CTOR_* moved to js_builtins.h] */

/* Return the constructor and. Define it as a global variable unless
   JS_NEW_CTOR_NO_GLOBAL is set. The new class inherit from
   parent_ctor if it is not JS_UNDEFINED. if class_id is != -1,
   class_proto[class_id] is set. */
JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                  JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                                  JSValueConst parent_ctor,
                                  const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                  const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                  int flags)
{
    JSValue ctor = JS_UNDEFINED, proto, parent_proto;
    int proto_class_id, proto_flags, ctor_flags;

    proto_flags = 0;
    if (flags & JS_NEW_CTOR_READONLY) {
        ctor_flags = JS_PROP_CONFIGURABLE;
    } else {
        ctor_flags = JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
    }
    
    if (JS_IsUndefined(parent_ctor)) {
        parent_proto = JS_DupValue(ctx, ctx->class_proto[JS_CLASS_OBJECT]);
        parent_ctor = ctx->function_proto;
    } else {
        parent_proto = JS_GetProperty(ctx, parent_ctor, JS_ATOM_prototype);
        if (JS_IsException(parent_proto))
            return JS_EXCEPTION;
    }
    
    if (flags & JS_NEW_CTOR_PROTO_EXIST) {
        proto = JS_DupValue(ctx, ctx->class_proto[class_id]);
    } else {
        if (flags & JS_NEW_CTOR_PROTO_CLASS)
            proto_class_id = class_id;
        else
            proto_class_id = JS_CLASS_OBJECT;
        /* one additional field: constructor */
        proto = JS_NewObjectProtoClassAlloc(ctx, parent_proto, proto_class_id,
                                            n_proto_fields + 1);
        if (JS_IsException(proto))
            goto fail;
        if (class_id >= 0)
            ctx->class_proto[class_id] = JS_DupValue(ctx, proto);
    }
    if (JS_SetPropertyFunctionList(ctx, proto, proto_fields, n_proto_fields))
        goto fail;

    /* additional fields: name, length, prototype */
    ctor = JS_NewCFunction3(ctx, func, name, length, cproto, magic, parent_ctor,
                            n_ctor_fields + 3);
    if (JS_IsException(ctor))
        goto fail;
    if (JS_SetPropertyFunctionList(ctx, ctor, ctor_fields, n_ctor_fields))
        goto fail;
    if (!(flags & JS_NEW_CTOR_NO_GLOBAL)) {
        if (JS_DefinePropertyValueStr(ctx, ctx->global_obj, name,
                                      JS_DupValue(ctx, ctor),
                                      JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0)
            goto fail;
    }
    JS_SetConstructor2(ctx, ctor, proto, proto_flags, ctor_flags);

    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, parent_proto);
    return ctor;
 fail:
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, parent_proto);
    JS_FreeValue(ctx, ctor);
    return JS_EXCEPTION;
}

/* [js_global_eval, js_global_isNaN, js_global_isFinite moved to src/builtins/js_builtin_init.c] */

/* Object class */

JSValue JS_ToObject(JSContext *ctx, JSValueConst val)
{
    int tag = JS_VALUE_GET_NORM_TAG(val);
    JSValue obj;

    switch(tag) {
    default:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        return JS_ThrowTypeError(ctx, "cannot convert to object");
    case JS_TAG_OBJECT:
    case JS_TAG_EXCEPTION:
        return JS_DupValue(ctx, val);
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        obj = JS_NewObjectClass(ctx, JS_CLASS_BIG_INT);
        goto set_value;
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
        obj = JS_NewObjectClass(ctx, JS_CLASS_NUMBER);
        goto set_value;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        /* XXX: should call the string constructor */
        {
            JSValue str;
            str = JS_ToString(ctx, val); /* ensure that we never store a rope */
            if (JS_IsException(str))
                return JS_EXCEPTION;
            obj = JS_NewObjectClass(ctx, JS_CLASS_STRING);
            if (!JS_IsException(obj)) {
                JS_DefinePropertyValue(ctx, obj, JS_ATOM_length,
                                       JS_NewInt32(ctx, JS_VALUE_GET_STRING(str)->len), 0);
                JS_SetObjectData(ctx, obj, JS_DupValue(ctx, str));
            }
            JS_FreeValue(ctx, str);
            return obj;
        }
    case JS_TAG_BOOL:
        obj = JS_NewObjectClass(ctx, JS_CLASS_BOOLEAN);
        goto set_value;
    case JS_TAG_SYMBOL:
        obj = JS_NewObjectClass(ctx, JS_CLASS_SYMBOL);
    set_value:
        if (!JS_IsException(obj))
            JS_SetObjectData(ctx, obj, JS_DupValue(ctx, val));
        return obj;
    }
}

/* [JS_ToObjectFree moved to quickjs-internal.h] */

/* [Object, Function, Error builtins moved to src/builtins/js_builtin_core.c] */

/* [Array builtin moved to src/builtins/js_builtin_array.c] */


/* [Number builtin moved to src/builtins/js_builtin_number.c] */

/* [Boolean builtin moved to src/builtins/js_builtin_core.c] */

/* [String builtin moved to src/builtins/js_builtin_string.c] */


/* [Math builtin moved to src/builtins/js_builtin_number.c] */

/* [getTimezoneOffset moved to src/builtins/js_builtin_date.c] */

#if 0
static JSValue js___date_getTimezoneOffset(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    double dd;

    if (JS_ToFloat64(ctx, &dd, argv[0]))
        return JS_EXCEPTION;
    if (isnan(dd))
        return __JS_NewFloat64(ctx, dd);
    else
        return JS_NewInt32(ctx, getTimezoneOffset((int64_t)dd));
}

static JSValue js_get_prototype_from_ctor(JSContext *ctx, JSValueConst ctor,
                                          JSValueConst def_proto)
{
    JSValue proto;
    proto = JS_GetProperty(ctx, ctor, JS_ATOM_prototype);
    if (JS_IsException(proto))
        return proto;
    if (!JS_IsObject(proto)) {
        JS_FreeValue(ctx, proto);
        proto = JS_DupValue(ctx, def_proto);
    }
    return proto;
}

/* create a new date object */
static JSValue js___date_create(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue obj, proto;
    proto = js_get_prototype_from_ctor(ctx, argv[0], argv[1]);
    if (JS_IsException(proto))
        return proto;
    obj = JS_NewObjectProtoClass(ctx, proto, JS_CLASS_DATE);
    JS_FreeValue(ctx, proto);
    if (!JS_IsException(obj))
        JS_SetObjectData(ctx, obj, JS_DupValue(ctx, argv[2]));
    return obj;
}
#endif

/* [RegExp builtin moved to src/builtins/js_builtin_string.c] */

/* [JSON builtin moved to src/builtins/js_builtin_json.c] */

/* [Reflect and Proxy builtins moved to src/builtins/js_builtin_proxy.c] */

/* [Symbol builtin moved to src/builtins/js_builtin_string.c] */


/* [Map, Set, WeakMap, WeakSet builtins moved to src/builtins/js_builtin_collections.c] */

/* [Generator, URI, Global funcs, BasicObjects, BaseObjects, Eval moved to src/builtins/js_builtin_init.c] */

/* [TypedArray, ArrayBuffer, SharedArrayBuffer, DataView & Atomics builtins moved to src/builtins/js_builtin_typedarray.c] */

/* [WeakRef and FinalizationRegistry builtins moved to src/builtins/js_builtin_collections.c] */
