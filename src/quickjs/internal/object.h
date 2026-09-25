/*
 * QuickJS object and property types
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
#ifndef QUICKJS_OBJECT_H
#define QUICKJS_OBJECT_H

#include "runtime.h"
#include "function.h"

typedef struct JSRegExp {
    JSString *pattern;
    JSString *bytecode; /* also contains the flags */
} JSRegExp;

typedef struct JSProxyData {
    JSValue target;
    JSValue handler;
    uint8_t is_func;
    uint8_t is_revoked;
} JSProxyData;

typedef struct JSArrayBuffer {
    int byte_length; /* 0 if detached */
    int max_byte_length; /* -1 if not resizable; >= byte_length otherwise */
    uint8_t detached;
    uint8_t shared; /* if shared, the array buffer cannot be detached */
    uint8_t *data; /* NULL if detached */
    struct list_head array_list;
    void *opaque;
    JSFreeArrayBufferDataFunc *free_func;
} JSArrayBuffer;

typedef struct JSTypedArray {
    struct list_head link; /* link to arraybuffer */
    JSObject *obj; /* back pointer to the TypedArray/DataView object */
    JSObject *buffer; /* based array buffer */
    uint32_t offset; /* byte offset in the array buffer */
    uint32_t length; /* byte length in the array buffer */
    BOOL track_rab; /* auto-track length of backing array buffer */
} JSTypedArray;

typedef struct JSGlobalObject {
    JSValue uninitialized_vars; /* hidden object containing the list of uninitialized variables */
} JSGlobalObject;

typedef struct JSProperty {
    union {
        JSValue value;      /* JS_PROP_NORMAL */
        struct {            /* JS_PROP_GETSET */
            JSObject *getter; /* NULL if undefined */
            JSObject *setter; /* NULL if undefined */
        } getset;
        JSVarRef *var_ref;  /* JS_PROP_VARREF */
        struct {            /* JS_PROP_AUTOINIT */
            /* in order to use only 2 pointers, we compress the realm
               and the init function pointer */
            uintptr_t realm_and_id; /* realm and init_id (JS_AUTOINIT_ID_x)
                                       in the 2 low bits */
            void *opaque;
        } init;
    } u;
} JSProperty;

#define JS_PROP_INITIAL_SIZE 2
#define JS_PROP_INITIAL_HASH_SIZE 4 /* must be a power of two */

typedef struct JSShapeProperty {
    uint32_t hash_next : 26; /* 0 if last in list */
    uint32_t flags : 6;   /* JS_PROP_XXX */
    JSAtom atom; /* JS_ATOM_NULL = free property entry */
} JSShapeProperty;

struct JSShape {
    JSGCObjectHeader header;
    /* true if the shape is inserted in the shape hash table. If not,
       JSShape.hash is not valid */
    uint8_t is_hashed;
    uint32_t hash; /* current hash value */
    uint32_t prop_hash_mask; /* >= 2 */
    int prop_size; /* allocated properties */
    int prop_count; /* include deleted properties */
    int deleted_prop_count;
    JSShape *shape_hash_next; /* in JSRuntime.shape_hash[h] list */
    JSObject *proto;
    uint32_t hash_table[]; /* prop_hash_mask + 1 elements */
    /* followed by JSShapeProperty prop[prop_size]; */
};

struct JSObject {
    JSGCObjectHeader header;
    /* TRUE if the array prototype is "normal":
       - no small index properties which are get/set or non writable
       - its prototype is Object.prototype
       - Object.prototype has no small index properties which are get/set or non writable
       - the prototype of Object.prototype is null (always true as it is immutable)
    */
    uint8_t is_std_array_prototype : 1;
    
    uint8_t extensible : 1;
    uint8_t free_mark : 1; /* only used when freeing objects with cycles */
    uint8_t is_exotic : 1; /* TRUE if object has exotic property handlers */
    uint8_t fast_array : 1; /* TRUE if u.array is used for get/put (for JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS, JS_CLASS_MAPPED_ARGUMENTS and typed arrays) */
    uint8_t is_constructor : 1; /* TRUE if object is a constructor function */
    uint8_t has_immutable_prototype : 1; /* cannot modify the prototype */
    uint8_t tmp_mark : 1; /* used in JS_WriteObjectRec() */
    uint8_t is_HTMLDDA : 1; /* specific annex B IsHtmlDDA behavior */
    uint16_t class_id; /* see JS_CLASS_x */
    /* count the number of weak references to this object. The object
       structure is freed only if header.ref_count = 0 and
       weakref_count = 0 */
    uint32_t weakref_count; 
    JSShape *shape; /* prototype and property names + flag */
    JSProperty *prop; /* array of properties */
    union {
        void *opaque;
        struct JSBoundFunction *bound_function; /* JS_CLASS_BOUND_FUNCTION */
        struct JSCFunctionDataRecord *c_function_data_record; /* JS_CLASS_C_FUNCTION_DATA */
        struct JSForInIterator *for_in_iterator; /* JS_CLASS_FOR_IN_ITERATOR */
        struct JSArrayBuffer *array_buffer; /* JS_CLASS_ARRAY_BUFFER, JS_CLASS_SHARED_ARRAY_BUFFER */
        struct JSTypedArray *typed_array; /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_DATAVIEW */
        struct JSMapState *map_state;   /* JS_CLASS_MAP..JS_CLASS_WEAKSET */
        struct JSMapIteratorData *map_iterator_data; /* JS_CLASS_MAP_ITERATOR, JS_CLASS_SET_ITERATOR */
        struct JSArrayIteratorData *array_iterator_data; /* JS_CLASS_ARRAY_ITERATOR, JS_CLASS_STRING_ITERATOR */
        struct JSRegExpStringIteratorData *regexp_string_iterator_data; /* JS_CLASS_REGEXP_STRING_ITERATOR */
        struct JSGeneratorData *generator_data; /* JS_CLASS_GENERATOR */
        struct JSIteratorConcatData *iterator_concat_data; /* JS_CLASS_ITERATOR_CONCAT */
        struct JSIteratorHelperData *iterator_helper_data; /* JS_CLASS_ITERATOR_HELPER */
        struct JSIteratorWrapData *iterator_wrap_data; /* JS_CLASS_ITERATOR_WRAP */
        struct JSProxyData *proxy_data; /* JS_CLASS_PROXY */
        struct JSPromiseData *promise_data; /* JS_CLASS_PROMISE */
        struct JSPromiseFunctionData *promise_function_data; /* JS_CLASS_PROMISE_RESOLVE_FUNCTION, JS_CLASS_PROMISE_REJECT_FUNCTION */
        struct JSAsyncFunctionState *async_function_data; /* JS_CLASS_ASYNC_FUNCTION_RESOLVE, JS_CLASS_ASYNC_FUNCTION_REJECT */
        struct JSAsyncFromSyncIteratorData *async_from_sync_iterator_data; /* JS_CLASS_ASYNC_FROM_SYNC_ITERATOR */
        struct JSAsyncGeneratorData *async_generator_data; /* JS_CLASS_ASYNC_GENERATOR */
        struct { /* JS_CLASS_BYTECODE_FUNCTION: 12/24 bytes */
            /* also used by JS_CLASS_GENERATOR_FUNCTION, JS_CLASS_ASYNC_FUNCTION and JS_CLASS_ASYNC_GENERATOR_FUNCTION */
            struct JSFunctionBytecode *function_bytecode;
            JSVarRef **var_refs;
            JSObject *home_object; /* for 'super' access */
        } func;
        struct { /* JS_CLASS_C_FUNCTION: 12/20 bytes */
            JSContext *realm;
            JSCFunctionType c_function;
            uint8_t length;
            uint8_t cproto;
            int16_t magic;
        } cfunc;
        /* array part for fast arrays and typed arrays */
        struct { /* JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS, JS_CLASS_MAPPED_ARGUMENTS, JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
            union {
                uint32_t size;          /* JS_CLASS_ARRAY */
                struct JSTypedArray *typed_array; /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
            } u1;
            union {
                JSValue *values;        /* JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS */
                JSVarRef **var_refs;     /* JS_CLASS_MAPPED_ARGUMENTS */
                void *ptr;              /* JS_CLASS_UINT8C_ARRAY..JS_CLASS_FLOAT64_ARRAY */
                int8_t *int8_ptr;       /* JS_CLASS_INT8_ARRAY */
                uint8_t *uint8_ptr;     /* JS_CLASS_UINT8_ARRAY, JS_CLASS_UINT8C_ARRAY */
                int16_t *int16_ptr;     /* JS_CLASS_INT16_ARRAY */
                uint16_t *uint16_ptr;   /* JS_CLASS_UINT16_ARRAY */
                int32_t *int32_ptr;     /* JS_CLASS_INT32_ARRAY */
                uint32_t *uint32_ptr;   /* JS_CLASS_UINT32_ARRAY */
                int64_t *int64_ptr;     /* JS_CLASS_INT64_ARRAY */
                uint64_t *uint64_ptr;   /* JS_CLASS_UINT64_ARRAY */
                uint16_t *fp16_ptr;     /* JS_CLASS_FLOAT16_ARRAY */
                float *float_ptr;       /* JS_CLASS_FLOAT32_ARRAY */
                double *double_ptr;     /* JS_CLASS_FLOAT64_ARRAY */
            } u;
            uint32_t count; /* <= 2^31-1. 0 for a detached typed array */
        } array;    /* 12/20 bytes */
        JSRegExp regexp;    /* JS_CLASS_REGEXP: 8/16 bytes */
        JSValue object_data;    /* for JS_SetObjectData(): 8/16/16 bytes */
        JSGlobalObject global_object;
    } u;
};

JSValue JS_ToObject(JSContext *ctx, JSValueConst val);

int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);

JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab);
int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                            JSValueConst proto_val,
                            BOOL throw_flag);
JSValue JS_GetOwnPropertyNames2(JSContext *ctx, JSValueConst obj1,
                                int flags, int kind);
int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                              JSObject *p, JSAtom prop);
void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);

int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                   JSValueConst desc);
BOOL check_define_prop_flags(int prop_flags, int flags);
int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                               JSPropertyEnum **ptab,
                                               uint32_t *plen,
                                               JSObject *p, int flags);
__exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                JSValueConst obj);

static inline size_t get_shape_size(size_t hash_size, size_t prop_size)
{
    return sizeof(JSShape) + hash_size * sizeof(uint32_t) +
        prop_size * sizeof(JSShapeProperty);
}


static inline JSShapeProperty *get_shape_prop(JSShape *sh)
{
    return (JSShapeProperty *)((uint32_t *)(sh + 1) + sh->prop_hash_mask + 1);
}

static force_inline JSShapeProperty *find_own_property1(JSObject *p,
                                                        JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            return pr;
        }
        h = pr->hash_next;
    }
    return NULL;
}

static force_inline JSShapeProperty *find_own_property(JSProperty **ppr,
                                                       JSObject *p,
                                                       JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            *ppr = &p->prop[h - 1];
            /* the compiler should be able to assume that pr != NULL here */
            return pr;
        }
        h = pr->hash_next;
    }
    *ppr = NULL;
    return NULL;
}

JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                              JSProperty *props);
JSShape *js_dup_shape(JSShape *sh);
int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);
no_inline JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                                 int hash_size, int prop_size);
JSObject *get_proto_obj(JSValueConst proto_val);
int add_shape_property(JSContext *ctx, JSShape **psh,
                       JSObject *p, JSAtom atom, int prop_flags);
JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx);
JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                              JSValueConst defaultConstructor);
__exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                JSValueConst obj);

int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                 int64_t idx, JSValue val, int flags);

int js_update_property_flags(JSContext *ctx, JSObject *p,
                             JSShapeProperty **pprs, int flags);
JSProperty *add_property(JSContext *ctx,
                         JSObject *p, JSAtom prop, int prop_flags);
int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                              JSAtom prop, JSAutoInitIDEnum id,
                              void *opaque, int flags);

JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                            JSValue prop);
int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                        JSValue prop, JSValue val, int flags);

int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags);


int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval);
BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                       JSValue **arrpp, uint32_t *countp);
JSValue js_allocate_fast_array(JSContext *ctx, int64_t len);

int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags);


static force_inline BOOL can_extend_fast_array(JSObject *p)
{
    JSObject *proto;
    if (!p->extensible)
        return FALSE;
    proto = p->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}

JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);


#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)

#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

static inline BOOL JS_IsHTMLDDA(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return p->is_HTMLDDA;
}

int JS_AddBrand(JSContext *ctx, JSValueConst obj, JSValueConst home_obj);
int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                        JSProperty *pr, JSShapeProperty *prs);
int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func);
int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                        JSAtom name, int flags);
int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                JSValueConst str, int flags);
int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                          JSValueConst name, JSValue val);
int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags);
int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);
__maybe_unused void JS_DumpShapes(JSRuntime *rt);
int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                           JSValueConst name);
JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);
int JS_NewClass1(JSRuntime *rt, JSClassID class_id,
                 const JSClassDef *class_def, JSAtom name);
JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                    JSClassID class_id, int n_alloc_props);
void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj);
int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                       JSValueConst name, JSValue val);
JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop);
int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom);
no_inline __exception int convert_fast_array_to_array(JSContext *ctx,
                                                      JSObject *p);
int delete_property(JSContext *ctx, JSObject *p, JSAtom atom);
void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
void free_zero_refcount(JSRuntime *rt);
void gc_decref(JSRuntime *rt);
int init_shape_hash(JSRuntime *rt);
void js_array_finalizer(JSRuntime *rt, JSValue val);
void js_array_mark(JSRuntime *rt, JSValueConst val,
                   JS_MarkFunc *mark_func);
JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr);
JSContext *js_autoinit_get_realm(JSProperty *pr);
JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab);
void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
void js_free_shape_null(JSRuntime *rt, JSShape *sh);
void js_object_data_finalizer(JSRuntime *rt, JSValue val);
void js_object_data_mark(JSRuntime *rt, JSValueConst val,
                         JS_MarkFunc *mark_func);
void set_cycle_flag(JSContext *ctx, JSValueConst obj);

JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                  JSAtom atom, BOOL is_lexical);

__exception int JS_CopyDataProperties(JSContext *ctx, JSValueConst target, JSValueConst source, JSValueConst excluded, BOOL setprop);

void js_global_object_finalizer(JSRuntime *rt, JSValue obj);

void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                           JS_MarkFunc *mark_func);

#endif
