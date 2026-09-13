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
#include "src/quickjs/internal-frontend.h"
#include "src/quickjs/internal-regexp.h"
#include "src/quickjs/internal-builtin.h"
#include "src/quickjs/internal-global.h"
#include "src/quickjs/internal-date.h"
#include "src/quickjs/internal-proxy.h"
#include "src/quickjs/internal-collection.h"
#include "src/quickjs/internal-array.h"
#include "src/quickjs/internal-typed-array.h"
#include "src/quickjs/internal-async.h"
#include "src/quickjs/internal-base.h"
#include "src/quickjs/internal-operator.h"

#define check_function qjs_check_function
#define check_exception_free qjs_check_exception_free
#define JS_NewObjectProtoList qjs_new_object_proto_list
#define JS_InstantiateFunctionListItem2 qjs_instantiate_function_list_item
#define JS_SetConstructor2 qjs_set_constructor2
#define JS_NewCConstructor qjs_new_c_constructor
#define js_map_finalizer qjs_map_finalizer
#define js_map_mark qjs_map_mark
#define js_map_iterator_finalizer qjs_map_iterator_finalizer
#define js_map_iterator_mark qjs_map_iterator_mark
#define map_delete_weakrefs qjs_map_delete_weakrefs
#define weakref_delete_weakref qjs_weakref_delete
#define finrec_delete_weakref qjs_finrec_delete
#define js_object_groupBy qjs_object_group_by
#define can_extend_fast_array qjs_can_extend_fast_array
#define js_array_finalizer qjs_array_finalizer
#define js_array_mark qjs_array_mark
#define js_array_iterator_finalizer qjs_array_iterator_finalizer
#define js_array_iterator_mark qjs_array_iterator_mark
#define js_iterator_concat_finalizer qjs_iterator_concat_finalizer
#define js_iterator_concat_mark qjs_iterator_concat_mark
#define js_iterator_helper_finalizer qjs_iterator_helper_finalizer
#define js_iterator_helper_mark qjs_iterator_helper_mark
#define js_iterator_wrap_finalizer qjs_iterator_wrap_finalizer
#define js_iterator_wrap_mark qjs_iterator_wrap_mark
#define js_array_buffer_finalizer qjs_array_buffer_finalizer
#define js_typed_array_finalizer qjs_typed_array_finalizer
#define js_typed_array_mark qjs_typed_array_mark
#define array_buffer_is_resizable qjs_array_buffer_is_resizable

#define JS_AddBrand qjs_add_brand
#define JS_AtomIsArrayIndex qjs_atom_is_array_index
#define JS_AutoInitProperty qjs_auto_init_property
#define JS_CheckBrand qjs_check_brand
#define JS_CheckDefineGlobalVar qjs_check_define_global_var
#define JS_ConcatString qjs_concat_string
#define JS_ConcatStringInPlace qjs_concat_string_in_place
#define JS_DefineAutoInitProperty qjs_define_auto_init_property
#define JS_DefineObjectName qjs_define_object_name
#define JS_DefineObjectNameComputed qjs_define_object_name_computed
#define JS_DefinePrivateField qjs_define_private_field
#define JS_DefinePropertyValueValue qjs_define_property_value_value
#define JS_DeleteGlobalVar qjs_delete_global_var
#define JS_GetGlobalVarRef qjs_get_global_var_ref
#define JS_GetOwnPropertyInternal qjs_get_own_property_internal
#define JS_GetOwnPropertyNamesInternal qjs_get_own_property_names_internal
#define JS_GetPrivateField qjs_get_private_field
#define JS_GetPropertyValue qjs_get_property_value
#define JS_GetPrototypeFree qjs_get_prototype_free
#define JS_IsCFunction qjs_is_c_function
#define JS_NewObjectFromShape qjs_new_object_from_shape
#define JS_NewSymbolFromAtom qjs_new_symbol_from_atom
#define JS_SetPrivateField qjs_set_private_field
#define JS_SetPropertyValue qjs_set_property_value
#define JS_SetPrototypeInternal qjs_set_prototype_internal
#define JS_ThrowReferenceErrorNotDefined qjs_throw_reference_error_not_defined
#define JS_ThrowReferenceErrorUninitialized qjs_throw_reference_error_uninitialized
#define JS_ThrowReferenceErrorUninitialized2 qjs_throw_reference_error_uninitialized2
#define JS_ThrowStackOverflow qjs_throw_stack_overflow
#define JS_ThrowSyntaxErrorVarRedeclaration qjs_throw_syntax_error_var_redeclaration
#define JS_ThrowTypeErrorNotAConstructor qjs_throw_type_error_not_constructor
#define JS_ThrowTypeErrorNotAnObject qjs_throw_type_error_not_object
#define JS_ThrowTypeErrorReadOnly qjs_throw_type_error_read_only
#define JS_ToBoolFree qjs_to_bool_free
#define JS_ToObject qjs_to_object
#define JS_ToObjectFree qjs_to_object_free
#define __JS_AtomFromUInt32 qjs_atom_from_uint32
#define add_gc_object qjs_add_gc_object
#define add_property qjs_add_property
#define build_backtrace qjs_build_backtrace
#define convert_fast_array_to_array qjs_convert_fast_array_to_array
#define delete_property qjs_delete_property
#define find_own_property qjs_find_own_property_fast
#define free_property qjs_free_property
#define free_var_ref qjs_free_var_ref
#define free_zero_refcount qjs_free_zero_refcount
#define get_shape_prop qjs_get_shape_prop
#define is_backtrace_needed qjs_is_backtrace_needed
#define is_strict_mode qjs_is_strict_mode
#define js_add_slow qjs_add_slow
#define js_binary_arith_slow qjs_binary_arith_slow
#define js_binary_logic_slow qjs_binary_logic_slow
#define js_check_stack_overflow qjs_check_stack_overflow
#define js_create_array qjs_create_array
#define js_create_array_free qjs_create_array_free
#define js_dup_shape qjs_dup_shape
#define js_eq_slow qjs_eq_slow
#define js_free_desc qjs_proxy_free_desc
#define js_function_set_properties qjs_function_set_properties
#define js_has_unscopable qjs_has_unscopable
#define js_method_set_home_object qjs_method_set_home_object
#define js_method_set_properties qjs_method_set_properties
#define js_not_slow qjs_not_slow
#define js_operator_delete qjs_operator_delete
#define js_operator_in qjs_operator_in
#define js_operator_instanceof qjs_operator_instanceof
#define js_operator_private_in qjs_operator_private_in
#define js_operator_typeof qjs_operator_typeof
#define js_poll_interrupts qjs_poll_interrupts
#define js_post_inc_slow qjs_post_inc_slow
#define js_rc qjs_get_ref_header
#define js_relational_slow qjs_relational_slow
#define js_same_value qjs_same_value
#define js_shr_slow qjs_shr_slow
#define js_strict_eq2 qjs_strict_equal
#define js_string_eq(ctx, left, right) qjs_string_equal((left), (right))
#define js_unary_arith_slow qjs_unary_arith_slow
#define remove_gc_object qjs_remove_gc_object
#define set_cycle_flag qjs_set_cycle_flag
#define set_value qjs_set_value

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)
#define JS_EQ_STRICT QJS_EQ_STRICT

static JSValue __attribute__((hot, aligned(128)))
JS_CallInternal(JSContext *ctx, JSValueConst func_obj,
                JSValueConst this_obj, JSValueConst new_target,
                int argc, JSValue *argv, int flags);
static JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags);
static JSValue JS_CallFree(JSContext *ctx, JSValue func_obj,
                           JSValueConst this_obj, int argc,
                           JSValueConst *argv);
static JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
static JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
static JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *frame,
                             int var_idx, BOOL is_arg);
static void async_func_free(JSRuntime *rt, JSAsyncFunctionState *state);
static JSValue js_generator_function_call(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int flags);

QJS_INTERNAL void qjs_free_var_ref(JSRuntime *rt, JSVarRef *var_ref)
{
    if (var_ref) {
        assert(js_rc(var_ref)->ref_count > 0);
        if (--js_rc(var_ref)->ref_count == 0) {
            if (var_ref->is_detached) {
                JS_FreeValueRT(rt, var_ref->value);
            } else {
                JSStackFrame *frame = var_ref->stack_frame;
                assert(frame->var_refs[var_ref->var_ref_idx] == var_ref);
                frame->var_refs[var_ref->var_ref_idx] = NULL;
                if (frame->js_mode & JS_MODE_ASYNC) {
                    JSAsyncFunctionState *state =
                        container_of(frame, JSAsyncFunctionState, frame);
                    async_func_free(rt, state);
                }
            }
            remove_gc_object(&var_ref->header);
            js_free_rt(rt, var_ref);
        }
    }
}
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

static const JSClassExoticMethods js_arguments_exotic_methods = {
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

static void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSVarRef **var_refs = p->u.array.u.var_refs;
    int i;
    for(i = 0; i < p->u.array.count; i++)
        free_var_ref(rt, var_refs[i]);
    js_free_rt(rt, var_refs);
}

static void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val,
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

static JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
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

static JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async)
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
            ret = qjs_async_create_from_sync_iterator(ctx, sync_iter);
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
static JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
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
static JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
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
static int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
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

static JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
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

static JSValue js_create_iterator_result(JSContext *ctx,
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

/* Access an Array's internal JSValue array if available */
static BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp)
{
    /* Try and handle fast arrays explicitly */
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(obj);
        if (p->class_id == JS_CLASS_ARRAY && p->fast_array) {
            *countp = p->u.array.count;
            *arrpp = p->u.array.u.values;
            return TRUE;
        }
    }
    return FALSE;
}

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
    ft.generic_magic = qjs_create_array_iterator;
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

    ft.iterator_next = qjs_array_iterator_next;
    if (is_array_iterator
    &&  JS_IsCFunction(ctx, method, ft.generic, 0)
    &&  js_get_fast_array(ctx, sp[-1], &arrp, &count32)) {
        uint32_t len;
        if (qjs_get_length32(ctx, &len, sp[-1]))
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

static __exception int JS_CopyDataProperties(JSContext *ctx,
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

/* only valid inside C functions */
QJS_INTERNAL JSValueConst qjs_get_active_function(JSContext *ctx)
{
    return ctx->rt->current_stack_frame->cur_func;
}

static JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical)
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

static void js_global_object_finalizer(JSRuntime *rt, JSValue obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    JS_FreeValueRT(rt, p->u.global_object.uninitialized_vars);
}

static void js_global_object_mark(JSRuntime *rt, JSValueConst val,
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
static JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
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

QJS_INTERNAL JSValue qjs_instantiate_prototype(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque)
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

static const uint16_t func_kind_to_class_id[] = {
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

static JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
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

static JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
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

#define FUNC_RET_AWAIT         0
#define FUNC_RET_YIELD         1
#define FUNC_RET_YIELD_STAR    2
#define FUNC_RET_INITIAL_YIELD 3

#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
#endif

/* argv[] is modified if (flags & JS_CALL_FLAG_COPY_ARGV) = 0. */
static JSValue __attribute__((hot, aligned(128)))
JS_CallInternal(JSContext *caller_ctx, JSValueConst func_obj,
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
                    *sp++ = qjs_import_meta(ctx);
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

                ret_val = qjs_base_function_apply(ctx, sp[-3], 2, (JSValueConst *)&sp[-2], magic);
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
                    ret_val = qjs_eval_object(ctx, JS_UNDEFINED, obj,
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
                tab = qjs_base_build_arg_list(ctx, &len, sp[-1]);
                if (!tab)
                    goto exception;
                if (js_same_value(ctx, sp[-2], ctx->eval_obj)) {
                    if (len >= 1)
                        obj = tab[0];
                    else
                        obj = JS_UNDEFINED;
                    ret_val = qjs_eval_object(ctx, JS_UNDEFINED, obj,
                                            JS_EVAL_TYPE_DIRECT, scope_idx);
                } else {
                    ret_val = JS_Call(ctx, sp[-2], JS_UNDEFINED, len,
                                      (JSValueConst *)tab);
                }
                qjs_base_free_arg_list(ctx, tab, len);
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
                sp[-2] = qjs_new_regexp(ctx, sp[-2], sp[-1]);
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
                val = qjs_dynamic_import(ctx, sp[-2], sp[-1]);
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

static JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv)
{
    JSValue res = JS_CallInternal(ctx, func_obj, this_obj, JS_UNDEFINED,
                                  argc, (JSValue *)argv, JS_CALL_FLAG_COPY_ARGV);
    JS_FreeValue(ctx, func_obj);
    return res;
}

/* warning: the refcount of the context is not incremented. Return
   NULL in case of exception (case of revoked proxy only) */
static JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj)
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
                qjs_proxy_throw_revoked(ctx);
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

static JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
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

static JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
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

QJS_INTERNAL void qjs_async_function_gc_free(JSRuntime *rt,
                                             JSAsyncFunctionState *s)
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

static void js_generator_finalizer(JSRuntime *rt, JSValue obj)
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

static void js_generator_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSGeneratorData *s = p->u.generator_data;

    if (!s || !s->func_state)
        return;
    mark_func(rt, &s->func_state->header);
}

/* XXX: use enum */
#define GEN_MAGIC_NEXT   QJS_GEN_MAGIC_NEXT
#define GEN_MAGIC_RETURN QJS_GEN_MAGIC_RETURN
#define GEN_MAGIC_THROW  QJS_GEN_MAGIC_THROW

QJS_INTERNAL JSValue qjs_generator_next(JSContext *ctx,
                                        JSValueConst this_val,
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

static JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
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

QJS_INTERNAL void qjs_async_resolve_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSAsyncFunctionState *s = p->u.async_function_data;
    if (s) {
        async_func_free(rt, s);
    }
}

QJS_INTERNAL void qjs_async_resolve_mark(JSRuntime *rt, JSValueConst val,
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
        promise = qjs_async_promise_resolve(ctx, ctx->promise_ctor,
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
        res = qjs_async_perform_promise_then(ctx, promise,
                                   (JSValueConst *)resolving_funcs,
                                   (JSValueConst *)resolving_funcs1);
        JS_FreeValue(ctx, promise);
        for(i = 0; i < 2; i++)
            JS_FreeValue(ctx, resolving_funcs[i]);
        if (res)
            goto fail;
    }
}

QJS_INTERNAL JSValue qjs_async_resolve_call(
    JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
    int argc, JSValueConst *argv, int flags)
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

QJS_INTERNAL JSValue qjs_async_function_call(
    JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
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

QJS_INTERNAL void qjs_async_generator_finalizer(JSRuntime *rt, JSValue obj)
{
    JSAsyncGeneratorData *s = JS_GetOpaque(obj, JS_CLASS_ASYNC_GENERATOR);

    if (s) {
        js_async_generator_free(rt, s);
    }
}

QJS_INTERNAL void qjs_async_generator_mark(JSRuntime *rt, JSValueConst val,
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

    promise = qjs_async_promise_resolve(ctx, ctx->promise_ctor,
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
    res = qjs_async_perform_promise_then(ctx, promise,
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
    promise = qjs_async_promise_resolve(ctx, ctx->promise_ctor, 1, &value,
                                 /*is_reject*/0);
    // A poisoned .constructor property is observable and the resulting
    // exception should be delivered to the catch handler.
    if (JS_IsException(promise)) {
        JSValue err = JS_GetException(ctx);
        promise = qjs_async_promise_resolve(ctx, ctx->promise_ctor, 1, (JSValueConst *)&err,
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
    res = qjs_async_perform_promise_then(ctx, promise,
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
QJS_INTERNAL JSValue qjs_async_generator_next(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
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

QJS_INTERNAL JSValue qjs_async_generator_function_call(
    JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
    int argc, JSValueConst *argv, int flags)
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

QJS_INTERNAL void qjs_function_vm_init_runtime(JSRuntime *rt)
{
    JSClass *mapped_arguments = &rt->class_array[JS_CLASS_MAPPED_ARGUMENTS];
    JSClass *generator = &rt->class_array[JS_CLASS_GENERATOR];
    JSClass *global_object = &rt->class_array[JS_CLASS_GLOBAL_OBJECT];

    rt->class_array[JS_CLASS_ARGUMENTS].exotic = &js_arguments_exotic_methods;
    mapped_arguments->finalizer = js_mapped_arguments_finalizer;
    mapped_arguments->gc_mark = js_mapped_arguments_mark;
    mapped_arguments->exotic = &js_arguments_exotic_methods;
    generator->finalizer = js_generator_finalizer;
    generator->gc_mark = js_generator_mark;
    global_object->finalizer = js_global_object_finalizer;
    global_object->gc_mark = js_global_object_mark;
    rt->class_array[JS_CLASS_C_FUNCTION].call = js_call_c_function;
    rt->class_array[JS_CLASS_BOUND_FUNCTION].call = js_call_bound_function;
    rt->class_array[JS_CLASS_GENERATOR_FUNCTION].call =
        js_generator_function_call;
}

QJS_INTERNAL JSValue qjs_invoke_free(JSContext *ctx, JSValue value,
                                     JSAtom atom, int argc,
                                     JSValueConst *argv)
{
    return JS_InvokeFree(ctx, value, atom, argc, argv);
}

QJS_INTERNAL JSVarRef *qjs_create_var_ref(JSContext *ctx, BOOL is_lexical)
{
    return js_create_var_ref(ctx, is_lexical);
}

QJS_INTERNAL JSValue qjs_closure2(JSContext *ctx, JSValue func_obj,
                                  JSFunctionBytecode *bytecode,
                                  JSVarRef **cur_var_refs,
                                  JSStackFrame *stack_frame, BOOL is_eval,
                                  JSModuleDef *module)
{
    return js_closure2(ctx, func_obj, bytecode, cur_var_refs, stack_frame,
                       is_eval, module);
}

QJS_INTERNAL JSVarRef *qjs_get_var_ref(JSContext *ctx, JSStackFrame *frame,
                                       int var_idx, BOOL is_arg)
{
    return get_var_ref(ctx, frame, var_idx, is_arg);
}

QJS_INTERNAL JSValue qjs_closure(JSContext *ctx, JSValue bytecode_func,
                                 JSVarRef **cur_var_refs,
                                 JSStackFrame *stack_frame, BOOL is_eval)
{
    return js_closure(ctx, bytecode_func, cur_var_refs, stack_frame, is_eval);
}

QJS_INTERNAL JSValue qjs_call_free(JSContext *ctx, JSValue func_obj,
                                   JSValueConst this_obj, int argc,
                                   JSValueConst *argv)
{
    return JS_CallFree(ctx, func_obj, this_obj, argc, argv);
}

QJS_INTERNAL JSVarRef *qjs_global_object_find_uninitialized_var(
    JSContext *ctx, JSObject *obj, JSAtom atom, BOOL is_lexical)
{
    return js_global_object_find_uninitialized_var(ctx, obj, atom,
                                                    is_lexical);
}

QJS_INTERNAL JSValue qjs_get_iterator2(JSContext *ctx, JSValueConst obj,
                                       JSValueConst method)
{
    return JS_GetIterator2(ctx, obj, method);
}

QJS_INTERNAL JSValue qjs_get_iterator(JSContext *ctx, JSValueConst obj,
                                      BOOL is_async)
{
    return JS_GetIterator(ctx, obj, is_async);
}

QJS_INTERNAL JSValue qjs_iterator_next2(JSContext *ctx,
                                        JSValueConst iterator,
                                        JSValueConst method, int argc,
                                        JSValueConst *argv, int *done)
{
    return JS_IteratorNext2(ctx, iterator, method, argc, argv, done);
}

QJS_INTERNAL JSValue qjs_iterator_next(JSContext *ctx,
                                       JSValueConst iterator,
                                       JSValueConst method, int argc,
                                       JSValueConst *argv, BOOL *done)
{
    return JS_IteratorNext(ctx, iterator, method, argc, argv, done);
}

QJS_INTERNAL int qjs_iterator_close(JSContext *ctx, JSValueConst iterator,
                                    BOOL is_exception_pending)
{
    return JS_IteratorClose(ctx, iterator, is_exception_pending);
}

QJS_INTERNAL JSValue qjs_iterator_get_complete_value(JSContext *ctx,
                                                     JSValueConst obj,
                                                     BOOL *done)
{
    return JS_IteratorGetCompleteValue(ctx, obj, done);
}

QJS_INTERNAL JSValue qjs_create_iterator_result(JSContext *ctx,
                                                JSValue value, BOOL done)
{
    return js_create_iterator_result(ctx, value, done);
}

QJS_INTERNAL BOOL qjs_get_fast_array(JSContext *ctx, JSValueConst obj,
                                     JSValue **values, uint32_t *count)
{
    return js_get_fast_array(ctx, obj, values, count);
}

QJS_INTERNAL int qjs_copy_data_properties(JSContext *ctx,
                                          JSValueConst target,
                                          JSValueConst source,
                                          JSValueConst excluded,
                                          BOOL set_property)
{
    return JS_CopyDataProperties(ctx, target, source, excluded, set_property);
}

QJS_INTERNAL JSContext *qjs_get_function_realm(JSContext *ctx,
                                               JSValueConst func_obj)
{
    return JS_GetFunctionRealm(ctx, func_obj);
}

QJS_INTERNAL JSValue qjs_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                          JSClassID class_id)
{
    return js_create_from_ctor(ctx, ctor, class_id);
}

QJS_INTERNAL JSClassID qjs_function_class_id(int function_kind)
{
    return func_kind_to_class_id[function_kind];
}
