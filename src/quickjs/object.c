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
#include "internal-canonical.h"
#include "internal-allocator.h"
#include "internal-frontend.h"
#include "internal-regexp.h"
#include "internal-builtin.h"
#include "internal-global.h"
#include "internal-date.h"
#include "internal-proxy.h"
#include "internal-collection.h"
#include "internal-array.h"
#include "internal-typed-array.h"
#include "internal-async.h"
#include "internal-base.h"
#include "internal-operator.h"

JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
QJS_INTERNAL __maybe_unused void JS_DumpObjectHeader(JSRuntime *rt);
static __maybe_unused void JS_DumpObject(JSRuntime *rt, JSObject *p);
QJS_INTERNAL __maybe_unused void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p);
static __maybe_unused void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom);
static __maybe_unused void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val);
QJS_INTERNAL __maybe_unused void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val);
QJS_INTERNAL __maybe_unused void JS_DumpShapes(JSRuntime *rt);
QJS_INTERNAL void js_dump_value_write(void *opaque, const char *buf, size_t len);
static void js_object_data_finalizer(JSRuntime *rt, JSValue val);
static void js_object_data_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL void js_c_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func);
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

QJS_INTERNAL JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
QJS_INTERNAL int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags);
QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);
QJS_INTERNAL int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
QJS_INTERNAL void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
QJS_INTERNAL void js_free_shape(JSRuntime *rt, JSShape *sh);
QJS_INTERNAL void js_free_shape_null(JSRuntime *rt, JSShape *sh);
QJS_INTERNAL int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs);
QJS_INTERNAL int init_shape_hash(JSRuntime *rt);
QJS_INTERNAL void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);
QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h);
QJS_INTERNAL void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
static void js_trigger_gc(JSRuntime *rt, size_t size)
{
    BOOL force_gc;
#ifdef FORCE_GC_AT_MALLOC
    force_gc = TRUE;
#else
    force_gc = ((rt->malloc_ctx.malloc_state.malloc_size + size) >
                rt->malloc_gc_threshold);
#endif
    if (force_gc) {
#ifdef DUMP_GC
        printf("GC: size=%" PRIu64 "\n",
               (uint64_t)rt->malloc_ctx.malloc_state.malloc_size);
#endif
        JS_RunGC(rt);
        rt->malloc_gc_threshold = rt->malloc_ctx.malloc_state.malloc_size +
            (rt->malloc_ctx.malloc_state.malloc_size >> 1);
    }
}

QJS_INTERNAL JSClassShortDef const js_std_class_def[] = {
    { JS_ATOM_Object, NULL, NULL },                             /* JS_CLASS_OBJECT */
    { JS_ATOM_Array, js_array_finalizer, js_array_mark },       /* JS_CLASS_ARRAY */
    { JS_ATOM_Error, NULL, NULL }, /* JS_CLASS_ERROR */
    { JS_ATOM_Number, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_NUMBER */
    { JS_ATOM_String, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_STRING */
    { JS_ATOM_Boolean, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_BOOLEAN */
    { JS_ATOM_Symbol, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_SYMBOL */
    { JS_ATOM_Arguments, js_array_finalizer, js_array_mark },   /* JS_CLASS_ARGUMENTS */
    { JS_ATOM_Arguments, js_mapped_arguments_finalizer, js_mapped_arguments_mark }, /* JS_CLASS_MAPPED_ARGUMENTS */
    { JS_ATOM_Date, js_object_data_finalizer, js_object_data_mark }, /* JS_CLASS_DATE */
    { JS_ATOM_Object, NULL, NULL },                             /* JS_CLASS_MODULE_NS */
    { JS_ATOM_Function, js_c_function_finalizer, js_c_function_mark }, /* JS_CLASS_C_FUNCTION */
    { JS_ATOM_Function, js_bytecode_function_finalizer, js_bytecode_function_mark }, /* JS_CLASS_BYTECODE_FUNCTION */
    { JS_ATOM_Function, js_bound_function_finalizer, js_bound_function_mark }, /* JS_CLASS_BOUND_FUNCTION */
    { JS_ATOM_Function, js_c_function_data_finalizer, js_c_function_data_mark }, /* JS_CLASS_C_FUNCTION_DATA */
    { JS_ATOM_GeneratorFunction, js_bytecode_function_finalizer, js_bytecode_function_mark },  /* JS_CLASS_GENERATOR_FUNCTION */
    { JS_ATOM_ForInIterator, js_for_in_iterator_finalizer, js_for_in_iterator_mark },      /* JS_CLASS_FOR_IN_ITERATOR */
    { JS_ATOM_RegExp, js_regexp_finalizer, NULL },                              /* JS_CLASS_REGEXP */
    { JS_ATOM_ArrayBuffer, js_array_buffer_finalizer, NULL },                   /* JS_CLASS_ARRAY_BUFFER */
    { JS_ATOM_SharedArrayBuffer, js_array_buffer_finalizer, NULL },             /* JS_CLASS_SHARED_ARRAY_BUFFER */
    { JS_ATOM_Uint8ClampedArray, js_typed_array_finalizer, js_typed_array_mark }, /* JS_CLASS_UINT8C_ARRAY */
    { JS_ATOM_Int8Array, js_typed_array_finalizer, js_typed_array_mark },       /* JS_CLASS_INT8_ARRAY */
    { JS_ATOM_Uint8Array, js_typed_array_finalizer, js_typed_array_mark },      /* JS_CLASS_UINT8_ARRAY */
    { JS_ATOM_Int16Array, js_typed_array_finalizer, js_typed_array_mark },      /* JS_CLASS_INT16_ARRAY */
    { JS_ATOM_Uint16Array, js_typed_array_finalizer, js_typed_array_mark },     /* JS_CLASS_UINT16_ARRAY */
    { JS_ATOM_Int32Array, js_typed_array_finalizer, js_typed_array_mark },      /* JS_CLASS_INT32_ARRAY */
    { JS_ATOM_Uint32Array, js_typed_array_finalizer, js_typed_array_mark },     /* JS_CLASS_UINT32_ARRAY */
    { JS_ATOM_BigInt64Array, js_typed_array_finalizer, js_typed_array_mark },   /* JS_CLASS_BIG_INT64_ARRAY */
    { JS_ATOM_BigUint64Array, js_typed_array_finalizer, js_typed_array_mark },  /* JS_CLASS_BIG_UINT64_ARRAY */
    { JS_ATOM_Float16Array, js_typed_array_finalizer, js_typed_array_mark },    /* JS_CLASS_FLOAT16_ARRAY */
    { JS_ATOM_Float32Array, js_typed_array_finalizer, js_typed_array_mark },    /* JS_CLASS_FLOAT32_ARRAY */
    { JS_ATOM_Float64Array, js_typed_array_finalizer, js_typed_array_mark },    /* JS_CLASS_FLOAT64_ARRAY */
    { JS_ATOM_DataView, js_typed_array_finalizer, js_typed_array_mark },        /* JS_CLASS_DATAVIEW */
    { JS_ATOM_BigInt, js_object_data_finalizer, js_object_data_mark },      /* JS_CLASS_BIG_INT */
    { JS_ATOM_Map, js_map_finalizer, js_map_mark },             /* JS_CLASS_MAP */
    { JS_ATOM_Set, js_map_finalizer, js_map_mark },             /* JS_CLASS_SET */
    { JS_ATOM_WeakMap, js_map_finalizer, js_map_mark },         /* JS_CLASS_WEAKMAP */
    { JS_ATOM_WeakSet, js_map_finalizer, js_map_mark },         /* JS_CLASS_WEAKSET */
    { JS_ATOM_Iterator, NULL, NULL },                           /* JS_CLASS_ITERATOR */
    { JS_ATOM_IteratorConcat, js_iterator_concat_finalizer, js_iterator_concat_mark }, /* JS_CLASS_ITERATOR_CONCAT */
    { JS_ATOM_IteratorHelper, js_iterator_helper_finalizer, js_iterator_helper_mark }, /* JS_CLASS_ITERATOR_HELPER */
    { JS_ATOM_IteratorWrap, js_iterator_wrap_finalizer, js_iterator_wrap_mark }, /* JS_CLASS_ITERATOR_WRAP */
    { JS_ATOM_Map_Iterator, js_map_iterator_finalizer, js_map_iterator_mark }, /* JS_CLASS_MAP_ITERATOR */
    { JS_ATOM_Set_Iterator, js_map_iterator_finalizer, js_map_iterator_mark }, /* JS_CLASS_SET_ITERATOR */
    { JS_ATOM_Array_Iterator, js_array_iterator_finalizer, js_array_iterator_mark }, /* JS_CLASS_ARRAY_ITERATOR */
    { JS_ATOM_String_Iterator, js_array_iterator_finalizer, js_array_iterator_mark }, /* JS_CLASS_STRING_ITERATOR */
    { JS_ATOM_RegExp_String_Iterator, js_regexp_string_iterator_finalizer, js_regexp_string_iterator_mark }, /* JS_CLASS_REGEXP_STRING_ITERATOR */
    { JS_ATOM_Generator, js_generator_finalizer, js_generator_mark }, /* JS_CLASS_GENERATOR */
    { JS_ATOM_Object, js_global_object_finalizer, js_global_object_mark }, /* JS_CLASS_GLOBAL_OBJECT */
    { JS_ATOM_Object, NULL, NULL }, /* JS_CLASS_RAWJSON */
};

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

static void js_object_data_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_FreeValueRT(rt, p->u.object_data);
    p->u.object_data = JS_UNDEFINED;
}

static void js_object_data_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_MarkValue(rt, p->u.object_data, mark_func);
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
static const char *get_prop_string(JSContext *ctx, JSValueConst obj, JSAtom prop)
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

QJS_INTERNAL __exception int js_poll_interrupts(JSContext *ctx)
{
    if (unlikely(--ctx->interrupt_counter <= 0)) {
        return __js_poll_interrupts(ctx);
    } else {
        return 0;
    }
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

BOOL JS_IsError(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    return (p->class_id == JS_CLASS_ERROR);
}

/* must be called after JS_Throw() */
void JS_SetUncatchableException(JSContext *ctx, BOOL flag)
{
    ctx->rt->current_exception_is_uncatchable = flag;
}

void JS_SetOpaque(JSValue obj, void *opaque)
{
   JSObject *p;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        p->u.opaque = opaque;
    }
}

/* return NULL if not an object of class class_id */
void *JS_GetOpaque(JSValueConst obj, JSClassID class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != class_id)
        return NULL;
    return p->u.opaque;
}

void *JS_GetOpaque2(JSContext *ctx, JSValueConst obj, JSClassID class_id)
{
    void *p = JS_GetOpaque(obj, class_id);
    if (unlikely(!p)) {
        JS_ThrowTypeErrorInvalidClass(ctx, class_id);
    }
    return p;
}

void *JS_GetAnyOpaque(JSValueConst obj, JSClassID *class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        *class_id = 0;
        return NULL;
    }
    p = JS_VALUE_GET_OBJ(obj);
    *class_id = p->class_id;
    return p->u.opaque;
}

#define JS_PRINT_MAX_DEPTH 8

typedef struct {
    JSRuntime *rt;
    JSContext *ctx; /* may be NULL */
    JSPrintValueOptions options;
    JSPrintValueWrite *write_func;
    void *write_opaque;
    int level;
    JSObject *print_stack[JS_PRINT_MAX_DEPTH]; /* level values */
} JSPrintValueState;

static void js_print_value(JSPrintValueState *s, JSValueConst val);

static void js_putc(JSPrintValueState *s, char c)
{
    s->write_func(s->write_opaque, &c, 1);
}

static void js_puts(JSPrintValueState *s, const char *str)
{
    s->write_func(s->write_opaque, str, strlen(str));
}

static void __attribute__((format(printf, 2, 3))) js_printf(JSPrintValueState *s, const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    s->write_func(s->write_opaque, buf, strlen(buf));
}

static void js_print_float64(JSPrintValueState *s, double d)
{
    JSDTOATempMem dtoa_mem;
    char buf[32];
    int len;
    len = js_dtoa(buf, d, 10, 0, JS_DTOA_FORMAT_FREE | JS_DTOA_MINUS_ZERO, &dtoa_mem);
    s->write_func(s->write_opaque, buf, len);
}

static uint32_t js_string_get_length(JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        return p->len;
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        return r->len;
    } else {
        return 0;
    }
}

/* pretty print the first 'len' characters of 'p' */
static void js_print_string1(JSPrintValueState *s, JSString *p, int len, int sep)
{
    uint8_t buf[UTF8_CHAR_LEN_MAX];
    int l, i, c, c1;

    for(i = 0; i < len; i++) {
        c = string_get(p, i);
        switch(c) {
        case '\t':
            c = 't';
            goto quote;
        case '\r':
            c = 'r';
            goto quote;
        case '\n':
            c = 'n';
            goto quote;
        case '\b':
            c = 'b';
            goto quote;
        case '\f':
            c = 'f';
            goto quote;
        case '\\':
        quote:
            js_putc(s, '\\');
            js_putc(s, c);
            break;
        default:
            if (c == sep)
                goto quote;
            if (c >= 32 && c <= 126) {
                js_putc(s, c);
            } else if (c < 32 || 
                       (c >= 0x7f && c <= 0x9f)) {
            escape:
                js_printf(s, "\\u%04x", c);
            } else {
                if (is_hi_surrogate(c)) {
                    if ((i + 1) >= len)
                        goto escape;
                    c1 = string_get(p, i + 1);
                    if (!is_lo_surrogate(c1))
                        goto escape;
                    i++;
                    c = from_surrogate(c, c1);
                } else if (is_lo_surrogate(c)) {
                    goto escape;
                }
                l = unicode_to_utf8(buf, c);
                s->write_func(s->write_opaque, (char *)buf, l);
            }
            break;
        }
    }
}

static void js_print_string_rec(JSPrintValueState *s, JSValueConst val,
                                int sep, uint32_t pos)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        uint32_t len;
        if (pos < s->options.max_string_length) {
            len = min_uint32(p->len, s->options.max_string_length - pos);
            js_print_string1(s, p, len, sep);
        }
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        js_print_string_rec(s, r->left, sep, pos);
        js_print_string_rec(s, r->right, sep, pos + js_string_get_length(r->left));
    } else {
        js_printf(s, "<invalid string tag %d>", (int)JS_VALUE_GET_TAG(val));
    }
}

static void js_print_string(JSPrintValueState *s, JSValueConst val)
{
    int sep;
    if (s->options.raw_dump && JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        js_printf(s, "%d", js_rc(p)->ref_count);
        sep = (js_rc(p)->ref_count == 1) ? '\"' : '\'';
    } else {
        sep = '\"';
    }
    js_putc(s, sep);
    js_print_string_rec(s, val, sep, 0);
    js_putc(s, sep);
    if (js_string_get_length(val) > s->options.max_string_length) {
        uint32_t n = js_string_get_length(val) - s->options.max_string_length;
        js_printf(s, "... %u more character%s", n, n > 1 ? "s" : "");
    }
}

static void js_print_raw_string(JSPrintValueState *s, JSValueConst val)
{
    const char *cstr;
    size_t len;
    cstr = JS_ToCStringLen(s->ctx, &len, val);
    if (cstr) {
        s->write_func(s->write_opaque, cstr, len);
        JS_FreeCString(s->ctx, cstr);
    }
}

static BOOL is_ascii_ident(const JSString *p)
{
    int i, c;

    if (p->len == 0)
        return FALSE;
    for(i = 0; i < p->len; i++) {
        c = string_get(p, i);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c == '_' || c == '$') || (c >= '0' && c <= '9' && i > 0)))
            return FALSE;
    }
    return TRUE;
}

static void js_print_atom(JSPrintValueState *s, JSAtom atom)
{
    int i;
    if (__JS_AtomIsTaggedInt(atom)) {
        js_printf(s, "%u", __JS_AtomToUInt32(atom));
    } else if (atom == JS_ATOM_NULL) {
        js_puts(s, "<null>");
    } else {
        assert(atom < s->rt->atom_size);
        JSString *p;
        p = s->rt->atom_array[atom];
        if (is_ascii_ident(p)) {
            for(i = 0; i < p->len; i++) {
                js_putc(s, string_get(p, i));
            }
        } else {
            js_putc(s, '"');
            js_print_string1(s, p, p->len, '\"');
            js_putc(s, '"');
        }
    }
}

/* return 0 if invalid length */
static uint32_t js_print_array_get_length(JSObject *p)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;

    prs = find_own_property(&pr, p, JS_ATOM_length);
    if (!prs)
        return 0;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return 0;
    val = pr->u.value;
    switch(JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_INT:
        return JS_VALUE_GET_INT(val);
    case JS_TAG_FLOAT64:
        return (uint32_t)JS_VALUE_GET_FLOAT64(val);
    default:
        return 0;
    }
}

static void js_print_comma(JSPrintValueState *s, int *pcomma_state)
{
    switch(*pcomma_state) {
    case 0:
        break;
    case 1:
        js_printf(s, ", ");
        break;
    case 2:
        js_printf(s, " { ");
        break;
    }
    *pcomma_state = 1;
}

static void js_print_more_items(JSPrintValueState *s, int *pcomma_state,
                                uint32_t n)
{
    js_print_comma(s, pcomma_state);
    js_printf(s, "... %u more item%s", n, n > 1 ? "s" : "");
}

/* similar to js_regexp_toString() but without side effect */
static void js_print_regexp(JSPrintValueState *s, JSObject *p1)
{
    JSRegExp *re = &p1->u.regexp;
    JSString *p;
    int i, n, c, c2, bra, flags;
    static const char regexp_flags[] = { 'g', 'i', 'm', 's', 'u', 'y', 'd', 'v' };

    if (!re->pattern || !re->bytecode) {
        /* the regexp fields are zeroed at init */
        js_puts(s, "[uninitialized_regexp]");
        return;
    }
    p = re->pattern;
    js_putc(s, '/');
    if (p->len == 0) {
        js_puts(s, "(?:)");
    } else {
        bra = 0;
        for (i = 0, n = p->len; i < n;) {
            c2 = -1;
            switch (c = string_get(p, i++)) {
            case '\\':
                if (i < n)
                    c2 = string_get(p, i++);
                break;
            case ']':
                bra = 0;
                break;
            case '[':
                if (!bra) {
                    if (i < n && string_get(p, i) == ']')
                        c2 = string_get(p, i++);
                    bra = 1;
                }
                break;
            case '\n':
                c = '\\';
                c2 = 'n';
                break;
            case '\r':
                c = '\\';
                c2 = 'r';
                break;
            case '/':
                if (!bra) {
                    c = '\\';
                    c2 = '/';
                }
                break;
            }
            js_putc(s, c);
            if (c2 >= 0)
                js_putc(s, c2);
        }
    }
    js_putc(s, '/');

    flags = lre_get_flags(re->bytecode->u.str8);
    for(i = 0; i < countof(regexp_flags); i++) {
        if ((flags >> i) & 1) {
            js_putc(s, regexp_flags[i]);
        }
    }
}

/* similar to js_error_toString() but without side effect */
static void js_print_error(JSPrintValueState *s, JSObject *p)
{
    const char *str;
    size_t len;

    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
    if (!str) {
        js_puts(s, "Error");
    } else {
        js_puts(s, str);
        JS_FreeCString(s->ctx, str);
    }
    
    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_message);
    if (str && str[0] != '\0') {
        js_puts(s, ": ");
        js_puts(s, str);
    }
    JS_FreeCString(s->ctx, str);

    /* dump the stack if present */
    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_stack);
    if (str) {
        js_putc(s, '\n');
        
        /* XXX: should remove the last '\n' in stack as
           v8. SpiderMonkey does not do it */
        len = strlen(str);
        if (len > 0 && str[len - 1] == '\n')
            len--;
        s->write_func(s->write_opaque, str, len);
        
        JS_FreeCString(s->ctx, str);
    }
}

static void js_print_object(JSPrintValueState *s, JSObject *p)
{
    JSRuntime *rt = s->rt;
    JSShape *sh;
    JSShapeProperty *prs;
    JSProperty *pr;
    int comma_state;
    BOOL is_array;
    uint32_t i;
    
    comma_state = 0;
    is_array = FALSE;
    if (p->class_id == JS_CLASS_ARRAY) {
        is_array = TRUE;
        js_printf(s, "[ ");
        /* XXX: print array like properties even if not fast array */
        if (p->fast_array) {
            uint32_t len, n, len1;
            len = js_print_array_get_length(p);

            len1 = min_uint32(p->u.array.count, s->options.max_item_count);
            for(i = 0; i < len1; i++) {
                js_print_comma(s, &comma_state);
                js_print_value(s, p->u.array.u.values[i]);
            }
            if (len1 < p->u.array.count)
                js_print_more_items(s, &comma_state, p->u.array.count - len1);
            if (p->u.array.count < len) {
                n = len - p->u.array.count;
                js_print_comma(s, &comma_state);
                js_printf(s, "<%u empty item%s>", n, n > 1 ? "s" : "");
            }
        }
    } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY && p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
        uint32_t size = 1 << typed_array_size_log2(p->class_id);
        uint32_t len1;
        int64_t v;

        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) [ ", p->u.array.count);
        
        is_array = TRUE;
        len1 = min_uint32(p->u.array.count, s->options.max_item_count);
        for(i = 0; i < len1; i++) {
            const uint8_t *ptr = p->u.array.u.uint8_ptr + i * size;
            js_print_comma(s, &comma_state);
            switch(p->class_id) {
            case JS_CLASS_UINT8C_ARRAY:
            case JS_CLASS_UINT8_ARRAY:
                v = *ptr;
                goto ta_int64;
            case JS_CLASS_INT8_ARRAY:
                v = *(int8_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT16_ARRAY:
                v = *(int16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT16_ARRAY:
                v = *(uint16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT32_ARRAY:
                v = *(int32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT32_ARRAY:
                v = *(uint32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_BIG_INT64_ARRAY:
                v = *(int64_t *)ptr;
            ta_int64:
                js_printf(s, "%" PRId64, v);
                break;
            case JS_CLASS_BIG_UINT64_ARRAY:
                js_printf(s, "%" PRIu64, *(uint64_t *)ptr);
                break;
            case JS_CLASS_FLOAT16_ARRAY:
                js_print_float64(s, fromfp16(*(uint16_t *)ptr));
                break;
            case JS_CLASS_FLOAT32_ARRAY:
                js_print_float64(s, *(float *)ptr);
                break;
            case JS_CLASS_FLOAT64_ARRAY:
                js_print_float64(s, *(double *)ptr);
                break;
            }
        }
        if (len1 < p->u.array.count)
            js_print_more_items(s, &comma_state, p->u.array.count - len1);
    } else if (p->class_id == JS_CLASS_BYTECODE_FUNCTION ||
               (rt->class_array[p->class_id].call != NULL &&
                p->class_id != JS_CLASS_PROXY)) {
        js_printf(s, "[Function");
        /* XXX: allow dump without ctx */
        if (!s->options.raw_dump && s->ctx) {
            const char *func_name_str;
            js_putc(s, ' ');
            func_name_str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
            if (!func_name_str || func_name_str[0] == '\0')
                js_puts(s, "(anonymous)");
            else
                js_puts(s, func_name_str);
            JS_FreeCString(s->ctx, func_name_str);
        }
        js_printf(s, "]");
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_MAP || p->class_id == JS_CLASS_SET) {
        JSMapState *ms = p->u.opaque;
        struct list_head *el;
        
        if (!ms)
            goto default_obj;
        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) { ", ms->record_count);
        i = 0;
        list_for_each(el, &ms->records) {
            JSMapRecord *mr = list_entry(el, JSMapRecord, link);
            js_print_comma(s, &comma_state);
            if (mr->empty)
                continue;
            js_print_value(s, mr->key);
            if (p->class_id == JS_CLASS_MAP) {
                js_printf(s, " => ");
                js_print_value(s, mr->value);
            }
            i++;
            if (i >= s->options.max_item_count)
                break;
        }
        if (i < ms->record_count)
            js_print_more_items(s, &comma_state, ms->record_count - i);
    } else if (p->class_id == JS_CLASS_REGEXP && s->ctx) {
        js_print_regexp(s, p);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_DATE && s->ctx) {
        /* get_date_string() has no side effect */
        JSValue str = get_date_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), 0, NULL, 0x23); /* toISOString() */
        if (JS_IsException(str))
            goto default_obj;
        js_print_raw_string(s, str);
        JS_FreeValueRT(s->rt, str);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_ERROR && s->ctx) {
        js_print_error(s, p);
        comma_state = 2;
    } else {
        default_obj:
        if (p->class_id != JS_CLASS_OBJECT) {
            js_print_atom(s, rt->class_array[p->class_id].class_name);
            js_printf(s, " ");
        }
        js_printf(s, "{ ");
    }
    
    sh = p->shape; /* the shape can be NULL while freeing an object */
    if (sh) {
        uint32_t j;
        
        j = 0;
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->atom != JS_ATOM_NULL) {
                if (!(prs->flags & JS_PROP_ENUMERABLE) &&
                    !s->options.show_hidden) {
                    continue;
                }
                if (j < s->options.max_item_count) {
                    pr = &p->prop[i];
                    js_print_comma(s, &comma_state);
                    js_print_atom(s, prs->atom);
                    js_printf(s, ": ");
                    
                    /* XXX: autoinit property */
                    if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[Getter %p Setter %p]",
                                    pr->u.getset.getter, pr->u.getset.setter);
                        } else {
                            if (pr->u.getset.getter && pr->u.getset.setter) {
                                js_printf(s, "[Getter/Setter]");
                            } else if (pr->u.getset.setter) {
                                js_printf(s, "[Setter]");
                            } else {
                                js_printf(s, "[Getter]");
                            }
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[varref %p]", (void *)pr->u.var_ref);
                        } else {
                            js_print_value(s, *pr->u.var_ref->pvalue);
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[autoinit %p %d %p]",
                                    (void *)js_autoinit_get_realm(pr),
                                    js_autoinit_get_id(pr),
                                    (void *)pr->u.init.opaque);
                        } else {
                            /* XXX: could autoinit but need to restart
                               the iteration */
                            js_printf(s, "[autoinit]");
                        }
                    } else {
                        js_print_value(s, pr->u.value);
                    }
                }
                j++;
            }
        }
        if (j > s->options.max_item_count)
            js_print_more_items(s, &comma_state, j - s->options.max_item_count);
    }
    if (s->options.raw_dump && js_class_has_bytecode(p->class_id)) {
        JSFunctionBytecode *b = p->u.func.function_bytecode;
        if (b->closure_var_count) {
            JSVarRef **var_refs;
            var_refs = p->u.func.var_refs;
            
            js_print_comma(s, &comma_state);
            js_printf(s, "[[Closure]]: [");
            for(i = 0; i < b->closure_var_count; i++) {
                if (i != 0)
                    js_printf(s, ", ");
                js_print_value(s, var_refs[i]->value);
            }
            js_printf(s, " ]");
        }
        if (p->u.func.home_object) {
            js_print_comma(s, &comma_state);
            js_printf(s, "[[HomeObject]]: ");
            js_print_value(s, JS_MKPTR(JS_TAG_OBJECT, p->u.func.home_object));
        }
    }

    if (!is_array) {
        if (comma_state != 2) {
            js_printf(s, " }");
        }
    } else {
        js_printf(s, " ]");
    }
}

static int js_print_stack_index(JSPrintValueState *s, JSObject *p)
{
    int i;
    for(i = 0; i < s->level; i++)
        if (s->print_stack[i] == p)
            return i;
    return -1;
}

static void js_print_value(JSPrintValueState *s, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_NORM_TAG(val);
    const char *str;

    switch(tag) {
    case JS_TAG_INT:
        js_printf(s, "%d", JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_BOOL:
        if (JS_VALUE_GET_BOOL(val))
            str = "true";
        else
            str = "false";
        goto print_str;
    case JS_TAG_NULL:
        str = "null";
        goto print_str;
    case JS_TAG_EXCEPTION:
        str = "exception";
        goto print_str;
    case JS_TAG_UNINITIALIZED:
        str = "uninitialized";
        goto print_str;
    case JS_TAG_UNDEFINED:
        str = "undefined";
    print_str:
        js_puts(s, str);
        break;
    case JS_TAG_FLOAT64:
        js_print_float64(s, JS_VALUE_GET_FLOAT64(val));
        break;
    case JS_TAG_SHORT_BIG_INT:
        js_printf(s, "%" PRId64 "n", (int64_t)JS_VALUE_GET_SHORT_BIG_INT(val));
        break;
    case JS_TAG_BIG_INT:
        if (!s->options.raw_dump && s->ctx) {
            JSValue str = js_bigint_to_string(s->ctx, val);
            if (JS_IsException(str))
                goto raw_bigint;
            js_print_raw_string(s, str);
            js_putc(s, 'n');
            JS_FreeValueRT(s->rt, str);
        } else {
            JSBigInt *p;
            int sgn, i;
        raw_bigint:
            p = JS_VALUE_GET_PTR(val);
            /* In order to avoid allocations we just dump the limbs */
            sgn = js_bigint_sign(p);
            if (sgn)
                js_printf(s, "BigInt.asIntN(%d,", p->len * JS_LIMB_BITS);
            js_printf(s, "0x");
            for(i = p->len - 1; i >= 0; i--) {
                if (i != p->len - 1)
                    js_putc(s, '_');
#if JS_LIMB_BITS == 32
                js_printf(s, "%08x", p->tab[i]);
#else
                js_printf(s, "%016" PRIx64, p->tab[i]);
#endif
            }
            js_putc(s, 'n');
            if (sgn)
                js_putc(s, ')');
        }
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        if (s->options.raw_dump && tag == JS_TAG_STRING_ROPE) {
            JSStringRope *r = JS_VALUE_GET_STRING_ROPE(val);
            js_printf(s, "[rope len=%d depth=%d]", r->len, r->depth);
        } else {
            js_print_string(s, val);
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        {
            JSFunctionBytecode *b = JS_VALUE_GET_PTR(val);
            js_puts(s, "[bytecode ");
            js_print_atom(s, b->func_name);
            js_putc(s, ']');
        }
        break;
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            int idx;
            idx = js_print_stack_index(s, p);
            if (idx >= 0) {
                js_printf(s, "[circular %d]", idx);
            } else if (s->level < s->options.max_depth) {
                s->print_stack[s->level++] = p;
                js_print_object(s, JS_VALUE_GET_OBJ(val));
                s->level--;
            } else {
                JSAtom atom = s->rt->class_array[p->class_id].class_name;
                js_putc(s, '[');
                js_print_atom(s, atom);
                if (s->options.raw_dump) {
                    js_printf(s, " %p", (void *)p);
                }
                js_putc(s, ']');
            }
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            js_puts(s, "Symbol(");
            js_print_atom(s, js_get_atom_index(s->rt, p));
            js_putc(s, ')');
        }
        break;
    case JS_TAG_MODULE:
        js_puts(s, "[module]");
        break;
    default:
        js_printf(s, "[unknown tag %d]", tag);
        break;
    }
}

void JS_PrintValueSetDefaultOptions(JSPrintValueOptions *options)
{
    memset(options, 0, sizeof(*options));
    options->max_depth = 2;
    options->max_string_length = 1000;
    options->max_item_count = 100;
}

static void JS_PrintValueInternal(JSRuntime *rt, JSContext *ctx, 
                                  JSPrintValueWrite *write_func, void *write_opaque,
                                  JSValueConst val, const JSPrintValueOptions *options)
{
    JSPrintValueState ss, *s = &ss;
    if (options)
        s->options = *options;
    else
        JS_PrintValueSetDefaultOptions(&s->options);
    if (s->options.max_depth <= 0)
        s->options.max_depth = JS_PRINT_MAX_DEPTH;
    else
        s->options.max_depth = min_int(s->options.max_depth, JS_PRINT_MAX_DEPTH);
    if (s->options.max_string_length == 0)
        s->options.max_string_length = UINT32_MAX;
    if (s->options.max_item_count == 0)
        s->options.max_item_count = UINT32_MAX;
    s->rt = rt;
    s->ctx = ctx;
    s->write_func = write_func;
    s->write_opaque = write_opaque;
    s->level = 0;
    js_print_value(s, val);
}

void JS_PrintValueRT(JSRuntime *rt, JSPrintValueWrite *write_func, void *write_opaque,
                     JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(rt, NULL, write_func, write_opaque, val, options);
}

void JS_PrintValue(JSContext *ctx, JSPrintValueWrite *write_func, void *write_opaque,
                   JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(ctx->rt, ctx, write_func, write_opaque, val, options);
}

QJS_INTERNAL void js_dump_value_write(void *opaque, const char *buf, size_t len)
{
    FILE *fo = opaque;
    fwrite(buf, 1, len, fo);
}

QJS_INTERNAL __maybe_unused void print_atom(JSContext *ctx, JSAtom atom)
{
    JSPrintValueState ss, *s = &ss;
    memset(s, 0, sizeof(*s));
    s->rt = ctx->rt;
    s->ctx = ctx;
    s->write_func = js_dump_value_write;
    s->write_opaque = stdout;
    js_print_atom(s, atom);
}

static __maybe_unused void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom)
{
    printf("%s=", str);
    print_atom(ctx, atom);
    printf("\n");
}

QJS_INTERNAL __maybe_unused void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValue(ctx, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

static __maybe_unused void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValueRT(rt, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

QJS_INTERNAL __maybe_unused void JS_DumpObjectHeader(JSRuntime *rt)
{
    printf("%14s %4s %4s %14s %s\n",
           "ADDRESS", "REFS", "SHRF", "PROTO", "CONTENT");
}

/* for debug only: dump an object without side effect */
static __maybe_unused void JS_DumpObject(JSRuntime *rt, JSObject *p)
{
    JSShape *sh;
    JSPrintValueOptions options;
    
    /* XXX: should encode atoms with special characters */
    sh = p->shape; /* the shape can be NULL while freeing an object */
    printf("%14p %4d ",
           (void *)p,
           js_rc(p)->ref_count);
    if (sh) {
        printf("%3d%c %14p ",
               js_rc(sh)->ref_count,
               " *"[sh->is_hashed],
               (void *)sh->proto);
    } else {
        printf("%3s  %14s ", "-", "-");
    }

    JS_PrintValueSetDefaultOptions(&options);
    options.max_depth = 1;
    options.show_hidden = TRUE;
    options.raw_dump = TRUE;
    JS_PrintValueRT(rt, js_dump_value_write, stdout, JS_MKPTR(JS_TAG_OBJECT, p), &options);

    printf("\n");
}

QJS_INTERNAL __maybe_unused void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p)
{
    if (js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT) {
        JS_DumpObject(rt, (JSObject *)p);
    } else {
        printf("%14p %4d ",
               (void *)p,
               js_rc(p)->ref_count);
        switch(js_rc(p)->gc_obj_type) {
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
            printf("[function bytecode]");
            break;
        case JS_GC_OBJ_TYPE_SHAPE:
            printf("[shape]");
            break;
        case JS_GC_OBJ_TYPE_VAR_REF:
            printf("[var_ref]");
            break;
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
            printf("[async_function]");
            break;
        case JS_GC_OBJ_TYPE_JS_CONTEXT:
            printf("[js_context]");
            break;
        case JS_GC_OBJ_TYPE_MODULE:
            printf("[module]");
            break;
        default:
            printf("[unknown %d]", js_rc(p)->gc_obj_type);
            break;
        }
        printf("\n");
    }
}

/* return -1 if exception (proxy case) or TRUE/FALSE */
// TODO: should take flags to make proxy resolution and exceptions optional
int JS_IsArray(JSContext *ctx, JSValueConst val)
{
    if (js_resolve_proxy(ctx, &val, TRUE))
        return -1;
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        return p->class_id == JS_CLASS_ARRAY;
    } else {
        return FALSE;
    }
}

/* Object class */

QJS_INTERNAL JSValue JS_ToObject(JSContext *ctx, JSValueConst val)
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

QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val)
{
    JSValue obj = JS_ToObject(ctx, val);
    JS_FreeValue(ctx, val);
    return obj;
}

/* eval */

int JS_AddIntrinsicEval(JSContext *ctx)
{
    ctx->eval_internal = __JS_EvalInternal;
    return 0;
}

QJS_INTERNAL void set_value(JSContext *ctx, JSValue *pval, JSValue new_val)
{
    JSValue old_val;
    old_val = *pval;
    *pval = new_val;
    JS_FreeValue(ctx, old_val);
}
