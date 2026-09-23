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

#include "cutils.h"
#include "list.h"
#include "quickjs.h"
#include "libregexp.h"
#include "libunicode.h"
#include "dtoa.h"
#include "internal/runtime.h"
#include "internal/weakref.h"
#include "internal/object.h"
#include "internal/number.h"
#include "internal/operator.h"
#include "internal/bigint.h"
#include "internal/primitive.h"
#include "internal/function.h"
#include "internal/function-list.h"
#include "internal/atom-string.h"
#include "internal/value-print.h"
#include "internal/iterator.h"
#include "internal/property.h"
#include "internal/shape.h"
#include "internal/array.h"
#include "internal/parser.h"
#include "internal/vm.h"
#include "internal/serialization.h"
#include "internal/module.h"
#include "internal/opcode.h"
#include "internal/error.h"
#include "builtins/number.h"
#include "builtins/boolean.h"
#include "builtins/bigint.h"
#include "builtins/string.h"
#include "builtins/regexp.h"
#include "builtins/object.h"
#include "builtins/reflect.h"
#include "builtins/proxy.h"
#include "builtins/collection.h"
#include "builtins/async.h"
#include "builtins/typed-array.h"
#include "builtins/error.h"
#include "builtins/function.h"
#include "builtins/array.h"
#include "builtins/iterator.h"
#include "builtins/base.h"
#include "builtins/symbol.h"
#include "builtins/global.h"
#include "builtins/math.h"
#include "builtins/date.h"
#include "builtins/weakref.h"


#if !defined(_WIN32)
/* define it if printf uses the RNDN rounding mode instead of RNDNA */
#define CONFIG_PRINTF_RNDN
#endif

/* dump object free */
//#define DUMP_FREE
//#define DUMP_CLOSURE
/* dump the bytecode of the compiled functions: combination of bits
   1: dump pass 3 final byte code
   2: dump pass 2 code
   4: dump pass 1 code
   8: dump stdlib functions
  16: dump bytecode in hex
  32: dump line number table
  64: dump compute_stack_size
 */
//#define DUMP_BYTECODE  (1)
/* dump the occurence of the automatic GC */
//#define DUMP_GC
/* dump objects freed by the garbage collector */
//#define DUMP_GC_FREE
/* dump memory usage before running the garbage collector */
//#define DUMP_MEM
//#define DUMP_OBJECTS    /* dump objects in JS_FreeContext */
//#define DUMP_ATOMS      /* dump atoms in JS_FreeContext */
//#define DUMP_SHAPES     /* dump shapes in JS_FreeContext */
//#define DUMP_MODULE_RESOLVE
//#define DUMP_MODULE_EXEC
//#define DUMP_PROMISE
//#define DUMP_READ_OBJECT
//#define DUMP_ROPE_REBALANCE
/* add asm labels to each opcode so that it is easier to see the generated code */
//#define OPCODE_ASM_LABEL

/* test the GC by forcing it before each object allocation */
//#define FORCE_GC_AT_MALLOC







typedef struct JSJobEntry {
    struct list_head link;
    JSContext *realm;
    JSJobFunc *job_func;
    int argc;
    JSValue argv[0];
} JSJobEntry;

QJS_INTERNAL JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                                  JSValueConst this_obj,
                                  int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
QJS_INTERNAL __exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor);
QJS_INTERNAL JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                             JSValueConst val, int flags, int scope_idx);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
QJS_INTERNAL JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);
static void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val);
static void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
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
QJS_INTERNAL void js_generator_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL void js_generator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL void js_global_object_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);

QJS_INTERNAL int JS_ToBoolFree(JSContext *ctx, JSValue val);
QJS_INTERNAL int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);
QJS_INTERNAL int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
QJS_INTERNAL void gc_decref(JSRuntime *rt);
QJS_INTERNAL int JS_NewClass1(JSRuntime *rt, JSClassID class_id,
                        const JSClassDef *class_def, JSAtom name);

QJS_INTERNAL BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode);
QJS_INTERNAL BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);

QJS_INTERNAL JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
QJS_INTERNAL JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);
QJS_INTERNAL void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
QJS_INTERNAL void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
QJS_INTERNAL JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int flags);
QJS_INTERNAL void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
QJS_INTERNAL void js_free_module_def(JSRuntime *rt, JSModuleDef *m);
QJS_INTERNAL void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_import_meta(JSContext *ctx);
QJS_INTERNAL JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
QJS_INTERNAL void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
QJS_INTERNAL int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2);
QJS_INTERNAL JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);
QJS_INTERNAL JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
QJS_INTERNAL int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
QJS_INTERNAL void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
QJS_INTERNAL void js_free_shape(JSRuntime *rt, JSShape *sh);
QJS_INTERNAL void js_free_shape_null(JSRuntime *rt, JSShape *sh);
QJS_INTERNAL int init_shape_hash(JSRuntime *rt);
QJS_INTERNAL __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL __exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
QJS_INTERNAL JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);
QJS_INTERNAL void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags);
QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h);
QJS_INTERNAL JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
QJS_INTERNAL JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                 void *opaque);
QJS_INTERNAL void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
QJS_INTERNAL JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                         JSAtom atom, BOOL is_lexical);


static const JSClassExoticMethods js_arguments_exotic_methods;

static JSClassShortDef const js_std_class_def[] = {
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

QJS_INTERNAL int init_class_range(JSRuntime *rt, JSClassShortDef const *tab,
                            int start, int count)
{
    JSClassDef cm_s, *cm = &cm_s;
    int i, class_id;

    for(i = 0; i < count; i++) {
        class_id = i + start;
        memset(cm, 0, sizeof(*cm));
        cm->finalizer = tab[i].finalizer;
        cm->gc_mark = tab[i].gc_mark;
        if (JS_NewClass1(rt, class_id, cm, tab[i].class_name) < 0)
            return -1;
    }
    return 0;
}

JSRuntime *JS_NewRuntime2(const JSMallocFunctions *mf, void *opaque)
{
    JSRuntime *rt;
    JSMallocState ms;

    memset(&ms, 0, sizeof(ms));
    ms.opaque = opaque;
    ms.malloc_limit = -1;

    rt = mf->js_malloc(&ms, sizeof(JSRuntime));
    if (!rt)
        return NULL;
    memset(rt, 0, sizeof(*rt));
    js_malloc_init(&rt->malloc_ctx);
    rt->malloc_ctx.mf = *mf;
    rt->malloc_ctx.malloc_state = ms;
    rt->malloc_gc_threshold = 256 * 1024;

    init_list_head(&rt->context_list);
    init_list_head(&rt->gc_obj_list);
    init_list_head(&rt->gc_zero_ref_count_list);
    rt->gc_phase = JS_GC_PHASE_NONE;
    init_list_head(&rt->weakref_list);

#ifdef DUMP_LEAKS
    init_list_head(&rt->string_list);
#endif
    init_list_head(&rt->job_list);

    if (JS_InitAtoms(rt))
        goto fail;

    /* create the object, array and function classes */
    if (init_class_range(rt, js_std_class_def, JS_CLASS_OBJECT,
                         countof(js_std_class_def)) < 0)
        goto fail;
    rt->class_array[JS_CLASS_ARGUMENTS].exotic = &js_arguments_exotic_methods;
    rt->class_array[JS_CLASS_MAPPED_ARGUMENTS].exotic = &js_arguments_exotic_methods;
    rt->class_array[JS_CLASS_STRING].exotic = &js_string_exotic_methods;
    rt->class_array[JS_CLASS_MODULE_NS].exotic = &js_module_ns_exotic_methods;

    rt->class_array[JS_CLASS_C_FUNCTION].call = js_call_c_function;
    rt->class_array[JS_CLASS_C_FUNCTION_DATA].call = js_c_function_data_call;
    rt->class_array[JS_CLASS_BOUND_FUNCTION].call = js_call_bound_function;
    rt->class_array[JS_CLASS_GENERATOR_FUNCTION].call = js_generator_function_call;
    if (init_shape_hash(rt))
        goto fail;

    rt->stack_size = JS_DEFAULT_STACK_SIZE;
    JS_UpdateStackTop(rt);

    rt->current_exception = JS_UNINITIALIZED;

    return rt;
 fail:
    JS_FreeRuntime(rt);
    return NULL;
}

void *JS_GetRuntimeOpaque(JSRuntime *rt)
{
    return rt->user_opaque;
}

void JS_SetRuntimeOpaque(JSRuntime *rt, void *opaque)
{
    rt->user_opaque = opaque;
}

void JS_SetMemoryLimit(JSRuntime *rt, size_t limit)
{
    rt->malloc_ctx.malloc_state.malloc_limit = limit;
}

/* use -1 to disable automatic GC */
void JS_SetGCThreshold(JSRuntime *rt, size_t gc_threshold)
{
    rt->malloc_gc_threshold = gc_threshold;
}

#define malloc(s) malloc_is_forbidden(s)
#define free(p) free_is_forbidden(p)
#define realloc(p,s) realloc_is_forbidden(p,s)

void JS_SetInterruptHandler(JSRuntime *rt, JSInterruptHandler *cb, void *opaque)
{
    rt->interrupt_handler = cb;
    rt->interrupt_opaque = opaque;
}

void JS_SetCanBlock(JSRuntime *rt, BOOL can_block)
{
    rt->can_block = can_block;
}

void JS_SetSharedArrayBufferFunctions(JSRuntime *rt,
                                      const JSSharedArrayBufferFunctions *sf)
{
    rt->sab_funcs = *sf;
}

void JS_SetStripInfo(JSRuntime *rt, int flags)
{
    rt->strip_flags = flags;
}

int JS_GetStripInfo(JSRuntime *rt)
{
    return rt->strip_flags;
}

QJS_INTERNAL int JS_EnqueueJob2(JSContext *ctx, JSJobFunc *job_func,
                          int argc, JSValueConst *argv, BOOL no_exception)
{
    JSRuntime *rt = ctx->rt;
    JSJobEntry *e;
    int i;

    if (no_exception)
        e = js_malloc_rt(ctx->rt, sizeof(*e) + argc * sizeof(JSValue));
    else
        e = js_malloc(ctx, sizeof(*e) + argc * sizeof(JSValue));
    if (!e)
        return -1;
    e->realm = JS_DupContext(ctx);
    e->job_func = job_func;
    e->argc = argc;
    for(i = 0; i < argc; i++) {
        e->argv[i] = JS_DupValue(ctx, argv[i]);
    }
    list_add_tail(&e->link, &rt->job_list);
    return 0;
}

/* return 0 if OK, < 0 if exception */
int JS_EnqueueJob(JSContext *ctx, JSJobFunc *job_func,
                  int argc, JSValueConst *argv)
{
    return JS_EnqueueJob2(ctx, job_func, argc, argv, FALSE);
}

BOOL JS_IsJobPending(JSRuntime *rt)
{
    return !list_empty(&rt->job_list);
}

/* return < 0 if exception, 0 if no job pending, 1 if a job was
   executed successfully. The context of the job is stored in '*pctx'
   if pctx != NULL. It may be NULL if the context was already
   destroyed or if no job was pending. The 'pctx' parameter is now
   absolete. */
int JS_ExecutePendingJob(JSRuntime *rt, JSContext **pctx)
{
    JSContext *ctx;
    JSJobEntry *e;
    JSValue res;
    int i, ret;

    if (list_empty(&rt->job_list)) {
        if (pctx)
            *pctx = NULL;
        return 0;
    }

    /* get the first pending job and execute it */
    e = list_entry(rt->job_list.next, JSJobEntry, link);
    list_del(&e->link);
    ctx = e->realm;
    res = e->job_func(ctx, e->argc, (JSValueConst *)e->argv);
    for(i = 0; i < e->argc; i++)
        JS_FreeValue(ctx, e->argv[i]);
    if (JS_IsException(res))
        ret = -1;
    else
        ret = 1;
    JS_FreeValue(ctx, res);
    js_free(ctx, e);
    if (pctx) {
        if (js_rc(ctx)->ref_count > 1)
            *pctx = ctx;
        else
            *pctx = NULL;
    }
    JS_FreeContext(ctx);
    return ret;
}

/* Note: the string contents are uninitialized */
QJS_INTERNAL JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char)
{
    JSString *str;
    str = js_malloc_rt(rt, sizeof(JSString) + (max_len << is_wide_char) + 1 - is_wide_char);
    if (unlikely(!str))
        return NULL;
    js_rc(str)->ref_count = 1;
    str->is_wide_char = is_wide_char;
    str->len = max_len;
    str->atom_type = 0;
    str->hash = 0;          /* optional but costless */
    str->hash_next = 0;     /* optional */
#ifdef DUMP_LEAKS
    list_add_tail(&str->link, &rt->string_list);
#endif
    return str;
}

QJS_INTERNAL JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char)
{
    JSString *p;
    p = js_alloc_string_rt(ctx->rt, max_len, is_wide_char);
    if (unlikely(!p)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return p;
}

/* same as JS_FreeValueRT() but faster */
void JS_SetRuntimeInfo(JSRuntime *rt, const char *s)
{
    if (rt)
        rt->rt_info = s;
}

void JS_FreeRuntime(JSRuntime *rt)
{
    struct list_head *el, *el1;
    int i;

    JS_FreeValueRT(rt, rt->current_exception);

    list_for_each_safe(el, el1, &rt->job_list) {
        JSJobEntry *e = list_entry(el, JSJobEntry, link);
        for(i = 0; i < e->argc; i++)
            JS_FreeValueRT(rt, e->argv[i]);
        JS_FreeContext(e->realm);
        js_free_rt(rt, e);
    }
    init_list_head(&rt->job_list);

    /* don't remove the weak objects to avoid create new jobs with
       FinalizationRegistry */
    JS_RunGCInternal(rt, FALSE);

#ifdef DUMP_LEAKS
    /* leaking objects */
    {
        BOOL header_done;
        JSGCObjectHeader *p;
        int count;

        /* remove the internal refcounts to display only the object
           referenced externally */
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            js_rc(p)->mark = 0;
        }
        gc_decref(rt);

        header_done = FALSE;
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            if (js_rc(p)->ref_count != 0) {
                if (!header_done) {
                    printf("Object leaks:\n");
                    JS_DumpObjectHeader(rt);
                    header_done = TRUE;
                }
                JS_DumpGCObject(rt, p);
            }
        }

        count = 0;
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            if (js_rc(p)->ref_count == 0) {
                count++;
            }
        }
        if (count != 0)
            printf("Secondary object leaks: %d\n", count);
    }
#endif
    assert(list_empty(&rt->gc_obj_list));
    assert(list_empty(&rt->weakref_list));

    /* free the classes */
    for(i = 0; i < rt->class_count; i++) {
        JSClass *cl = &rt->class_array[i];
        if (cl->class_id != 0) {
            JS_FreeAtomRT(rt, cl->class_name);
        }
    }
    js_free_rt(rt, rt->class_array);

#ifdef DUMP_LEAKS
    /* only the atoms defined in JS_InitAtoms() should be left */
    {
        BOOL header_done = FALSE;

        for(i = 0; i < rt->atom_size; i++) {
            JSAtomStruct *p = rt->atom_array[i];
            if (!atom_is_free(p) /* && p->str*/) {
                if (i >= JS_ATOM_END || js_rc(p)->ref_count != 1) {
                    if (!header_done) {
                        header_done = TRUE;
                        if (rt->rt_info) {
                            printf("%s:1: atom leakage:", rt->rt_info);
                        } else {
                            printf("Atom leaks:\n"
                                   "    %6s %6s %s\n",
                                   "ID", "REFCNT", "NAME");
                        }
                    }
                    if (rt->rt_info) {
                        printf(" ");
                    } else {
                        printf("    %6u %6u ", i, js_rc(p)->ref_count);
                    }
                    switch (p->atom_type) {
                    case JS_ATOM_TYPE_STRING:
                        JS_DumpString(rt, p);
                        break;
                    case JS_ATOM_TYPE_GLOBAL_SYMBOL:
                        printf("Symbol.for(");
                        JS_DumpString(rt, p);
                        printf(")");
                        break;
                    case JS_ATOM_TYPE_SYMBOL:
                        if (p->hash != JS_ATOM_HASH_PRIVATE) {
                            printf("Symbol(");
                            JS_DumpString(rt, p);
                            printf(")");
                        } else {
                            printf("Private(");
                            JS_DumpString(rt, p);
                            printf(")");
                        }
                        break;
                    }
                    if (rt->rt_info) {
                        printf(":%u", js_rc(p)->ref_count);
                    } else {
                        printf("\n");
                    }
                }
            }
        }
        if (rt->rt_info && header_done)
            printf("\n");
    }
#endif

    /* free the atoms */
    for(i = 0; i < rt->atom_size; i++) {
        JSAtomStruct *p = rt->atom_array[i];
        if (!atom_is_free(p)) {
#ifdef DUMP_LEAKS
            list_del(&p->link);
#endif
            js_free_rt(rt, p);
        }
    }
    js_free_rt(rt, rt->atom_array);
    js_free_rt(rt, rt->atom_hash);
    js_free_rt(rt, rt->shape_hash);
#ifdef DUMP_LEAKS
    if (!list_empty(&rt->string_list)) {
        if (rt->rt_info) {
            printf("%s:1: string leakage:", rt->rt_info);
        } else {
            printf("String leaks:\n"
                   "    %6s %s\n",
                   "REFCNT", "VALUE");
        }
        list_for_each_safe(el, el1, &rt->string_list) {
            JSString *str = list_entry(el, JSString, link);
            if (rt->rt_info) {
                printf(" ");
            } else {
                printf("    %6u ", js_rc(str)->ref_count);
            }
            JS_DumpString(rt, str);
            if (rt->rt_info) {
                printf(":%u", js_rc(str)->ref_count);
            } else {
                printf("\n");
            }
            list_del(&str->link);
            js_free_rt(rt, str);
        }
        if (rt->rt_info)
            printf("\n");
    }
    {
        JSMallocState *s = &rt->malloc_ctx.malloc_state;
        if (s->malloc_count > 1) {
            if (rt->rt_info)
                printf("%s:1: ", rt->rt_info);
            printf("Memory leak: %"PRIu64" bytes lost in %"PRIu64" block%s\n",
                   (uint64_t)(s->malloc_size - sizeof(JSRuntime)),
                   (uint64_t)(s->malloc_count - 1), &"s"[s->malloc_count == 2]);
        }
    }
#endif

    {
        JSMallocState ms = rt->malloc_ctx.malloc_state;
        rt->malloc_ctx.mf.js_free(&ms, rt);
    }
}

JSContext *JS_NewContextRaw(JSRuntime *rt)
{
    JSContext *ctx;
    int i;

    ctx = js_mallocz_rt(rt, sizeof(JSContext));
    if (!ctx)
        return NULL;
    js_rc(ctx)->ref_count = 1;
    add_gc_object(rt, &ctx->header, JS_GC_OBJ_TYPE_JS_CONTEXT);

    ctx->class_proto = js_malloc_rt(rt, sizeof(ctx->class_proto[0]) *
                                    rt->class_count);
    if (!ctx->class_proto) {
        js_free_rt(rt, ctx);
        return NULL;
    }
    ctx->rt = rt;
    list_add_tail(&ctx->link, &rt->context_list);
    for(i = 0; i < rt->class_count; i++)
        ctx->class_proto[i] = JS_NULL;
    ctx->array_ctor = JS_NULL;
    ctx->iterator_ctor = JS_NULL;
    ctx->regexp_ctor = JS_NULL;
    ctx->promise_ctor = JS_NULL;
    init_list_head(&ctx->loaded_modules);

    if (JS_AddIntrinsicBasicObjects(ctx)) {
        JS_FreeContext(ctx);
        return NULL;
    }
    return ctx;
}

JSContext *JS_NewContext(JSRuntime *rt)
{
    JSContext *ctx;

    ctx = JS_NewContextRaw(rt);
    if (!ctx)
        return NULL;

    if (JS_AddIntrinsicBaseObjects(ctx) ||
        JS_AddIntrinsicDate(ctx) ||
        JS_AddIntrinsicEval(ctx) ||
        JS_AddIntrinsicStringNormalize(ctx) ||
        JS_AddIntrinsicRegExp(ctx) ||
        JS_AddIntrinsicJSON(ctx) ||
        JS_AddIntrinsicProxy(ctx) ||
        JS_AddIntrinsicMapSet(ctx) ||
        JS_AddIntrinsicTypedArrays(ctx) ||
        JS_AddIntrinsicPromise(ctx) ||
        JS_AddIntrinsicWeakRef(ctx)) {
        JS_FreeContext(ctx);
        return NULL;
    }
    return ctx;
}

void *JS_GetContextOpaque(JSContext *ctx)
{
    return ctx->user_opaque;
}

void JS_SetContextOpaque(JSContext *ctx, void *opaque)
{
    ctx->user_opaque = opaque;
}

void JS_SetClassProto(JSContext *ctx, JSClassID class_id, JSValue obj)
{
    JSRuntime *rt = ctx->rt;
    assert(class_id < rt->class_count);
    set_value(ctx, &ctx->class_proto[class_id], obj);
}

JSValue JS_GetClassProto(JSContext *ctx, JSClassID class_id)
{
    JSRuntime *rt = ctx->rt;
    assert(class_id < rt->class_count);
    return JS_DupValue(ctx, ctx->class_proto[class_id]);
}

/* XXX: would be more efficient with separate module lists */
QJS_INTERNAL void js_free_modules(JSContext *ctx, JSFreeModuleEnum flag)
{
    struct list_head *el, *el1;
    list_for_each_safe(el, el1, &ctx->loaded_modules) {
        JSModuleDef *m = list_entry(el, JSModuleDef, link);
        if (flag == JS_FREE_MODULE_ALL ||
            (flag == JS_FREE_MODULE_NOT_RESOLVED && !m->resolved)) {
            /* warning: the module may be referenced elsewhere. It
               could be simpler to use an array instead of a list for
               'ctx->loaded_modules' */
            list_del(&m->link);
            m->link.prev = NULL;
            m->link.next = NULL;
            JS_FreeValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
        }
    }
}

JSContext *JS_DupContext(JSContext *ctx)
{
    js_rc(ctx)->ref_count++;
    return ctx;
}

void JS_FreeContext(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    int i;

    if (--js_rc(ctx)->ref_count > 0)
        return;
    assert(js_rc(ctx)->ref_count == 0);

#ifdef DUMP_ATOMS
    JS_DumpAtoms(ctx->rt);
#endif
#ifdef DUMP_SHAPES
    JS_DumpShapes(ctx->rt);
#endif
#ifdef DUMP_OBJECTS
    {
        struct list_head *el;
        JSGCObjectHeader *p;
        printf("JSObjects: {\n");
        JS_DumpObjectHeader(ctx->rt);
        list_for_each(el, &rt->gc_obj_list) {
            p = list_entry(el, JSGCObjectHeader, link);
            JS_DumpGCObject(rt, p);
        }
        printf("}\n");
    }
#endif
#ifdef DUMP_MEM
    {
        JSMemoryUsage stats;
        JS_ComputeMemoryUsage(rt, &stats);
        JS_DumpMemoryUsage(stdout, &stats, rt);
    }
#endif

    js_free_modules(ctx, JS_FREE_MODULE_ALL);

    JS_FreeValue(ctx, ctx->global_obj);
    JS_FreeValue(ctx, ctx->global_var_obj);

    JS_FreeValue(ctx, ctx->throw_type_error);
    JS_FreeValue(ctx, ctx->eval_obj);

    JS_FreeValue(ctx, ctx->array_proto_values);
    for(i = 0; i < JS_NATIVE_ERROR_COUNT; i++) {
        JS_FreeValue(ctx, ctx->native_error_proto[i]);
    }
    for(i = 0; i < rt->class_count; i++) {
        JS_FreeValue(ctx, ctx->class_proto[i]);
    }
    js_free_rt(rt, ctx->class_proto);
    JS_FreeValue(ctx, ctx->iterator_ctor);
    JS_FreeValue(ctx, ctx->async_iterator_proto);
    JS_FreeValue(ctx, ctx->promise_ctor);
    JS_FreeValue(ctx, ctx->array_ctor);
    JS_FreeValue(ctx, ctx->regexp_ctor);
    JS_FreeValue(ctx, ctx->function_ctor);
    JS_FreeValue(ctx, ctx->function_proto);

    js_free_shape_null(ctx->rt, ctx->array_shape);
    js_free_shape_null(ctx->rt, ctx->arguments_shape);
    js_free_shape_null(ctx->rt, ctx->mapped_arguments_shape);
    js_free_shape_null(ctx->rt, ctx->regexp_shape);
    js_free_shape_null(ctx->rt, ctx->regexp_result_shape);

    list_del(&ctx->link);
    remove_gc_object(&ctx->header);
    js_free_rt(ctx->rt, ctx);
}

JSRuntime *JS_GetRuntime(JSContext *ctx)
{
    return ctx->rt;
}

static void update_stack_limit(JSRuntime *rt)
{
    if (rt->stack_size == 0) {
        rt->stack_limit = 0; /* no limit */
    } else {
        rt->stack_limit = rt->stack_top - rt->stack_size;
    }
}

void JS_SetMaxStackSize(JSRuntime *rt, size_t stack_size)
{
    rt->stack_size = stack_size;
    update_stack_limit(rt);
}

void JS_UpdateStackTop(JSRuntime *rt)
{
    rt->stack_top = js_get_stack_pointer();
    update_stack_limit(rt);
}


JSValue JS_GetGlobalObject(JSContext *ctx)
{
    return JS_DupValue(ctx, ctx->global_obj);
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

QJS_INTERNAL BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic)
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

QJS_INTERNAL double js_pow(double a, double b)
{
    if (unlikely(!isfinite(b)) && fabs(a) == 1) {
        /* not compatible with IEEE 754 */
        return JS_FLOAT64_NAN;
    } else {
        return pow(a, b);
    }
}

JSValue JS_NewBigInt64(JSContext *ctx, int64_t v)
{
#if JS_SHORT_BIG_INT_BITS == 64
    return __JS_NewShortBigInt(ctx, v);
#else
    if (v >= JS_SHORT_BIG_INT_MIN && v <= JS_SHORT_BIG_INT_MAX) {
        return __JS_NewShortBigInt(ctx, v);
    } else {
        JSBigInt *p;
        p = js_bigint_new_si64(ctx, v);
        if (!p)
            return JS_EXCEPTION;
        return JS_MKPTR(JS_TAG_BIG_INT, p);
    }
#endif
}

JSValue JS_NewBigUint64(JSContext *ctx, uint64_t v)
{
    if (v <= JS_SHORT_BIG_INT_MAX) {
        return __JS_NewShortBigInt(ctx, v);
    } else {
        JSBigInt *p;
        p = js_bigint_new_ui64(ctx, v);
        if (!p)
            return JS_EXCEPTION;
        return JS_MKPTR(JS_TAG_BIG_INT, p);
    }
}

/* return NaN if bad bigint literal */
QJS_INTERNAL JSValue JS_StringToBigInt(JSContext *ctx, JSValue val)
{
    const char *str, *p;
    size_t len;
    int flags;

    str = JS_ToCStringLen(ctx, &len, val);
    JS_FreeValue(ctx, val);
    if (!str)
        return JS_EXCEPTION;
    p = str;
    p += skip_spaces(p);
    if ((p - str) == len) {
        val = JS_NewBigInt64(ctx, 0);
    } else {
        flags = ATOD_INT_ONLY | ATOD_ACCEPT_BIN_OCT | ATOD_TYPE_BIG_INT;
        val = js_atof(ctx, p, &p, 0, flags);
        p += skip_spaces(p);
        if (!JS_IsException(val)) {
            if ((p - str) != len) {
                JS_FreeValue(ctx, val);
                val = JS_NAN;
            }
        }
    }
    JS_FreeCString(ctx, str);
    return val;
}

QJS_INTERNAL JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val)
{
    val = JS_StringToBigInt(ctx, val);
    if (JS_VALUE_IS_NAN(val))
        return JS_ThrowSyntaxError(ctx, "invalid bigint literal");
    return val;
}

/* JS Numbers are not allowed */
QJS_INTERNAL JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val)
{
    uint32_t tag;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        break;
    case JS_TAG_INT:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
    case JS_TAG_FLOAT64:
        goto fail;
    case JS_TAG_BOOL:
        val = __JS_NewShortBigInt(ctx, JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        val = JS_StringToBigIntErr(ctx, val);
        if (JS_IsException(val))
            return val;
        goto redo;
    case JS_TAG_OBJECT:
        val = JS_ToPrimitiveFree(ctx, val, HINT_NUMBER);
        if (JS_IsException(val))
            return val;
        goto redo;
    default:
    fail:
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeError(ctx, "cannot convert to bigint");
    }
    return val;
}

QJS_INTERNAL JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val)
{
    return JS_ToBigIntFree(ctx, JS_DupValue(ctx, val));
}

/* XXX: merge with JS_ToInt64Free with a specific flag ? */
QJS_INTERNAL int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val)
{
    uint64_t res;

    val = JS_ToBigIntFree(ctx, val);
    if (JS_IsException(val)) {
        *pres = 0;
        return -1;
    }
    if (JS_VALUE_GET_TAG(val) == JS_TAG_SHORT_BIG_INT) {
        res = JS_VALUE_GET_SHORT_BIG_INT(val);
    } else {
        JSBigInt *p = JS_VALUE_GET_PTR(val);
        /* return the value mod 2^64 */
        res = p->tab[0];
#if JS_LIMB_BITS == 32
        if (p->len >= 2)
            res |= (uint64_t)p->tab[1] << 32;
#endif
        JS_FreeValue(ctx, val);
    }
    *pres = res;
    return 0;
}

int JS_ToBigInt64(JSContext *ctx, int64_t *pres, JSValueConst val)
{
    return JS_ToBigInt64Free(ctx, pres, JS_DupValue(ctx, val));
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

QJS_INTERNAL JSValue js_build_arguments(JSContext *ctx, int argc, JSValueConst *argv)
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
QJS_INTERNAL JSValue js_build_mapped_arguments(JSContext *ctx, int argc,
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

QJS_INTERNAL __exception int JS_CopyDataProperties(JSContext *ctx,
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

QJS_INTERNAL int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                          JSValueConst desc)
{
    JSValue val, getter, setter;
    int flags;

    if (!JS_IsObject(desc)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    flags = 0;
    val = JS_UNDEFINED;
    getter = JS_UNDEFINED;
    setter = JS_UNDEFINED;
    if (JS_HasProperty(ctx, desc, JS_ATOM_enumerable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_enumerable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_ENUMERABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_ENUMERABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_configurable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_configurable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_CONFIGURABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_CONFIGURABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_value)) {
        flags |= JS_PROP_HAS_VALUE;
        val = JS_GetProperty(ctx, desc, JS_ATOM_value);
        if (JS_IsException(val))
            goto fail;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_writable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_writable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_WRITABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_WRITABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_get)) {
        flags |= JS_PROP_HAS_GET;
        getter = JS_GetProperty(ctx, desc, JS_ATOM_get);
        if (JS_IsException(getter) ||
            !(JS_IsUndefined(getter) || JS_IsFunction(ctx, getter))) {
            JS_ThrowTypeError(ctx, "invalid getter");
            goto fail;
        }
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_set)) {
        flags |= JS_PROP_HAS_SET;
        setter = JS_GetProperty(ctx, desc, JS_ATOM_set);
        if (JS_IsException(setter) ||
            !(JS_IsUndefined(setter) || JS_IsFunction(ctx, setter))) {
            JS_ThrowTypeError(ctx, "invalid setter");
            goto fail;
        }
    }
    if ((flags & (JS_PROP_HAS_SET | JS_PROP_HAS_GET)) &&
        (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE))) {
        JS_ThrowTypeError(ctx, "cannot have setter/getter and value or writable");
        goto fail;
    }
    d->flags = flags;
    d->value = val;
    d->getter = getter;
    d->setter = setter;
    return 0;
 fail:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, getter);
    JS_FreeValue(ctx, setter);
    return -1;
}
